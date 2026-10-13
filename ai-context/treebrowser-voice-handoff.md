# Treebrowser Voice Control — Handoff

## Goal
Add voice control for the TreeBrowser plugin via geanyvosk, using geanycontrol as the IPC bus. No voice-specific code in treebrowser itself.

## Architecture
- GLib signals on `geany->object` carry commands
- treebrowser.c registers and handles `treebrowser-*` signals
- geanycontrol.c registers the same signals (load-order safety) and forwards them from the Unix socket
- geanyvosk.c emits signals via `ctrl_emit()` and handles `VSTATE_TREEBROWSER` mode
- Special case: popup menu — treebrowser emits `treebrowser-menu-ready` (with GtkWidget* menu arg) just before showing it; geanyvosk stores the menu in `tb_pending_menu`, then enters `menu_mode` with the already-shown menu

## Signals to register on geany->object

| Signal name              | Arg type      | Handler in treebrowser         |
|--------------------------|---------------|-------------------------------|
| treebrowser-refresh      | none          | on_treebrowser_refresh_signal |
| treebrowser-home         | none          | on_signal_tb_home             |
| treebrowser-project-root | none          | on_signal_tb_project          |
| treebrowser-set-path     | none          | on_signal_tb_set_path         |
| treebrowser-navigate     | G_TYPE_INT    | on_signal_tb_navigate         |
| treebrowser-activate     | none          | on_signal_tb_activate         |
| treebrowser-focus        | none          | on_signal_tb_focus            |
| treebrowser-popup-menu   | none          | on_signal_tb_popup_menu       |
| treebrowser-menu-ready   | G_TYPE_POINTER| (emitted by treebrowser, handled by geanyvosk) |

## Status: DONE

### treebrowser/src/treebrowser.c — COMPLETE
All 7 new signal handler functions added just before `plugin_init` (after `on_treebrowser_refresh_signal`):
- `on_signal_tb_home` — calls `on_button_go_home()`
- `on_signal_tb_project` — calls `on_button_go_project()`
- `on_signal_tb_set_path` — calls `on_button_current_path()`
- `on_signal_tb_navigate(delta)` — emits `move-cursor` on treeview with GTK_MOVEMENT_DISPLAY_LINES
- `on_signal_tb_activate` — gets cursor path, calls `on_treeview_row_activated`
- `on_signal_tb_focus` — sets sidebar page + `gtk_widget_grab_focus(treeview)`
- `on_signal_tb_popup_menu` — creates menu, emits `treebrowser-menu-ready`, shows menu with `gtk_menu_popup_at_widget`

In `plugin_init`, after the geanycontrol-refresh block: signal registration loop + all 8 `plugin_signal_connect` calls (auto-disconnect on unload, no cleanup needed). Done.

`plugin_cleanup` — no changes needed (plugin_signal_connect auto-disconnects).

## Status: TODO

### geanycontrol/src/geanycontrol.c — NOT DONE

**Change 1:** In `gc_register_signals()`, add to the `sigs[]` array before `{ NULL, 0 }`:
```c
        { "treebrowser-refresh",       G_TYPE_NONE    },
        { "treebrowser-home",          G_TYPE_NONE    },
        { "treebrowser-project-root",  G_TYPE_NONE    },
        { "treebrowser-set-path",      G_TYPE_NONE    },
        { "treebrowser-navigate",      G_TYPE_INT     },
        { "treebrowser-activate",      G_TYPE_NONE    },
        { "treebrowser-focus",         G_TYPE_NONE    },
        { "treebrowser-popup-menu",    G_TYPE_NONE    },
        { "treebrowser-menu-ready",    G_TYPE_POINTER },
```

**Change 2:** In `dispatch_command()`, add before the final `return g_strdup("error: unknown command\n")`:
```c
    if (strcmp(line, "treebrowser-refresh") == 0) {
        g_signal_emit_by_name(geany->object, "treebrowser-refresh");
        return g_strdup("ok\n");
    }
    if (strcmp(line, "treebrowser-home") == 0) {
        g_signal_emit_by_name(geany->object, "treebrowser-home");
        return g_strdup("ok\n");
    }
    if (strcmp(line, "treebrowser-project-root") == 0) {
        g_signal_emit_by_name(geany->object, "treebrowser-project-root");
        return g_strdup("ok\n");
    }
    if (strcmp(line, "treebrowser-set-path") == 0) {
        g_signal_emit_by_name(geany->object, "treebrowser-set-path");
        return g_strdup("ok\n");
    }
    if (strcmp(line, "treebrowser-activate") == 0) {
        g_signal_emit_by_name(geany->object, "treebrowser-activate");
        return g_strdup("ok\n");
    }
    if (strcmp(line, "treebrowser-focus") == 0) {
        g_signal_emit_by_name(geany->object, "treebrowser-focus");
        return g_strdup("ok\n");
    }
    if (strcmp(line, "treebrowser-popup-menu") == 0) {
        g_signal_emit_by_name(geany->object, "treebrowser-popup-menu");
        return g_strdup("ok\n");
    }
    if (g_str_has_prefix(line, "treebrowser-navigate ")) {
        gint delta = (gint)g_ascii_strtoll(line + 21, NULL, 10);
        g_signal_emit_by_name(geany->object, "treebrowser-navigate", delta);
        return g_strdup("ok\n");
    }
```

Also update geanycontrol's command protocol comment at the top of the file to document the new commands.

---

### geanyvosk/src/geanyvosk.c — NOT DONE

**Change 1:** Add `VSTATE_TREEBROWSER` to VoiceState enum after `VSTATE_MENU`:
```c
    VSTATE_TREEBROWSER,
```

**Change 2:** Add case in `status_update()` after `VSTATE_MENU` case:
```c
        case VSTATE_TREEBROWSER:
            color = "#44ff88"; text = "tree";
            icon_opacity = 1.0;
            break;
```

**Change 3:** Add `tb_pending_menu` to the `#ifdef HAVE_VOSK` state variables block (after `menu_item_idx = -1` line):
```c
static GtkWidget *tb_pending_menu  = NULL;
```

**Change 4:** Extend `ctrl_emit()` — the long if/else chain currently ends with `else if (strcmp(cmd, "refresh") == 0)`. Add after the last existing `else if` branch:
```c
    else if (strcmp(cmd, "treebrowser-refresh") == 0)
        g_signal_emit_by_name(obj, "treebrowser-refresh");
    else if (strcmp(cmd, "treebrowser-home") == 0)
        g_signal_emit_by_name(obj, "treebrowser-home");
    else if (strcmp(cmd, "treebrowser-project-root") == 0)
        g_signal_emit_by_name(obj, "treebrowser-project-root");
    else if (strcmp(cmd, "treebrowser-set-path") == 0)
        g_signal_emit_by_name(obj, "treebrowser-set-path");
    else if (strcmp(cmd, "treebrowser-activate") == 0)
        g_signal_emit_by_name(obj, "treebrowser-activate");
    else if (strcmp(cmd, "treebrowser-focus") == 0)
        g_signal_emit_by_name(obj, "treebrowser-focus");
    else if (strcmp(cmd, "treebrowser-popup-menu") == 0)
        g_signal_emit_by_name(obj, "treebrowser-popup-menu");
    else if (g_str_has_prefix(cmd, "treebrowser-navigate ")) {
        gint delta = (gint)g_ascii_strtoll(cmd + 21, NULL, 10);
        g_signal_emit_by_name(obj, "treebrowser-navigate", delta);
    }
```

**Change 5:** Add `on_tb_menu_ready` handler — insert inside `#ifdef HAVE_VOSK`, just before `on_status_icon_click`:
```c
static void on_tb_menu_ready(G_GNUC_UNUSED GObject *obj, GtkWidget *menu,
                             G_GNUC_UNUSED gpointer data)
{
    tb_pending_menu = menu;
}
```

**Change 6:** In `vosk_handle_command`, add treebrowser mode block after the `menu_mode` block and before the `agent_composing` block:
```c
    /* Treebrowser mode: voice navigates the file tree */
    if (voice_state == VSTATE_TREEBROWSER) {
        const gchar *dw = cfg_deact_word ? cfg_deact_word : DEACTIVATE_DEFAULT;
        if (strstr(text, dw) || strstr(text, "cancel")) {
            set_voice_state(VSTATE_ACTIVE);
            return;
        }
        if (strstr(text, "refresh")) {
            ctrl_emit("treebrowser-refresh");
            return;
        }
        if (strstr(text, "home")) {
            ctrl_emit("treebrowser-home");
            return;
        }
        if (strstr(text, "project")) {
            ctrl_emit("treebrowser-project-root");
            return;
        }
        if (strstr(text, "set path") || strstr(text, "current path") ||
            strstr(text, "current file")) {
            ctrl_emit("treebrowser-set-path");
            return;
        }
        if (strcmp(text, "go") == 0 || strstr(text, "open") ||
            strstr(text, "activate")) {
            ctrl_emit("treebrowser-activate");
            return;
        }
        if (strcmp(text, "menu") == 0 || strstr(text, "context menu") ||
            strstr(text, "popup")) {
            tb_pending_menu = NULL;
            ctrl_emit("treebrowser-popup-menu");
            if (tb_pending_menu) {
                open_menu       = tb_pending_menu;
                tb_pending_menu = NULL;
                menu_item_list  = menu_collect_items(open_menu);
                menu_item_idx   = menu_item_list ? 0 : -1;
                menu_mode       = TRUE;
                set_voice_state(VSTATE_MENU);
                if (menu_item_list)
                    gtk_menu_shell_select_item(GTK_MENU_SHELL(open_menu),
                                              g_list_nth_data(menu_item_list, 0));
            }
            return;
        }
        if (strstr(text, "down")) {
            gint  count = word_to_count(strstr(text, "down") + 4);
            gchar *cmd  = g_strdup_printf("treebrowser-navigate %d", count);
            ctrl_emit(cmd);
            g_free(cmd);
            return;
        }
        if (strstr(text, "up")) {
            gint  count = word_to_count(strstr(text, "up") + 2);
            gchar *cmd  = g_strdup_printf("treebrowser-navigate -%d", count);
            ctrl_emit(cmd);
            g_free(cmd);
            return;
        }
        return;
    }
```

**Change 7:** Add "tree browser" entry command — after the `"menu "` block and before `vosk_handle_skill_command`:
```c
    /* "tree browser" / "treebrowser" — enter file tree navigation mode */
    if (strstr(text, "tree browser") || strstr(text, "treebrowser")) {
        ctrl_emit("treebrowser-focus");
        set_voice_state(VSTATE_TREEBROWSER);
        return;
    }
```

**Change 8:** In `plugin_init`, inside `#ifdef HAVE_VOSK`, before the `ac = g_new0(AudioCapture, 1)` line, add:
```c
    /* Register treebrowser IPC signals and connect the menu-ready handler */
    {
        GType obj_type = G_OBJECT_TYPE(geany->object);
        static const gchar *tb_noarg[] = {
            "treebrowser-refresh", "treebrowser-home", "treebrowser-project-root",
            "treebrowser-set-path", "treebrowser-activate", "treebrowser-focus",
            "treebrowser-popup-menu", NULL
        };
        for (gint i = 0; tb_noarg[i]; i++)
            if (!g_signal_lookup(tb_noarg[i], obj_type))
                g_signal_new(tb_noarg[i], obj_type, G_SIGNAL_RUN_LAST,
                             0, NULL, NULL, NULL, G_TYPE_NONE, 0);
        if (!g_signal_lookup("treebrowser-navigate", obj_type))
            g_signal_new("treebrowser-navigate", obj_type, G_SIGNAL_RUN_LAST,
                         0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_INT);
        if (!g_signal_lookup("treebrowser-menu-ready", obj_type))
            g_signal_new("treebrowser-menu-ready", obj_type, G_SIGNAL_RUN_LAST,
                         0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_POINTER);
        plugin_signal_connect(geany_plugin, geany->object, "treebrowser-menu-ready",
                              FALSE, G_CALLBACK(on_tb_menu_ready), NULL);
    }
```

---

### geanyvosk/README — NOT DONE

Add a "Tree browser control" section between "Menu navigation" and "Dictation" sections:

```
  Tree browser control:
    "tree browser"              — enter tree navigation mode.  Status bar shows
                                  "tree" (green).  The TreeBrowser sidebar panel
                                  is focused automatically.
    "up" / "up <N>"             — move cursor up N rows in the file tree
    "down" / "down <N>"         — move cursor down N rows
    "go" / "open"               — open selected file or expand/collapse directory
    "refresh"                   — refresh the current directory listing
    "home"                      — navigate to home directory (~/)
    "project root"              — navigate to the open project's base path
    "set path"                  — set tree root to the current document's directory
    "menu"                      — show context menu for the selected item; then
                                  navigate it with "up"/"down"/"<item name>"/"go"
    "back in the lamp" / "cancel" — exit tree browser mode

    Example:
      "tree browser"   → focus tree panel, status = "tree"
      "down three"     → move cursor down 3 rows
      "go"             → open the selected file
      "menu"           → show right-click menu, status = "menu"
      "delete"         → highlight Delete item
      "go"             → execute it
```

Also add "tree" to the Status Bar Indicator section:
```
  tree       — TreeBrowser navigation mode active.  Icon solid green.  Voice
               commands navigate the file tree.
```

---

## Build commands (run after all changes)
```
cd /home/teknopaul/github_workspace/geany-plugins/treebrowser && make 2>&1 | tail -5
cd /home/teknopaul/github_workspace/geany-plugins/geanycontrol && make 2>&1 | tail -5
cd /home/teknopaul/github_workspace/geany-plugins/geanyvosk && make 2>&1 | tail -5
```

## Key file locations
- treebrowser.c: `/home/teknopaul/github_workspace/geany-plugins/treebrowser/src/treebrowser.c`
- geanycontrol.c: `/home/teknopaul/github_workspace/geany-plugins/geanycontrol/src/geanycontrol.c`
- geanyvosk.c: `/home/teknopaul/github_workspace/geany-plugins/geanyvosk/src/geanyvosk.c`
- geanyvosk README: `/home/teknopaul/github_workspace/geany-plugins/geanyvosk/README`

## Important existing code context

### ctrl_emit() in geanyvosk.c (currently ends with):
```c
    else if (strcmp(cmd, "refresh") == 0)
        g_signal_emit_by_name(obj, "geanycontrol-refresh");
}
```
Add the treebrowser branches before the closing `}`.

### vosk_handle_command() structure (in order):
1. menu_mode block  ← insert VSTATE_TREEBROWSER block after this
2. agent_composing block
3. dictate_mode block
4. "dictate" entry
5. various command patterns (agent create, new ai prompt, agent type, etc.)
6. Phase 4 CMDS table
7. Phase 7 "menu " → voice_menu_open
8. ← insert "tree browser" entry here
9. vosk_handle_skill_command(text)

### on_status_icon_click() in geanyvosk.c
This already calls vosk_set_active(FALSE) which cleans up menu_mode.
VSTATE_TREEBROWSER also needs to be cleared when the icon is clicked.
Add to the icon click handler inside `if (active_mode)`:
```c
        if (voice_state == VSTATE_TREEBROWSER)
            voice_state = VSTATE_ACTIVE;  /* will be overridden by vosk_set_active below */
```
Actually vosk_set_active(FALSE) calls set_voice_state(VSTATE_IDLE) which overwrites VSTATE_TREEBROWSER anyway. No change needed.

### plugin_init structure (geanyvosk.c)
The `#ifdef HAVE_VOSK` block in plugin_init currently ends before `ac = g_new0(AudioCapture, 1)`.
Insert the signal registration block there.
