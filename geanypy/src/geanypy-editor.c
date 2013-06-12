#include "geanypy.h"


static void
Editor_dealloc(Editor *self)
{
	self->ob_type->tp_free((PyObject *) self);
}


static int
Editor_init(Editor *self)
{
	self->editor = NULL;
	return 0;
}


static PyObject *
Editor_get_property(Editor *self, const gchar *prop_name)
{
	g_return_val_if_fail(self != NULL, NULL);
	g_return_val_if_fail(prop_name != NULL, NULL);

	if (!self->editor)
	{
		PyErr_SetString(PyExc_RuntimeError,
			"Editor instance not initialized properly");
		return NULL;
	}

	if (g_str_equal(prop_name, "eol_char"))
		return PyString_FromString(editor_get_eol_char(self->editor));
	else if (g_str_equal(prop_name, "eol_char_name"))
		return PyString_FromString(editor_get_eol_char_name(self->editor));
	else if (g_str_equal(prop_name, "indent_prefs"))
	{
		const GeanyIndentPrefs *indent_prefs;
		IndentPrefs *py_prefs;
		indent_prefs = editor_get_indent_prefs(self->editor);
		if (indent_prefs)
		{
			py_prefs = IndentPrefs_create_new_from_geany_indent_prefs(
							(GeanyIndentPrefs *) indent_prefs);
			return (PyObject *) py_prefs;
		}
		Py_RETURN_NONE;
	}
	else if (g_str_equal(prop_name, "auto_indent"))
	{
		if (self->editor->auto_indent)
			Py_RETURN_TRUE;
		else
			Py_RETURN_FALSE;
	}
	else if (g_str_equal(prop_name, "document"))
	{
		PyObject *py_doc;
		py_doc = (PyObject *) Document_create_new_from_geany_document(
									self->editor->document);
		if (py_doc && py_doc != Py_None)
			Py_RETURN_NONE;
		return py_doc;
	}
	else if (g_str_equal(prop_name, "line_breaking"))
	{
		if (self->editor->line_breaking)
			Py_RETURN_TRUE;
		else
			Py_RETURN_FALSE;
	}
	else if (g_str_equal(prop_name, "line_wrapping"))
	{
		if (self->editor->line_wrapping)
			Py_RETURN_TRUE;
		else
			Py_RETURN_FALSE;
	}
	else if (g_str_equal(prop_name, "scintilla"))
	{
		Scintilla *sci;
		sci = Scintilla_create_new_from_scintilla(self->editor->sci);
		if (!sci)
			Py_RETURN_NONE;
		return (PyObject *) sci;
	}
	else if (g_str_equal(prop_name, "scroll_percent"))
		return PyFloat_FromDouble((gdouble) self->editor->scroll_percent);

	PyErr_SetString(PyExc_AttributeError, "can't get property");
	Py_RETURN_NONE;
}


static int
Editor_set_property(Editor *self, PyObject *value, const gchar *prop_name)
{
	g_return_val_if_fail(self != NULL, -1);
	g_return_val_if_fail(value != NULL, -1);
	g_return_val_if_fail(prop_name != NULL, -1);

	if (!self->editor)
	{
		PyErr_SetString(PyExc_RuntimeError,
			"Editor instance not initialized properly");
		return -1;
	}

	if (g_str_equal(prop_name, "indent_type"))
	{
		long indent_type = PyInt_AsLong(value);
		if (indent_type == -1 && PyErr_Occurred())
		{
			PyErr_Print();
			return -1;
		}
		editor_set_indent_type(self->editor, (GeanyIndentType) indent_type);
		return 0;
	}

	PyErr_SetString(PyExc_AttributeError, "can't set property");
	return -1;
}


static PyObject *
Editor_create_widget(Editor *self)
{
	ScintillaObject *sci;
	PyObject *pysci;

	if (self->editor == NULL)
		Py_RETURN_NONE;

	sci = editor_create_widget(self->editor);
	if (!sci)
		Py_RETURN_NONE;

	pysci = pygobject_new(G_OBJECT(sci));
	if (!pysci)
	{
		gtk_widget_destroy(GTK_WIDGET(sci));
		Py_RETURN_NONE;
	}

	return pysci;
}


static PyObject *
Editor_find_snippet(Editor *self, PyObject *args, PyObject *kwargs)
{
	gchar *name;
	const gchar *snippet;
	static gchar *kwlist[] = { "snippet_name", NULL };

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "s", kwlist, &name))
	{
		if (name != NULL)
		{
			snippet = editor_find_snippet(self->editor, name);
			if (snippet != NULL)
				return Py_BuildValue("s", snippet);
		}
	}
	Py_RETURN_NONE;
}


static PyObject *
Editor_get_word_at_position(Editor *self, PyObject *args, PyObject *kwargs)
{
	gint pos = -1;
	gchar *wordchars = NULL, *word = NULL;
	PyObject *py_word;
	static gchar *kwlist[] = { "pos", "wordchars", NULL };

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "|iz", kwlist, &pos, &wordchars))
	{
		word = editor_get_word_at_pos(self->editor, pos, wordchars);
		if (word != NULL)
		{
			py_word = Py_BuildValue("s", word);
			g_free(word);
			return py_word;
		}
	}

	Py_RETURN_NONE;
}


static PyObject *
Editor_goto_pos(Editor *self, PyObject *args, PyObject *kwargs)
{
	gint pos, mark = FALSE, result;
	static gchar *kwlist[] = { "pos", "mark", NULL };

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "i|i", kwlist, &pos, &mark))
	{
		result = editor_goto_pos(self->editor, pos, mark);
		if (result)
			Py_RETURN_TRUE;
		else
			Py_RETURN_FALSE;
	}
	Py_RETURN_NONE;
}


static PyObject *
Editor_indicator_clear(Editor *self, PyObject *args, PyObject *kwargs)
{
	gint indic;
	static gchar *kwlist[] = { "indic", NULL };
	if (PyArg_ParseTupleAndKeywords(args, kwargs, "i", kwlist, &indic))
		editor_indicator_clear(self->editor, indic);
	Py_RETURN_NONE;
}


static PyObject *
Editor_indicator_set_on_line(Editor *self, PyObject *args, PyObject *kwargs)
{
	gint indic, line_num;
	static gchar *kwlist[] = { "indic", "line", NULL };
	if (PyArg_ParseTupleAndKeywords(args, kwargs, "ii", kwlist, &indic, &line_num))
		editor_indicator_set_on_line(self->editor, indic, line_num);
	Py_RETURN_NONE;
}


static PyObject *
Editor_indicator_set_on_range(Editor *self, PyObject *args, PyObject *kwargs)
{
	gint indic, start, end;
	static gchar *kwlist[] = { "indic", "start", "end", NULL };
	if (PyArg_ParseTupleAndKeywords(args, kwargs, "iii", kwlist, &indic, &start, &end))
		editor_indicator_set_on_range(self->editor, indic, start, end);
	Py_RETURN_NONE;
}


static PyObject *
Editor_insert_snippet(Editor *self, PyObject *args, PyObject *kwargs)
{
	gint pos = 0;
	gchar *snippet = NULL;
	static gchar *kwlist[] = { "pos", "snippet", NULL };
	if (PyArg_ParseTupleAndKeywords(args, kwargs, "is", kwlist, &pos, &snippet))
		editor_insert_snippet(self->editor, pos, snippet);
	Py_RETURN_NONE;
}


static PyObject *
Editor_insert_text_block(Editor *self, PyObject *args, PyObject *kwargs)
{
	gchar *text = NULL;
	gint insert_pos, cursor_index = -1, newline_indent_size = -1;
	gint replace_newlines = 0;
	static gchar *kwlist[] = { "text", "insert_pos", "cursor_index",
		"newline_indent_size", "replace_newlines", NULL };

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "si|iii", kwlist, &text,
		&insert_pos, &cursor_index, &newline_indent_size, &replace_newlines))
	{
		editor_insert_text_block(self->editor, text, insert_pos, cursor_index,
			newline_indent_size, (gboolean) replace_newlines);
	}

	Py_RETURN_NONE;
}


static PyMethodDef Editor_methods[] = {
	{ "create_widget", (PyCFunction) Editor_create_widget, METH_NOARGS,
		"Creates a new Scintilla widget based on the settings for "
		"editor." },
	{ "find_snippet", (PyCFunction) Editor_find_snippet, METH_KEYWORDS,
		"Get snippet by name." },
	{ "get_word_at_position", (PyCFunction) Editor_get_word_at_position, METH_KEYWORDS,
		"Finds the word at the position specified." },
	{ "goto_pos", (PyCFunction) Editor_goto_pos, METH_KEYWORDS,
		"Moves to position, switching to the current document if "
		"necessary, setting a marker if mark is set." },
	{ "indicator_clear", (PyCFunction) Editor_indicator_clear, METH_KEYWORDS,
		"Deletes all currently set indicators matching the specified "
		"indicator in the editor." },
	{ "indicator_set_on_line", (PyCFunction) Editor_indicator_set_on_line, METH_KEYWORDS,
		"Sets an indicator on the specified line." },
	{ "indicator_set_on_range", (PyCFunction) Editor_indicator_set_on_range, METH_KEYWORDS,
		"Sets an indicator on the range specified." },
	{ "insert_snippet", (PyCFunction) Editor_insert_snippet, METH_KEYWORDS,
		"Replces all special sequences in snippet and inserts it at "
		"the specified position." },
	{ "insert_text_block", (PyCFunction) Editor_insert_text_block, METH_KEYWORDS,
		"Inserts text, replacing tab chars and newline chars accordingly "
		"for the document." },
	{ NULL }
};


static PyGetSetDef Editor_getseters[] = {
	GEANYPY_GETSETDEF(Editor, "eol_char",
		"The endo of line character(s)."),
	GEANYPY_GETSETDEF(Editor, "eol_char_name",
		"The localized name (for displaying) of the used end of line "
		"characters in the editor."),
	GEANYPY_GETSETDEF(Editor, "indent_prefs",
		"Editor's indentation preferences."),
	GEANYPY_GETSETDEF(Editor, "indent_type",
		"Sets the indent type for the editor."),
	GEANYPY_GETSETDEF(Editor, "auto_indent",
		"True if auto-indentation is enabled."),
	GEANYPY_GETSETDEF(Editor, "document",
		"The document associated with the editor."),
	GEANYPY_GETSETDEF(Editor, "line_breaking",
		"Whether to split long lines as you type."),
	GEANYPY_GETSETDEF(Editor, "line_wrapping",
		"True if line wrapping is enabled."),
	GEANYPY_GETSETDEF(Editor, "scintilla",
		"The Scintilla widget used by the editor."),
	GEANYPY_GETSETDEF(Editor, "scroll_percent",
		"Percentage to scroll view by on paint, if positive."),
	{ NULL },
};


static PyTypeObject EditorType = {
	PyObject_HEAD_INIT(NULL)
	0,											/* ob_size */
	"geany.editor.Editor",						/* tp_name */
	sizeof(Editor),								/* tp_basicsize */
	0,											/* tp_itemsize */
	(destructor)Editor_dealloc,					/* tp_dealloc */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* tp_print - tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,	/* tp_flags */
	"Wrapper around a GeanyEditor structure.",	/* tp_doc */
	0, 0, 0, 0, 0, 0,							/* tp_traverse - tp_iternext */
	Editor_methods,								/* tp_methods */
	0,											/* tp_members */
	Editor_getseters,							/* tp_getset */
	0, 0, 0, 0, 0,								/* tp_base - tp_dictoffset */
	(initproc)Editor_init,						/* tp_init */
	0, 0,										/* tp_alloc - tp_new */

};


static PyObject *
Editor__find_snippet(PyObject *module, PyObject *args, PyObject *kwargs)
{
	gchar *name;
	const gchar *snippet;
	static gchar *kwlist[] = { "snippet_name", NULL };

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "s", kwlist, &name))
	{
		if (name != NULL)
		{
			snippet = editor_find_snippet(NULL, name);
			if (snippet != NULL)
				return Py_BuildValue("s", snippet);
		}
	}
	Py_RETURN_NONE;
}


static PyObject *
Editor_get_default_eol_char(PyObject *module)
{
	const gchar *eol_char = editor_get_eol_char(NULL);
	if (eol_char != NULL)
		return Py_BuildValue("s", eol_char);
	Py_RETURN_NONE;
}


static PyObject *
Editor_get_default_eol_char_name(PyObject *module)
{
	const gchar *eol_char_name = editor_get_eol_char_name(NULL);
	if (eol_char_name != NULL)
		return Py_BuildValue("s", eol_char_name);
	Py_RETURN_NONE;
}


static PyObject *
Editor_get_default_indent_prefs(PyObject *module)
{
	const GeanyIndentPrefs *indent_prefs;
	IndentPrefs *py_prefs;
	indent_prefs = editor_get_indent_prefs(NULL);
	if (indent_prefs != NULL)
	{
		py_prefs = IndentPrefs_create_new_from_geany_indent_prefs((GeanyIndentPrefs *)indent_prefs);
		return (PyObject *) py_prefs;
	}
	Py_RETURN_NONE;
}


static
PyMethodDef EditorModule_methods[] = {
	{ "find_snippet", (PyCFunction) Editor__find_snippet, METH_VARARGS,
		"Gets a snippet by name from the default set." },
	{ "get_default_eol_char", (PyCFunction) Editor_get_default_eol_char, METH_NOARGS,
		"The default eol char." },
	{ "get_default_eol_char_name", (PyCFunction) Editor_get_default_eol_char_name, METH_NOARGS,
		"Retrieves the localized name (for displaying) of the default end "
		"of line characters." },
	{ "get_default_indent_prefs", (PyCFunction) Editor_get_default_indent_prefs, METH_NOARGS,
		"Gets the default indentation preferences." },
	{ NULL }
};


PyMODINIT_FUNC initeditor(void)
{
	PyObject *m;

	EditorType.tp_new = PyType_GenericNew;
	if (PyType_Ready(&EditorType) < 0)
		return;

	IndentPrefsType.tp_new = PyType_GenericNew;
	if (PyType_Ready(&IndentPrefsType) < 0)
		return;

	m = Py_InitModule3("editor", EditorModule_methods,
			"Editor information and management.");

	Py_INCREF(&EditorType);
	PyModule_AddObject(m, "Editor", (PyObject *)&EditorType);

	Py_INCREF(&IndentPrefsType);
	PyModule_AddObject(m, "IndentPrefs", (PyObject *)&IndentPrefsType);

	PyModule_AddIntConstant(m, "INDENT_TYPE_SPACES",
		(glong) GEANY_INDENT_TYPE_SPACES);
	PyModule_AddIntConstant(m, "INDENT_TYPE_TABS",
		(glong) GEANY_INDENT_TYPE_TABS);
	PyModule_AddIntConstant(m, "INDENT_TYPE_BOTH",
		(glong) GEANY_INDENT_TYPE_BOTH);
	PyModule_AddIntConstant(m, "INDICATOR_ERROR",
		(glong) GEANY_INDICATOR_ERROR);
	PyModule_AddIntConstant(m, "INDICATOR_SEARCH",
		(glong) GEANY_INDICATOR_SEARCH);
	PyModule_AddStringConstant(m, "WORDCHARS", GEANY_WORDCHARS);

	PyModule_AddIntConstant(m, "INDENT_TYPE_SPACES", (glong) GEANY_INDENT_TYPE_SPACES);
	PyModule_AddIntConstant(m, "INDENT_TYPE_TABS", (glong) GEANY_INDENT_TYPE_TABS);
	PyModule_AddIntConstant(m, "INDENT_TYPE_BOTH", (glong) GEANY_INDENT_TYPE_BOTH);
}


Editor *Editor_create_new_from_geany_editor(GeanyEditor *editor)
{
	Editor *self;
	self = (Editor *) PyObject_CallObject((PyObject *) &EditorType, NULL);
	self->editor = editor;
	return self;
}
