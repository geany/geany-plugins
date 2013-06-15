#include "geanypy.h"

/* TODO: see if the TemplatePrefs members are safe to modify. */

typedef struct
{
	PyObject_HEAD
	GeanyTemplatePrefs *template_prefs;
} TemplatePrefs;


static void
TemplatePrefs_dealloc(TemplatePrefs *self)
{
	g_return_if_fail(self != NULL);
	self->ob_type->tp_free((PyObject *) self);
}


static int
TemplatePrefs_init(TemplatePrefs *self)
{
	g_return_val_if_fail(self != NULL, -1);
	self->template_prefs = geany_data->template_prefs;
	return 0;
}


static PyObject *
TemplatePrefs_get_property(TemplatePrefs *self, const gchar *prop_name)
{
	g_return_val_if_fail(self != NULL, NULL);
	g_return_val_if_fail(prop_name != NULL, NULL);

	if (!self->template_prefs)
	{
		PyErr_SetString(PyExc_RuntimeError,
			"TemplatePrefs instance not initialized properly");
		return NULL;
	}

	if (g_str_equal(prop_name, "company"))
		return PyString_FromString(self->template_prefs->company);
	else if (g_str_equal(prop_name, "developer"))
		return PyString_FromString(self->template_prefs->developer);
	else if (g_str_equal(prop_name, "initials"))
		return PyString_FromString(self->template_prefs->initials);
	else if (g_str_equal(prop_name, "mail"))
		return PyString_FromString(self->template_prefs->mail);
	else if (g_str_equal(prop_name, "version"))
		return PyString_FromString(self->template_prefs->version);

	Py_RETURN_NONE;
}
GEANYPY_PROPS_READONLY(TemplatePrefs);


static PyGetSetDef TemplatePrefs_getseters[] = {
	GEANYPY_GETSETDEF(TemplatePrefs, "company", ""),
	GEANYPY_GETSETDEF(TemplatePrefs, "developer", ""),
	GEANYPY_GETSETDEF(TemplatePrefs, "initials", ""),
	GEANYPY_GETSETDEF(TemplatePrefs, "mail", "Email address"),
	GEANYPY_GETSETDEF(TemplatePrefs, "version", "Initial version"),
	{ NULL }
};


static PyTypeObject TemplatePrefsType = {
	PyObject_HEAD_INIT(NULL)
	0,											/* ob_size */
	"geany.templates.TemplatePrefs",							/* tp_name */
	sizeof(TemplatePrefs),								/* tp_basicsize */
	0,											/* tp_itemsize */
	(destructor) TemplatePrefs_dealloc,					/* tp_dealloc */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* tp_print - tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,	/* tp_flags */
	"Wrapper around a GeanyTemplatePrefs structure.",		/* tp_doc  */
	0, 0, 0, 0, 0, 0, 0, 0,						/* tp_traverse - tp_members */
	TemplatePrefs_getseters,								/* tp_getset */
	0, 0, 0, 0, 0,								/* tp_base - tp_dictoffset */
	(initproc) TemplatePrefs_init,						/* tp_init */
	0, 0,										/* tp_alloc - tp_new */
};


static PyMethodDef TemplatePrefsModule_methods[] = { { NULL } };


PyMODINIT_FUNC inittemplates(void)
{
	PyObject *m;

	TemplatePrefsType.tp_new = PyType_GenericNew;
	if (PyType_Ready(&TemplatePrefsType) < 0)
		return;

	m = Py_InitModule3("templates", TemplatePrefsModule_methods,
			"Template information and management.");

	Py_INCREF(&TemplatePrefsType);
	PyModule_AddObject(m, "TemplatePrefs", (PyObject *) &TemplatePrefsType);
}
