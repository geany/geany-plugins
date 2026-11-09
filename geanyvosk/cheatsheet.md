Voice Commands
--------------

Wake phrase - **rub lamp**
Sleep phrase - **back in the lamp**

  UI control (requires GeanyControl plugin):
    **switch to agent**           — focus the Agent tab in the message window
    **go to agent** / **show agent**   — aliases for switch to agent (Vosk-friendly)
    **switch to cli** / **go to cli**  — focus the CLI tab
    **save all**                  — save all open documents
    **save current file**         — save the active document
    **save the file**             — alias for save current file
    **save as <spoken path>**     — save-as with fuzzy filesystem path resolution
                                    e.g. "save as docs slash readme dot md"
    **open file <name>**          — switch to open document matching <name> (fuzzy)
                                    e.g. "open file geany control" → geanycontrol.c
                                         "open file tree browser"  → treebrowser.c
    **switch to file <name>**     — alias for open file <name> (fuzzy)
    **new file**                  — open a new untitled document
    **close file**                — close the current file
    **refresh**                   — emit geanycontrol-refresh signal

  File creation:
    **new skill <name>**                — shorter alias: create ~/.claude/skills/<name>.md
    **agent create new skill <name>**   — create ~/.claude/skills/<name>.md from
                                        template and open it in the editor
    **new ai prompt <name> dot md**     — create ./ai-prompts/<name>.md and open it
    **new ay eye prompt <name>**        — Vosk-friendly alias for new ai prompt
    **new a i prompt <name>**           — alternate alias for new ai prompt

  Fuzzy path matching (used by "save as"):
    Spoken path components are split on " slash " and each directory
    segment is matched case-insensitively against entries on disk.
    The last component is used as-is for the filename.
    " dot " within any component becomes ".".
    e.g. "my docs slash github slash readme dot md"
         → /base/my-docs/github/README.md  (fuzzy matched)

  Fuzzy file matching (used by "open file" / "switch to file"):
    Spoken name is split into tokens; each token that appears in an open
    document's filename scores 1 point. The highest-scoring open file is
    activated. Tokens are matched as substrings (case-insensitive).
    e.g. "open file geany control" → tokens ["geany","control"]
         geanycontrol.c scores 2, geanyvosk.c scores 1 → geanycontrol.c wins

  Menu navigation:
    **menu <name>**               — open a top-level menu by name (fuzzy) e.g. "menu file",
                                  "menu edit", "menu build"
                                  Status bar shows "menu". The first item is
                                  pre-selected.
    **<item name>**               — highlight the item whose label contains the spoken
                                  text (fuzzy — case-insensitive substring match)
    "up" / "up <N>"             — move selection up one or N items
    "down" / "down <N>"         — move selection down one or N items
    "go"                        — activate the highlighted item
    **back in the lamp** / **cancel** — close the menu without activating anything

    Example:
      "menu file"       → opens File menu, first item selected
      "save all"        → highlights "Save All" item
      "go"              → executes it

  Tree browser control (requires TreeBrowser + GeanyControl plugins):
    "sidebar"                   — enter tree navigation mode.  Status bar shows
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
      "sidebar"        → focus tree panel, status = "tree"
      "down three"     → move cursor down 3 rows
      "go"             → open the selected file
      "menu"           → show right-click menu, status = "menu"
      "delete"         → highlight Delete item
      "go"             → execute it

  Dictation:
    "dictate"                   — enter dictation mode; subsequent speech is appended as text to the current document
    "stop dictating"            — return to command mode

    While dictating, the following spoken phrases are converted to punctuation
    or formatting instead of being inserted literally:

      "full stop"         → .
      "comma"             → ,
      "question mark"     → ?
      "exclamation mark"  → !
      "colon"             → :
      "semicolon"         → ;
      "open bracket"      → (
      "close bracket"     → )
      "new line"          → newline character
      "new paragraph"     → two newlines
      "start list"        → newline + "- "
      "list item"         → newline + "- "
      "tab"               → tab character

  Agent skill execution (requires GeanyAgent + GeanyControl):
    "agent skill <model> <verb> [context <file> dot md]"
                                — arm a skill command (status bar shows
                                  "Skill armed — say Go")
    "go"                        — execute the armed skill in the agent terminal

    model values : opus, sonnet, haiku
    verb values  : plan, exec

    Example:
      "agent skill opus plan context my project dot md"
      "go"
      → sends: /opus-plan @ai-prompts/my-project.md
