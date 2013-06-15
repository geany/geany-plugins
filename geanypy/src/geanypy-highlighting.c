#include "geanypy.h"


typedef struct
{
	PyObject_HEAD
	const GeanyLexerStyle *lexer_style;
} LexerStyle;


static void
LexerStyle_dealloc(LexerStyle *self)
{
	self->ob_type->tp_free((PyObject *) self);
}


static int
LexerStyle_init(LexerStyle *self, PyObject *args, PyObject *kwds)
{
	self->lexer_style = NULL;
	return 0;
}


static PyObject *
LexerStyle_get_property(LexerStyle *self, const gchar *prop_name)
{
	g_return_val_if_fail(self != NULL, NULL);
	g_return_val_if_fail(prop_name != NULL, NULL);

	if (!self->lexer_style)
	{
		PyErr_SetString(PyExc_RuntimeError,
			"LexerStyle instance not initialized properly");
		return NULL;
	}

	if (g_str_equal(prop_name, "background"))
	{
		guint16 red, green, blue;
		red = self->lexer_style->background & 255;
		green = (self->lexer_style->background >> 8) & 255;
		blue = (self->lexer_style->background >> 16) & 255;
		return Py_BuildValue("iii", red, green, blue);
	}
	else if (g_str_equal(prop_name, "foreground"))
	{
		guint16 red, green, blue;
		red = self->lexer_style->foreground & 255;
		green = (self->lexer_style->foreground >> 8) & 255;
		blue = (self->lexer_style->foreground >> 16) & 255;
		return Py_BuildValue("iii", red, green, blue);
	}
	else if (g_str_equal(prop_name, "bold"))
	{
		if (self->lexer_style->bold)
			Py_RETURN_TRUE;
		else
			Py_RETURN_FALSE;
	}
	else if (g_str_equal(prop_name, "italic"))
	{
		if (self->lexer_style->italic)
			Py_RETURN_TRUE;
		else
			Py_RETURN_FALSE;
	}

	Py_RETURN_NONE;
}
GEANYPY_PROPS_READONLY(LexerStyle);


static PyGetSetDef LexerStyle_getseters[] = {
	GEANYPY_GETSETDEF(LexerStyle, "background",
		"Background color of text, as an (R,G,B) tuple."),
	GEANYPY_GETSETDEF(LexerStyle, "foreground",
		"Foreground color of text, as an (R,G,B) tuple."),
	GEANYPY_GETSETDEF(LexerStyle, "bold",
		"Whether the text is bold or not."),
	GEANYPY_GETSETDEF(LexerStyle, "italic",
		"Whether the text is italic or not."),
	{ NULL }
};


static PyTypeObject LexerStyleType = {
	PyObject_HEAD_INIT(NULL)
	0,												/*ob_size*/
	"geany.highlighting.LexerStyle",				/*tp_name*/
	sizeof(Editor),									/*tp_basicsize*/
	0,												/*tp_itemsize*/
	(destructor) LexerStyle_dealloc,				/*tp_dealloc*/
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* tp_print - tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,		/*tp_flags*/
	"Wrapper around a GeanyLexerStyle structure.",	/* tp_doc */
	0, 0, 0, 0, 0, 0, 0, 0,							/* tp_traverse - tp_members */
	LexerStyle_getseters,							/* tp_getset */
	0, 0, 0, 0, 0,									/* tp_base - tp_dictoffset */
	(initproc) LexerStyle_init,						/* tp_init */
	0,  0,											/* tp_alloc - tp_new */
};


static PyObject *
Highlighting_get_style(PyObject *module, PyObject *args, PyObject *kwargs)
{
	gint ft_id, style_id;
	LexerStyle *lexer_style;
	const GeanyLexerStyle *ls;
	static gchar *kwlist[] = { "filetype_id", "style_id", NULL };

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "ii", kwlist, &ft_id, &style_id))
	{
		ls = highlighting_get_style(ft_id, style_id);
		if (ls != NULL)
		{
			lexer_style = (LexerStyle *) PyObject_CallObject((PyObject *) &LexerStyleType, NULL);
			lexer_style->lexer_style = ls;
			return (PyObject *) lexer_style;
		}
	}

	Py_RETURN_NONE;
}


static PyObject *
Highlighting_is_code_style(PyObject *module, PyObject *args, PyObject *kwargs)
{
	gint lexer, style;
	static gchar *kwlist[] = { "lexer", "style", NULL };

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "ii", kwlist, &lexer, &style))
	{
		if (highlighting_is_code_style(lexer, style))
			Py_RETURN_TRUE;
		else
			Py_RETURN_FALSE;
	}

	Py_RETURN_NONE;
}


static PyObject *
Highlighting_is_comment_style(PyObject *module, PyObject *args, PyObject *kwargs)
{
	gint lexer, style;
	static gchar *kwlist[] = { "lexer", "style", NULL };

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "ii", kwlist, &lexer, &style))
	{
		if (highlighting_is_comment_style(lexer, style))
			Py_RETURN_TRUE;
		else
			Py_RETURN_FALSE;
	}

	Py_RETURN_NONE;
}


static PyObject *
Highlighting_is_string_style(PyObject *module, PyObject *args, PyObject *kwargs)
{
	gint lexer, style;
	static gchar *kwlist[] = { "lexer", "style", NULL };

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "ii", kwlist, &lexer, &style))
	{
		if (highlighting_is_string_style(lexer, style))
			Py_RETURN_TRUE;
		else
			Py_RETURN_FALSE;
	}

	Py_RETURN_NONE;
}


static PyObject *
Highlighting_set_styles(PyObject *module, PyObject *args, PyObject *kwargs)
{
	PyObject *py_sci, *py_ft;
	Scintilla *sci;
	Filetype *ft;
	static gchar *kwlist[] = { "sci", "filetype", NULL };

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "OO", kwlist, &py_sci, &py_ft))
	{
		if (py_sci != Py_None && py_ft != Py_None)
		{
			sci = (Scintilla *) py_sci;
			ft = (Filetype *) py_ft;
			highlighting_set_styles(sci->sci, ft->ft);
		}
	}

	Py_RETURN_NONE;
}


static
PyMethodDef EditorModule_methods[] = {
	{ "get_style", (PyCFunction) Highlighting_get_style, METH_KEYWORDS },
	{ "is_code_style", (PyCFunction) Highlighting_is_code_style, METH_KEYWORDS },
	{ "is_comment_style", (PyCFunction) Highlighting_is_comment_style, METH_KEYWORDS },
	{ "is_string_style", (PyCFunction) Highlighting_is_string_style, METH_KEYWORDS },
	{ "set_styles", (PyCFunction) Highlighting_set_styles, METH_KEYWORDS },
	{ NULL }
};


PyMODINIT_FUNC
inithighlighting(void)
{
	PyObject *m;

	LexerStyleType.tp_new = PyType_GenericNew;
	if (PyType_Ready(&LexerStyleType) < 0)
		return;

	m = Py_InitModule3("highlighting", EditorModule_methods,
			"Highlighting information and management.");

	Py_INCREF(&LexerStyleType);
	PyModule_AddObject(m, "LexerStyle", (PyObject *)&LexerStyleType);
}
