# unrecognized words through the geanyvosk plugin

"switch to " is not working (would benefit from visual feedback)
  → added "go to agent", "show agent", "go to cli" aliases to CMDS table

"new skill" followed by skill name    not working at all (would benefit from visual feedback)
  → DONE: "new skill <name>" now works (shorter alias, no "agent create" prefix needed)

"new ai prompt" not working, should we write this as "new ay eye" in the config?
  → DONE: "new ay eye prompt <name>" and "new a i prompt <name>" aliases added

"new file" should be supported
  → DONE: "new file" opens a new untitled document

"save as" should be supported, text after should be interpreted as a path with fuzzy matching
  → DONE: "save as <spoken path>" uses fuzzy_resolve_path() + document_save_file_as()

when dictating

"start list" should be supported
  → already implemented in DICT_KW: "start list" → "\n- "

"save current file" should be supported
  → DONE: "save current file" / "save the file" saves the active document

Regarding fuzzy path matching — create a library function that takes voice input and
fuzzy matches to an existing folder.
  → DONE: fuzzy_resolve_path(spoken, base) in geanyvosk.c
    splits on " slash ", case-insensitive substring match per directory component,
    " dot " within a component becomes ".", falls back to spoken name if no match.

## Still TODO / open questions

- Visual feedback flash when a command fires (brief color pulse on status label?)
- "switch to" still relies on Vosk producing the exact string; the new aliases help
  but could add even more variants if still unreliable
