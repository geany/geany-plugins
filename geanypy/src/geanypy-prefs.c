#include "geanypy.h"


typedef struct
{
	PyObject_HEAD
	GeanyPrefs *prefs;
} Prefs;


static void
Prefs_dealloc(Prefs *self)
{
	g_return_if_fail(self != NULL);
	self->ob_type->tp_free((PyObject *) self);
}


static int
Prefs_init(Prefs *self)
{
	g_return_val_if_fail(self != NULL, -1);
	self->prefs = geany_data->prefs;
	return 0;
}


static PyObject *
Prefs_get_property(Prefs *self, const gchar *prop_name)
{
	g_return_val_if_fail(self != NULL, NULL);
	g_return_val_if_fail(prop_name != NULL, NULL);

	if (!self->prefs)
	{
		PyErr_SetString(PyExc_RuntimeError,
			"Prefs instance not initialized properly");
		return NULL;
	}

	if (g_str_equal(prop_name, "default_open_path") && self->prefs->default_open_path)
		return PyString_FromString(self->prefs->default_open_path);

	Py_RETURN_NONE;
}
GEANYPY_PROPS_READONLY(Prefs);


static PyGetSetDef Prefs_getseters[] = {
	GEANYPY_GETSETDEF(Prefs, "default_open_path",
		"Default path to look for files when no other path is appropriate."),
	{ NULL }
};


static PyTypeObject PrefsType = {
	PyObject_HEAD_INIT(NULL)
	0,											/* ob_size */
	"geany.prefs.Prefs",						/* tp_name */
	sizeof(Prefs),								/* tp_basicsize */
	0,											/* tp_itemsize */
	(destructor) Prefs_dealloc,					/* tp_dealloc */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* tp_print - tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,	/* tp_flags */
	"Wrapper around a GeanyPrefs structure.",	/* tp_doc  */
	0, 0, 0, 0, 0, 0, 0, 0,						/* tp_traverse - tp_members */
	Prefs_getseters,							/* tp_getset */
	0, 0, 0, 0, 0,								/* tp_base - tp_dictoffset */
	(initproc) Prefs_init,						/* tp_init */
	0, 0,										/* tp_alloc - tp_new */
};


typedef struct
{
	PyObject_HEAD
	GeanyToolPrefs *tool_prefs;
} ToolPrefs;


static void
ToolPrefs_dealloc(ToolPrefs *self)
{
	g_return_if_fail(self != NULL);
	self->ob_type->tp_free((PyObject *) self);
}


static int
ToolPrefs_init(ToolPrefs *self)
{
	g_return_val_if_fail(self != NULL, -1);
	self->tool_prefs = geany_data->tool_prefs;
	return 0;
}


static PyObject *
ToolPrefs_get_property(ToolPrefs *self, const gchar *prop_name)
{
	g_return_val_if_fail(self != NULL, NULL);
	g_return_val_if_fail(prop_name != NULL, NULL);

	if (!self->tool_prefs)
	{
		PyErr_SetString(PyExc_RuntimeError,
			"ToolPrefs instance not initialized properly");
		return NULL;
	}

	if (g_str_equal(prop_name, "browser_cmd") && self->tool_prefs->browser_cmd)
		return PyString_FromString(self->tool_prefs->browser_cmd);
	else if (g_str_equal(prop_name, "context_action_cmd") && self->tool_prefs->context_action_cmd)
		return PyString_FromString(self->tool_prefs->context_action_cmd);
	else if (g_str_equal(prop_name, "grep_cmd") && self->tool_prefs->grep_cmd)
		return PyString_FromString(self->tool_prefs->grep_cmd);
	else if (g_str_equal(prop_name, "term_cmd") && self->tool_prefs->term_cmd)
		return PyString_FromString(self->tool_prefs->term_cmd);

	Py_RETURN_NONE;
}
GEANYPY_PROPS_READONLY(ToolPrefs);


static PyGetSetDef ToolPrefs_getseters[] = {
	GEANYPY_GETSETDEF(ToolPrefs, "browser_cmd", ""),
	GEANYPY_GETSETDEF(ToolPrefs, "context_action_cmd", ""),
	GEANYPY_GETSETDEF(ToolPrefs, "grep_cmd", ""),
	GEANYPY_GETSETDEF(ToolPrefs, "term_cmd", ""),
	{ NULL }
};


static PyTypeObject ToolPrefsType = {
	PyObject_HEAD_INIT(NULL)
	0,											/* ob_size */
	"geany.prefs.ToolPrefs",						/* tp_name */
	sizeof(ToolPrefs),								/* tp_basicsize */
	0,											/* tp_itemsize */
	(destructor) ToolPrefs_dealloc,					/* tp_dealloc */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* tp_print - tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,	/* tp_flags */
	"Wrapper around a GeanyToolPrefs structure.",	/* tp_doc  */
	0, 0, 0, 0, 0, 0, 0, 0,						/* tp_traverse - tp_members */
	ToolPrefs_getseters,							/* tp_getset */
	0, 0, 0, 0, 0,								/* tp_base - tp_dictoffset */
	(initproc) ToolPrefs_init,						/* tp_init */
	0, 0,										/* tp_alloc - tp_new */
};


#ifdef ENABLE_PRIVATE
static PyObject *
Prefs_show_dialog(PyObject *module)
{
	prefs_show_dialog();
	Py_RETURN_NONE;
}
#endif


static PyMethodDef PrefsModule_methods[] = {
#ifdef ENABLE_PRIVATE
	{ "show_dialog", (PyCFunction) Prefs_show_dialog, METH_NOARGS,
		"Show the preferences dialog." },
#endif
	{ NULL }
};


PyMODINIT_FUNC initprefs(void)
{
	PyObject *m;

	PrefsType.tp_new = PyType_GenericNew;
	if (PyType_Ready(&PrefsType) < 0)
		return;

	ToolPrefsType.tp_new = PyType_GenericNew;
	if (PyType_Ready(&ToolPrefsType) < 0)
		return;

	m = Py_InitModule3("prefs", PrefsModule_methods,
			"General preferences dialog settings");

	Py_INCREF(&PrefsType);
	PyModule_AddObject(m, "Prefs", (PyObject *) &PrefsType);

	Py_INCREF(&ToolPrefsType);
	PyModule_AddObject(m, "ToolPrefs", (PyObject *) &ToolPrefsType);
}
