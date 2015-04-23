geanynumberedbookmarks is a plugin to provide users with 10 numbered bookmarks
(in addition to the usual bookkmarks). It started out as part of the ConText
feature parity plugin, which was split into individual plugins to better suit
Geany's ethos of being as light as possible while allowing users to select
which features they want to add to the core editor. The idea was taken from a
Text Editor for Windows called ConText.

Normaly if you had more than one bookmark, you would have to cycle through them
until you reached the one you wanted. With this plugin you can go straight to
the bookmark that you want with a single key combination.

To set a numbered bookmark press Ctrl+Shift+(a number from 0 to 9). You will
see a marker apear next to the line number. If you press Ctrl+Shift+(a number)
on a line that already has that bookmark number then it removes the bookmark,
otherwise it will move the bookmark there if it was set on a different line,
or create it if it had not already been set. To move to a previously set
bookmark press Ctrl+(bookmark number). You can also specify when in the
bookmarked line the cursor is moved to when you move to a previously set
bookmark. You can choose to move to the start of the line, the end of the line,
how far into the line the cursor was when the bookmark was set, or try and keep
the cursor in the column that you are in at the moment (line length allowing).
Only the most recently set bookmark on a line will be shown, but you can have
more than one bookmark per line. This plugin does not interfer with regular
bookmarks. When a file is saved, Geany will remember the numbered bookmarks and
make sure that they are set the next time you open the file.

This plugin also will remember the state of folds in a file (open or not) if
you want it to and re-apply this the next time you open the file.

This plugin will also remember standard non-numbered bookmarks and restore
these when a file is next reloaded if you want it to.

You can alter the default behaviour of this plugin by selecting Plugin Manager
under the Tools menu, selecting this plugin, and cliking Preferences.
You can change:

Remember fold state
    if this is set then this plugin will remember the state of any folds along
    with the numbered bookmarks and set them when the file is next loaded.
Center view when goto bookmark
    If this is set it will try to make sure that the numbered bookmark that you
    are going to is in the center of the screen, otherwise it will simply be on
    the screen somewhere.
Move to...
    This allows you to choose where in the bookmarked line the cursor is placed
    when you move to a bookmarked line.
Save file settings...
    This allows you the option of saving the settings of a file (the numbered
    bookmark positions, folding states, and standard bookmark positions) in
    either the central settings file for geany plugins, or to a file with the
    same name but a suffix (by default this is ".gnbs.conf") in the same
    directory as the file. This allows the user the ability to synchronise the
    settings for a file along with the file itself across more than one
    computer. The default suffix can be changed by editing the numbered
    bookmarks pluggin settings file. This will vary from OS to OS but will
    always be "settings.conf" in a directory called "Geany_Numbered_Bookmarks".
    On my OS it's
    ".config/geany/plugins/Geany_Numbered_Bookmarks/settings.conf" in my home
    directory, but otherwise you'll have to search for it. In the Settings
    section of the file near the top will be a line
    "File_Details_Suffix=.gnbs.conf". Simply change this to whatever you would
    prefer (if geany is running while you edit this file it may over-write your
    new suffix so best close geany before editing this file).
Remember normal Bookmarks
    If this is set then the plugin will remember standard non-numbered
    bookmarks, and restore them when the file is next loaded.
