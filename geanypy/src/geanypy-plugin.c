/*
 * plugin.c
 *
 * Copyright 2011 Matthew Brush <mbrush@codebrainz.ca>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#if defined(HAVE_CONFIG_H) && !defined(GEANYPY_WINDOWS)
# include "config.h"
#endif

#define INCLUDE_PYGOBJECT_ONCE_FULL

#include "geanypy.h"

#include <glib.h>
#include <glib/gstdio.h>

GeanyData *geany_data;

/* Forward declarations to prevent compiler warnings. */
PyMODINIT_FUNC initapp(void);
PyMODINIT_FUNC initdialogs(void);
PyMODINIT_FUNC initdocument(void);
PyMODINIT_FUNC initeditor(void);
PyMODINIT_FUNC initencoding(void);
PyMODINIT_FUNC initfiletypes(void);
PyMODINIT_FUNC inithighlighting(void);
PyMODINIT_FUNC initmain(void);
PyMODINIT_FUNC initmsgwin(void);
PyMODINIT_FUNC initnavqueue(void);
PyMODINIT_FUNC initprefs(void);
PyMODINIT_FUNC initproject(void);
PyMODINIT_FUNC initscintilla(void);
PyMODINIT_FUNC initsearch(void);
PyMODINIT_FUNC inittemplates(void);
PyMODINIT_FUNC initui_utils(void);
PyMODINIT_FUNC initpluginbase(void);


static void
GeanyPy_start_interpreter(void)
{
    gchar *init_code;
    gchar *py_dir = NULL;


#ifndef GEANYPY_WINDOWS
	{ /* Prevents a crash in the dynload thingy
		 TODO: is this or the old dlopen version even needed? */
		GModule *mod = g_module_open(GEANYPY_PYTHON_LIBRARY, G_MODULE_BIND_LAZY);
		if (!mod) {
			g_warning(_("Unable to pre-load Python library: %s."), g_module_error());
			return;
		}
		g_module_close(mod);
	}
#endif

    Py_Initialize();

    /* Import the C modules */
    initapp();
    initdialogs();
    initdocument();
    initeditor();
    initencoding();
    initfiletypes();
    inithighlighting();
    initmain();
    initmsgwin();
    initnavqueue();
    initprefs();
    initproject();
    initscintilla();
    initsearch();
    inittemplates();
    initui_utils();
    initpluginbase();

#ifdef GEANYPY_WINDOWS
	{ /* On windows, get path at runtime since we don't really know where
	   * Geany is installed ahead of time. */
		gchar *geany_base_dir;
		geany_base_dir = g_win32_get_package_installation_directory_of_module(NULL);
		if (geany_base_dir)
		{
			py_dir = g_build_filename(geany_base_dir, "lib", "geanypy", NULL);
			g_free(geany_base_dir);
		}
		if (!g_file_test(py_dir, G_FILE_TEST_EXISTS))
		{
			g_critical("The path to the `geany' module was not found: %s", py_dir);
			g_free(py_dir);
			py_dir = g_strdup(""); /* will put current dir on path? */
		}
	}
#else
	py_dir = g_strdup(GEANYPY_PYTHON_DIR);
#endif

    /* Adjust Python path to find wrapper package (geany) */
    init_code = g_strdup_printf(
        "import os, sys\n"
        "path = '%s'.replace('~', os.path.expanduser('~'))\n"
        "sys.path.append(path)\n"
        "path = '%s'.replace('~', os.path.expanduser('~'))\n"
        "sys.path.append(path)\n"
        "import geany\n", py_dir, GEANYPY_PLUGIN_DIR);
    g_free(py_dir);

    PyRun_SimpleString(init_code);
    g_free(init_code);

}

static void
GeanyPy_stop_interpreter(void)
{
    if (Py_IsInitialized())
        Py_Finalize();
}

typedef struct
{
	PyObject *base;
	SignalManager *signal_manager;
}
GeanyPyData;

typedef struct
{
	PyObject *class;
	PyObject *module;
	PyObject *instance;
}
GeanyPyPluginData;

static gboolean has_error(void)
{
	if (PyErr_Occurred())
	{
		PyErr_Print();
		return TRUE;
	}
	return FALSE;
}

static PyTypeObject PluginBaseType;

static gboolean geanypy_proxy_init(GeanyPlugin *plugin, gpointer pdata)
{
	GeanyPyPluginData *data = (GeanyPyPluginData *) pdata;
	GeanyPyPluginBase *base;
	PyObject *args;

	base = PyObject_New(GeanyPyPluginBase, &PluginBaseType);
	base->plugin = plugin;

	/* The new-style constructor gets a context parameter, and the class must pass
	 * it to the geany.Plugin constructor. The new-style constructor is required
	 * to have certain APIs work in it (those that need the GeanyPlugin pointer) */
	args = Py_BuildValue("(O)", base);
	data->instance = PyObject_CallObject(data->class, args);
	Py_DECREF(args);
	Py_DECREF(base);

	if (PyErr_Occurred()) {
		PyErr_Clear();
		/* If the plugin still implements the old constructor we can catch this and try again */
		data->instance = PyObject_CallObject(data->class, NULL);
	}

	if (has_error())
		return FALSE;

	((GeanyPyPluginBase *)data->instance)->plugin = plugin;

	return TRUE;
}


static void geanypy_proxy_cleanup(GeanyPlugin *plugin, gpointer pdata)
{
	GeanyPyPluginData *data = (GeanyPyPluginData *) pdata;

	PyObject_CallMethod(data->instance, "cleanup", NULL);
	if (has_error())
		return;
}


static GtkWidget *geanypy_proxy_configure(GeanyPlugin *plugin, GtkDialog *parent, gpointer pdata)
{
	GeanyPyPluginData *data = (GeanyPyPluginData *) pdata;
	PyObject *o, *oparent;
	GObject *widget;

	oparent = pygobject_new(G_OBJECT(parent));
	o = PyObject_CallMethod(data->instance, "configure", "O", oparent, NULL);
	Py_DECREF(oparent);

	if (!has_error() && o != NULL)
	{
		/* Geany wants only the underlying GtkWidget, we must only ref that
		 * and free the pygobject wrapper */
		widget = g_object_ref(pygobject_get(o));
		Py_DECREF(o);
		return GTK_WIDGET(widget);
	}
	return NULL;
}

static void geanypy_proxy_help(GeanyPlugin *plugin, gpointer pdata)
{
	GeanyPyPluginData *data = (GeanyPyPluginData *) pdata;

	PyObject_CallMethod(data->instance, "help", NULL);
	if (has_error())
		return;
}

static gint
geanypy_probe(GeanyPlugin *proxy, const gchar *filename, gpointer pdata)
{
	gchar *file_plugin = g_strdup_printf("%.*s.plugin",
			(int)(strrchr(filename, '.') - filename), filename);
	gint ret = PROXY_IGNORED;

	/* avoid clash with libpeas py plugins, those come with a corresponding <plugin>.plugin file */
	if (!g_file_test(file_plugin, G_FILE_TEST_EXISTS))
		ret = PROXY_MATCHED;

	g_free(file_plugin);
	return ret;
}


static const gchar *string_from_attr(PyObject *o, const gchar *attr)
{
	PyObject *string = PyObject_GetAttrString(o, "__plugin_name__");
	const gchar *ret = PyString_AsString(string);
	Py_DECREF(string);

	return ret;
}


static gpointer
geanypy_load(GeanyPlugin *proxy, GeanyPlugin *subplugin, const gchar *filename, gpointer pdata)
{
	GeanyPyData *data = pdata;
	PyObject *fromlist, *module, *dict, *key, *val, *found = NULL;
	Py_ssize_t pos = 0;
	gchar *modulename, *dot;
	gpointer ret = NULL;

	modulename = g_path_get_basename(filename);
	/* We are guaranteed that filename has a .py extension
	 * because we did geany_plugin_register_proxy() for it */
	dot = strrchr(modulename, '.');
	*dot = '\0';
	/* we need a fromlist to be able to import modules with a '.' in the
	 * name. -- libpeas */
	fromlist = PyTuple_New (0);

	module = PyImport_ImportModuleEx(modulename, NULL, NULL, fromlist);
	if (has_error() || !module)
		goto err;

	dict = PyModule_GetDict(module);

	while (PyDict_Next (dict, &pos, &key, &val) && found == NULL)
	{
		if (PyType_Check(val) && PyObject_IsSubclass(val, data->base))
			found = val;
	}

	if (found)
	{
		GeanyPyPluginData *pdata = g_slice_new(GeanyPyPluginData);
		PluginInfo *info     = subplugin->info;
		GeanyPluginFuncs *funcs = subplugin->funcs;
		Py_INCREF(found);
		pdata->module        = module;
		pdata->class         = found;
		pdata->instance      = NULL;
		info->name           = string_from_attr(pdata->class, "__plugin_name__");
		info->description    = string_from_attr(pdata->class, "__plugin_description__");
		info->version        = string_from_attr(pdata->class, "__plugin_version__");
		info->author         = string_from_attr(pdata->class, "__plugin_author__");
		funcs->init          = geanypy_proxy_init;
		funcs->cleanup       = geanypy_proxy_cleanup;
		if (PyObject_HasAttrString(found, "configure"))
			funcs->configure = geanypy_proxy_configure;
		if (PyObject_HasAttrString(found, "help"))
			funcs->help      = geanypy_proxy_help;
		if (GEANY_PLUGIN_REGISTER_FULL(subplugin, 224, pdata, NULL))
			ret              = pdata;
	}

err:
	g_free(modulename);
	Py_DECREF(fromlist);
	return ret;
}


static void
geanypy_unload(GeanyPlugin *plugin, GeanyPlugin *subplugin, gpointer load_data, gpointer pdata_)
{
	GeanyPyPluginData *pdata = load_data;

	Py_XDECREF(pdata->instance);
	Py_DECREF(pdata->class);
	Py_DECREF(pdata->module);
	while (PyGC_Collect());
	g_slice_free(GeanyPyPluginData, pdata);
}


static gboolean geanypy_init(GeanyPlugin *plugin_, gpointer pdata)
{
	const gchar *exts[] = { "py", NULL };
	GeanyPyData *state = pdata;
	PyObject *module;

	plugin_->proxy_funcs->probe   = geanypy_probe;
	plugin_->proxy_funcs->load    = geanypy_load;
	plugin_->proxy_funcs->unload  = geanypy_unload;

	geany_data = plugin_->geany_data;

	GeanyPy_start_interpreter();
	state->signal_manager = signal_manager_new(plugin_);

	module = PyImport_ImportModule("geany.plugin");
	if (has_error() || !module)
		goto err;

	state->base = PyObject_GetAttrString(module, "Plugin");
	Py_DECREF(module);
	if (has_error() || !state->base)
		goto err;

	if (!geany_plugin_register_proxy(plugin_, exts)) {
		Py_DECREF(state->base);
		goto err;
	}

	return TRUE;

err:
	signal_manager_free(state->signal_manager);
	GeanyPy_stop_interpreter();
	return FALSE;
}


static void geanypy_cleanup(GeanyPlugin *plugin, gpointer pdata)
{
	GeanyPyData *state = pdata;
	signal_manager_free(state->signal_manager);
	Py_DECREF(state->base);
	GeanyPy_stop_interpreter();
}

G_MODULE_EXPORT void
geany_load_module(GeanyPlugin *plugin)
{
	GeanyPyData *state = g_new0(GeanyPyData, 1);

	plugin->info->name        = _("GeanyPy");
	plugin->info->description = _("Python plugins support");
	plugin->info->version     = "1.0";
	plugin->info->author      = "Matthew Brush <mbrush@codebrainz.ca>";
	plugin->funcs->init       = geanypy_init;
	plugin->funcs->cleanup    = geanypy_cleanup;

	GEANY_PLUGIN_REGISTER_FULL(plugin, 224, state, g_free);
}


static void PluginBase_dealloc(GeanyPyPluginBase *self) { }

static PyMethodDef
PluginBase_methods[] = {
	{ NULL }
};

static PyMethodDef
PluginModule_methods[] = {
	{ NULL }
};


static PyGetSetDef
PluginBase_getseters[] = {
	{ NULL },
};


static int
PluginBase_init(GeanyPyPluginBase *self, PyObject *args, PyObject *kwargs)
{
	GeanyPyPluginBase *py_context;

	if (PyArg_ParseTuple(args, "O", (PyObject **) &py_context) && (PyObject *)py_context != Py_None)
		self->plugin = py_context->plugin;

	return 0;
}


static PyTypeObject PluginBaseType = {
	PyObject_HEAD_INIT(NULL)
	0,											/* ob_size */
	"geany.pluginbase,PluginBase",					/* tp_name */
	sizeof(GeanyPyPluginBase),								/* tp_basicsize */
	0,											/* tp_itemsize */
	(destructor) PluginBase_dealloc,				/* tp_dealloc */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* tp_print - tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,	/* tp_flags */
	"Wrapper around a GeanyPlugin structure."	,/* tp_doc */
	0, 0, 0, 0, 0, 0,							/* tp_traverse - tp_iternext */
	PluginBase_methods,							/* tp_methods */
	0,											/* tp_members */
	PluginBase_getseters,							/* tp_getset */
	0, 0, 0, 0, 0,								/* tp_base - tp_dictoffset */
	(initproc) PluginBase_init,						/* tp_init */
	0, 0,										/* tp_alloc - tp_new */
};


PyMODINIT_FUNC initpluginbase(void)
{
	PyObject *m;

	PluginBaseType.tp_new = PyType_GenericNew;
	if (PyType_Ready(&PluginBaseType) < 0)
		return;

	m = Py_InitModule3("geany.pluginbase", PluginModule_methods,
			"Plugin management.");

	Py_INCREF(&PluginBaseType);
	PyModule_AddObject(m, "PluginBase", (PyObject *)&PluginBaseType);
}
