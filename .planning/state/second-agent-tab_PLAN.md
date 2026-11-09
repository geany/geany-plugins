# Second Agent Tab — Implementation Plan

## Goal

Add support for a second (and optionally more) AI chat tabs inside the existing
`geanyagent` plugin.  Each tab is an independent VTE terminal with its own
process, its own active-agent default, and its own tab label.  The feature is
opt-in: one tab by default; users add more via the configure dialog.

## Background / Architecture

`geanyagent.c` currently stores all per-tab state in static module-level
globals: `agent_term`, `agent_hbox`, `agent_label`, `tab_index`, `running`,
`agent_pid`, `active_agent`.  Every function that creates, spawns, or
interacts with the terminal touches these globals directly.

The `AgentConfig` struct (key, name, cmd) already supports multiple named
agents; they are listed in the right-click "Switch Agent" submenu.  The plan
reuses this and adds a parallel concept: an `AgentTab` struct that owns one
VTE terminal and its own `active_agent` index into the shared `agents` array.

Inter-plugin signals (`geanyagent-insert-to-agent`,
`geanycontrol-send-to-agent`) currently always target the single tab; after
this change they will target tab 0 by default (backward-compatible).

---

## Phase 1 — Extract per-tab state into `AgentTab` struct

**Scope:** `geanyagent/src/geanyagent.c` only.  No config changes, no new tabs
yet — identical runtime behaviour, just refactored internals.

### Changes

1. **Define `AgentTab`** near the top of the file (after `AgentConfig`):

   ```c
   typedef struct {
       VteTerminal *term;
       GtkWidget   *hbox;
       GtkWidget   *label;
       gint         tab_index;
       gint         active_agent;
       GPid         pid;
       gboolean     running;
   } AgentTab;
   ```

2. **Replace module-level globals** `agent_term`, `agent_hbox`, `agent_label`,
   `tab_index`, `agent_pid`, `running`, `active_agent` with:

   ```c
   static GPtrArray *tabs        = NULL;   /* AgentTab* elements */
   static AgentTab  *primary_tab = NULL;   /* tabs->pdata[0], convenience ptr */
   ```

3. **Add `agent_tab_new()` / `agent_tab_free()`** helpers that alloc/free an
   `AgentTab`.

4. **Rewrite every function that touches the old globals** to take an
   `AgentTab *at` parameter instead:
   - `ga_spawn(AgentTab *at)`
   - `ga_spawn_idle` — embed `AgentTab *` in a tiny heap struct passed as
     `gpointer data`
   - `on_spawn_done` / `on_child_exited` — same trick or use
     `g_object_set_data` on the VTE widget
   - `ga_switch_agent(AgentTab *at, gint index)`
   - `ga_restart(AgentTab *at)`, `ga_restart_all()`
   - `create_agent_tab(AgentTab *at)` — fills `at->term`, `at->hbox`, etc.
   - `update_agent_label(AgentTab *at)`
   - `ga_send_command(AgentTab *at, const gchar *cmd)`
   - `ga_run_agent_cmd(AgentTab *at, const gchar *expanded)`
   - All VTE signal handlers (`on_vte_button_press`, DnD handler, etc.) — use
     `g_object_get_data(G_OBJECT(vte), "agent-tab")` set during tab creation

5. **`ga_init`**: create one `AgentTab`, add to `tabs`, set `primary_tab`.

6. **`ga_cleanup`**: iterate `tabs`, kill each process, remove each notebook
   page, free all `AgentTab*`.

7. **Signal handlers** (`on_geanycontrol_send_to_agent`,
   `on_insert_to_agent_signal`, `on_restart_signal`) delegate to `primary_tab`
   unchanged.

### Verification

Compile and run.  Behaviour must be identical to pre-refactor: one tab,
auto-restart, right-click menu, agent switching all work.

```sh
geany-progress done 1 \
  -r geanyagent/src/geanyagent.c \
  -w "Binary behaviour must be identical to pre-refactor — test manually before proceeding"
```

---

## Phase 2 — Config support for multiple tabs

**Scope:** config load/save and `ga_init` / `ga_cleanup` only.

### Config format extension

New keys in `geanyagent.conf` under `[agent]`:

```ini
[agent]
tabs = tab1,tab2          ; comma-separated tab keys (default: tab1)

[tab:tab1]
label  = Claude           ; display label (defaults to active agent name)
agent  = claude           ; key into [agent:<key>] (defaults to first agent)

[tab:tab2]
label  = Gemini
agent  = gemini
```

Backward compat: if `tabs` key is absent, treat the existing config as a
single tab named `tab1` with the existing `active` agent as its agent.

### Changes

1. **Define `AgentTabConfig`** struct:

   ```c
   typedef struct {
       gchar *key;
       gchar *label;    /* NULL → use agent name */
       gchar *agent_key; /* which AgentConfig to default to */
   } AgentTabConfig;
   ```

2. **`ga_load_config`**: parse `tabs` key → build `GPtrArray *tab_configs`.
   Fallback to single-tab if absent.

3. **`ga_save_config`**: serialise all `AgentTabConfig` entries.

4. **`ga_init`**: iterate `tab_configs`, call `agent_tab_new()` +
   `create_agent_tab()` for each, `ga_spawn()` each via `g_idle_add`.

5. Each `AgentTab` holds a pointer to its `AgentTabConfig` so the label and
   default agent key are available at construction time.

6. `update_agent_label(AgentTab *at)` prefers `at->cfg->label` if set,
   otherwise falls back to the active `AgentConfig->name`.

### Default second tab

When a user manually edits `geanyagent.conf` to add `tabs = tab1,tab2` and a
`[tab:tab2]` group pointing to a different agent (e.g. `gemini`), a second tab
appears on next Geany launch.  No UI for this yet — that comes in Phase 3.

```sh
geany-progress done 2 \
  -r geanyagent/src/geanyagent.c \
  -w "Test: add tabs=tab1,tab2 manually to geanyagent.conf and verify two independent tabs spawn"
```

---

## Phase 3 — Configure dialog: add / remove tabs

**Scope:** `ga_configure()` and `on_configure_response()` only.

### UI layout

The existing configure dialog already has an "Agents" section (combo +
Add/Remove).  Add a **"Tabs"** section above or below it:

```
Tabs:
  [ tab1 ▼ ] [Add] [Remove]
  Label:   [________________]
  Agent:   [ claude ▼ ]
```

- Selecting a tab entry in the combo populates Label and Agent.
- "Add" creates a new tab config with a generated key and default label/agent.
- "Remove" deletes the selected tab (minimum 1 tab enforced).
- On OK/Apply: `ga_save_config()` then reconcile live tabs:
  - Tabs in config but not running → create + spawn.
  - Tabs running but removed from config → kill + remove notebook page + free.
  - Tabs that changed agent default → restart with new agent (optional: only
    if user hasn't manually switched).

### Implementation

1. Add `ConfigWidgets` fields: `tab_combo`, `tab_label_entry`, `tab_agent_combo`.
2. `on_tab_combo_changed`: populate label entry and agent combo from selected
   `AgentTabConfig`.
3. `on_tab_add_clicked`: append new `AgentTabConfig` to a local working copy.
4. `on_tab_remove_clicked`: remove from working copy; enforce len >= 1.
5. `on_configure_response`: diff working copy vs live tabs; apply changes.

```sh
geany-progress done 3 \
  -r geanyagent/src/geanyagent.c \
  -w "Reconcile logic (add/remove live tabs on Apply) is the tricky part — test add and remove separately"
```

---

## Phase 4 — Signal routing and inter-plugin polish

**Scope:** Signal handlers and minor UX fixes.

### Changes

1. **`geanyagent-insert-to-agent`**: add an optional tab-index parameter
   (string prefix `"tab1:"` or just plain text for primary tab).  Keep
   backward compat: plain text goes to `primary_tab`.

2. **Right-click "Add to Agent"** on editor tabs: if more than one agent tab
   is running, show a submenu "Add to Tab 1 / Tab 2 / …" instead of a single
   item.  Route each to the correct `AgentTab`.

3. **DnD handler** (`on_agent_vte_drag_data_received`): already per-tab since
   `agent_term` is resolved via `g_object_get_data` — no change needed if
   Phase 1 wired it correctly.

4. **`geanyagent_restart()`** exported symbol: restarts all tabs (keep
   existing behaviour for geanyenv).

5. **Tab label tooltip**: set the active agent name as tooltip on the notebook
   tab label widget so users can identify what is running without switching.

```sh
geany-progress done 4 \
  -r geanyagent/src/geanyagent.c \
  -w "Signal routing change is backward-compatible only if plain-text path stays identical — verify with geanyenv and geanyprogress"
```

---

## File inventory

| File | Phases touched |
|------|---------------|
| `geanyagent/src/geanyagent.c` | 1 2 3 4 |
| `geanyagent/src/Makefile.am` | — (no new files) |
| `~/.config/geany/plugins/geanyagent/geanyagent.conf` | 2 (format extends) |

No new source files are needed; all changes live in `geanyagent.c`.

---

## Out of scope

- Synchronising clipboard / context between tabs (each tab is independent).
- Saving per-tab terminal scrollback across sessions.
- A "broadcast to all tabs" mode.
- geanycli or geanychat as a separate plugin.
