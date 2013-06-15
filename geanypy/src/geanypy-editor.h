#ifndef GEANYPY_EDITOR_H__
#define GEANYPY_EDITOR_H__

PyTypeObject IndentPrefsType;

typedef struct
{
	PyObject_HEAD
	GeanyEditor *editor;
} Editor;

typedef struct
{
	PyObject_HEAD
	GeanyIndentPrefs *indent_prefs;
} IndentPrefs;

Editor *Editor_create_new_from_geany_editor(GeanyEditor *editor);
IndentPrefs *IndentPrefs_create_new_from_geany_indent_prefs(GeanyIndentPrefs *indent_prefs);

#endif /* GEANYPY_EDITOR_H__ */
