
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
	{"BUILD_COMPILE", GEANY_KEY_GROUP_BUILD, GEANY_KEYS_BUILD_COMPILE},
	{"BUILD_LINK", GEANY_KEY_GROUP_BUILD, GEANY_KEYS_BUILD_LINK},
	{"BUILD_MAKE", GEANY_KEY_GROUP_BUILD, GEANY_KEYS_BUILD_MAKE},
	{"BUILD_MAKEOBJECT", GEANY_KEY_GROUP_BUILD, GEANY_KEYS_BUILD_MAKEOBJECT},
	{"BUILD_MAKEOWNTARGET", GEANY_KEY_GROUP_BUILD, GEANY_KEYS_BUILD_MAKEOWNTARGET},
	{"BUILD_NEXTERROR", GEANY_KEY_GROUP_BUILD, GEANY_KEYS_BUILD_NEXTERROR},
	{"BUILD_OPTIONS", GEANY_KEY_GROUP_BUILD, GEANY_KEYS_BUILD_OPTIONS},
	{"BUILD_PREVIOUSERROR", GEANY_KEY_GROUP_BUILD, GEANY_KEYS_BUILD_PREVIOUSERROR},
	{"BUILD_RUN", GEANY_KEY_GROUP_BUILD, GEANY_KEYS_BUILD_RUN},
	{"CLIPBOARD_COPY", GEANY_KEY_GROUP_CLIPBOARD, GEANY_KEYS_CLIPBOARD_COPY},
	{"CLIPBOARD_COPYLINE", GEANY_KEY_GROUP_CLIPBOARD, GEANY_KEYS_CLIPBOARD_COPYLINE},
	{"CLIPBOARD_CUT", GEANY_KEY_GROUP_CLIPBOARD, GEANY_KEYS_CLIPBOARD_CUT},
	{"CLIPBOARD_CUTLINE", GEANY_KEY_GROUP_CLIPBOARD, GEANY_KEYS_CLIPBOARD_CUTLINE},
	{"CLIPBOARD_PASTE", GEANY_KEY_GROUP_CLIPBOARD, GEANY_KEYS_CLIPBOARD_PASTE},
	{"COUNT", GEANY_KEY_GROUP_COUNT, GEANY_KEYS_COUNT},
	{"DOCUMENT_CLONE", GEANY_KEY_GROUP_DOCUMENT, GEANY_KEYS_DOCUMENT_CLONE},
	{"DOCUMENT_FOLDALL", GEANY_KEY_GROUP_DOCUMENT, GEANY_KEYS_DOCUMENT_FOLDALL},
	{"DOCUMENT_LINEBREAK", GEANY_KEY_GROUP_DOCUMENT, GEANY_KEYS_DOCUMENT_LINEBREAK},
	{"DOCUMENT_LINEWRAP", GEANY_KEY_GROUP_DOCUMENT, GEANY_KEYS_DOCUMENT_LINEWRAP},
	{"DOCUMENT_RELOADTAGLIST", GEANY_KEY_GROUP_DOCUMENT, GEANY_KEYS_DOCUMENT_RELOADTAGLIST},
	{"DOCUMENT_REMOVE_ERROR_INDICATORS", GEANY_KEY_GROUP_DOCUMENT, GEANY_KEYS_DOCUMENT_REMOVE_ERROR_INDICATORS},
	{"DOCUMENT_REMOVE_MARKERS", GEANY_KEY_GROUP_DOCUMENT, GEANY_KEYS_DOCUMENT_REMOVE_MARKERS},
	{"DOCUMENT_REMOVE_MARKERS_INDICATORS", GEANY_KEY_GROUP_DOCUMENT, GEANY_KEYS_DOCUMENT_REMOVE_MARKERS_INDICATORS},
	{"DOCUMENT_REPLACESPACES", GEANY_KEY_GROUP_DOCUMENT, GEANY_KEYS_DOCUMENT_REPLACESPACES},
	{"DOCUMENT_REPLACETABS", GEANY_KEY_GROUP_DOCUMENT, GEANY_KEYS_DOCUMENT_REPLACETABS},
	{"DOCUMENT_STRIPTRAILINGSPACES", GEANY_KEY_GROUP_DOCUMENT, GEANY_KEYS_DOCUMENT_STRIPTRAILINGSPACES},
	{"DOCUMENT_TOGGLEFOLD", GEANY_KEY_GROUP_DOCUMENT, GEANY_KEYS_DOCUMENT_TOGGLEFOLD},
	{"DOCUMENT_UNFOLDALL", GEANY_KEY_GROUP_DOCUMENT, GEANY_KEYS_DOCUMENT_UNFOLDALL},
	{"EDITOR_AUTOCOMPLETE", GEANY_KEY_GROUP_EDITOR, GEANY_KEYS_EDITOR_AUTOCOMPLETE},
	{"EDITOR_CALLTIP", GEANY_KEY_GROUP_EDITOR, GEANY_KEYS_EDITOR_CALLTIP},
	{"EDITOR_COMPLETESNIPPET", GEANY_KEY_GROUP_EDITOR, GEANY_KEYS_EDITOR_COMPLETESNIPPET},
	{"EDITOR_CONTEXTACTION", GEANY_KEY_GROUP_EDITOR, GEANY_KEYS_EDITOR_CONTEXTACTION},
	{"EDITOR_DELETELINE", GEANY_KEY_GROUP_EDITOR, GEANY_KEYS_EDITOR_DELETELINE},
	{"EDITOR_DELETELINETOBEGINNING", GEANY_KEY_GROUP_EDITOR, GEANY_KEYS_EDITOR_DELETELINETOBEGINNING},
	{"EDITOR_DELETELINETOEND", GEANY_KEY_GROUP_EDITOR, GEANY_KEYS_EDITOR_DELETELINETOEND},
	{"EDITOR_DUPLICATELINE", GEANY_KEY_GROUP_EDITOR, GEANY_KEYS_EDITOR_DUPLICATELINE},
	{"EDITOR_MACROLIST", GEANY_KEY_GROUP_EDITOR, GEANY_KEYS_EDITOR_MACROLIST},
	{"EDITOR_MOVELINEDOWN", GEANY_KEY_GROUP_EDITOR, GEANY_KEYS_EDITOR_MOVELINEDOWN},
	{"EDITOR_MOVELINEUP", GEANY_KEY_GROUP_EDITOR, GEANY_KEYS_EDITOR_MOVELINEUP},
	{"EDITOR_REDO", GEANY_KEY_GROUP_EDITOR, GEANY_KEYS_EDITOR_REDO},
	{"EDITOR_SCROLLLINEDOWN", GEANY_KEY_GROUP_EDITOR, GEANY_KEYS_EDITOR_SCROLLLINEDOWN},
	{"EDITOR_SCROLLLINEUP", GEANY_KEY_GROUP_EDITOR, GEANY_KEYS_EDITOR_SCROLLLINEUP},
	{"EDITOR_SCROLLTOLINE", GEANY_KEY_GROUP_EDITOR, GEANY_KEYS_EDITOR_SCROLLTOLINE},
	{"EDITOR_SNIPPETNEXTCURSOR", GEANY_KEY_GROUP_EDITOR, GEANY_KEYS_EDITOR_SNIPPETNEXTCURSOR},
	{"EDITOR_SUPPRESSSNIPPETCOMPLETION", GEANY_KEY_GROUP_EDITOR, GEANY_KEYS_EDITOR_SUPPRESSSNIPPETCOMPLETION},
	{"EDITOR_TRANSPOSELINE", GEANY_KEY_GROUP_EDITOR, GEANY_KEYS_EDITOR_TRANSPOSELINE},
	{"EDITOR_UNDO", GEANY_KEY_GROUP_EDITOR, GEANY_KEYS_EDITOR_UNDO},
	{"EDITOR_WORDPARTCOMPLETION", GEANY_KEY_GROUP_EDITOR, GEANY_KEYS_EDITOR_WORDPARTCOMPLETION},
	{"FILE_CLOSE", GEANY_KEY_GROUP_FILE, GEANY_KEYS_FILE_CLOSE},
	{"FILE_CLOSEALL", GEANY_KEY_GROUP_FILE, GEANY_KEYS_FILE_CLOSEALL},
	{"FILE_NEW", GEANY_KEY_GROUP_FILE, GEANY_KEYS_FILE_NEW},
	{"FILE_OPEN", GEANY_KEY_GROUP_FILE, GEANY_KEYS_FILE_OPEN},
	{"FILE_OPENLASTTAB", GEANY_KEY_GROUP_FILE, GEANY_KEYS_FILE_OPENLASTTAB},
	{"FILE_OPENSELECTED", GEANY_KEY_GROUP_FILE, GEANY_KEYS_FILE_OPENSELECTED},
	{"FILE_PRINT", GEANY_KEY_GROUP_FILE, GEANY_KEYS_FILE_PRINT},
	{"FILE_PROPERTIES", GEANY_KEY_GROUP_FILE, GEANY_KEYS_FILE_PROPERTIES},
	{"FILE_QUIT", GEANY_KEY_GROUP_FILE, GEANY_KEYS_FILE_QUIT},
	{"FILE_RELOAD", GEANY_KEY_GROUP_FILE, GEANY_KEYS_FILE_RELOAD},
	{"FILE_SAVE", GEANY_KEY_GROUP_FILE, GEANY_KEYS_FILE_SAVE},
	{"FILE_SAVEALL", GEANY_KEY_GROUP_FILE, GEANY_KEYS_FILE_SAVEALL},
	{"FILE_SAVEAS", GEANY_KEY_GROUP_FILE, GEANY_KEYS_FILE_SAVEAS},
	{"FOCUS_COMPILER", GEANY_KEY_GROUP_FOCUS, GEANY_KEYS_FOCUS_COMPILER},
	{"FOCUS_EDITOR", GEANY_KEY_GROUP_FOCUS, GEANY_KEYS_FOCUS_EDITOR},
	{"FOCUS_MESSAGES", GEANY_KEY_GROUP_FOCUS, GEANY_KEYS_FOCUS_MESSAGES},
	{"FOCUS_MESSAGE_WINDOW", GEANY_KEY_GROUP_FOCUS, GEANY_KEYS_FOCUS_MESSAGE_WINDOW},
	{"FOCUS_SCRIBBLE", GEANY_KEY_GROUP_FOCUS, GEANY_KEYS_FOCUS_SCRIBBLE},
	{"FOCUS_SEARCHBAR", GEANY_KEY_GROUP_FOCUS, GEANY_KEYS_FOCUS_SEARCHBAR},
	{"FOCUS_SIDEBAR", GEANY_KEY_GROUP_FOCUS, GEANY_KEYS_FOCUS_SIDEBAR},
	{"FOCUS_SIDEBAR_DOCUMENT_LIST", GEANY_KEY_GROUP_FOCUS, GEANY_KEYS_FOCUS_SIDEBAR_DOCUMENT_LIST},
	{"FOCUS_SIDEBAR_SYMBOL_LIST", GEANY_KEY_GROUP_FOCUS, GEANY_KEYS_FOCUS_SIDEBAR_SYMBOL_LIST},
	{"FOCUS_VTE", GEANY_KEY_GROUP_FOCUS, GEANY_KEYS_FOCUS_VTE},
	{"FORMAT_AUTOINDENT", GEANY_KEY_GROUP_FORMAT, GEANY_KEYS_FORMAT_AUTOINDENT},
	{"FORMAT_COMMENTLINE", GEANY_KEY_GROUP_FORMAT, GEANY_KEYS_FORMAT_COMMENTLINE},
	{"FORMAT_COMMENTLINETOGGLE", GEANY_KEY_GROUP_FORMAT, GEANY_KEYS_FORMAT_COMMENTLINETOGGLE},
	{"FORMAT_DECREASEINDENT", GEANY_KEY_GROUP_FORMAT, GEANY_KEYS_FORMAT_DECREASEINDENT},
	{"FORMAT_DECREASEINDENTBYSPACE", GEANY_KEY_GROUP_FORMAT, GEANY_KEYS_FORMAT_DECREASEINDENTBYSPACE},
	{"FORMAT_INCREASEINDENT", GEANY_KEY_GROUP_FORMAT, GEANY_KEYS_FORMAT_INCREASEINDENT},
	{"FORMAT_INCREASEINDENTBYSPACE", GEANY_KEY_GROUP_FORMAT, GEANY_KEYS_FORMAT_INCREASEINDENTBYSPACE},
	{"FORMAT_JOINLINES", GEANY_KEY_GROUP_FORMAT, GEANY_KEYS_FORMAT_JOINLINES},
	{"FORMAT_REFLOWPARAGRAPH", GEANY_KEY_GROUP_FORMAT, GEANY_KEYS_FORMAT_REFLOWPARAGRAPH},
	{"FORMAT_SENDTOCMD1", GEANY_KEY_GROUP_FORMAT, GEANY_KEYS_FORMAT_SENDTOCMD1},
	{"FORMAT_SENDTOCMD2", GEANY_KEY_GROUP_FORMAT, GEANY_KEYS_FORMAT_SENDTOCMD2},
	{"FORMAT_SENDTOCMD3", GEANY_KEY_GROUP_FORMAT, GEANY_KEYS_FORMAT_SENDTOCMD3},
	{"FORMAT_SENDTOCMD4", GEANY_KEY_GROUP_FORMAT, GEANY_KEYS_FORMAT_SENDTOCMD4},
	{"FORMAT_SENDTOCMD5", GEANY_KEY_GROUP_FORMAT, GEANY_KEYS_FORMAT_SENDTOCMD5},
	{"FORMAT_SENDTOCMD6", GEANY_KEY_GROUP_FORMAT, GEANY_KEYS_FORMAT_SENDTOCMD6},
	{"FORMAT_SENDTOCMD7", GEANY_KEY_GROUP_FORMAT, GEANY_KEYS_FORMAT_SENDTOCMD7},
	{"FORMAT_SENDTOCMD8", GEANY_KEY_GROUP_FORMAT, GEANY_KEYS_FORMAT_SENDTOCMD8},
	{"FORMAT_SENDTOCMD9", GEANY_KEY_GROUP_FORMAT, GEANY_KEYS_FORMAT_SENDTOCMD9},
	{"FORMAT_SENDTOVTE", GEANY_KEY_GROUP_FORMAT, GEANY_KEYS_FORMAT_SENDTOVTE},
	{"FORMAT_TOGGLECASE", GEANY_KEY_GROUP_FORMAT, GEANY_KEYS_FORMAT_TOGGLECASE},
	{"FORMAT_UNCOMMENTLINE", GEANY_KEY_GROUP_FORMAT, GEANY_KEYS_FORMAT_UNCOMMENTLINE},
	{"GOTO_BACK", GEANY_KEY_GROUP_GOTO, GEANY_KEYS_GOTO_BACK},
	{"GOTO_FORWARD", GEANY_KEY_GROUP_GOTO, GEANY_KEYS_GOTO_FORWARD},
	{"GOTO_LINE", GEANY_KEY_GROUP_GOTO, GEANY_KEYS_GOTO_LINE},
	{"GOTO_LINEEND", GEANY_KEY_GROUP_GOTO, GEANY_KEYS_GOTO_LINEEND},
	{"GOTO_LINEENDVISUAL", GEANY_KEY_GROUP_GOTO, GEANY_KEYS_GOTO_LINEENDVISUAL},
	{"GOTO_LINESTART", GEANY_KEY_GROUP_GOTO, GEANY_KEYS_GOTO_LINESTART},
	{"GOTO_LINESTARTVISUAL", GEANY_KEY_GROUP_GOTO, GEANY_KEYS_GOTO_LINESTARTVISUAL},
	{"GOTO_MATCHINGBRACE", GEANY_KEY_GROUP_GOTO, GEANY_KEYS_GOTO_MATCHINGBRACE},
	{"GOTO_NEXTMARKER", GEANY_KEY_GROUP_GOTO, GEANY_KEYS_GOTO_NEXTMARKER},
	{"GOTO_NEXTWORDPART", GEANY_KEY_GROUP_GOTO, GEANY_KEYS_GOTO_NEXTWORDPART},
	{"GOTO_PREVIOUSMARKER", GEANY_KEY_GROUP_GOTO, GEANY_KEYS_GOTO_PREVIOUSMARKER},
	{"GOTO_PREVWORDPART", GEANY_KEY_GROUP_GOTO, GEANY_KEYS_GOTO_PREVWORDPART},
	{"GOTO_TAGDECLARATION", GEANY_KEY_GROUP_GOTO, GEANY_KEYS_GOTO_TAGDECLARATION},
	{"GOTO_TAGDEFINITION", GEANY_KEY_GROUP_GOTO, GEANY_KEYS_GOTO_TAGDEFINITION},
	{"GOTO_TOGGLEMARKER", GEANY_KEY_GROUP_GOTO, GEANY_KEYS_GOTO_TOGGLEMARKER},
	{"HELP_HELP", GEANY_KEY_GROUP_HELP, GEANY_KEYS_HELP_HELP},
	{"INSERT_ALTWHITESPACE", GEANY_KEY_GROUP_INSERT, GEANY_KEYS_INSERT_ALTWHITESPACE},
	{"INSERT_DATE", GEANY_KEY_GROUP_INSERT, GEANY_KEYS_INSERT_DATE},
	{"INSERT_LINEAFTER", GEANY_KEY_GROUP_INSERT, GEANY_KEYS_INSERT_LINEAFTER},
	{"INSERT_LINEBEFORE", GEANY_KEY_GROUP_INSERT, GEANY_KEYS_INSERT_LINEBEFORE},
	{"NOTEBOOK_MOVETABFIRST", GEANY_KEY_GROUP_NOTEBOOK, GEANY_KEYS_NOTEBOOK_MOVETABFIRST},
	{"NOTEBOOK_MOVETABLAST", GEANY_KEY_GROUP_NOTEBOOK, GEANY_KEYS_NOTEBOOK_MOVETABLAST},
	{"NOTEBOOK_MOVETABLEFT", GEANY_KEY_GROUP_NOTEBOOK, GEANY_KEYS_NOTEBOOK_MOVETABLEFT},
	{"NOTEBOOK_MOVETABRIGHT", GEANY_KEY_GROUP_NOTEBOOK, GEANY_KEYS_NOTEBOOK_MOVETABRIGHT},
	{"NOTEBOOK_SWITCHTABLASTUSED", GEANY_KEY_GROUP_NOTEBOOK, GEANY_KEYS_NOTEBOOK_SWITCHTABLASTUSED},
	{"NOTEBOOK_SWITCHTABLEFT", GEANY_KEY_GROUP_NOTEBOOK, GEANY_KEYS_NOTEBOOK_SWITCHTABLEFT},
	{"NOTEBOOK_SWITCHTABRIGHT", GEANY_KEY_GROUP_NOTEBOOK, GEANY_KEYS_NOTEBOOK_SWITCHTABRIGHT},
	{"PROJECT_CLOSE", GEANY_KEY_GROUP_PROJECT, GEANY_KEYS_PROJECT_CLOSE},
	{"PROJECT_NEW", GEANY_KEY_GROUP_PROJECT, GEANY_KEYS_PROJECT_NEW},
	{"PROJECT_OPEN", GEANY_KEY_GROUP_PROJECT, GEANY_KEYS_PROJECT_OPEN},
	{"PROJECT_PROPERTIES", GEANY_KEY_GROUP_PROJECT, GEANY_KEYS_PROJECT_PROPERTIES},
	{"SEARCH_FIND", GEANY_KEY_GROUP_SEARCH, GEANY_KEYS_SEARCH_FIND},
	{"SEARCH_FINDDOCUMENTUSAGE", GEANY_KEY_GROUP_SEARCH, GEANY_KEYS_SEARCH_FINDDOCUMENTUSAGE},
	{"SEARCH_FINDINFILES", GEANY_KEY_GROUP_SEARCH, GEANY_KEYS_SEARCH_FINDINFILES},
	{"SEARCH_FINDNEXT", GEANY_KEY_GROUP_SEARCH, GEANY_KEYS_SEARCH_FINDNEXT},
	{"SEARCH_FINDNEXTSEL", GEANY_KEY_GROUP_SEARCH, GEANY_KEYS_SEARCH_FINDNEXTSEL},
	{"SEARCH_FINDPREVIOUS", GEANY_KEY_GROUP_SEARCH, GEANY_KEYS_SEARCH_FINDPREVIOUS},
	{"SEARCH_FINDPREVSEL", GEANY_KEY_GROUP_SEARCH, GEANY_KEYS_SEARCH_FINDPREVSEL},
	{"SEARCH_FINDUSAGE", GEANY_KEY_GROUP_SEARCH, GEANY_KEYS_SEARCH_FINDUSAGE},
	{"SEARCH_MARKALL", GEANY_KEY_GROUP_SEARCH, GEANY_KEYS_SEARCH_MARKALL},
	{"SEARCH_NEXTMESSAGE", GEANY_KEY_GROUP_SEARCH, GEANY_KEYS_SEARCH_NEXTMESSAGE},
	{"SEARCH_PREVIOUSMESSAGE", GEANY_KEY_GROUP_SEARCH, GEANY_KEYS_SEARCH_PREVIOUSMESSAGE},
	{"SEARCH_REPLACE", GEANY_KEY_GROUP_SEARCH, GEANY_KEYS_SEARCH_REPLACE},
	{"SELECT_ALL", GEANY_KEY_GROUP_SELECT, GEANY_KEYS_SELECT_ALL},
	{"SELECT_LINE", GEANY_KEY_GROUP_SELECT, GEANY_KEYS_SELECT_LINE},
	{"SELECT_PARAGRAPH", GEANY_KEY_GROUP_SELECT, GEANY_KEYS_SELECT_PARAGRAPH},
	{"SELECT_WORD", GEANY_KEY_GROUP_SELECT, GEANY_KEYS_SELECT_WORD},
	{"SELECT_WORDPARTLEFT", GEANY_KEY_GROUP_SELECT, GEANY_KEYS_SELECT_WORDPARTLEFT},
	{"SELECT_WORDPARTRIGHT", GEANY_KEY_GROUP_SELECT, GEANY_KEYS_SELECT_WORDPARTRIGHT},
	{"SETTINGS_PLUGINPREFERENCES", GEANY_KEY_GROUP_SETTINGS, GEANY_KEYS_SETTINGS_PLUGINPREFERENCES},
	{"SETTINGS_PREFERENCES", GEANY_KEY_GROUP_SETTINGS, GEANY_KEYS_SETTINGS_PREFERENCES},
	{"TOOLS_OPENCOLORCHOOSER", GEANY_KEY_GROUP_TOOLS, GEANY_KEYS_TOOLS_OPENCOLORCHOOSER},
	{"VIEW_FULLSCREEN", GEANY_KEY_GROUP_VIEW, GEANY_KEYS_VIEW_FULLSCREEN},
	{"VIEW_MESSAGEWINDOW", GEANY_KEY_GROUP_VIEW, GEANY_KEYS_VIEW_MESSAGEWINDOW},
	{"VIEW_SIDEBAR", GEANY_KEY_GROUP_VIEW, GEANY_KEYS_VIEW_SIDEBAR},
	{"VIEW_TOGGLEALL", GEANY_KEY_GROUP_VIEW, GEANY_KEYS_VIEW_TOGGLEALL},
	{"VIEW_ZOOMIN", GEANY_KEY_GROUP_VIEW, GEANY_KEYS_VIEW_ZOOMIN},
	{"VIEW_ZOOMOUT", GEANY_KEY_GROUP_VIEW, GEANY_KEYS_VIEW_ZOOMOUT},
	{"VIEW_ZOOMRESET", GEANY_KEY_GROUP_VIEW, GEANY_KEYS_VIEW_ZOOMRESET},
	{NULL, 0, 0}
};
