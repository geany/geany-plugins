# agent-stop-focus — Plan

## Goal

When Claude Code finishes responding (the `Stop` hook fires), automatically bring
the Geany agent tab to the front and grab keyboard focus so the human can immediately
type a reply.  A clickable status-bar indicator lets the user toggle focus-stealing
on/off at any time.  All implementation lives in `geanyagent/src/geanyagent.c`.

---

## Phase 1 — Socket infrastructure (`GEANY_AGENT_SOCK`)

**Files:** `geanyagent/src/geanyagent.c`

Add a Unix domain socket owned by geanyagent, independent of geanyprogress.
The socket path is exported as `GEANY_AGENT_SOCK` so Claude Code hook scripts
can find it without any extra configuration.

### New globals (add near existing socket globals — there are none yet, add after `agent_pid`)

```c
static GSocketService *agent_sock_service = NULL;
static gchar          *agent_sock_path    = NULL;
```

### `agent_socket_init()` (model after geanyprogress `socket_init()`)

```c
static gboolean agent_socket_init(void)
{
    agent_sock_path = g_strdup_printf("/tmp/geany-agent-%d.sock", (int)getpid());
    GSocketAddress *addr = g_unix_socket_address_new(agent_sock_path);
    agent_sock_service = g_socket_service_new();

    GError *err = NULL;
    if (!g_socket_listener_add_address(G_SOCKET_LISTENER(agent_sock_service),
                                       addr, G_SOCKET_TYPE_STREAM,
                                       G_SOCKET_PROTOCOL_DEFAULT,
                                       NULL, NULL, &err)) {
        g_warning("geanyagent: socket bind failed: %s", err->message);
        g_error_free(err);
        g_object_unref(addr);
        g_object_unref(agent_sock_service);
        agent_sock_service = NULL;
        g_free(agent_sock_path);
        agent_sock_path = NULL;
        return FALSE;
    }
    g_object_unref(addr);

    g_signal_connect(agent_sock_service, "incoming",
                     G_CALLBACK(on_agent_sock_incoming), NULL);
    g_socket_service_start(agent_sock_service);
    g_setenv("GEANY_AGENT_SOCK", agent_sock_path, TRUE);
    return TRUE;
}
```

### `agent_socket_cleanup()`

```c
static void agent_socket_cleanup(void)
{
    if (agent_sock_service) {
        g_socket_service_stop(agent_sock_service);
        g_object_unref(agent_sock_service);
        agent_sock_service = NULL;
    }
    if (agent_sock_path) {
        unlink(agent_sock_path);
        g_unsetenv("GEANY_AGENT_SOCK");
        g_free(agent_sock_path);
        agent_sock_path = NULL;
    }
}
```

### Wire into lifecycle

- In `ga_init()`: call `agent_socket_init()` **after** `create_agent_tab()` (so `tab_index` is valid).
- In `ga_cleanup()`: call `agent_socket_cleanup()` **before** the tab is removed.

### Completion marker

```sh
geany-progress done 1 \
  -r geanyagent/src/geanyagent.c \
  -w "Socket binds to /tmp/geany-agent-PID.sock — confirm GEANY_AGENT_SOCK is exported and visible in a child terminal"
```

---

## Phase 2 — Incoming-event handler

**Files:** `geanyagent/src/geanyagent.c`

Add the `on_agent_sock_incoming` callback (declare before `agent_socket_init` so it
compiles without a forward declaration).  Also add the `focus_on_stop` config bool.

### New global

```c
static gboolean focus_on_stop = TRUE;   /* persisted in config */
```

### Callback

```c
static gboolean on_agent_sock_incoming(G_GNUC_UNUSED GSocketService *svc,
                                        GSocketConnection *conn,
                                        G_GNUC_UNUSED GObject *src,
                                        G_GNUC_UNUSED gpointer data)
{
    GInputStream *in = g_io_stream_get_input_stream(G_IO_STREAM(conn));
    gchar buf[512];
    gssize n = g_input_stream_read(in, buf, sizeof(buf) - 1, NULL, NULL);
    if (n <= 0)
        return TRUE;
    buf[n] = '\0';

    if (strstr(buf, "\"stop\"") && focus_on_stop && agent_term) {
        if (tab_index >= 0)
            gtk_notebook_set_current_page(
                GTK_NOTEBOOK(geany_data->main_widgets->message_window_notebook),
                tab_index);
        g_idle_add(grab_agent_focus_idle, NULL);
    }
    return TRUE;
}
```

The callback is invoked on the GLib main thread by GSocketService, so direct GTK
calls are safe; `g_idle_add` is used for focus because it must run after any
pending notebook-switch redraws settle (same pattern as `ga_send_command()`).

### Config: load and save `focus_on_stop`

In `ga_load_config()`, after loading `use_askpass`:
```c
focus_on_stop = g_key_file_get_boolean(kf, "agent", "focus_on_stop", NULL);
/* default TRUE when key absent: GKeyFile returns FALSE on missing key,
   so treat missing as TRUE */
if (!g_key_file_has_key(kf, "agent", "focus_on_stop", NULL))
    focus_on_stop = TRUE;
```

In `ga_save_config()`, alongside `use_askpass`:
```c
g_key_file_set_boolean(kf, "agent", "focus_on_stop", focus_on_stop);
```

### Completion marker

```sh
geany-progress done 2 \
  -r geanyagent/src/geanyagent.c \
  -w "Test with: printf '{\"event\":\"stop\"}' | nc -U \$GEANY_AGENT_SOCK — agent tab should come to front"
```

---

## Phase 3 — Status-bar toggle button

**Files:** `geanyagent/src/geanyagent.c`

Add a small clickable indicator to the Geany status bar, following the exact pattern
from geanyvosk (use `geany->main_widgets->progressbar` to find the parent box, then
`gtk_box_pack_end`).

### New globals (add near other static widget pointers)

```c
static GtkWidget *focus_event_box = NULL;
static GtkWidget *focus_label     = NULL;
```

### Helper: update the indicator

```c
static void focus_status_update(void)
{
    if (!focus_label)
        return;
    /* ⊙ = U+2299 CIRCLED DOT, ⊗ = U+2297 CIRCLED TIMES */
    const gchar *sym   = focus_on_stop ? "\xe2\x8a\x99" : "\xe2\x8a\x97";
    const gchar *color = focus_on_stop ? "#22aa22"       : "#666666";
    gchar *markup = g_strdup_printf("<span foreground='%s'>%s</span>", color, sym);
    gtk_label_set_markup(GTK_LABEL(focus_label), markup);
    g_free(markup);
}
```

### Click handler

```c
static gboolean on_focus_toggle_click(G_GNUC_UNUSED GtkWidget *w,
                                       GdkEventButton *event,
                                       G_GNUC_UNUSED gpointer data)
{
    if (event->button != 1)
        return FALSE;
    focus_on_stop = !focus_on_stop;
    focus_status_update();
    ga_save_config();
    return TRUE;
}
```

### Widget construction — call from `ga_init()` after `create_agent_tab()`

```c
static void create_focus_toggle(void)
{
    GtkWidget *pb = geany->main_widgets->progressbar;
    if (!pb)
        return;
    GtkWidget *sb_box = gtk_widget_get_parent(pb);
    if (!sb_box || !GTK_IS_BOX(sb_box))
        return;

    focus_label = gtk_label_new(NULL);
    focus_status_update();
    gtk_widget_show(focus_label);

    focus_event_box = gtk_event_box_new();
    gtk_container_add(GTK_CONTAINER(focus_event_box), focus_label);
    gtk_widget_set_tooltip_text(focus_event_box,
                                "Click to toggle auto-focus on agent response");
    gtk_widget_show(focus_event_box);
    gtk_box_pack_end(GTK_BOX(sb_box), focus_event_box, FALSE, FALSE, 4);

    g_signal_connect(focus_event_box, "button-press-event",
                     G_CALLBACK(on_focus_toggle_click), NULL);
}
```

### Cleanup — call from `ga_cleanup()` before notebook page removal

```c
if (focus_event_box) {
    gtk_widget_destroy(focus_event_box);
    focus_event_box = NULL;
    focus_label     = NULL;
}
```

### Completion marker

```sh
geany-progress done 3 \
  -r geanyagent/src/geanyagent.c \
  -w "Check symbol renders correctly at your GTK theme's status-bar font size" \
  -w "Verify config key focus_on_stop persists across plugin reload"
```

---

## Phase 4 — Configure dialog + hook documentation

**Files:** `geanyagent/src/geanyagent.c`

### Configure dialog

Add a checkbox for `focus_on_stop` in `ga_configure()`, mirroring `askpass_check`.

In `ConfigWidgets` struct add:
```c
GtkWidget *focus_check;
```

In `ga_configure()`, after the `askpass_check` block:
```c
cw->focus_check = gtk_check_button_new_with_label(
    _("Auto-focus agent tab when Claude finishes responding (Stop hook)"));
gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cw->focus_check), focus_on_stop);
gtk_widget_set_tooltip_text(cw->focus_check,
    "When enabled and GEANY_AGENT_SOCK receives {\"event\":\"stop\"},\n"
    "the agent tab is brought to the front and focused so you can\n"
    "immediately type a reply.\n\n"
    "Hook entry for ~/.config/claude/settings.json:\n"
    "  \"Stop\": [{\"type\":\"command\",\"command\":\n"
    "    \"printf '{\\\"event\\\":\\\"stop\\\"}' | nc -U \\\"$GEANY_AGENT_SOCK\\\"\"}]");
gtk_box_pack_start(GTK_BOX(vbox), cw->focus_check, FALSE, FALSE, 4);
```

In `on_configure_response()`, after saving `use_askpass`:
```c
focus_on_stop = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cw->focus_check));
focus_status_update();
```

### Hook documentation comment

Add above `ga_init()`:

```c
/*
 * Claude Code Stop-hook integration
 * ----------------------------------
 * geanyagent listens on $GEANY_AGENT_SOCK (Unix domain socket).
 * Add this to ~/.config/claude/settings.json to auto-focus Geany
 * when Claude finishes responding:
 *
 *   "hooks": {
 *     "Stop": [{
 *       "type": "command",
 *       "command": "printf '{\"event\":\"stop\"}' | nc -U \"$GEANY_AGENT_SOCK\" 2>/dev/null || true"
 *     }]
 *   }
 *
 * The hook must be silent (stdout/stderr suppressed) and fast.
 * nc exits immediately after delivering the single message.
 * The toggle in the status bar (⊙/⊗) and the plugin config dialog
 * both control whether incoming stop events actually steal focus.
 */
```

### Completion marker

```sh
geany-progress done 4 \
  -r geanyagent/src/geanyagent.c \
  -w "Add the Stop hook entry to your own settings.json and verify end-to-end: send a prompt, Claude responds, Geany tab comes forward"
```

---

## Summary

| Phase | What changes | Key risk |
|-------|-------------|----------|
| 1 | Socket init/cleanup, `GEANY_AGENT_SOCK` env var | Socket path collision if two Geany instances run |
| 2 | Incoming event handler, `focus_on_stop` config bool | GSocketService thread safety (safe — runs on main thread) |
| 3 | Status-bar ⊙/⊗ toggle, click handler, widget cleanup | Parent widget lookup via `progressbar` (same pattern as geanyvosk) |
| 4 | Configure dialog checkbox, hook doc comment | None — pure UI and docs |

Each phase is a self-contained diff to one file, safe to compile and test independently.
