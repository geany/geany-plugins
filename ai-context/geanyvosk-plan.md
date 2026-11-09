# geanyvosk — Voice Control Plugin for Geany
## Implementation Plan

**Spec source:** `ai-prompts/geanyspeach.md`  
**Model:** Claude Sonnet 4.6 (one phase per context window)  
**Progress tracking:** `./ai-context/PROGRESS.md`

---

## Architecture Overview

A new Geany plugin `geanyvosk` (directory: `geany-plugins/geanyvosk/`) written in C.

It runs a background thread that continuously feeds microphone audio through **Vosk**
for offline speech recognition. It operates in two modes:

- **Idle mode** — listens only for the wake phrase "Rub lamp". CPU-cheap keyword pass.
- **Active mode** — full ASR; dispatches recognized utterances to command handlers.
  Deactivated by "Back in bottle".

Commands are routed to **geanycontrol**'s Unix socket (`~/.config/geany/geanycontrol.sock`)
for UI operations, and directly into **geanyagent**'s VTE terminal for skill execution.

Visual state is shown via a status-bar label (active/idle indicator).

---

## Dependencies

- **Vosk** — single C API header + shared lib, runtime model download, excellent accuracy.
  No distro package; install `libvosk` from https://alphacephei.com/vosk/models
- **ALSA** (`libasound2-dev`) — microphone capture at 16 kHz mono 16-bit PCM.
  No PulseAudio dependency.

---

## Phase 1 — Project Scaffold & Build System

**Goal:** Bare plugin compiles and loads into Geany. No audio yet.

### Files to create

```
geanyvosk/
  AUTHORS
  ChangeLog
  COPYING              (copy from geanyagent/)
  NEWS
  README
  Makefile.am
  src/
    Makefile.am
    geanyvosk.c        (plugin boilerplate)
    geanyvosk.h        (shared constants and structs)
```

### Makefile.am (top-level geanyvosk/)

```makefile
include $(top_srcdir)/build/vars.auxfiles.mk
AUXFILES =
SUBDIRS = src
plugin = geanyvosk
```

### src/Makefile.am

Model on `geanyagent/src/Makefile.am`. Key additions:
```makefile
pkglib_LTLIBRARIES = geanyvosk.la
geanyvosk_la_SOURCES = geanyvosk.c
geanyvosk_la_CFLAGS  = $(PLUGIN_CFLAGS) $(VOSK_CFLAGS) $(ALSA_CFLAGS)
geanyvosk_la_LIBADD  = $(PLUGIN_LIBS) $(VOSK_LIBS) $(ALSA_LIBS)
geanyvosk_la_LDFLAGS = $(PLUGIN_LDFLAGS)
```

### configure.ac additions

After the `geanycontrol` block, add:
```m4
dnl geanyvosk — voice control
PKG_CHECK_MODULES([ALSA], [alsa], [], [AC_MSG_ERROR([libasound required])])
AC_CHECK_HEADER([vosk_api.h],
  [AC_CHECK_LIB([vosk], [vosk_model_new],
    [VOSK_LIBS="-lvosk" && AC_DEFINE([HAVE_VOSK],[1],[Vosk ASR available])],
    [AC_MSG_ERROR([libvosk not found])])],
  [AC_MSG_ERROR([vosk_api.h not found])])
AC_SUBST([VOSK_CFLAGS])
AC_SUBST([VOSK_LIBS])
```

### Makefile.am (top-level) addition

After `geanycontrol` block:
```makefile
if ENABLE_GEANYVOSK
SUBDIRS += geanyvosk
endif
```

### geanyvosk.c skeleton

```c
/*
 * geanyvosk.c — Geany plugin: voice control via Vosk
 */
#include <geanyplugin.h>
PLUGIN_VERSION_CHECK(224)
PLUGIN_SET_INFO("GeanyVosk", "Voice control for Geany", "0.1", "teknopaul")

GeanyPlugin *geany_plugin;
GeanyData   *geany_data;

void plugin_init(GeanyData *data) { /* Phase 2 fills this */ }
void plugin_cleanup(void)         { /* Phase 2 fills this */ }
```

### Acceptance criteria

- `./autogen.sh && ./configure && make` compiles without errors.
- Plugin appears in Geany Plugin Manager and can be loaded/unloaded.
- Write `PROGRESS.md` entry: Phase 1 complete.

---

## Phase 2 — ALSA Microphone Capture Thread

**Goal:** Plugin captures 16 kHz mono 16-bit PCM from default ALSA device into a
ring buffer on a background GLib thread. No ASR yet; just verify audio flows.

### New code in geanyvosk.c

**Structs:**
```c
#define SAMPLE_RATE   16000
#define FRAMES_CHUNK  4000   /* 250 ms */

typedef struct {
    snd_pcm_t  *pcm;
    GThread    *thread;
    GMutex      mutex;
    GCond       cond;
    gboolean    stop;
    gint16      buf[FRAMES_CHUNK * 2];  /* double-buffer */
    gsize       buf_len;
} AudioCapture;
```

**Thread function:**
```c
static gpointer audio_thread(gpointer data) {
    AudioCapture *ac = data;
    /* snd_pcm_open, set_hw_params (rate=16000, channels=1, format=S16_LE) */
    /* loop: snd_pcm_readi -> push to recognizer queue -> check ac->stop */
}
```

**init/cleanup:**
```c
static AudioCapture *ac = NULL;

void plugin_init(GeanyData *d) {
    ac = g_new0(AudioCapture, 1);
    /* open PCM, start thread */
}
void plugin_cleanup(void) {
    ac->stop = TRUE;
    g_thread_join(ac->thread);
    snd_pcm_close(ac->pcm);
    g_free(ac);
}
```

**Debug toggle:** compile with `-DVOSK_DUMP_PCM` to write raw audio to `/tmp/geanyvosk.raw`
for verification with `aplay -f S16_LE -r 16000 -c 1 /tmp/geanyvosk.raw`.

### Acceptance criteria

- Plugin loads; microphone capture starts without error log messages.
- With debug flag, `/tmp/geanyvosk.raw` plays back recognisably.
- Write `PROGRESS.md` entry: Phase 2 complete.

---

## Phase 3 — Vosk ASR Integration & Wake Word Detection

**Goal:** Feed captured audio into Vosk; detect "rub lamp" and "back in bottle".
Toggle `active_mode` boolean. Show state in status bar.

### Vosk model setup (runtime, not compiled in)

Model path: `~/.local/share/geanyvosk/model/` (e.g. `vosk-model-small-en-us`).
Plugin prints an actionable error to Geany's message window if model is absent:
```
GeanyVosk: Vosk model not found. Download from https://alphacephei.com/vosk/models
and extract to ~/.local/share/geanyvosk/model/
```

### Code additions

```c
#include <vosk_api.h>
static VoskModel      *vosk_model = NULL;
static VoskRecognizer *vosk_rec   = NULL;

static gboolean active_mode = FALSE;
static GtkWidget *status_label = NULL;   /* added to Geany status bar */
```

**Recognizer init (called from plugin_init after audio thread starts):**
```c
static gboolean vosk_init_recognizer(void) {
    gchar *model_path = g_build_filename(g_get_user_data_dir(),
                                         "geanyvosk", "model", NULL);
    vosk_model = vosk_model_new(model_path);
    g_free(model_path);
    if (!vosk_model) return FALSE;
    vosk_rec = vosk_recognizer_new(vosk_model, (float)SAMPLE_RATE);
    return TRUE;
}
```

**Audio thread feeds recognizer:**
```c
/* inside audio_thread loop, after snd_pcm_readi: */
if (vosk_recognizer_accept_waveform_s(vosk_rec,
        (const char *)buf, frames * 2)) {
    const char *result = vosk_recognizer_result(vosk_rec);
    /* parse JSON result for "text" field, dispatch to command handler */
    g_idle_add(vosk_dispatch, g_strdup(result));
}
```

**Wake word / deactivate detection (in vosk_dispatch, main thread):**
```c
static gboolean vosk_dispatch(gpointer raw_json) {
    gchar *text = extract_text_from_json(raw_json);
    if (!active_mode) {
        if (strstr(text, "rub lamp"))
            vosk_set_active(TRUE);
    } else {
        if (strstr(text, "back in bottle"))
            vosk_set_active(FALSE);
        else
            vosk_handle_command(text);
    }
    g_free(text);
    g_free(raw_json);
    return G_SOURCE_REMOVE;
}
```

**Status bar label:**
```c
static void vosk_set_active(gboolean on) {
    active_mode = on;
    gtk_label_set_text(GTK_LABEL(status_label),
                       on ? "Voice: ACTIVE" : "Voice: idle");
}
```

Add `status_label` to `geany->main_widgets->statusbar` hbox in `plugin_init`.

### Acceptance criteria

- Say "Rub lamp" → status bar shows "Voice: ACTIVE".
- Say "Back in bottle" → status bar shows "Voice: idle".
- Unrecognised speech in active mode logs text to Geany console (debug).
- Write `PROGRESS.md` entry: Phase 3 complete.

---

## Phase 4 — UI Command Dispatch via geanycontrol

**Goal:** Recognised utterances in active mode are mapped to geanycontrol socket
commands (open-file, tab-switching, menu activation).

### geanycontrol socket helper

```c
/* Send one command line to geanycontrol socket; return reply or NULL */
static gchar *ctrl_send(const gchar *cmd) {
    const gchar *sock = g_build_filename(g_get_user_config_dir(),
                                         "geany", "geanycontrol.sock", NULL);
    GSocketClient *client = g_socket_client_new();
    GSocketConnection *conn = g_socket_client_connect_to_path(
        client, sock, NULL, NULL);
    /* write cmd + "\n", read reply line */
    /* ... */
}
```

### Command table

Map spoken phrases → geanycontrol commands. Use a simple table of
`{spoken_pattern, ctrl_command}` pairs, matched with `strstr` in order:

| Spoken (substring match) | geanycontrol command    |
|----------------------|-----------------------------|
| "switch to agent"    | `switch-tab Agent`          |
| "switch to cli"      | `switch-tab CLI`            |
| "open file \<name\>" | `open-file <resolved path>` |
| "save all"           | `save-all`                  |
| "close file"         | `close-file <current>`      |
| "refresh"            | `refresh`                   |

**Tab switching** — geanycontrol doesn't yet have a `switch-tab` command. Phase 4
adds one:

New command in `geanycontrol.c`:
```
switch-tab <label>   (case-insensitive partial match on notebook tab labels)
```

Implementation in geanycontrol: iterate `gtk_notebook_get_n_pages` on the
message-window notebook, compare label text, call `gtk_notebook_set_current_page`.

### vosk_handle_command() skeleton

```c
typedef struct { const gchar *pattern; const gchar *ctrl_cmd_fmt; } CmdEntry;
static const CmdEntry CMDS[] = {
    { "switch to agent", "switch-tab Agent" },
    { "switch to cli",   "switch-tab CLI"   },
    { "save all",        "save-all"         },
    { "refresh",         "refresh"          },
    { NULL, NULL }
};

static void vosk_handle_command(const gchar *text) {
    for (const CmdEntry *e = CMDS; e->pattern; e++) {
        if (strstr(text, e->pattern)) {
            ctrl_send(e->ctrl_cmd_fmt);
            return;
        }
    }
    /* fall through to Phase 5 handler */
    vosk_handle_skill_command(text);
}
```

### Acceptance criteria

- Say "switch to Agent" → Geany focuses the Agent message-window tab.
- Say "switch to CLI" → Geany focuses the CLI tab.
- Say "save all" → all modified documents are saved.
- Write `PROGRESS.md` entry: Phase 4 complete.

---

## Phase 5 — Skill & AI-Prompt Creation by Voice

**Goal:** Voice commands create new `.claude/skills/NAME.md` files or
`ai-prompts/NAME.md` files, open them in the editor, and put focus there for
dictation (speech-to-text into the open buffer).

### Command parsing

Patterns (matched in order, before generic ctrl dispatch):

| Spoken prefix                     | Action                                                                        |
|-----------------------------------|-------------------------------------------------------------------------------|
| "agent create new skill \<name\>" | Create `~/.claude/skills/<name>.md` from SKILL_TEMPLATE, open in editor       |
| "new ai prompt \<name\> dot md"   | Create `./ai-prompts/<name>.md`, open in editor                               |
| "dictate"                         | Begin speech-to-text mode: subsequent utterances appended to current document |
| "stop dictating"                  | Exit speech-to-text mode                                                      |

### Skill template

Duplicate the `SKILL_TEMPLATE` from `geanyagent.c` locally in `geanyvosk.c`
to avoid a cross-plugin header dependency.

### Name extraction

```c
/* "agent create new skill my cool skill" → "my-cool-skill" */
static gchar *extract_skill_name(const gchar *text, const gchar *prefix) {
    const gchar *p = strstr(text, prefix);
    if (!p) return NULL;
    p += strlen(prefix);
    while (*p == ' ') p++;
    return g_strdelimit(g_strstrip(g_strdup(p)), " ", '-');
}
```

### File creation helper

```c
static void vosk_create_and_open(const gchar *path, const gchar *content) {
    if (!g_file_set_contents(path, content, -1, NULL)) {
        /* log error */
        return;
    }
    gchar *cmd = g_strdup_printf("open-file %s", path);
    ctrl_send(cmd);
    g_free(cmd);
}
```

### Speech-to-text dictation mode

When `dictate_mode` is TRUE, `vosk_handle_command` bypasses the command table
and instead appends the recognised text to the current document via the
geanycontrol `append-text` command (to be added to geanycontrol.c):

```
append-text <text>    (inserts text at current cursor position)
```

geanycontrol implementation: `sci_insert_text(doc->editor->sci, -1, text)`.

### Acceptance criteria

- Say "agent create new skill my test skill" → `~/.claude/skills/my-test-skill.md`
  is created from template and opens in Geany editor.
- Say "new ai prompt project notes dot md" → `./ai-prompts/project-notes.md` created
  and opened.
- Say "dictate" then speak sentences → text appears in the focused editor buffer.
- Say "stop dictating" → returns to command mode.
- Write `PROGRESS.md` entry: Phase 5 complete.

---

## Phase 6 — Agent Skill Execution by Voice

**Goal:** Recognise the "Agent Skill \<model\> \<verb\> context \<file\> go" pattern,
build the Claude Code skill invocation command, and send it to the active geanyagent
VTE terminal when "go" is heard.

### Command grammar

```
"agent skill" <model> <verb> ["context" <file>] "go"
```

Examples from spec:
```
Agent skill opus plan context my project dot md go
Agent skill opus exec context my project dot md go
```

| Token            | Values                                                                     | Maps to                 |
|------------------|----------------------------------------------------------------------------|-------------------------|
| model            | opus → claude-opus-4, sonnet → claude-sonnet-4-6, haiku → claude-haiku-4-5 | `--model` flag          |
| verb             | plan → `/opus-plan`, exec → `/opus-exec`                                   | skill name              |
| context \<file\> | optional                                                                   | `@ai-prompts/<file>.md` |

### State machine

```c
typedef enum { SCMD_IDLE, SCMD_ARMED } SkillCmdState;

typedef struct {
    SkillCmdState state;
    gchar        *model;
    gchar        *skill;
    gchar        *context_file;
} SkillCmd;

static SkillCmd pending_cmd = {0};
```

**In vosk_handle_command:**
```c
if (strstr(text, "agent skill")) {
    vosk_parse_skill_cmd(text, &pending_cmd);
    pending_cmd.state = SCMD_ARMED;
    /* visual feedback: status bar shows "Skill armed — say Go" */
    return;
}
if (pending_cmd.state == SCMD_ARMED && strstr(text, "go")) {
    vosk_execute_skill(&pending_cmd);
    memset(&pending_cmd, 0, sizeof(pending_cmd));
    return;
}
```

### Command construction

```c
static void vosk_execute_skill(const SkillCmd *cmd) {
    GString *line = g_string_new("/");
    g_string_append(line, cmd->skill);       /* e.g. "opus-plan" */
    if (cmd->context_file) {
        g_string_append_printf(line, " @ai-prompts/%s", cmd->context_file);
    }
    g_string_append(line, "\n");

    gchar *ctrl_cmd = g_strdup_printf("send-to-agent %s", line->str);
    ctrl_send(ctrl_cmd);
    g_free(ctrl_cmd);
    g_string_free(line, TRUE);
}
```

**geanycontrol addition — `send-to-agent <text>` command:**

Emits a GLib signal `"geanycontrol-send-to-agent"` with the text payload.
`geanyagent.c` connects to this signal and calls `vte_terminal_feed_child`
on the active agent VTE terminal.

### "dot md" normalization

```c
/* "my project dot md" → "my-project.md" */
static gchar *voice_to_filename(const gchar *spoken) {
    gchar *s = g_strreplace(g_strdup(spoken), " dot ", ".");
    return g_strdelimit(g_strstrip(s), " ", '-');
}
```

### Acceptance criteria

- Say "Agent skill opus plan context my project dot md" → status bar shows "Skill armed — say Go".
- Say "go" → `/opus-plan @ai-prompts/my-project.md\n` is sent to the active geanyagent terminal.
- Nothing executes before "go" is spoken.
- Say "Back in bottle" while armed → armed state clears, no execution.
- Write `PROGRESS.md` entry: Phase 6 complete.

---

## Cross-cutting concerns

### Thread safety

All Vosk/ALSA calls happen on the audio capture thread. All GTK/Geany calls happen
on the GLib main thread via `g_idle_add`. The dispatch function signature is
always `gboolean dispatch(gpointer raw_json)` scheduled with `g_idle_add`.

### Config file

`~/.config/geany/geanyvosk.conf`:
```ini
[geanyvosk]
model_path=/home/user/.local/share/geanyvosk/model
alsa_device=default
wake_word=rub lamp
deactivate_word=back in bottle
```

Loaded in `plugin_init`, editable via a preferences dialog added in Phase 6+.

### Error handling

- Model not found → message_window warning, plugin still loads (just silent).
- ALSA open fails → message_window error, audio thread not started.
- geanycontrol socket not available → log warning per command attempt, no crash.

---

## Phase execution order

| Phase | Key output                             | Est. context cost |
|-------|----------------------------------------|-------------------|
| 1     | Compilable scaffold                    | Low               |
| 2     | ALSA audio capture thread              | Medium            |
| 3     | Vosk ASR + wake word                   | High              |
| 4     | geanycontrol UI dispatch + switch-tab  | Medium            |
| 5     | Skill/prompt file creation + dictation | Medium            |
| 6     | Agent skill execution state machine    | Medium            |

Each phase ends with a `PROGRESS.md` update so the next session can `/clear`
and pick up from a clean context.
