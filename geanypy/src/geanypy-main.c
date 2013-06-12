#include "geanypy.h"


static PyObject *
Main_is_realized(PyObject *module)
{
	if (main_is_realized())
		Py_RETURN_TRUE;
	else
		Py_RETURN_FALSE;
}


static PyObject *
Main_locale_init(PyObject *module, PyObject *args, PyObject *kwargs)
{
	gchar *locale_dir = NULL, *package = NULL;
	static gchar *kwlist[] = { "locale_dir", "gettext_package", NULL };

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "ss", kwlist, &locale_dir, &package))
	{
		if (locale_dir && package)
			main_locale_init(locale_dir, package);
	}

	Py_RETURN_NONE;
}


static PyObject *
Main_reload_configuration(PyObject *module)
{
	main_reload_configuration();
	Py_RETURN_NONE;
}


static
PyMethodDef MainModule_methods[] = {
	{ "is_realized", (PyCFunction) Main_is_realized, METH_NOARGS,
		"Checks whether the main gtk.Window has been realized." },
	{ "locale_init", (PyCFunction) Main_locale_init, METH_KEYWORDS,
		"Initializes the gettext translation system." },
	{ "reload_configuration", (PyCFunction) Main_reload_configuration, METH_NOARGS,
		"Reloads most of Geany's configuration files without restarting." },
	{ NULL }
};


PyMODINIT_FUNC initmain(void)
{
	Py_InitModule3("main", MainModule_methods, "Main program related functions.");
}
