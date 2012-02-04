geanymacro is a plugin to provide user defined macros for Geany. It started out
as part of the ConText feature parity plugin, which was split into individual
plugins to better suit Geany's ethos of being as light as possible while
allowing users to select which features they want to add to the core editor.
The idea was taken from a Text Editor for Windows called ConText.

This plugin allows you to record and use your own macros. Macros are sequences
of actions that can then be repeated with a single key combination. So if you
had dozens of lines where you wanted to delete the last 2 characters, you could
simple start recording, press End, Backspace, Backspace, down line and then
stop recording. Then simply trigger the macro and it would automatically edit
the line and move to the next. You could then just repeatedly trigger the macro
to do as many lines as you want.

Select Record Macro from the Tools menu and you will be prompted with a dialog
box. You need to specify a key combination that isn't being used, and a name
for the macro to help you identify it. Then press Record. What you do in the
editor is then recorded until you select Stop Recording Macro from the Tools
menu. Simply pressing the specified key combination will re-run the macro.

To edit the macros you already have, select Edit Macro from the Tools menu. You
can select a macro and delete it, or re-record it. Selecting the edit option
allows you to view all the individual elements that make up the macro. You can
select a diferent command for each element, move them, add new elements, delete
elements, or if it's replace/insert, you can edit the text that replaces the
selected text, or is inserted. You can also click on a macro's name and change
it, or the key combination and re-define that assuming that the new name or key
combination are not already in use.

The only thing to bear in mind is that undo and redo actions are not recorded,
and won't be replayed when the macro is re-run.

You can alter the default behaviour of this plugin by selecting Plugin Manager
under the Tools menu, selecting this plugin, and cliking Preferences.
You can change:

Save Macros when close Geany - If this is selected then Geany will save any
    recorded macros and reload them for use the next time you open Geany, if
    not they will be lost when Geany is closed.
Ask before replaceing existing Macros - If this is selected then if you try
    recording a macro over an existing one it will check before over-writing
    it, giving you the option of trying a different name or key trigger
    combination, otherwise it will simply erase any existing macros with the
    same name, or the same key trigger combination.