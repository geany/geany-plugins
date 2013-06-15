#include "geanypy.h"


static void
Document_dealloc(Document *self)
{
	self->ob_type->tp_free((PyObject *) self);
}


static int
Document_init(Document *self)
{
	self->doc = NULL;
	return 0;
}


static PyObject *
Document_get_property(Document *self, const gchar *prop_name)
{
	g_return_val_if_fail(self != NULL, NULL);
	g_return_val_if_fail(prop_name != NULL, NULL);

	if (!self->doc)
	{
		PyErr_SetString(PyExc_RuntimeError,
			"Document instance not initialized properly");
		return NULL;
	}

	if (!DOC_VALID(self->doc))
	{
		PyErr_SetString(PyExc_RuntimeError, "Document is invalid");
		return NULL;
	}

	if (g_str_equal(prop_name, "basename_for_display"))
	{
		PyObject *py_str = NULL;
		gchar *res_string;
		res_string = document_get_basename_for_display(self->doc, -1);
		if (!res_string)
		{
			PyErr_SetString(PyExc_RuntimeError,
				"Geany API failed to return a string");
			Py_RETURN_NONE;
		}
		py_str = PyString_FromString(res_string);
		g_free(res_string);
		return py_str;
	}
	else if (g_str_equal(prop_name, "notebook_page"))
		return Py_BuildValue("i", document_get_notebook_page(self->doc));
	else if (g_str_equal(prop_name, "status_color"))
	{
		const GdkColor *color = document_get_status_color(self->doc);
		if (!color)
			Py_RETURN_NONE;
		return Py_BuildValue("iii", color->red, color->green, color->blue);
	}
	else if (g_str_equal(prop_name, "editor")  && self->doc->editor)
	{
		Editor *editor;
		if (self->doc->editor != NULL)
		{
			editor = Editor_create_new_from_geany_editor(self->doc->editor);
			return (PyObject *) editor;
		}
		Py_RETURN_NONE;
	}
	else if (g_str_equal(prop_name, "encoding") && self->doc->encoding)
		return PyString_FromString(self->doc->encoding);
	else if (g_str_equal(prop_name, "file_name") && self->doc->file_name)
		return PyString_FromString(self->doc->file_name);
	else if (g_str_equal(prop_name, "file_type") && self->doc->file_type)
		return (PyObject *) Filetype_create_new_from_geany_filetype(self->doc->file_type);
	else if (g_str_equal(prop_name, "has_bom"))
	{
		if (self->doc->has_bom)
			Py_RETURN_TRUE;
		else
			Py_RETURN_FALSE;
	}
	else if (g_str_equal(prop_name, "has_tags"))
	{
		if (self->doc->has_tags)
			Py_RETURN_TRUE;
		else
			Py_RETURN_FALSE;
	}
	else if (g_str_equal(prop_name, "index"))
		return Py_BuildValue("i", self->doc->index);
	else if (g_str_equal(prop_name, "is_valid"))
	{
		if (self->doc->is_valid)
			Py_RETURN_TRUE;
		else
			Py_RETURN_FALSE;
	}
	else if (g_str_equal(prop_name, "readonly"))
	{
		if (self->doc->readonly)
			Py_RETURN_TRUE;
		else
			Py_RETURN_FALSE;
	}
	else if (g_str_equal(prop_name, "real_path"))
	{
		if (self->doc->real_path)
			return PyString_FromString(self->doc->real_path);
		Py_RETURN_NONE;
	}
	else if (g_str_equal(prop_name, "text_changed"))
	{
		if (self->doc->changed)
			Py_RETURN_NONE;
		else
			Py_RETURN_NONE;
	}

	Py_RETURN_NONE;
}


static int
Document_set_property(Document *self, PyObject *value, const gchar *prop_name)
{
	g_return_val_if_fail(self != NULL, -1);
	g_return_val_if_fail(value != NULL, -1);
	g_return_val_if_fail(prop_name != NULL, -1);

	if (!self->doc)
	{
		PyErr_SetString(PyExc_RuntimeError,
			"Document instance not initialized properly");
		return -1;
	}

	if (g_str_equal(prop_name, "encoding"))
	{
		gchar *encoding = PyString_AsString(value);
		if (encoding)
		{
			document_set_encoding(self->doc, encoding);
			return 0;
		}
	}
	else if (g_str_equal(prop_name, "filetype"))
	{
		Filetype *filetype = (Filetype *) value;
		if (filetype->ft)
		{
			document_set_filetype(self->doc, filetype->ft);
			return 0;
		}
	}
	else if (g_str_equal(prop_name, "text_changed"))
	{
		long v = PyInt_AsLong(value);
		if (v == -1 && PyErr_Occurred())
		{
			PyErr_Print();
			return -1;
		}
		document_set_text_changed(self->doc, (gboolean) v);
		return 0;
	}

	PyErr_SetString(PyExc_AttributeError, "can't set property");
	return -1;
}


static PyObject*
Document_close(Document *self)
{
	if (document_close(self->doc))
		Py_RETURN_TRUE;
	else
		Py_RETURN_FALSE;
}


static PyObject*
Document_reload_file(Document *self, PyObject *args, PyObject *kwargs)
{
	gchar *forced_enc = NULL;
	static gchar *kwlist[] = { "forced_enc", NULL };

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "|z", kwlist, &forced_enc))
	{
		if (document_reload_file(self->doc, forced_enc))
			Py_RETURN_TRUE;
		else
			Py_RETURN_FALSE;
	}

	Py_RETURN_NONE;
}


static PyObject*
Document_rename_file(Document *self, PyObject *args, PyObject *kwargs)
{
	gchar *new_fn = NULL;
	static gchar *kwlist[] = { "new_filename", NULL };

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "s", kwlist, &new_fn))
	{
		if (new_fn != NULL)
			document_rename_file(self->doc, new_fn);
	}

	if (DOC_VALID(self->doc) && self->doc->file_name == new_fn)
		Py_RETURN_TRUE;
	else
		Py_RETURN_FALSE;

	Py_RETURN_NONE;
}


static PyObject*
Document_save_file(Document *self, PyObject *args, PyObject *kwargs)
{
	gboolean result;
	gint force = 0;
	static gchar *kwlist[] = { "force", NULL };

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "|i", kwlist, &force))
	{
		result = document_save_file(self->doc, (gboolean) force);
		if (result)
			Py_RETURN_TRUE;
		else
			Py_RETURN_FALSE;
	}
	Py_RETURN_NONE;
}


static PyObject*
Document_save_file_as(Document *self, PyObject *args, PyObject *kwargs)
{
	gboolean result;
	gchar *filename = NULL;
	static gchar *kwlist[] = { "new_filename", NULL };

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "s", kwlist, &filename))
	{
		if (filename != NULL)
		{
			result = document_save_file_as(self->doc, filename);
			if (result)
				Py_RETURN_TRUE;
			else
				Py_RETURN_FALSE;
		}
	}
	Py_RETURN_NONE;
}


static PyMethodDef Document_methods[] = {
	{ "close",				(PyCFunction)Document_close, 		METH_NOARGS,
		"Closes the document." },
	{ "reload_file",		(PyCFunction)Document_reload_file,	METH_KEYWORDS,
		"Reloads the document with the specified file encoding or None "
		"to auto-detect the file encoding." },
	{ "rename_file",		(PyCFunction)Document_rename_file,	METH_KEYWORDS,
		"Renames the document's file." },
	{ "save_file",			(PyCFunction)Document_save_file,	METH_KEYWORDS,
		"Saves the document's file." },
	{ "save_file_as",		(PyCFunction)Document_save_file_as,	METH_KEYWORDS,
		"Saves the document with a new filename, detecting the filetype." },
	{ NULL }
};


static PyGetSetDef Document_getseters[] = {
	GEANYPY_GETSETDEF(Document, "editor",
		"The editor associated with the document."),
	GEANYPY_GETSETDEF(Document, "encoding",
		"The encoding of the document."),
	GEANYPY_GETSETDEF(Document, "file_name",
		"The document's filename."),
	GEANYPY_GETSETDEF(Document, "file_type",
		"The filetype for this document."),
	GEANYPY_GETSETDEF(Document, "has_bom",
		"Internally used flag to indicate whether the file of this document "
		"has a byte-order-mark."),
	GEANYPY_GETSETDEF(Document, "has_tags",
		"Whether this document supports source code symbols (tags) to "
		"show in the sidebar."),
	GEANYPY_GETSETDEF(Document, "index",
		"Index of the document."),
	GEANYPY_GETSETDEF(Document, "is_valid",
		"General flag to represent this document is active and all properties "
		"are set correctly."),
	GEANYPY_GETSETDEF(Document, "readonly",
		"Whether this document is read-only."),
	GEANYPY_GETSETDEF(Document, "real_path",
		"The link-dereferenced filename of the document."),
	GEANYPY_GETSETDEF(Document, "basename_for_display",
		"Returns the basename of the document's file."),
	GEANYPY_GETSETDEF(Document, "notebook_page",
		"Gets the notebook page index for the document."),
	GEANYPY_GETSETDEF(Document, "status_color",
		"Gets the status color of the document or None for default."),
	GEANYPY_GETSETDEF(Document, "text_changed",
		"Whether the document has been changed since it was last saved."),
	{ NULL },
};



static PyTypeObject DocumentType = {
	PyObject_HEAD_INIT(NULL)
	0,											/* ob_size */
	"geany.document.Document",					/* tp_name */
	sizeof(Document),							/* tp_basicsize */
	0,											/* tp_itemsize */
	(destructor) Document_dealloc,				/* tp_dealloc */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* tp_print - tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,	/* tp_flags */
	"Wrapper around a GeanyDocument structure.",/* tp_doc */
	0, 0, 0, 0, 0, 0,							/* tp_traverse - tp_iternext */
	Document_methods,							/* tp_methods */
	0,											/* tp_members */
	Document_getseters,							/* tp_getset */
	0, 0, 0, 0, 0,								/* tp_base - tp_dictoffset */
	(initproc) Document_init,					/* tp_init */
	0, 0,										/* tp_alloc - tp_new */

};


static PyObject*
Document_find_by_filename(PyObject *self, PyObject *args, PyObject *kwargs)
{
	gchar *fn;
	GeanyDocument *doc;
	static gchar *kwlist[] = { "filename", NULL };

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "s", kwlist, &fn))
	{
		doc = document_find_by_filename(fn);
		if (DOC_VALID(doc))
			return (PyObject *) Document_create_new_from_geany_document(doc);
	}
	Py_RETURN_NONE;
}


static PyObject*
Document_find_by_real_path(PyObject *self, PyObject *args, PyObject *kwargs)
{
	gchar *fn;
	GeanyDocument *doc;
	static gchar *kwlist[] = { "real_path", NULL };

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "s", kwlist, &fn))
	{
		doc = document_find_by_real_path(fn);
		if (DOC_VALID(doc))
			return (PyObject *) Document_create_new_from_geany_document(doc);
	}
	Py_RETURN_NONE;
}


static PyObject*
Document_get_current(PyObject *self)
{
	GeanyDocument *doc;

	doc = document_get_current();
	if (DOC_VALID(doc))
		return (PyObject *) Document_create_new_from_geany_document(doc);
	Py_RETURN_NONE;
}


static PyObject*
Document_get_from_page(PyObject *self, PyObject *args, PyObject *kwargs)
{
	gint page_num;
	GeanyDocument *doc;
	static gchar *kwlist[] = { "page_num", NULL };

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "i", kwlist, &page_num))
	{
		doc = document_get_from_page(page_num);
		if (DOC_VALID(doc))
			return (PyObject *) Document_create_new_from_geany_document(doc);
	}
	Py_RETURN_NONE;
}


static PyObject*
Document_get_from_index(PyObject *self, PyObject *args, PyObject *kwargs)
{
	gint idx;
	GeanyDocument *doc;
	static gchar *kwlist[] = { "index", NULL };

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "i", kwlist, &idx))
	{
		doc = document_index(idx);
		if (DOC_VALID(doc))
			return (PyObject *) Document_create_new_from_geany_document(doc);
	}
	Py_RETURN_NONE;
}


static PyObject*
Document_new_file(PyObject *self, PyObject *args, PyObject *kwargs)
{
	gchar *filename = NULL, *initial_text = NULL;
	Filetype *filetype = NULL;
	PyObject *py_ft = NULL;
	GeanyDocument *doc;
	GeanyFiletype *ft = NULL;
	static gchar *kwlist[] = { "filename", "filetype", "initial_text", NULL };

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "|zOz", kwlist,
		&filename, &py_ft, &initial_text))
	{
		if (py_ft != NULL  && py_ft != Py_None)
		{
			filetype = (Filetype *) py_ft;
			if (filetype->ft != NULL)
				ft = filetype->ft;
		}
		doc = document_new_file(filename, ft, initial_text);
		if (DOC_VALID(doc))
			return (PyObject *) Document_create_new_from_geany_document(doc);
	}
	Py_RETURN_NONE;
}


static PyObject*
Document_open_file(PyObject *self, PyObject *args, PyObject *kwargs)
{
	gint read_only = 0;
	gchar *filename = NULL, *forced_encoding = NULL;
	GeanyDocument *doc;
	GeanyFiletype *ft = NULL;
	Filetype *filetype = NULL;
	PyObject *py_ft = NULL;
	static gchar *kwlist[] = { "filename", "read_only", "filetype",
		"forced_enc", NULL };

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "s|iOz", kwlist, &filename,
		&read_only, &py_ft, &forced_encoding))
	{
		if (py_ft != NULL && py_ft != Py_None)
		{
			filetype = (Filetype *) py_ft;
			if (filetype->ft != NULL)
				ft = filetype->ft;
		}
		doc = document_open_file(filename, read_only, ft, forced_encoding);
		if (DOC_VALID(doc))
			return (PyObject *) Document_create_new_from_geany_document(doc);
	}
	Py_RETURN_NONE;
}


static PyObject*
Document_remove_page(PyObject *self, PyObject *args, PyObject *kwargs)
{
	guint page_num;
	static gchar *kwlist[] = { "page_num", NULL };

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "i", kwlist, &page_num))
	{
		if (document_remove_page(page_num))
			Py_RETURN_TRUE;
		else
			Py_RETURN_FALSE;
	}
	Py_RETURN_NONE;
}


static PyObject *
Document_get_documents_list(PyObject *module)
{
	guint i;
	GeanyDocument *doc;
	PyObject *list;

	list = PyList_New(0);

	for (i = 0; i < geany_data->documents_array->len; i++)
	{
		doc = g_ptr_array_index(geany_data->documents_array, i);
		if (DOC_VALID(doc))
		{
			PyList_Append(list,
				(PyObject *) Document_create_new_from_geany_document(doc));
		}
	}

	return list;
}


static
PyMethodDef DocumentModule_methods[] = {
	{ "find_by_filename",	(PyCFunction) Document_find_by_filename,	METH_KEYWORDS,
		"Finds a document with the given filename." },
	{ "find_by_real_path",	(PyCFunction) Document_find_by_real_path,	METH_KEYWORDS,
		"Finds a document whose real path matches the given filename." },
	{ "get_current",		(PyCFunction) Document_get_current,			METH_NOARGS,
		"Finds the current document."},
	{ "get_from_page",		(PyCFunction) Document_get_from_page,		METH_KEYWORDS,
		"Finds the document for the given notebook page." },
	{ "index",				(PyCFunction) Document_get_from_index,		METH_KEYWORDS,
		"Finds the document with the given index." },
	{ "new_file",			(PyCFunction) Document_new_file,			METH_KEYWORDS,
		"Creates a new document." },
	{ "open_file",			(PyCFunction) Document_open_file,			METH_KEYWORDS,
		"Opens a new document specified by the given filename." },
	{ "remove_page",		(PyCFunction) Document_remove_page,			METH_KEYWORDS,
		"Removes the given notebook tab page and clears all related "
		"information in the documents list." },
	{ "get_documents_list",	(PyCFunction) Document_get_documents_list,	METH_NOARGS,
		"Returns a sequence of valid documents." },
	{ NULL }
};


PyMODINIT_FUNC initdocument(void)
{
	PyObject *m;

	DocumentType.tp_new = PyType_GenericNew;
	if (PyType_Ready(&DocumentType) < 0)
		return;

	m = Py_InitModule3("document", DocumentModule_methods,
			"Document information and management.");

	Py_INCREF(&DocumentType);
	PyModule_AddObject(m, "Document", (PyObject *)&DocumentType);
}


Document *Document_create_new_from_geany_document(GeanyDocument *doc)
{
	Document *self;
	self = (Document *) PyObject_CallObject((PyObject *) &DocumentType, NULL);
	self->doc = doc;
	return self;
}
