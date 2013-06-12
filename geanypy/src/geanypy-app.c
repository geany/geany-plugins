#include "geanypy.h"


typedef struct
{
	PyObject_HEAD
	GeanyApp *app;
} App;


static void
App_dealloc(App *self)
{
	g_return_if_fail(self != NULL);
	self->ob_type->tp_free((PyObject *) self);
}


static int
App_init(App *self)
{
	g_return_val_if_fail(self != NULL, -1);
	g_return_val_if_fail(geany_data, -1);
	self->app = geany_data->app;
	return 0;
}


static PyObject *
App_get_property(App *self, const gchar *prop_name)
{
	g_return_val_if_fail(self != NULL, NULL);
	g_return_val_if_fail(prop_name != NULL, NULL);

	if (!self->app)
	{
		PyErr_SetString(PyExc_RuntimeError,
			"App instance not initialized properly");
		return NULL;
	}

	if (g_str_equal(prop_name, "configdir") && self->app->configdir)
		return PyString_FromString(self->app->configdir);
#if ENABLE_PRIVATE
	else if (g_str_equal(prop_name, "datadir") && self->app->datadir)
		return PyString_FromString(self->app->datadir);
	else if (g_str_equal(prop_name, "docdir") && self->app->docdir)
		return PyString_FromString(self->app->docdir);
#endif
	else if (g_str_equal(prop_name, "debug_mode") && self->app->debug_mode)
	{
		if (self->app->debug_mode)
			Py_RETURN_TRUE;
		else
			Py_RETURN_FALSE;
	}
	else if (g_str_equal(prop_name, "project") && self->app->project)
		return (PyObject *) GEANYPY_NEW(Project);

	Py_RETURN_NONE;
}
GEANYPY_PROPS_READONLY(App);


static PyGetSetDef App_getseters[] = {
	GEANYPY_GETSETDEF(App, "configdir",
		"User configuration directory, usually ~/.config/geany. "),
#ifdef ENABLE_PRIVATE
	GEANYPY_GETSETDEF(App, "datadir", "Geany's data directory."),
	GEANYPY_GETSETDEF(App, "docdir", "Geany's documentation directory."),
#endif
	GEANYPY_GETSETDEF(App, "debug_mode",
		"True if debug messages should be printed."),
	GEANYPY_GETSETDEF(App, "project",
		"Currently active project or None if none is open."),
	{ NULL }
};


static PyTypeObject AppType = {
	PyObject_HEAD_INIT(NULL)
	0,											/* ob_size */
	"geany.app.App",							/* tp_name */
	sizeof(App),								/* tp_basicsize */
	0,											/* tp_itemsize */
	(destructor) App_dealloc,					/* tp_dealloc */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* tp_print - tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,	/* tp_flags */
	"Wrapper around a GeanyApp structure.",		/* tp_doc  */
	0, 0, 0, 0, 0, 0, 0, 0,						/* tp_traverse - tp_members */
	App_getseters,								/* tp_getset */
	0, 0, 0, 0, 0,								/* tp_base - tp_dictoffset */
	(initproc) App_init,						/* tp_init */
	0, 0,										/* tp_alloc - tp_new */
};


static PyMethodDef AppModule_methods[] = { { NULL } };


PyMODINIT_FUNC initapp(void)
{
	PyObject *m;

	AppType.tp_new = PyType_GenericNew;
	if (PyType_Ready(&AppType) < 0)
		return;

	m = Py_InitModule3("app", AppModule_methods, "Application information");

	Py_INCREF(&AppType);
	PyModule_AddObject(m, "App", (PyObject *) &AppType);
}
