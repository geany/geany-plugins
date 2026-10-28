 # Find File

Geany has a "Find in Files..." feature but no "Find File" (by name) feature.

Windows has no such thing, but Linux has a fast indexed find file features in the OS called "locate" which we can use for the search engine.

Thus, we will call the new module the "locate" module.  

It should work very much like the "Find in Files..." feature, similar UI, similar output.

Our plugin needs the following features...

- Menu Item - "Find File"
- Search context is always from the project root.
- If possible, in GTK key binding to open the dialog should be double tapping the shift key.
- New dialog window.
- Support for case in-sensitive searches with `-i` (default to case-sensitive)
- If there are **no results** the dialog should stay open, so the user can refine the search.
- If there is **exactly one result** found, it should presented to the user in the dialog window (without another popup), to open that file, the user just clicks enter.
- If there are **multiple results** found, it should behave like "Find in Files..." by adding clickable links in the Messages tab.
- Menu Item "updatedb"  whis will run `sudo updatedb` in the background, sudo users privs are needed via popup of some sort.

It should be fast, and with focusing UI elements done corerctly, so by typing the key strokes...  
Shift, Shift (if possible), the exact name of a unique file in the project, Enter, Enter  
you can open a file in the editor and that file has focus.

