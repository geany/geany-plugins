Extra Selection Plugin
======================

About
-----
The Extra Selection plugin adds the following functions to Geany:

Goto matching brace and select (Select to Matching Brace).

Goto line and select (Select to Line).

Toggle the current selection type between stream and rectangular
(without changing column mode, can be invoked while drag-selecting).

Ctrl-Shift-Alt-Left/Right/Home/End keys - same as Ctrl-Shift, but for
rectangular selection.

Column mode - while active, all (Ctrl)-Shift-movement keys do rectangle
selection instead of stream.

Selection with anchor instead of the Shift-movement keys.

--

"Movement keys" refers to the arrows, Home, End, Page Up and Page Down.

For more information, see the Usage section below.


Requirements
------------
Geany 0.19 or later and the respective headers and development libraries.


Installation
------------
This plugin is part of the geany-plugins project.
See the README file of that package.


Usage
-----
Under Tools -> Extra Selection, there are 7 new items: "Column Mode",
"Select to Line", "Select to Matching Brace", "Toggle Rectangular/Stream",
"Set Anchor", "Select to Anchor" and "Rectangle Select to Anchor".
Normally these should be bound to keys, for example Alt-C, Alt-Shift-L,
Ctrl-Shift-B, Ctrl+2, F12, Shift-F12 and Alt-Shift-F12. Now:

1. Position the cursor on an opening brace and invoke "Select to Matching
   Brace". The cursor will move to the closing brace, and the braced area
   will be selected.

2. Position the cursor on line 10, invoke "Select to Line" and enter 15.
   The cursor will move to line 15, and the area between the previous and
   the current cursor position will be selected.

3. Select a small rectangular area and press Control-Alt-Shift-Right. The
   cursor will move to the next word, extending the rectangurar selection.
   The complete new keys list is:

   Control-Alt-Shift	Extends the selection to
   -----------------	------------------------
   Left                 Previous word
   Right                Next word
   Up                   Previous paragraph
   Down                 Next paragraph
   Home                 Start of file
   End                  End of file

   Unfortunately, holding Alt for rectangular selection has some problems,
   which apply both to the standard Geany keys and these added by the
   plugin. Under Windows, the Alt-keypad keys generate unicodes, even if
   used with Shift or Control. With X11, some Alt-(Ctrl)-(Shift)-movement
   keys may be used by the window manager for moving windows, switching to
   the previous/next desktop etc. So then:

4. Turn "Column Mode" on. While active, the (Control)-Shift-movement keys
   will select a rectangle instead of stream, without the need to hold
   Alt (in fact, the (Control)-Alt-Shift-movement keys will be temporarily
   blocked). This way, you will avoid the Alt key problems mentioned
   above, and it's more convenient to select while holding Shift only.

5. Finally, invoke "Set Anchor", navigate to another point in the document
   and invoke "Select to Anchor" - the result is obvious. Note that the
   anchor position is set exactly where the insertion cursor is, not on
   the character right of the cursor (or on the overtype cursor). When the
   column mode is active, "Rectangle Selection to Anchor" is disabled, and
   "Select to Anchor" does rectangular selection.


Known issues
------------
Rectangular selection of more than a few thousand lines is slow, so you
may prefer to use stream selection and convert it to rectangular. However,
if the cursor is on a large line number, such conversion may cause a
noticable delay. Both issues are scintilla related and can not be fixed by
this plugin.

If you set the anchor beyond a line end (in the "virtual space"), and edit
the text before it, the anchor will be moved to the line end. Tracking a
virtual (after EOLN) cursor position is impossible in scintilla.


License
-------
Extra Selection is distributed under the terms of the GNU General Public
License as published by the Free Software Foundation; either version 2 of
the License, or (at your option) any later version. A copy of this license
can be found in the file COPYING included with the source code of this
program.


Ideas, questions, patches and bug reports
-----------------------------------------
Dimitar Zhekov <dimitar.zhekov@gmail.com>
