
/*
 *******************  !!! IMPORTANT !!!  ***************************
 *
 * This is a machine generated file, do not edit by hand!
 * If you need to modify this file, see "geanylua/util/mk-keytab.lua"
 *
 *******************************************************************
 *
 */


typedef struct _KeyCmdHashEntry {
	const gchar *name;
	guint group;
	guint key_id;
} KeyCmdHashEntry;


static KeyCmdHashEntry key_cmd_hash_entries[] = {
	{"FILE_NEW", GEANY_KEY_GROUP_FILE, GEANY_KEYS_FILE_NEW},
	{"FILE_OPEN", GEANY_KEY_GROUP_FILE, GEANY_KEYS_FILE_OPEN},
	{"FILE_OPENSELECTED", GEANY_KEY_GROUP_FILE, GEANY_KEYS_FILE_OPENSELECTED},
	{"FILE_SAVE", GEANY_KEY_GROUP_FILE, GEANY_KEYS_FILE_SAVE},
	{"FILE_SAVEAS", GEANY_KEY_GROUP_FILE, GEANY_KEYS_FILE_SAVEAS},
	{"FILE_SAVEALL", GEANY_KEY_GROUP_FILE, GEANY_KEYS_FILE_SAVEALL},
	{"FILE_PRINT", GEANY_KEY_GROUP_FILE, GEANY_KEYS_FILE_PRINT},
	{"FILE_CLOSE", GEANY_KEY_GROUP_FILE, GEANY_KEYS_FILE_CLOSE},
	{"FILE_CLOSEALL", GEANY_KEY_GROUP_FILE, GEANY_KEYS_FILE_CLOSEALL},
	{"FILE_RELOAD", GEANY_KEY_GROUP_FILE, GEANY_KEYS_FILE_RELOAD},
	{"PROJECT_PROPERTIES", GEANY_KEY_GROUP_PROJECT, GEANY_KEYS_PROJECT_PROPERTIES},
	{"EDITOR_UNDO", GEANY_KEY_GROUP_EDITOR, GEANY_KEYS_EDITOR_UNDO},
	{"EDITOR_REDO", GEANY_KEY_GROUP_EDITOR, GEANY_KEYS_EDITOR_REDO},
	{"EDITOR_DELETELINE", GEANY_KEY_GROUP_EDITOR, GEANY_KEYS_EDITOR_DELETELINE},
	{"EDITOR_DUPLICATELINE", GEANY_KEY_GROUP_EDITOR, GEANY_KEYS_EDITOR_DUPLICATELINE},
	{"EDITOR_TRANSPOSELINE", GEANY_KEY_GROUP_EDITOR, GEANY_KEYS_EDITOR_TRANSPOSELINE},
	{"EDITOR_SCROLLTOLINE", GEANY_KEY_GROUP_EDITOR, GEANY_KEYS_EDITOR_SCROLLTOLINE},
	{"EDITOR_SCROLLLINEUP", GEANY_KEY_GROUP_EDITOR, GEANY_KEYS_EDITOR_SCROLLLINEUP},
	{"EDITOR_SCROLLLINEDOWN", GEANY_KEY_GROUP_EDITOR, GEANY_KEYS_EDITOR_SCROLLLINEDOWN},
	{"EDITOR_COMPLETESNIPPET", GEANY_KEY_GROUP_EDITOR, GEANY_KEYS_EDITOR_COMPLETESNIPPET},
	{"EDITOR_SUPPRESSSNIPPETCOMPLETION", GEANY_KEY_GROUP_EDITOR, GEANY_KEYS_EDITOR_SUPPRESSSNIPPETCOMPLETION},
	{"EDITOR_CONTEXTACTION", GEANY_KEY_GROUP_EDITOR, GEANY_KEYS_EDITOR_CONTEXTACTION},
	{"EDITOR_AUTOCOMPLETE", GEANY_KEY_GROUP_EDITOR, GEANY_KEYS_EDITOR_AUTOCOMPLETE},
	{"EDITOR_CALLTIP", GEANY_KEY_GROUP_EDITOR, GEANY_KEYS_EDITOR_CALLTIP},
	{"EDITOR_MACROLIST", GEANY_KEY_GROUP_EDITOR, GEANY_KEYS_EDITOR_MACROLIST},
	{"CLIPBOARD_CUT", GEANY_KEY_GROUP_CLIPBOARD, GEANY_KEYS_CLIPBOARD_CUT},
	{"CLIPBOARD_COPY", GEANY_KEY_GROUP_CLIPBOARD, GEANY_KEYS_CLIPBOARD_COPY},
	{"CLIPBOARD_PASTE", GEANY_KEY_GROUP_CLIPBOARD, GEANY_KEYS_CLIPBOARD_PASTE},
	{"CLIPBOARD_CUTLINE", GEANY_KEY_GROUP_CLIPBOARD, GEANY_KEYS_CLIPBOARD_CUTLINE},
	{"CLIPBOARD_COPYLINE", GEANY_KEY_GROUP_CLIPBOARD, GEANY_KEYS_CLIPBOARD_COPYLINE},
	{"SELECT_ALL", GEANY_KEY_GROUP_SELECT, GEANY_KEYS_SELECT_ALL},
	{"SELECT_WORD", GEANY_KEY_GROUP_SELECT, GEANY_KEYS_SELECT_WORD},
	{"SELECT_LINE", GEANY_KEY_GROUP_SELECT, GEANY_KEYS_SELECT_LINE},
	{"SELECT_PARAGRAPH", GEANY_KEY_GROUP_SELECT, GEANY_KEYS_SELECT_PARAGRAPH},
	{"FORMAT_TOGGLECASE", GEANY_KEY_GROUP_FORMAT, GEANY_KEYS_FORMAT_TOGGLECASE},
	{"FORMAT_COMMENTLINETOGGLE", GEANY_KEY_GROUP_FORMAT, GEANY_KEYS_FORMAT_COMMENTLINETOGGLE},
	{"FORMAT_COMMENTLINE", GEANY_KEY_GROUP_FORMAT, GEANY_KEYS_FORMAT_COMMENTLINE},
	{"FORMAT_UNCOMMENTLINE", GEANY_KEY_GROUP_FORMAT, GEANY_KEYS_FORMAT_UNCOMMENTLINE},
	{"FORMAT_INCREASEINDENT", GEANY_KEY_GROUP_FORMAT, GEANY_KEYS_FORMAT_INCREASEINDENT},
	{"FORMAT_DECREASEINDENT", GEANY_KEY_GROUP_FORMAT, GEANY_KEYS_FORMAT_DECREASEINDENT},
	{"FORMAT_INCREASEINDENTBYSPACE", GEANY_KEY_GROUP_FORMAT, GEANY_KEYS_FORMAT_INCREASEINDENTBYSPACE},
	{"FORMAT_DECREASEINDENTBYSPACE", GEANY_KEY_GROUP_FORMAT, GEANY_KEYS_FORMAT_DECREASEINDENTBYSPACE},
	{"FORMAT_AUTOINDENT", GEANY_KEY_GROUP_FORMAT, GEANY_KEYS_FORMAT_AUTOINDENT},
	{"FORMAT_SENDTOCMD1", GEANY_KEY_GROUP_FORMAT, GEANY_KEYS_FORMAT_SENDTOCMD1},
	{"FORMAT_SENDTOCMD2", GEANY_KEY_GROUP_FORMAT, GEANY_KEYS_FORMAT_SENDTOCMD2},
	{"FORMAT_SENDTOCMD3", GEANY_KEY_GROUP_FORMAT, GEANY_KEYS_FORMAT_SENDTOCMD3},
	{"INSERT_DATE", GEANY_KEY_GROUP_INSERT, GEANY_KEYS_INSERT_DATE},
	{"INSERT_ALTWHITESPACE", GEANY_KEY_GROUP_INSERT, GEANY_KEYS_INSERT_ALTWHITESPACE},
	{"SETTINGS_PREFERENCES", GEANY_KEY_GROUP_SETTINGS, GEANY_KEYS_SETTINGS_PREFERENCES},
	{"SEARCH_FIND", GEANY_KEY_GROUP_SEARCH, GEANY_KEYS_SEARCH_FIND},
	{"SEARCH_FINDNEXT", GEANY_KEY_GROUP_SEARCH, GEANY_KEYS_SEARCH_FINDNEXT},
	{"SEARCH_FINDPREVIOUS", GEANY_KEY_GROUP_SEARCH, GEANY_KEYS_SEARCH_FINDPREVIOUS},
	{"SEARCH_FINDINFILES", GEANY_KEY_GROUP_SEARCH, GEANY_KEYS_SEARCH_FINDINFILES},
	{"SEARCH_REPLACE", GEANY_KEY_GROUP_SEARCH, GEANY_KEYS_SEARCH_REPLACE},
	{"SEARCH_FINDNEXTSEL", GEANY_KEY_GROUP_SEARCH, GEANY_KEYS_SEARCH_FINDNEXTSEL},
	{"SEARCH_FINDPREVSEL", GEANY_KEY_GROUP_SEARCH, GEANY_KEYS_SEARCH_FINDPREVSEL},
	{"SEARCH_NEXTMESSAGE", GEANY_KEY_GROUP_SEARCH, GEANY_KEYS_SEARCH_NEXTMESSAGE},
	{"SEARCH_FINDUSAGE", GEANY_KEY_GROUP_SEARCH, GEANY_KEYS_SEARCH_FINDUSAGE},
	{"GOTO_FORWARD", GEANY_KEY_GROUP_GOTO, GEANY_KEYS_GOTO_FORWARD},
	{"GOTO_BACK", GEANY_KEY_GROUP_GOTO, GEANY_KEYS_GOTO_BACK},
	{"GOTO_LINE", GEANY_KEY_GROUP_GOTO, GEANY_KEYS_GOTO_LINE},
	{"GOTO_MATCHINGBRACE", GEANY_KEY_GROUP_GOTO, GEANY_KEYS_GOTO_MATCHINGBRACE},
	{"GOTO_TOGGLEMARKER", GEANY_KEY_GROUP_GOTO, GEANY_KEYS_GOTO_TOGGLEMARKER},
	{"GOTO_NEXTMARKER", GEANY_KEY_GROUP_GOTO, GEANY_KEYS_GOTO_NEXTMARKER},
	{"GOTO_PREVIOUSMARKER", GEANY_KEY_GROUP_GOTO, GEANY_KEYS_GOTO_PREVIOUSMARKER},
	{"GOTO_TAGDEFINITION", GEANY_KEY_GROUP_GOTO, GEANY_KEYS_GOTO_TAGDEFINITION},
	{"GOTO_TAGDECLARATION", GEANY_KEY_GROUP_GOTO, GEANY_KEYS_GOTO_TAGDECLARATION},
	{"VIEW_TOGGLEALL", GEANY_KEY_GROUP_VIEW, GEANY_KEYS_VIEW_TOGGLEALL},
	{"VIEW_FULLSCREEN", GEANY_KEY_GROUP_VIEW, GEANY_KEYS_VIEW_FULLSCREEN},
	{"VIEW_MESSAGEWINDOW", GEANY_KEY_GROUP_VIEW, GEANY_KEYS_VIEW_MESSAGEWINDOW},
	{"VIEW_SIDEBAR", GEANY_KEY_GROUP_VIEW, GEANY_KEYS_VIEW_SIDEBAR},
	{"VIEW_ZOOMIN", GEANY_KEY_GROUP_VIEW, GEANY_KEYS_VIEW_ZOOMIN},
	{"VIEW_ZOOMOUT", GEANY_KEY_GROUP_VIEW, GEANY_KEYS_VIEW_ZOOMOUT},
	{"FOCUS_EDITOR", GEANY_KEY_GROUP_FOCUS, GEANY_KEYS_FOCUS_EDITOR},
	{"FOCUS_SCRIBBLE", GEANY_KEY_GROUP_FOCUS, GEANY_KEYS_FOCUS_SCRIBBLE},
	{"FOCUS_VTE", GEANY_KEY_GROUP_FOCUS, GEANY_KEYS_FOCUS_VTE},
	{"FOCUS_SEARCHBAR", GEANY_KEY_GROUP_FOCUS, GEANY_KEYS_FOCUS_SEARCHBAR},
	{"NOTEBOOK_SWITCHTABLEFT", GEANY_KEY_GROUP_NOTEBOOK, GEANY_KEYS_NOTEBOOK_SWITCHTABLEFT},
	{"NOTEBOOK_SWITCHTABRIGHT", GEANY_KEY_GROUP_NOTEBOOK, GEANY_KEYS_NOTEBOOK_SWITCHTABRIGHT},
	{"NOTEBOOK_SWITCHTABLASTUSED", GEANY_KEY_GROUP_NOTEBOOK, GEANY_KEYS_NOTEBOOK_SWITCHTABLASTUSED},
	{"NOTEBOOK_MOVETABLEFT", GEANY_KEY_GROUP_NOTEBOOK, GEANY_KEYS_NOTEBOOK_MOVETABLEFT},
	{"NOTEBOOK_MOVETABRIGHT", GEANY_KEY_GROUP_NOTEBOOK, GEANY_KEYS_NOTEBOOK_MOVETABRIGHT},
	{"NOTEBOOK_MOVETABFIRST", GEANY_KEY_GROUP_NOTEBOOK, GEANY_KEYS_NOTEBOOK_MOVETABFIRST},
	{"NOTEBOOK_MOVETABLAST", GEANY_KEY_GROUP_NOTEBOOK, GEANY_KEYS_NOTEBOOK_MOVETABLAST},
	{"DOCUMENT_REPLACETABS", GEANY_KEY_GROUP_DOCUMENT, GEANY_KEYS_DOCUMENT_REPLACETABS},
	{"DOCUMENT_FOLDALL", GEANY_KEY_GROUP_DOCUMENT, GEANY_KEYS_DOCUMENT_FOLDALL},
	{"DOCUMENT_UNFOLDALL", GEANY_KEY_GROUP_DOCUMENT, GEANY_KEYS_DOCUMENT_UNFOLDALL},
	{"DOCUMENT_RELOADTAGLIST", GEANY_KEY_GROUP_DOCUMENT, GEANY_KEYS_DOCUMENT_RELOADTAGLIST},
	{"BUILD_COMPILE", GEANY_KEY_GROUP_BUILD, GEANY_KEYS_BUILD_COMPILE},
	{"BUILD_LINK", GEANY_KEY_GROUP_BUILD, GEANY_KEYS_BUILD_LINK},
	{"BUILD_MAKE", GEANY_KEY_GROUP_BUILD, GEANY_KEYS_BUILD_MAKE},
	{"BUILD_MAKEOWNTARGET", GEANY_KEY_GROUP_BUILD, GEANY_KEYS_BUILD_MAKEOWNTARGET},
	{"BUILD_MAKEOBJECT", GEANY_KEY_GROUP_BUILD, GEANY_KEYS_BUILD_MAKEOBJECT},
	{"BUILD_NEXTERROR", GEANY_KEY_GROUP_BUILD, GEANY_KEYS_BUILD_NEXTERROR},
	{"BUILD_RUN", GEANY_KEY_GROUP_BUILD, GEANY_KEYS_BUILD_RUN},
	{"BUILD_OPTIONS", GEANY_KEY_GROUP_BUILD, GEANY_KEYS_BUILD_OPTIONS},
	{"TOOLS_OPENCOLORCHOOSER", GEANY_KEY_GROUP_TOOLS, GEANY_KEYS_TOOLS_OPENCOLORCHOOSER},
	{"HELP_HELP", GEANY_KEY_GROUP_HELP, GEANY_KEYS_HELP_HELP},
	{NULL, 0, 0}
};
