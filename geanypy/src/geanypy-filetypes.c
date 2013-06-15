#include "geanypy.h"


static void
Filetype_dealloc(Filetype *self)
{
	self->ob_type->tp_free((PyObject *) self);
}


static int
Filetype_init(Filetype *self, PyObject *args, PyObject *kwds)
{
	self->ft = NULL;
	return 0;
}


static PyObject *
Filetype_get_property(Filetype *self, const gchar *prop_name)
{
	g_return_val_if_fail(self != NULL, NULL);
	g_return_val_if_fail(prop_name != NULL, NULL);

	if (!self->ft)
	{
		PyErr_SetString(PyExc_RuntimeError,
			"Filetype instance not initialized properly");
		return NULL;
	}

	if (g_str_equal(prop_name, "display_name"))
		GEANYPY_RETURN_STRING(filetypes_get_display_name(self->ft))
	else if (g_str_equal(prop_name, "extension"))
		GEANYPY_RETURN_STRING(self->ft->extension)
	else if (g_str_equal(prop_name, "id"))
		return PyInt_FromLong((glong) self->ft->id);
	else if (g_str_equal(prop_name, "lang"))
		return PyInt_FromLong((glong) self->ft->lang);
	else if (g_str_equal(prop_name, "name"))
		GEANYPY_RETURN_STRING(self->ft->name)
	else if (g_str_equal(prop_name, "pattern"))
	{
		gint i, len;
		PyObject *list = PyList_New(0);
		if (self->ft->pattern)
		{
			len = g_strv_length(self->ft->pattern);
			for (i = 0; i < len; i++)
				PyList_Append(list, PyString_FromString(self->ft->pattern[i]));
		}
		return list;

	}
	else if (g_str_equal(prop_name, "title"))
		GEANYPY_RETURN_STRING(self->ft->title)
#ifdef ENABLE_PRIVATE
	else if (g_str_equal(prop_name, "context_action_cmd"))
		GEANYPY_RETURN_STRING(self->ft->context_action_cmd)
	else if (g_str_equal(prop_name, "comment_open"))
		GEANYPY_RETURN_STRING(self->ft->comment_open)
	else if (g_str_equal(prop_name, "comment_use_indent"))
	{
		if (self->ft->comment_use_indent)
			Py_RETURN_TRUE;
		else
			Py_RETURN_FALSE;
	}
	else if (g_str_equal(prop_name, "group"))
		return PyInt_FromLong((glong) self->ft->group);
	else if (g_str_equal(prop_name, "error_regex_string"))
		return PyString_FromString(self->ft->error_regex_string);
	else if (g_str_equal(prop_name, "lexer_filetype"))
	{
		if (self->ft->lexer_filetype)
			return Filetype_create_new_from_geany_filetype(self->ft->lexer_filetype);
	}
	else if (g_str_equal(prop_name, "mime_type"))
		GEANYPY_RETURN_STRING(self->ft->lexer_filetype)
	else if (g_str_equal(prop_name, "icon"))
	{
		if (self->ft->icon)
			return (PyObject *) pygobject_new(G_OBJECT(self->ft->icon));
	}
	else if (g_str_equal(prop_name, "comment_single"))
		GEANYPY_RETURN_STRING(self->ft->comment_single)
	else if (g_str_equal(prop_name, "indent_type"))
		return PyInt_FromLong((glong) self->ft->indent_type);
	else if (g_str_equal(prop_name, "indent_width"))
		return PyInt_FromLong((glong) self->ft->indent_width);
#endif

	Py_RETURN_NONE;
}
GEANYPY_PROPS_READONLY(Filetype);


static PyGetSetDef Filetype_getseters[] = {
	GEANYPY_GETSETDEF(Filetype, "display_name",
		"Gets the filetype's name for display."),
	GEANYPY_GETSETDEF(Filetype, "extension",
		"Default file extension for new files or None."),
	GEANYPY_GETSETDEF(Filetype, "id",
		"Index of the the filetype."),
	GEANYPY_GETSETDEF(Filetype, "lang",
		"TagManager language type, -1 for all, -2 for none."),
	GEANYPY_GETSETDEF(Filetype, "name",
		"Untranslated short name, such as 'C' or 'None'."),
	GEANYPY_GETSETDEF(Filetype, "pattern",
		"List of filename-matching wildcard strings."),
	GEANYPY_GETSETDEF(Filetype, "title",
		"Shown in the open dialog, such as 'C source file'."),
#ifdef ENABLE_PRIVATE
	GEANYPY_GETSETDEF(Filetype, "context_action_cmd", ""),
	GEANYPY_GETSETDEF(Filetype, "comment_open", ""),
	GEANYPY_GETSETDEF(Filetype, "comment_use_indent", ""),
	GEANYPY_GETSETDEF(Filetype, "group", ""),
	GEANYPY_GETSETDEF(Filetype, "error_regex_string", ""),
	GEANYPY_GETSETDEF(Filetype, "lexer_filetype", ""),
	GEANYPY_GETSETDEF(Filetype, "mime_type", ""),
	GEANYPY_GETSETDEF(Filetype, "icon", ""),
	GEANYPY_GETSETDEF(Filetype, "comment_single", ""),
	GEANYPY_GETSETDEF(Filetype, "indent_type", ""),
	GEANYPY_GETSETDEF(Filetype, "indent_width", ""),
#endif
	{ NULL }
};


static PyTypeObject FiletypeType = {
	PyObject_HEAD_INIT(NULL)
	0,											/* ob_size */
	"geany.filetypes.Filetype",					/* tp_name */
	sizeof(Filetype),							/* tp_basicsize */
	0,											/* tp_itemsize */
	(destructor) Filetype_dealloc,				/* tp_dealloc */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* tp_print - tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,	/* tp_flags */
	"Wrapper around a GeanyFiletype structure.",/* tp_doc */
	0, 0, 0, 0, 0, 0, 0, 0,						/* tp_traverse - tp_members */
	Filetype_getseters,							/* tp_getset */
	0, 0, 0, 0, 0,								/* tp_base - tp_dictoffset */
	(initproc) Filetype_init,					/* tp_init */
	0, 0,										/* tp_alloc - tp_new */

};


static PyObject *
Filetype_detect_from_file(PyObject *self, PyObject *args, PyObject *kwargs)
{
	GeanyFiletype *ft;
	gchar *filename = NULL;
	static gchar *kwlist[] = { "filename", NULL };

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "s", kwlist, &filename))
	{
		if (filename)
		{
			if ((ft = filetypes_detect_from_file(filename)))
				return (PyObject *) Filetype_create_new_from_geany_filetype(ft);
		}
	}

	Py_RETURN_NONE;
}


static PyObject *
Filetype_get_sorted_by_name(PyObject *self)
{
	const GSList *glist, *iter;
	PyObject *list;

	glist = filetypes_get_sorted_by_name();
	list = PyList_New(0);

	for (iter = glist; iter != NULL; iter = g_slist_next(iter))
	{
		if (!iter->data)
			continue;
		PyList_Append(list, (PyObject *)
			Filetype_create_new_from_geany_filetype((GeanyFiletype *) iter->data));
	}

	return list;
}


static PyObject *
Filetype_index(PyObject *self, PyObject *args, PyObject *kwargs)
{
	GeanyFiletype *ft;
	gint idx = -1;
	static gchar *kwlist[] = { "index", NULL };

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "i", kwlist, &idx))
	{
		if ((ft = filetypes_index(idx)))
			return (PyObject *) Filetype_create_new_from_geany_filetype(ft);
	}

	Py_RETURN_NONE;
}


static PyObject *
Filetype_lookup_by_name(PyObject *self, PyObject *args, PyObject *kwargs)
{
	GeanyFiletype *ft;
	gchar *filetype = NULL;
	static gchar *kwlist[] = { "name", NULL };

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "s", kwlist, &filetype))
	{
		if (filetype && (ft = filetypes_lookup_by_name(filetype)))
			return (PyObject *) Filetype_create_new_from_geany_filetype(ft);
	}

	Py_RETURN_NONE;
}


static PyObject *
Filetype_get_sorted_by_title(PyObject *self, PyObject *args)
{
	const GSList *iter;
	PyObject *list;

	list = PyList_New(0);

	for (iter = geany_data->filetypes_by_title; iter != NULL; iter = g_slist_next(iter))
	{
		if (!iter->data)
			continue;
		PyList_Append(list, (PyObject *)
			Filetype_create_new_from_geany_filetype((GeanyFiletype *) iter->data));
	}

	return list;
}


static
PyMethodDef FiletypeModule_methods[] = {
	{ "detect_from_file", (PyCFunction) Filetype_detect_from_file, METH_KEYWORDS },
	{ "index", (PyCFunction) Filetype_index, METH_KEYWORDS },
	{ "get_sorted_by_name", (PyCFunction) Filetype_get_sorted_by_name, METH_NOARGS },
	{ "lookup_by_name", (PyCFunction) Filetype_lookup_by_name, METH_KEYWORDS },
	{ "get_sorted_by_title", (PyCFunction) Filetype_get_sorted_by_title, METH_NOARGS },
	{ NULL }
};


PyMODINIT_FUNC
initfiletypes(void)
{
	PyObject *m;

	FiletypeType.tp_new = PyType_GenericNew;
	if (PyType_Ready(&FiletypeType) < 0)
		return;

	m = Py_InitModule3("filetypes", FiletypeModule_methods,
			"Filetype information and management.");

	Py_INCREF(&FiletypeType);
	PyModule_AddObject(m, "Filetype", (PyObject *)&FiletypeType);
}


Filetype *Filetype_create_new_from_geany_filetype(GeanyFiletype *ft)
{
	Filetype *self;
	self = (Filetype *) PyObject_CallObject((PyObject *) &FiletypeType, NULL);
	self->ft = ft;
	return self;
}
