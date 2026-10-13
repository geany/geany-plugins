/*
 * geanyvosk.c — Geany plugin: voice control via Vosk offline ASR
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <geanyplugin.h>
#include <alsa/asoundlib.h>
#include <gio/gio.h>
#include <string.h>
#include "geanyvosk.h"

#ifdef HAVE_VOSK
#include <vosk_api.h>
#endif

PLUGIN_VERSION_CHECK(224)
PLUGIN_SET_INFO("GeanyVosk", "Voice control for Geany via Vosk offline ASR",
                "0.1", "teknopaul")

GeanyPlugin *geany_plugin;
GeanyData   *geany_data;

/* ------------------------------------------------------------------ */
/* Runtime config (loaded from ~/.config/geany/geanyvosk.conf)         */
/* ------------------------------------------------------------------ */

static gchar *cfg_model_path   = NULL;
static gchar *cfg_alsa_device  = NULL;
static gchar *cfg_wake_word    = NULL;
static gchar *cfg_deact_word   = NULL;
static gchar *cfg_whisper_model = NULL;
static gchar *cfg_whisper_cli   = NULL;

static void load_config(void)
{
    gchar *conf = g_build_filename(g_get_user_config_dir(), "geany",
                                   GEANYVOSK_CONF_FILE, NULL);
    GKeyFile *kf = g_key_file_new();

    if (g_key_file_load_from_file(kf, conf, G_KEY_FILE_NONE, NULL)) {
        cfg_model_path   = g_key_file_get_string(kf, "geanyvosk", "model_path",    NULL);
        cfg_alsa_device  = g_key_file_get_string(kf, "geanyvosk", "alsa_device",  NULL);
        cfg_wake_word    = g_key_file_get_string(kf, "geanyvosk", "wake_word",     NULL);
        cfg_deact_word   = g_key_file_get_string(kf, "geanyvosk", "deactivate_word", NULL);
        cfg_whisper_model = g_key_file_get_string(kf, "geanyvosk", "whisper_model", NULL);
        cfg_whisper_cli   = g_key_file_get_string(kf, "geanyvosk", "whisper_cli",   NULL);
    }

    if (!cfg_model_path)
        cfg_model_path = g_build_filename(g_get_user_data_dir(),
                                          GEANYVOSK_MODEL_DIR, NULL);
    if (!cfg_alsa_device) cfg_alsa_device = g_strdup("pulse");
    if (!cfg_wake_word)   cfg_wake_word   = g_strdup(WAKE_WORD_DEFAULT);
    if (!cfg_deact_word)  cfg_deact_word  = g_strdup(DEACTIVATE_DEFAULT);

    g_key_file_free(kf);
    g_free(conf);
}

static void free_config(void)
{
    g_free(cfg_model_path);    cfg_model_path    = NULL;
    g_free(cfg_alsa_device);   cfg_alsa_device   = NULL;
    g_free(cfg_wake_word);     cfg_wake_word     = NULL;
    g_free(cfg_deact_word);    cfg_deact_word    = NULL;
    g_free(cfg_whisper_model); cfg_whisper_model = NULL;
    g_free(cfg_whisper_cli);   cfg_whisper_cli   = NULL;
}

/* ------------------------------------------------------------------ */
/* ASR state (Vosk)                                                     */
/* ------------------------------------------------------------------ */

#ifdef HAVE_VOSK
static VoskModel      *vosk_model    = NULL;
static VoskRecognizer *vosk_rec      = NULL;  /* current (points to one below) */
static VoskRecognizer *vosk_rec_idle = NULL;  /* grammar-restricted: wake word */
static VoskRecognizer *vosk_rec_cmd  = NULL;  /* full vocabulary: commands */
#endif

#ifdef HAVE_VOSK
static gboolean active_mode      = FALSE;
static gboolean vosk_enabled     = TRUE;
static gboolean agent_composing  = FALSE;
static gboolean dictate_mode     = FALSE;
static gboolean menu_mode        = FALSE;
static GtkWidget *open_menu      = NULL;
static GList     *menu_item_list = NULL;
static gint       menu_item_idx  = -1;
static GtkWidget *tb_pending_menu = NULL;
static void vosk_handle_command(const gchar *text);
static void voice_menu_reset_state(void);

/* Grammar-mode switch requested by the main thread, applied by the audio
   thread which owns the recognizer (avoids a data race on vosk_rec).
   0 = no change, 1 = wake-word grammar (idle), 2 = full free vocabulary. */
static volatile gint pending_grm = 0;

/* Whisper dictation globals (audio-thread-owned except whisper_dictating which is atomic) */
static volatile gint whisper_dictating = 0;
static GAsyncQueue  *wqueue      = NULL;
static GThread      *wthread     = NULL;
static gint16       *wbuf        = NULL;
static gsize         wbuf_len    = 0;
static gboolean      wbuf_speech = FALSE;
static gint          wsilence    = 0;
static gint          wchunk_id   = 0;
#endif
static GtkWidget *status_event_box  = NULL;
static GtkWidget *status_box        = NULL;
static GtkWidget *status_icon       = NULL;
static GtkWidget *status_label      = NULL;
static GtkWidget *voice_toggle_item = NULL;

/* ------------------------------------------------------------------ */
/* Visual state indicator                                               */
/* ------------------------------------------------------------------ */

typedef enum {
    VSTATE_OFF,
    VSTATE_IDLE,
    VSTATE_ACTIVE,
    VSTATE_DICTATING,
    VSTATE_AGENT,
    VSTATE_ARMED,
    VSTATE_MENU,
    VSTATE_TREEBROWSER,
} VoiceState;

static VoiceState voice_state = VSTATE_IDLE;
static guint      blink_id    = 0;
static gboolean   blink_on    = FALSE;

static void status_update(void)
{
    if (!status_label) return;
    const gchar *color;
    const gchar *text;
    gdouble      icon_opacity;

    switch (voice_state) {
        case VSTATE_OFF:
            color = "#666666"; text = "off";
            icon_opacity = 0.25;
            break;
        case VSTATE_IDLE:
            color = blink_on ? "#aaaaaa" : "#555555";
            text  = "idle";
            icon_opacity = blink_on ? 0.5 : 0.2;
            break;
        case VSTATE_ACTIVE:
            color = blink_on ? "#44ff44" : "#22cc22";
            text  = "listening";
            icon_opacity = blink_on ? 1.0 : 0.6;
            break;
        case VSTATE_DICTATING:
            color = "#ff8800"; text = "dictating";
            icon_opacity = 1.0;
            break;
        case VSTATE_AGENT:
            color = "#44aaff"; text = "agent";
            icon_opacity = 1.0;
            break;
        case VSTATE_ARMED:
            color = blink_on ? "#ffcc00" : "#aa8800";
            text  = "say go!";
            icon_opacity = blink_on ? 1.0 : 0.5;
            break;
        case VSTATE_MENU:
            color = "#cc44ff"; text = "menu";
            icon_opacity = 1.0;
            break;
        case VSTATE_TREEBROWSER:
            color = "#44ff88"; text = "tree";
            icon_opacity = 1.0;
            break;
        default:
            color = "#888888"; text = "voice";
            icon_opacity = 0.5;
            break;
    }

    if (status_icon)
        gtk_widget_set_opacity(status_icon, icon_opacity);

    gchar *markup = g_strdup_printf(
        "<span foreground=\"%s\">%s</span>", color, text);
    gtk_label_set_markup(GTK_LABEL(status_label), markup);
    g_free(markup);
}

static void set_voice_state(VoiceState state)
{
    voice_state = state;
    status_update();
}

static gboolean blink_cb(gpointer data)
{
    (void)data;
    blink_on = !blink_on;
    if (voice_state == VSTATE_IDLE || voice_state == VSTATE_ACTIVE ||
        voice_state == VSTATE_ARMED)
        status_update();
    return G_SOURCE_CONTINUE;
}

/* Extract "text" value from Vosk JSON result: {"text": "hello world"} */
#ifdef HAVE_VOSK
static gchar *extract_text_from_json(const gchar *json)
{
    const gchar *key = "\"text\" : \"";
    const gchar *p = strstr(json, key);
    if (!p) {
        key = "\"text\":\"";
        p = strstr(json, key);
    }
    if (!p) return g_strdup("");
    p += strlen(key);
    const gchar *end = strchr(p, '"');
    if (!end) return g_strdup("");
    return g_strndup(p, (gsize)(end - p));
}
#endif /* HAVE_VOSK */

#ifdef HAVE_VOSK
static void vosk_set_active(gboolean on)
{
    active_mode = on;
    if (!on) {
        agent_composing = FALSE;
        if (menu_mode)
            voice_menu_reset_state();
    }
    /* Idle: bias the recognizer to the wake phrase so it is reliably caught.
       Active: open up to the full graph for free-form commands/dictation. */
    g_atomic_int_set(&pending_grm, on ? 2 : 1);
    set_voice_state(on ? VSTATE_ACTIVE : VSTATE_IDLE);
}

static void on_voice_toggle(GtkCheckMenuItem *item, gpointer data)
{
    (void)data;
    vosk_enabled = gtk_check_menu_item_get_active(item);
    if (!vosk_enabled) {
        active_mode     = FALSE;
        dictate_mode    = FALSE;
        agent_composing = FALSE;
    } else {
        /* Re-enabled — go back to listening for the wake phrase. */
        g_atomic_int_set(&pending_grm, 1);
    }
    set_voice_state(vosk_enabled ? VSTATE_IDLE : VSTATE_OFF);
}
#endif /* HAVE_VOSK */

#ifdef HAVE_VOSK
static gboolean vosk_dispatch(gpointer raw_json)
{
    if (!vosk_enabled) {
        g_free(raw_json);
        return G_SOURCE_REMOVE;
    }

    gchar *text = extract_text_from_json((const gchar *)raw_json);

    if (!active_mode) {
        if (strstr(text, cfg_wake_word ? cfg_wake_word : WAKE_WORD_DEFAULT))
            vosk_set_active(TRUE);
    } else {
        if (strstr(text, cfg_deact_word ? cfg_deact_word : DEACTIVATE_DEFAULT))
            vosk_set_active(FALSE);
        else if (*text)
            vosk_handle_command(text);
    }

    g_free(text);
    g_free(raw_json);
    return G_SOURCE_REMOVE;
}
#endif /* HAVE_VOSK */

/* ------------------------------------------------------------------ */
/* geanycontrol in-process signal helper                               */
/* ------------------------------------------------------------------ */

/* Dispatch a geanycontrol-style command via in-process GLib signals.
 * Using the socket from the GTK main thread deadlocks because geanycontrol's
 * socket server also runs on the main thread and can never respond while we
 * are blocking on a read. */
#ifdef HAVE_VOSK
static void ctrl_emit(const gchar *cmd)
{
    GObject *obj = G_OBJECT(geany->object);

    if (!cmd || !*cmd)
        return;

    if (g_str_has_prefix(cmd, "send-to-agent "))
        g_signal_emit_by_name(obj, "geanycontrol-send-to-agent", cmd + 14);
    else if (strcmp(cmd, "agent-enter") == 0)
        g_signal_emit_by_name(obj, "geanycontrol-send-to-agent", "\n");
    else if (g_str_has_prefix(cmd, "switch-tab "))
        g_signal_emit_by_name(obj, "geanycontrol-switch-tab", cmd + 11);
    else if (g_str_has_prefix(cmd, "append-text "))
        g_signal_emit_by_name(obj, "geanycontrol-append-text", cmd + 12);
    else if (strcmp(cmd, "save-all") == 0)
        g_signal_emit_by_name(obj, "geanycontrol-save-all");
    else if (g_str_has_prefix(cmd, "open-file "))
        g_signal_emit_by_name(obj, "geanycontrol-open-file", cmd + 10);
    else if (g_str_has_prefix(cmd, "close-file "))
        g_signal_emit_by_name(obj, "geanycontrol-close-file", cmd + 11);
    else if (strcmp(cmd, "close-file") == 0) {
        GeanyDocument *doc = document_get_current();
        if (doc && doc->file_name)
            g_signal_emit_by_name(obj, "geanycontrol-close-file", doc->file_name);
    }
    else if (strcmp(cmd, "refresh") == 0)
        g_signal_emit_by_name(obj, "geanycontrol-refresh");
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
}
#endif /* HAVE_VOSK */

/* ------------------------------------------------------------------ */
/* Command dispatch (Phase 4)                                           */
/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* Phase 5 — skill/prompt file creation & dictation mode               */
/* ------------------------------------------------------------------ */

#ifdef HAVE_VOSK

static const gchar SKILL_TEMPLATE[] =
    "---\n"
    "name: %s\n"
    "description: \n"
    "---\n"
    "\n"
    "<objective>\n"
    "\n"
    "</objective>\n"
    "\n"
    "<process>\n"
    "\n"
    "</process>\n";

/* "my cool skill" → "my-cool-skill" (space-separated words to kebab-case) */
static gchar *extract_name_after(const gchar *text, const gchar *prefix)
{
    const gchar *p = strstr(text, prefix);
    if (!p) return NULL;
    p += strlen(prefix);
    while (*p == ' ') p++;
    gchar *name = g_strstrip(g_strdup(p));
    /* replace spaces with dashes */
    for (gchar *c = name; *c; c++)
        if (*c == ' ') *c = '-';
    return name;
}

/* "my project dot md" → "my-project.md" */
static gchar *voice_to_filename(const gchar *spoken)
{
    gchar *s = g_strdup(spoken);
    /* replace " dot " with "." */
    gchar **parts = g_strsplit(s, " dot ", -1);
    g_free(s);
    s = g_strjoinv(".", parts);
    g_strfreev(parts);
    /* replace remaining spaces with dashes */
    for (gchar *c = s; *c; c++)
        if (*c == ' ') *c = '-';
    return g_strstrip(s);
}

/* Fuzzy-resolve a voiced path against the filesystem.
 * Splits on " slash ", matches each directory component case-insensitively,
 * falls back to the spoken component when no match exists on disk.
 * The last token is treated as a filename and used verbatim (after " dot " → "."). */
static gchar *fuzzy_resolve_path(const gchar *spoken, const gchar *base)
{
    gchar **parts = g_strsplit(spoken, " slash ", -1);
    gint    n     = g_strv_length(parts);
    GString *res  = g_string_new(base);

    for (gint i = 0; i < n; i++) {
        gchar *part = g_strstrip(g_strdup(parts[i]));
        /* handle " dot " within a single path component */
        gchar **dp = g_strsplit(part, " dot ", -1);
        g_free(part);
        part = g_strjoinv(".", dp);
        g_strfreev(dp);
        /* spaces → dashes */
        for (gchar *c = part; *c; c++)
            if (*c == ' ') *c = '-';

        if (i < n - 1) {
            /* directory component: try fuzzy case-insensitive substring match */
            gchar *lower   = g_utf8_strdown(part, -1);
            GDir  *gd      = g_dir_open(res->str, 0, NULL);
            gchar *matched = NULL;
            if (gd) {
                const gchar *entry;
                while ((entry = g_dir_read_name(gd))) {
                    gchar *le = g_utf8_strdown(entry, -1);
                    if (strstr(le, lower)) {
                        g_free(matched);
                        matched = g_strdup(entry);
                        g_free(le);
                        break;
                    }
                    g_free(le);
                }
                g_dir_close(gd);
            }
            g_free(lower);
            g_string_append_c(res, G_DIR_SEPARATOR);
            g_string_append(res, matched ? matched : part);
            g_free(matched);
        } else {
            g_string_append_c(res, G_DIR_SEPARATOR);
            g_string_append(res, part);
        }
        g_free(part);
    }

    g_strfreev(parts);
    return g_string_free(res, FALSE);
}

static void vosk_create_and_open(const gchar *path, const gchar *content)
{
    GError *err = NULL;
    if (!g_file_set_contents(path, content, -1, &err)) {
        msgwin_status_add("GeanyVosk: cannot create %s: %s",
                          path, err ? err->message : "unknown");
        if (err) g_error_free(err);
        return;
    }
    gchar *cmd = g_strdup_printf("open-file %s", path);
    ctrl_emit(cmd);
    g_free(cmd);
}

/* ------------------------------------------------------------------ */
/* Phase 4 — geanycontrol UI command dispatch                          */
/* ------------------------------------------------------------------ */

/* Map spoken number words to digit strings and counts */
static const struct { const gchar *word; const gchar *digit; } DIGIT_WORDS[] = {
    { "zero", "0" }, { "one", "1" }, { "two", "2" }, { "three", "3" },
    { "four", "4" }, { "five", "5" }, { "six", "6" }, { "seven", "7" },
    { "eight", "8" }, { "nine", "9" }, { NULL, NULL }
};

static const gchar *word_to_digit(const gchar *text)
{
    for (gint i = 0; DIGIT_WORDS[i].word; i++)
        if (strstr(text, DIGIT_WORDS[i].word))
            return DIGIT_WORDS[i].digit;
    return NULL;
}

/* Returns repetition count for arrow-key commands; defaults to 1 */
static gint word_to_count(const gchar *text)
{
    if (!text) return 1;
    if (strstr(text, "ten"))   return 10;
    if (strstr(text, "nine"))  return 9;
    if (strstr(text, "eight")) return 8;
    if (strstr(text, "seven")) return 7;
    if (strstr(text, "six"))   return 6;
    if (strstr(text, "five"))  return 5;
    if (strstr(text, "four"))  return 4;
    if (strstr(text, "three")) return 3;
    if (strstr(text, "two"))   return 2;
    return 1;
}

static void ctrl_emit_append(const gchar *text)
{
    gchar *cmd = g_strdup_printf("append-text %s", text);
    ctrl_emit(cmd);
    g_free(cmd);
}

/* Dictation keyword substitution table: spoken phrase → inserted text.
   Checked in order; first match wins. Trailing space is added after non-punctuation. */
typedef struct { const gchar *spoken; const gchar *insert; gboolean add_space; } DictKw;

static const DictKw DICT_KW[] = {
    { "full stop",        ".",    FALSE },
    { "comma",            ",",    FALSE },
    { "question mark",   "?",    FALSE },
    { "exclamation mark","!",    FALSE },
    { "colon",            ":",    FALSE },
    { "semicolon",        ";",    FALSE },
    { "open bracket",    "(",    FALSE },
    { "close bracket",   ")",    FALSE },
    { "new line",         "\n",   FALSE },
    { "new paragraph",   "\n\n", FALSE },
    { "start list",       "\n- ", FALSE },
    { "list item",        "\n- ", FALSE },
    { "tab",              "\t",   FALSE },
    { NULL, NULL, FALSE }
};

static void dictate_insert(const gchar *text)
{
    /* Check for keyword substitutions first */
    for (const DictKw *kw = DICT_KW; kw->spoken; kw++) {
        if (strstr(text, kw->spoken)) {
            ctrl_emit_append(kw->insert);
            return;
        }
    }
    /* Plain dictated text: append with trailing space */
    gchar *cmd = g_strdup_printf("append-text %s ", text);
    ctrl_emit(cmd);
    g_free(cmd);
}

typedef struct { const gchar *pattern; const gchar *ctrl_cmd; } CmdEntry;

static const CmdEntry CMDS[] = {
    { "switch to agent", "switch-tab Agent" },
    { "switch to cli",   "switch-tab CLI"   },
    { "go to agent",     "switch-tab Agent" },
    { "go to cli",       "switch-tab CLI"   },
    { "show agent",      "switch-tab Agent" },
    { "open agent tab",  "switch-tab Agent" },
    { "save all",        "save-all"         },
    { "close file",      "close-file"       },
    { "refresh",         "refresh"          },
    { NULL, NULL }
};

/* forward declaration for Phase 6 */
static void vosk_handle_skill_command(const gchar *text);

/* ------------------------------------------------------------------ */
/* Phase 7 — menu bar navigation by voice                             */
/* ------------------------------------------------------------------ */

/* Recursively find the GtkMenuBar in the widget tree */
static GtkWidget *find_menubar(GtkWidget *widget)
{
    if (GTK_IS_MENU_BAR(widget))
        return widget;
    if (!GTK_IS_CONTAINER(widget))
        return NULL;
    GList *children = gtk_container_get_children(GTK_CONTAINER(widget));
    GtkWidget *found = NULL;
    for (GList *l = children; l && !found; l = l->next)
        found = find_menubar(l->data);
    g_list_free(children);
    return found;
}

/* Remove underscore accelerator markers from a menu label */
static gchar *menu_label_clean(const gchar *label)
{
    GString *s = g_string_new(NULL);
    for (; *label; label++)
        if (*label != '_') g_string_append_c(s, *label);
    return g_string_free(s, FALSE);
}

/* Collect visible, sensitive, non-separator items from a GtkMenu */
static GList *menu_collect_items(GtkWidget *menu)
{
    GList *all   = gtk_container_get_children(GTK_CONTAINER(menu));
    GList *items = NULL;
    for (GList *l = all; l; l = l->next) {
        if (GTK_IS_SEPARATOR_MENU_ITEM(l->data)) continue;
        if (!gtk_widget_get_visible(l->data))     continue;
        if (!gtk_widget_get_sensitive(l->data))   continue;
        items = g_list_append(items, l->data);
    }
    g_list_free(all);
    return items;
}

/* Clear menu state variables without touching GTK widgets */
static void voice_menu_reset_state(void)
{
    if (open_menu)
        gtk_menu_shell_deactivate(GTK_MENU_SHELL(open_menu));
    if (menu_item_list) { g_list_free(menu_item_list); menu_item_list = NULL; }
    open_menu     = NULL;
    menu_item_idx = -1;
    menu_mode     = FALSE;
}

/* Deactivate open menu and return to listening state */
static void voice_menu_close(void)
{
    voice_menu_reset_state();
    set_voice_state(VSTATE_ACTIVE);
}

/* Activate the currently highlighted menu item */
static void voice_menu_go(void)
{
    if (!menu_mode || menu_item_idx < 0) return;
    GtkWidget *item = g_list_nth_data(menu_item_list, menu_item_idx);
    voice_menu_reset_state();
    set_voice_state(VSTATE_ACTIVE);
    if (item)
        gtk_menu_item_activate(GTK_MENU_ITEM(item));
}

/* Move the selection by delta steps, wrapping around */
static void voice_menu_navigate(gint delta)
{
    if (!menu_mode || !open_menu || !menu_item_list) return;
    gint n = (gint)g_list_length(menu_item_list);
    if (n == 0) return;
    menu_item_idx = (menu_item_idx + delta % n + n) % n;
    GtkWidget *item = g_list_nth_data(menu_item_list, menu_item_idx);
    gtk_menu_shell_select_item(GTK_MENU_SHELL(open_menu), item);
}

/* Highlight the first item whose label contains name (case-insensitive) */
static void voice_menu_select_by_name(const gchar *name)
{
    if (!menu_mode || !open_menu || !menu_item_list) return;
    gchar *lower = g_utf8_strdown(name, -1);
    gint   i     = 0;
    for (GList *l = menu_item_list; l; l = l->next, i++) {
        GtkWidget *child = gtk_bin_get_child(GTK_BIN(l->data));
        if (!GTK_IS_LABEL(child)) continue;
        gchar *clean = menu_label_clean(gtk_label_get_text(GTK_LABEL(child)));
        gchar *lc    = g_utf8_strdown(clean, -1);
        g_free(clean);
        gboolean match = (strstr(lc, lower) != NULL);
        g_free(lc);
        if (match) {
            menu_item_idx = i;
            gtk_menu_shell_select_item(GTK_MENU_SHELL(open_menu), l->data);
            break;
        }
    }
    g_free(lower);
}

/* Open the top-level menubar entry whose name contains spoken name */
static void voice_menu_open(const gchar *name)
{
    GtkWidget *menubar = find_menubar(geany->main_widgets->window);
    if (!menubar) {
        msgwin_status_add("GeanyVosk: could not find menu bar");
        return;
    }

    gchar    *lower = g_utf8_strdown(name, -1);
    g_strstrip(lower);
    GList    *all   = gtk_container_get_children(GTK_CONTAINER(menubar));
    GtkWidget *found = NULL;

    for (GList *l = all; l && !found; l = l->next) {
        if (!GTK_IS_MENU_ITEM(l->data)) continue;
        GtkWidget *child = gtk_bin_get_child(GTK_BIN(l->data));
        if (!GTK_IS_LABEL(child)) continue;
        gchar *clean = menu_label_clean(gtk_label_get_text(GTK_LABEL(child)));
        gchar *lc    = g_utf8_strdown(clean, -1);
        g_free(clean);
        if (strstr(lc, lower)) found = l->data;
        g_free(lc);
    }
    g_list_free(all);
    g_free(lower);

    if (!found) {
        msgwin_status_add("GeanyVosk: menu '%s' not found", name);
        return;
    }

    GtkWidget *submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(found));
    if (!submenu) return;

    open_menu      = submenu;
    menu_item_list = menu_collect_items(submenu);
    menu_item_idx  = menu_item_list ? 0 : -1;
    menu_mode      = TRUE;
    set_voice_state(VSTATE_MENU);

    gtk_menu_popup_at_widget(GTK_MENU(submenu), found,
                             GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST,
                             NULL);
    if (menu_item_list)
        gtk_menu_shell_select_item(GTK_MENU_SHELL(submenu),
                                   g_list_nth_data(menu_item_list, 0));
}

static void vosk_handle_command(const gchar *text)
{
    /* Menu navigation mode: voice controls an open menu */
    if (menu_mode) {
        const gchar *dw = cfg_deact_word ? cfg_deact_word : DEACTIVATE_DEFAULT;
        if (strstr(text, dw) || strstr(text, "cancel")) {
            voice_menu_close();
            return;
        }
        if (strcmp(text, "go") == 0) {
            voice_menu_go();
            return;
        }
        if (strstr(text, "down")) {
            voice_menu_navigate(word_to_count(strstr(text, "down") + 4));
            return;
        }
        if (strstr(text, "up")) {
            voice_menu_navigate(-word_to_count(strstr(text, "up") + 2));
            return;
        }
        /* Try to select an item by spoken name */
        voice_menu_select_by_name(text);
        return;
    }

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

    /* Agent-composing mode: route speech directly to agent terminal input */
    if (agent_composing) {
        const gchar *dw = cfg_deact_word ? cfg_deact_word : DEACTIVATE_DEFAULT;
        if (strstr(text, dw) || strstr(text, "cancel")) {
            agent_composing = FALSE;
            set_voice_state(VSTATE_ACTIVE);
            return;
        }
        if (strcmp(text, "go") == 0 || strstr(text, "agent go")) {
            agent_composing = FALSE;
            ctrl_emit("agent-enter");
            set_voice_state(VSTATE_ACTIVE);
            return;
        }
        if (strstr(text, "tab")) {
            ctrl_emit("send-to-agent \t");
            return;
        }
        /* Append spoken text to the agent input field */
        gchar *cmd = g_strdup_printf("send-to-agent %s ", text);
        ctrl_emit(cmd);
        g_free(cmd);
        return;
    }

    /* Dictation mode: append speech to current document */
    if (dictate_mode) {
        if (strstr(text, "stop dictating")) {
            /* If whisper is running, let audio thread discard the partial buffer
               (it contains "stop dictating") by clearing the flag first. */
            g_atomic_int_set(&whisper_dictating, 0);
            dictate_mode = FALSE;
            vosk_set_active(TRUE);  /* stay active, just leave dictation */
            return;
        }
        /* When whisper is active, skip Vosk insertions — whisper handles text. */
        if (!g_atomic_int_get(&whisper_dictating))
            dictate_insert(text);
        return;
    }

    /* "dictate" / "start dictating" — enter dictation mode */
    if (strstr(text, "dictate") && !strstr(text, "stop")) {
        dictate_mode = TRUE;
        if (cfg_whisper_model && wqueue)
            g_atomic_int_set(&whisper_dictating, 1);
        set_voice_state(VSTATE_DICTATING);
        return;
    }

    /* "new file" — open a fresh untitled document */
    if (strstr(text, "new file") &&
        !strstr(text, "new skill") && !strstr(text, "new ai") &&
        !strstr(text, "new ay") && !strstr(text, "new a i")) {
        document_new_file(NULL, NULL, NULL);
        msgwin_status_add("GeanyVosk: new file");
        return;
    }

    /* "save current file" / "save the file" — save active document */
    if (strstr(text, "save current") || strstr(text, "save the file")) {
        GeanyDocument *doc = document_get_current();
        if (doc && doc->file_name) {
            document_save_file(doc, FALSE);
            msgwin_status_add("GeanyVosk: saved %s", doc->file_name);
        } else {
            msgwin_status_add("GeanyVosk: no file to save");
        }
        return;
    }

    /* "save as <spoken path>" — save to fuzzy-resolved path */
    if (strstr(text, "save as ")) {
        gchar *spoken = extract_name_after(text, "save as ");
        if (spoken && *spoken) {
            GeanyDocument *doc = document_get_current();
            if (doc) {
                gchar *base;
                if (doc->file_name) {
                    base = g_path_get_dirname(doc->file_name);
                } else {
                    GeanyApp *app = geany->app;
                    base = (app->project && app->project->base_path &&
                            *app->project->base_path)
                        ? g_strdup(app->project->base_path)
                        : g_strdup(g_get_home_dir());
                }
                gchar *path = fuzzy_resolve_path(spoken, base);
                g_free(base);
                gchar *dir = g_path_get_dirname(path);
                g_mkdir_with_parents(dir, 0755);
                g_free(dir);
                if (document_save_file_as(doc, path))
                    msgwin_status_add("GeanyVosk: saved as %s", path);
                else
                    msgwin_status_add("GeanyVosk: save as failed: %s", path);
                g_free(path);
            }
        }
        g_free(spoken);
        return;
    }

    /* "new skill <name>" — shorter alias, no "agent create" prefix needed */
    if (strstr(text, "new skill ") && !strstr(text, "agent create")) {
        gchar *name = extract_name_after(text, "new skill ");
        if (name && *name) {
            gchar *path    = g_build_filename(g_get_home_dir(), ".claude", "skills", NULL);
            g_mkdir_with_parents(path, 0755);
            gchar *fname   = g_strdup_printf("%s.md", name);
            gchar *full    = g_build_filename(path, fname, NULL);
            gchar *content = g_strdup_printf(SKILL_TEMPLATE, name);
            vosk_create_and_open(full, content);
            g_free(content);
            g_free(full);
            g_free(fname);
            g_free(path);
        }
        g_free(name);
        return;
    }

    /* "agent create new skill <name>" */
    if (strstr(text, "agent create new skill ")) {
        gchar *name = extract_name_after(text, "agent create new skill ");
        if (name && *name) {
            gchar *path = g_build_filename(g_get_home_dir(), ".claude", "skills",
                                           NULL);
            g_mkdir_with_parents(path, 0755);
            gchar *fname = g_strdup_printf("%s.md", name);
            gchar *full  = g_build_filename(path, fname, NULL);
            gchar *content = g_strdup_printf(SKILL_TEMPLATE, name);
            vosk_create_and_open(full, content);
            g_free(content);
            g_free(full);
            g_free(fname);
            g_free(path);
        }
        g_free(name);
        return;
    }

    /* "new ay eye prompt <name>" / "new a i prompt <name>" — Vosk-friendly aliases */
    if (strstr(text, "new ay eye prompt ") || strstr(text, "new a i prompt ")) {
        const gchar *pfx = strstr(text, "new ay eye prompt ")
            ? "new ay eye prompt " : "new a i prompt ";
        gchar *spoken = extract_name_after(text, pfx);
        if (spoken && *spoken) {
            gchar *fname = voice_to_filename(spoken);
            GeanyApp *app = geany->app;
            gchar *base_path;
            if (app->project && app->project->base_path && *app->project->base_path)
                base_path = g_strdup(app->project->base_path);
            else
                base_path = g_strdup(g_get_home_dir());
            gchar *dir  = g_build_filename(base_path, "ai-prompts", NULL);
            g_free(base_path);
            g_mkdir_with_parents(dir, 0755);
            gchar *full = g_build_filename(dir, fname, NULL);
            vosk_create_and_open(full, "");
            g_free(full);
            g_free(dir);
            g_free(fname);
        }
        g_free(spoken);
        return;
    }

    /* "new ai prompt <name> dot md" */
    if (strstr(text, "new ai prompt ")) {
        gchar *spoken = extract_name_after(text, "new ai prompt ");
        if (spoken && *spoken) {
            gchar *fname = voice_to_filename(spoken);
            GeanyApp *app = geany->app;
            gchar *base_path;
            if (app->project && app->project->base_path && *app->project->base_path)
                base_path = g_strdup(app->project->base_path);
            else
                base_path = g_strdup(g_get_home_dir());
            gchar *dir  = g_build_filename(base_path, "ai-prompts", NULL);
            g_free(base_path);
            g_mkdir_with_parents(dir, 0755);
            gchar *full = g_build_filename(dir, fname, NULL);
            vosk_create_and_open(full, "");
            g_free(full);
            g_free(dir);
            g_free(fname);
        }
        g_free(spoken);
        return;
    }

    /* "agent type <number/yes/no/enter>" — immediate reply to Claude prompts */
    if (strstr(text, "agent type ")) {
        const gchar *after = strstr(text, "agent type ") + strlen("agent type ");
        const gchar *digit = word_to_digit(after);
        if (digit) {
            gchar *cmd = g_strdup_printf("send-to-agent %s", digit);
            ctrl_emit(cmd);
            g_free(cmd);
        } else if (strstr(after, "yes")) {
            ctrl_emit("send-to-agent y");
        } else if (strstr(after, "no")) {
            ctrl_emit("send-to-agent n");
        }
        ctrl_emit("agent-enter");
        return;
    }

    /* "agent yes" / "agent no" — shorthand confirmations */
    if (strstr(text, "agent yes")) {
        ctrl_emit("send-to-agent y");
        ctrl_emit("agent-enter");
        return;
    }
    if (strstr(text, "agent no")) {
        ctrl_emit("send-to-agent n");
        ctrl_emit("agent-enter");
        return;
    }

    /* "agent go" — press Enter in agent terminal */
    if (strstr(text, "agent go")) {
        ctrl_emit("agent-enter");
        return;
    }

    /* "agent tab" — send Tab to agent input (outside composing mode) */
    if (strstr(text, "agent tab")) {
        ctrl_emit("send-to-agent \t");
        return;
    }

    /* "agent down [N]" / "agent up [N]" — navigate, then enter composing mode */
    if (strstr(text, "agent down") || strstr(text, "agent up")) {
        gboolean going_down = strstr(text, "agent down") != NULL;
        const gchar *after = going_down
            ? strstr(text, "agent down") + strlen("agent down")
            : strstr(text, "agent up")   + strlen("agent up");
        gint count = word_to_count(after);
        /* ANSI arrow escape sequences: down = ESC[B, up = ESC[A */
        const gchar *arrow = going_down ? "\033[B" : "\033[A";
        GString *seq = g_string_new(NULL);
        for (gint i = 0; i < count; i++)
            g_string_append(seq, arrow);
        gchar *cmd = g_strdup_printf("send-to-agent %s", seq->str);
        ctrl_emit(cmd);
        g_free(cmd);
        g_string_free(seq, TRUE);
        agent_composing = TRUE;
        set_voice_state(VSTATE_AGENT);
        return;
    }

    /* "open file <name>" / "switch to file <name>" — fuzzy match open documents */
    if (strstr(text, "open file ") && !strstr(text, "new file")) {
        gchar *spoken = extract_name_after(text, "open file ");
        if (spoken && *spoken) {
            gchar *cmd = g_strdup_printf("fuzzy-open-file %s", spoken);
            ctrl_emit(cmd);
            g_free(cmd);
        }
        g_free(spoken);
        return;
    }
    if (strstr(text, "switch to file ")) {
        gchar *spoken = extract_name_after(text, "switch to file ");
        if (spoken && *spoken) {
            gchar *cmd = g_strdup_printf("fuzzy-open-file %s", spoken);
            ctrl_emit(cmd);
            g_free(cmd);
        }
        g_free(spoken);
        return;
    }

    /* Phase 4 — geanycontrol commands */
    for (const CmdEntry *e = CMDS; e->pattern; e++) {
        if (strstr(text, e->pattern)) {
            ctrl_emit(e->ctrl_cmd);
            return;
        }
    }

    /* Phase 7 — menu bar navigation */
    if (g_str_has_prefix(text, "menu ")) {
        voice_menu_open(text + 5);
        return;
    }

    /* "side bar" / "sidebar" — enter file tree navigation mode */
    if (strstr(text, "side bar") || strstr(text, "sidebar")) {
        ctrl_emit("treebrowser-focus");
        set_voice_state(VSTATE_TREEBROWSER);
        return;
    }

    /* Phase 6 — agent skill execution */
    vosk_handle_skill_command(text);
}
#endif /* HAVE_VOSK */

#ifdef HAVE_VOSK
/* Build the Vosk phrase grammar used while idle. Restricting decoding to the
   wake phrase (plus "[unk]" for everything else) dramatically improves the
   odds of catching it, compared with the full free-vocabulary graph.
   Requires a lookahead model (e.g. vosk-model-small-en-us). */
static gchar *build_wake_grammar(void)
{
    const gchar *w = cfg_wake_word ? cfg_wake_word : WAKE_WORD_DEFAULT;
    return g_strdup_printf("[\"%s\", \"[unk]\"]", w);
}

static gboolean vosk_init_recognizer(void)
{
    if (!g_file_test(cfg_model_path, G_FILE_TEST_IS_DIR)) {
        msgwin_status_add(
            "GeanyVosk: Vosk model not found. Download from "
            "https://alphacephei.com/vosk/models and extract to %s",
            cfg_model_path);
        return FALSE;
    }
    vosk_model = vosk_model_new(cfg_model_path);
    if (!vosk_model) {
        msgwin_status_add("GeanyVosk: failed to load Vosk model from %s",
                          cfg_model_path);
        return FALSE;
    }
    /* Wake-word recognizer: grammar-restricted so the phrase is caught reliably. */
    vosk_rec_idle = vosk_recognizer_new(vosk_model, (float)SAMPLE_RATE);
    if (vosk_rec_idle) {
        gchar *grm = build_wake_grammar();
        vosk_recognizer_set_grm(vosk_rec_idle, grm);
        g_free(grm);
    }

    /* Command recognizer: full vocabulary — no grammar restriction. */
    vosk_rec_cmd = vosk_recognizer_new(vosk_model, (float)SAMPLE_RATE);

    /* Start in idle mode. */
    vosk_rec = vosk_rec_idle;
    return vosk_rec_idle != NULL && vosk_rec_cmd != NULL;
}
#endif /* HAVE_VOSK */

/* ------------------------------------------------------------------ */
/* Phase 6 — Agent skill execution by voice                            */
/* ------------------------------------------------------------------ */

#ifdef HAVE_VOSK

typedef enum { SCMD_IDLE, SCMD_ARMED } SkillCmdState;

typedef struct {
    SkillCmdState  state;
    gchar         *model;
    gchar         *skill;
    gchar         *context_file;
} SkillCmd;

static SkillCmd pending_cmd = { SCMD_IDLE, NULL, NULL, NULL };

static void skill_cmd_reset(SkillCmd *cmd)
{
    g_free(cmd->model);
    g_free(cmd->skill);
    g_free(cmd->context_file);
    cmd->model        = NULL;
    cmd->skill        = NULL;
    cmd->context_file = NULL;
    cmd->state        = SCMD_IDLE;
}

/* Map spoken model name to Claude model id */
static const gchar *map_model(const gchar *spoken)
{
    if (strstr(spoken, "opus"))   return "claude-opus-4";
    if (strstr(spoken, "sonnet")) return "claude-sonnet-4-6";
    if (strstr(spoken, "haiku"))  return "claude-haiku-4-5";
    return "claude-sonnet-4-6";  /* default */
}

/* Map spoken verb to skill name */
static const gchar *map_verb(const gchar *spoken)
{
    if (strstr(spoken, "plan")) return "opus-plan";
    if (strstr(spoken, "exec")) return "opus-exec";
    return NULL;
}

/* Parse: "agent skill <model> <verb> [context <file>]" */
static void vosk_parse_skill_cmd(const gchar *text, SkillCmd *cmd)
{
    /* extract tokens after "agent skill " */
    const gchar *p = strstr(text, "agent skill ");
    if (!p) return;
    p += strlen("agent skill ");

    gchar **toks = g_strsplit(p, " ", -1);
    gint    n    = g_strv_length(toks);

    skill_cmd_reset(cmd);

    if (n >= 1) cmd->model = g_strdup(map_model(toks[0]));
    if (n >= 2) cmd->skill = g_strdup(map_verb(toks[1]));

    /* look for "context" token */
    for (gint i = 2; i < n - 1; i++) {
        if (strcmp(toks[i], "context") == 0) {
            /* remaining tokens (after "context") form the filename */
            GString *fname = g_string_new(NULL);
            for (gint j = i + 1; j < n; j++) {
                if (j > i + 1) g_string_append_c(fname, ' ');
                g_string_append(fname, toks[j]);
            }
            cmd->context_file = voice_to_filename(fname->str);
            g_string_free(fname, TRUE);
            break;
        }
    }

    g_strfreev(toks);
}

static void vosk_execute_skill(const SkillCmd *cmd)
{
    if (!cmd->skill) {
        msgwin_status_add("GeanyVosk: unknown skill verb");
        return;
    }

    GString *line = g_string_new("/");
    g_string_append(line, cmd->skill);
    if (cmd->context_file)
        g_string_append_printf(line, " @ai-prompts/%s", cmd->context_file);
    g_string_append_c(line, '\n');

    gchar *ctrl_cmd = g_strdup_printf("send-to-agent %s", line->str);
    ctrl_emit(ctrl_cmd);
    g_free(ctrl_cmd);
    g_string_free(line, TRUE);
}

static void vosk_handle_skill_command(const gchar *text)
{
    /* "back in the lamp" while armed — cancel without executing */
    if (pending_cmd.state == SCMD_ARMED) {
        const gchar *dw = cfg_deact_word ? cfg_deact_word : DEACTIVATE_DEFAULT;
        if (strstr(text, dw)) {
            skill_cmd_reset(&pending_cmd);
            vosk_set_active(FALSE);
            return;
        }
        /* "go" — fire */
        if (strstr(text, "go")) {
            vosk_execute_skill(&pending_cmd);
            skill_cmd_reset(&pending_cmd);
            set_voice_state(VSTATE_ACTIVE);
            return;
        }
    }

    /* "agent skill <model> <verb> [context <file>]" — arm */
    if (strstr(text, "agent skill ")) {
        vosk_parse_skill_cmd(text, &pending_cmd);
        if (pending_cmd.skill) {
            pending_cmd.state = SCMD_ARMED;
            set_voice_state(VSTATE_ARMED);
        } else {
            msgwin_status_add("GeanyVosk: could not parse skill command: %s", text);
        }
        return;
    }

    msgwin_status_add("GeanyVosk: unhandled: %s", text);
}

#endif /* HAVE_VOSK */

#ifdef HAVE_VOSK
static void on_tb_menu_ready(G_GNUC_UNUSED GObject *obj, GtkWidget *menu,
                             G_GNUC_UNUSED gpointer data)
{
    tb_pending_menu = menu;
}
#endif /* HAVE_VOSK */

#ifdef HAVE_VOSK
static gboolean on_status_icon_click(GtkWidget *widget, GdkEventButton *event,
                                     gpointer data)
{
    (void)widget; (void)data;
    if (event->button != 1) return FALSE;
    if (!vosk_enabled) return FALSE;
    if (active_mode) {
        dictate_mode    = FALSE;
        agent_composing = FALSE;
        skill_cmd_reset(&pending_cmd);
        vosk_set_active(FALSE);
    } else {
        vosk_set_active(TRUE);
    }
    return TRUE;
}
#endif /* HAVE_VOSK */

/* ------------------------------------------------------------------ */
/* Whisper dictation — high-quality offline ASR via whisper-cli        */
/* ------------------------------------------------------------------ */

#ifdef HAVE_VOSK

#define WHISPER_BUF_SECS      30
#define WHISPER_BUF_FRAMES    (SAMPLE_RATE * WHISPER_BUF_SECS)
/* Silence: sum-of-squares / n < 600^2 = 360000 (avoids linking -lm) */
#define WHISPER_SILENCE_RMS2  360000LL
/* 800 ms of consecutive silence before flushing */
#define WHISPER_SILENCE_FRAMES (SAMPLE_RATE * 4 / 5)

static gboolean is_silent_chunk(const gint16 *buf, gsize n)
{
    gint64 sum = 0;
    for (gsize i = 0; i < n; i++)
        sum += (gint64)buf[i] * buf[i];
    return sum < WHISPER_SILENCE_RMS2 * (gint64)n;
}

static gboolean wav_write(const gchar *path, const gint16 *pcm, gsize n_frames)
{
    FILE *f = fopen(path, "wb");
    if (!f) return FALSE;

    guint32 data_bytes = (guint32)(n_frames * 2);
    guint32 chunk_sz   = 36 + data_bytes;
    guint32 sr         = SAMPLE_RATE;
    guint32 byte_rate  = SAMPLE_RATE * 2;
    guint32 fmt_sz     = 16;
    guint16 pcm_fmt    = 1;
    guint16 channels   = 1;
    guint16 block_align = 2;
    guint16 bits       = 16;

    fwrite("RIFF", 1, 4, f);
    fwrite(&chunk_sz,    4, 1, f);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    fwrite(&fmt_sz,      4, 1, f);
    fwrite(&pcm_fmt,     2, 1, f);
    fwrite(&channels,    2, 1, f);
    fwrite(&sr,          4, 1, f);
    fwrite(&byte_rate,   4, 1, f);
    fwrite(&block_align, 2, 1, f);
    fwrite(&bits,        2, 1, f);
    fwrite("data", 1, 4, f);
    fwrite(&data_bytes,  4, 1, f);
    fwrite(pcm, 2, n_frames, f);
    return fclose(f) == 0;
}

/* Strip whisper special tokens like [BLANK_AUDIO] [MUSIC] etc. */
static gchar *whisper_strip_specials(const gchar *text)
{
    GString *out = g_string_new(NULL);
    const gchar *p = text;
    while (*p) {
        if (*p == '[') {
            const gchar *end = strchr(p, ']');
            if (end) { p = end + 1; continue; }
        }
        g_string_append_c(out, *p++);
    }
    return g_string_free(out, FALSE);
}

static gboolean whisper_insert_cb(gpointer data)
{
    gchar *text = data;
    gchar *stripped = g_strstrip(text);
    if (*stripped)
        ctrl_emit_append(stripped);
    g_free(text);
    return G_SOURCE_REMOVE;
}

static gpointer whisper_worker_thread(gpointer _)
{
    (void)_;
    while (TRUE) {
        gchar *wav_path = g_async_queue_pop(wqueue);
        if (!wav_path) break;   /* NULL sentinel = shutdown */

        const gchar *model = cfg_whisper_model;
        const gchar *cli   = cfg_whisper_cli
            ? cfg_whisper_cli : "/usr/local/bin/whisper-cli";

        if (!model || !g_file_test(model, G_FILE_TEST_EXISTS)) {
            msgwin_status_add("GeanyVosk: whisper model not found: %s",
                              model ? model : "(none configured)");
            remove(wav_path);
            g_free(wav_path);
            continue;
        }

        /* g_spawn_sync needs gchar** — cast away const via explicit array */
        gchar *w_argv[10];
        gint   w_argc = 0;
        w_argv[w_argc++] = (gchar *)cli;
        w_argv[w_argc++] = (gchar *)"--model";
        w_argv[w_argc++] = (gchar *)model;
        w_argv[w_argc++] = (gchar *)"--no-timestamps";
        w_argv[w_argc++] = (gchar *)"--no-prints";
        w_argv[w_argc++] = (gchar *)"--language";
        w_argv[w_argc++] = (gchar *)"en";
        w_argv[w_argc++] = (gchar *)"--file";
        w_argv[w_argc++] = wav_path;
        w_argv[w_argc]   = NULL;

        gchar *out = NULL;
        GError *err = NULL;

        g_spawn_sync(NULL, w_argv, NULL,
                     G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL,
                     NULL, NULL, &out, NULL, NULL, &err);

        if (err) {
            msgwin_status_add("GeanyVosk: whisper-cli error: %s", err->message);
            g_error_free(err);
        } else if (out && *out) {
            gchar *clean = whisper_strip_specials(out);
            g_free(out);
            gchar *trimmed = g_strstrip(clean);
            if (*trimmed)
                g_idle_add(whisper_insert_cb, g_strdup(trimmed));
            g_free(clean);
        } else {
            g_free(out);
        }

        remove(wav_path);
        g_free(wav_path);
    }
    return NULL;
}

/* Called from audio thread when silence threshold reached. */
static void whisper_flush_audio(void)
{
    if (!wbuf_speech || wbuf_len == 0) {
        wbuf_len = 0; wbuf_speech = FALSE; wsilence = 0;
        return;
    }
    /* Trim trailing silence, keeping a 100ms margin for natural endings */
    gsize margin = SAMPLE_RATE / 10;
    gsize trim   = (gsize)wsilence > margin ? (gsize)wsilence - margin : 0;
    gsize n      = wbuf_len > trim ? wbuf_len - trim : wbuf_len;

    gchar *path = g_strdup_printf("/tmp/geanyvosk_w%d.wav", ++wchunk_id);
    if (wav_write(path, wbuf, n))
        g_async_queue_push(wqueue, path);
    else {
        msgwin_status_add("GeanyVosk: failed to write whisper WAV: %s", path);
        g_free(path);
    }

    wbuf_len = 0; wbuf_speech = FALSE; wsilence = 0;
}

#endif /* HAVE_VOSK (whisper) */

/* ------------------------------------------------------------------ */
/* Audio capture state                                                  */
/* ------------------------------------------------------------------ */

typedef struct {
    snd_pcm_t  *pcm;
    GThread    *thread;
    GMutex      mutex;
    GCond       cond;
    gboolean    stop;
    gint16      buf[FRAMES_CHUNK * 2];  /* double-buffer */
    gsize       buf_len;
} AudioCapture;

static AudioCapture *ac = NULL;

static gboolean audio_open_pcm(AudioCapture *cap, const gchar *device)
{
    snd_pcm_hw_params_t *params = NULL;
    int err;

    err = snd_pcm_open(&cap->pcm, device, SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0) {
        g_warning("GeanyVosk: cannot open PCM device '%s': %s",
                  device, snd_strerror(err));
        return FALSE;
    }

    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(cap->pcm, params);

    if ((err = snd_pcm_hw_params_set_access(cap->pcm, params,
              SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) goto fail;
    if ((err = snd_pcm_hw_params_set_format(cap->pcm, params,
              SND_PCM_FORMAT_S16_LE)) < 0) goto fail;
    if ((err = snd_pcm_hw_params_set_channels(cap->pcm, params, 1)) < 0) goto fail;

    unsigned int rate = SAMPLE_RATE;
    if ((err = snd_pcm_hw_params_set_rate_near(cap->pcm, params, &rate, 0)) < 0)
        goto fail;
    if ((err = snd_pcm_hw_params(cap->pcm, params)) < 0) goto fail;

    snd_pcm_prepare(cap->pcm);
    return TRUE;

fail:
    g_warning("GeanyVosk: PCM hw_params error: %s", snd_strerror(err));
    snd_pcm_close(cap->pcm);
    cap->pcm = NULL;
    return FALSE;
}

static gpointer audio_thread(gpointer data)
{
    AudioCapture *cap = data;

#ifdef VOSK_DUMP_PCM
    FILE *dump = fopen("/tmp/geanyvosk.raw", "wb");
#endif

    while (!g_atomic_int_get((gint *)&cap->stop)) {
        snd_pcm_sframes_t frames = snd_pcm_readi(cap->pcm, cap->buf, FRAMES_CHUNK);
        if (frames < 0) {
            frames = snd_pcm_recover(cap->pcm, (int)frames, 0);
            if (frames < 0) {
                g_warning("GeanyVosk: snd_pcm_readi error: %s",
                          snd_strerror((int)frames));
                break;
            }
            continue;
        }

#ifdef HAVE_VOSK
        if (vosk_rec) {
            /* Apply any grammar switch requested by the main thread here, so
               the recognizer is only ever touched from this thread. */
            gint req = g_atomic_int_get(&pending_grm);
            if (req) {
                g_atomic_int_set(&pending_grm, 0);
                vosk_rec = (req == 1) ? vosk_rec_idle : vosk_rec_cmd;
                if (vosk_rec)
                    vosk_recognizer_reset(vosk_rec);
            }
            if (vosk_recognizer_accept_waveform_s(vosk_rec,
                    cap->buf, (int)frames)) {
                const char *result = vosk_recognizer_result(vosk_rec);
                g_idle_add(vosk_dispatch, g_strdup(result));
            }
        }
#endif

#ifdef VOSK_DUMP_PCM
        if (dump)
            fwrite(cap->buf, sizeof(gint16), (size_t)frames, dump);
#endif

#ifdef HAVE_VOSK
        /* Whisper dictation buffer */
        if (g_atomic_int_get(&whisper_dictating)) {
            if (wbuf) {
                if (is_silent_chunk(cap->buf, (gsize)frames)) {
                    wsilence += (gint)frames;
                } else {
                    wsilence = 0;
                    wbuf_speech = TRUE;
                }
                if (wbuf_len + (gsize)frames <= WHISPER_BUF_FRAMES) {
                    memcpy(wbuf + wbuf_len, cap->buf,
                           (gsize)frames * sizeof(gint16));
                    wbuf_len += (gsize)frames;
                }
                /* Flush on silence threshold or buffer full */
                if (wbuf_speech &&
                    (wsilence >= WHISPER_SILENCE_FRAMES ||
                     wbuf_len >= WHISPER_BUF_FRAMES))
                    whisper_flush_audio();
            }
        } else if (wbuf_len > 0) {
            /* Dictation stopped — discard partial buffer (may contain "stop dictating") */
            wbuf_len = 0; wbuf_speech = FALSE; wsilence = 0;
        }
#endif
    }

#ifdef VOSK_DUMP_PCM
    if (dump) fclose(dump);
#endif
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Plugin lifecycle                                                     */
/* ------------------------------------------------------------------ */

void plugin_init(GeanyData *data)
{
    (void)data;

    load_config();

    /* Add microphone icon + status label to the status bar */
    GtkWidget *pb = geany->main_widgets->progressbar;
    if (pb) {
        GtkWidget *sb_box = gtk_widget_get_parent(pb);
        if (sb_box && GTK_IS_BOX(sb_box)) {
            status_box  = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
            status_icon = gtk_image_new_from_icon_name(
                              "audio-input-microphone-symbolic",
                              GTK_ICON_SIZE_MENU);
            status_label = gtk_label_new("");
            gtk_box_pack_start(GTK_BOX(status_box), status_icon,  FALSE, FALSE, 0);
            gtk_box_pack_start(GTK_BOX(status_box), status_label, FALSE, FALSE, 2);
            gtk_widget_show_all(status_box);
            status_event_box = gtk_event_box_new();
            gtk_container_add(GTK_CONTAINER(status_event_box), status_box);
            gtk_widget_set_tooltip_text(status_event_box,
                                        "Click to toggle voice listening");
            gtk_widget_show(status_event_box);
            gtk_box_pack_end(GTK_BOX(sb_box), status_event_box, FALSE, FALSE, 4);
#ifdef HAVE_VOSK
            g_signal_connect(status_event_box, "button-press-event",
                             G_CALLBACK(on_status_icon_click), NULL);
#endif
            status_update();
        }
    }

    blink_id = g_timeout_add(600, blink_cb, NULL);

#ifdef HAVE_VOSK
    /* Add on/off toggle to the Tools menu */
    voice_toggle_item = gtk_check_menu_item_new_with_label("Voice Commands");
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(voice_toggle_item), TRUE);
    gtk_widget_show(voice_toggle_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(geany->main_widgets->tools_menu),
                          voice_toggle_item);
    g_signal_connect(voice_toggle_item, "toggled",
                     G_CALLBACK(on_voice_toggle), NULL);
#endif

#ifdef HAVE_VOSK
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
#endif

#ifdef HAVE_VOSK
    /* Whisper dictation — start worker thread if a model is configured */
    if (cfg_whisper_model) {
        wbuf    = g_new(gint16, WHISPER_BUF_FRAMES);
        wqueue  = g_async_queue_new();
        wthread = g_thread_new("geanyvosk-whisper", whisper_worker_thread, NULL);
        msgwin_status_add("GeanyVosk: whisper dictation ready (%s)",
                          cfg_whisper_model);
    }
#endif

    ac = g_new0(AudioCapture, 1);
    g_mutex_init(&ac->mutex);
    g_cond_init(&ac->cond);

    if (!audio_open_pcm(ac, cfg_alsa_device)) {
        msgwin_status_add("GeanyVosk: audio capture unavailable");
        set_voice_state(VSTATE_OFF);
        return;
    }

#ifdef HAVE_VOSK
    if (!vosk_init_recognizer())
        set_voice_state(VSTATE_OFF);
#else
    set_voice_state(VSTATE_OFF);
#endif

    ac->stop = FALSE;
    ac->thread = g_thread_new("geanyvosk-audio", audio_thread, ac);
}

void plugin_cleanup(void)
{
    if (blink_id) {
        g_source_remove(blink_id);
        blink_id = 0;
    }

    if (ac) {
        if (ac->thread) {
            g_atomic_int_set((gint *)&ac->stop, TRUE);
            g_thread_join(ac->thread);
            ac->thread = NULL;
        }
        if (ac->pcm) {
            snd_pcm_close(ac->pcm);
            ac->pcm = NULL;
        }
        g_mutex_clear(&ac->mutex);
        g_cond_clear(&ac->cond);
        g_free(ac);
        ac = NULL;
    }

#ifdef HAVE_VOSK
    /* Whisper — audio thread is gone, safe to shut down worker */
    g_atomic_int_set(&whisper_dictating, 0);
    if (wthread) {
        g_async_queue_push(wqueue, NULL);   /* NULL sentinel = shutdown */
        g_thread_join(wthread);
        wthread = NULL;
    }
    if (wqueue) { g_async_queue_unref(wqueue); wqueue = NULL; }
    g_free(wbuf); wbuf = NULL;
    wbuf_len = 0; wbuf_speech = FALSE; wsilence = 0;
#endif

#ifdef HAVE_VOSK
    if (menu_mode) voice_menu_reset_state();
    vosk_rec = NULL;
    if (vosk_rec_cmd)  { vosk_recognizer_free(vosk_rec_cmd);  vosk_rec_cmd  = NULL; }
    if (vosk_rec_idle) { vosk_recognizer_free(vosk_rec_idle); vosk_rec_idle = NULL; }
    if (vosk_model)    { vosk_model_free(vosk_model);         vosk_model    = NULL; }
#endif

    if (voice_toggle_item) {
        gtk_widget_destroy(voice_toggle_item);
        voice_toggle_item = NULL;
    }

    if (status_event_box) {
        gtk_widget_destroy(status_event_box);
        status_event_box = NULL;
        status_box   = NULL;
        status_icon  = NULL;
        status_label = NULL;
    }

    free_config();
}
