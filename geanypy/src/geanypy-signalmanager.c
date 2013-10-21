#include "geanypy.h"

struct _SignalManager
{
	GeanyPlugin *geany_plugin;
	PyObject *py_obj;
	GObject *obj;
};


static void signal_manager_connect_signals(SignalManager *man);

static void on_build_start(GObject *geany_object, SignalManager *man);
static void on_document_activate(GObject *geany_object, GeanyDocument *doc, SignalManager *man);
static void on_document_before_save(GObject *geany_object, GeanyDocument *doc, SignalManager *man);
static void on_document_close(GObject *geany_object, GeanyDocument *doc, SignalManager *man);
static void on_document_filetype_set(GObject *geany_object, GeanyDocument *doc, GeanyFiletype *filetype_old, SignalManager *man);
static void on_document_new(GObject *geany_object, GeanyDocument *doc, SignalManager *man);
static void on_document_open(GObject *geany_object, GeanyDocument *doc, SignalManager *man);
static void on_document_reload(GObject *geany_object, GeanyDocument *doc, SignalManager *man);
static void on_document_save(GObject *geany_object, GeanyDocument *doc, SignalManager *man);
static gboolean on_editor_notify(GObject *geany_object, GeanyEditor *editor, SCNotification *nt, SignalManager *man);
static void on_geany_startup_complete(GObject *geany_object, SignalManager *man);
static void on_project_close(GObject *geany_object, SignalManager *man);
static void on_project_dialog_confirmed(GObject *geany_object, GtkWidget *notebook, SignalManager *man);
static void on_project_dialog_open(GObject *geany_object, GtkWidget *notebook, SignalManager *man);
static void on_project_dialog_close(GObject *geany_object, GtkWidget *notebook, SignalManager *man);
static void on_project_open(GObject *geany_object, GKeyFile *config, SignalManager *man);
static void on_project_save(GObject *geany_object, GKeyFile *config, SignalManager *man);
static void on_update_editor_menu(GObject *geany_object, const gchar *word, gint pos, GeanyDocument *doc, SignalManager *man);


SignalManager *signal_manager_new(GeanyPlugin *geany_plugin)
{
	SignalManager *man;
	PyObject *module;

	man = g_new0(SignalManager, 1);

	man->geany_plugin = geany_plugin;
	man->py_obj = NULL;
	man->obj = NULL;

	module = PyImport_ImportModule("geany");
	if (!module)
	{
		if (PyErr_Occurred())
			PyErr_Print();
		g_warning("Unable to import 'geany' module");
		g_free(man);
		return NULL;
	}

	man->py_obj = PyObject_GetAttrString(module, "signals");
	Py_DECREF(module);
	if (!man->py_obj)
	{
		if (PyErr_Occurred())
			PyErr_Print();
		g_warning("Unable to get 'SignalManager' instance from 'geany' module.");
		g_free(man);
		return NULL;
	}
	man->obj = pygobject_get(man->py_obj);

	signal_manager_connect_signals(man);

	return man;
}


void signal_manager_free(SignalManager *man)
{
	g_return_if_fail(man != NULL);
	Py_XDECREF(man->py_obj);
	g_free(man);
}

GObject *signal_manager_get_gobject(SignalManager *signal_manager)
{
	return G_OBJECT(signal_manager->obj);
}


static void signal_manager_connect_signals(SignalManager *man)
{
	plugin_signal_connect(geany_plugin, NULL, "build-start", TRUE, G_CALLBACK(on_build_start), man);
	plugin_signal_connect(geany_plugin, NULL, "document-activate", TRUE, G_CALLBACK(on_document_activate), man);
	plugin_signal_connect(geany_plugin, NULL, "document-before-save", TRUE, G_CALLBACK(on_document_before_save), man);
	plugin_signal_connect(geany_plugin, NULL, "document-close", TRUE, G_CALLBACK(on_document_close), man);
	plugin_signal_connect(geany_plugin, NULL, "document-filetype-set", TRUE, G_CALLBACK(on_document_filetype_set), man);
	plugin_signal_connect(geany_plugin, NULL, "document-new", TRUE, G_CALLBACK(on_document_new), man);
	plugin_signal_connect(geany_plugin, NULL, "document-open", TRUE, G_CALLBACK(on_document_open), man);
	plugin_signal_connect(geany_plugin, NULL, "document-reload", TRUE, G_CALLBACK(on_document_reload), man);
	plugin_signal_connect(geany_plugin, NULL, "document-save", TRUE, G_CALLBACK(on_document_save), man);
	plugin_signal_connect(geany_plugin, NULL, "editor-notify", TRUE, G_CALLBACK(on_editor_notify), man);
	plugin_signal_connect(geany_plugin, NULL, "geany-startup-complete", TRUE, G_CALLBACK(on_geany_startup_complete), man);
	plugin_signal_connect(geany_plugin, NULL, "project-close", TRUE, G_CALLBACK(on_project_close), man);
	plugin_signal_connect(geany_plugin, NULL, "project-dialog-confirmed", TRUE, G_CALLBACK(on_project_dialog_confirmed), man);
	plugin_signal_connect(geany_plugin, NULL, "project-dialog-open", TRUE, G_CALLBACK(on_project_dialog_open), man);
	plugin_signal_connect(geany_plugin, NULL, "project-dialog-close", TRUE, G_CALLBACK(on_project_dialog_close), man);
	plugin_signal_connect(geany_plugin, NULL, "project-open", TRUE, G_CALLBACK(on_project_open), man);
	plugin_signal_connect(geany_plugin, NULL, "project-save", TRUE, G_CALLBACK(on_project_save), man);
	plugin_signal_connect(geany_plugin, NULL, "update-editor-menu", TRUE, G_CALLBACK(on_update_editor_menu), man);
}

static void on_build_start(GObject *geany_object, SignalManager *man)
{
	g_signal_emit_by_name(man->obj, "build-start");
}


static void on_document_event(GObject *geany_object, GeanyDocument *doc, SignalManager *man, const gchar *signal_name)
{
	PyObject *py_doc = (PyObject *) Document_create_new_from_geany_document(doc);
	g_signal_emit_by_name(man->obj, signal_name, py_doc);
	Py_XDECREF(py_doc);
}


static void on_document_activate(GObject *geany_object, GeanyDocument *doc, SignalManager *man)
{
	on_document_event(geany_object, doc, man, "document-activate");
}


static void on_document_before_save(GObject *geany_object, GeanyDocument *doc, SignalManager *man)
{
	on_document_event(geany_object, doc, man, "document-before-save");
}


static void on_document_close(GObject *geany_object, GeanyDocument *doc, SignalManager *man)
{
	on_document_event(geany_object, doc, man, "document-close");
}


static void on_document_filetype_set(GObject *geany_object, GeanyDocument *doc, GeanyFiletype *filetype_old, SignalManager *man)
{
	PyObject *py_doc, *py_ft;
	py_doc = (PyObject *) Document_create_new_from_geany_document(doc);
	py_ft = (PyObject *) Filetype_create_new_from_geany_filetype(filetype_old);
	g_signal_emit_by_name(man->obj, "document-filetype-set", py_doc, py_ft);
	Py_XDECREF(py_doc);
	Py_XDECREF(py_ft);
}


static void on_document_new(GObject *geany_object, GeanyDocument *doc, SignalManager *man)
{
	on_document_event(geany_object, doc, man, "document-new");
}


static void on_document_open(GObject *geany_object, GeanyDocument *doc, SignalManager *man)
{
	on_document_event(geany_object, doc, man, "document-open");
}


static void on_document_reload(GObject *geany_object, GeanyDocument *doc, SignalManager *man)
{
	on_document_event(geany_object, doc, man, "document-reload");
}


static void on_document_save(GObject *geany_object, GeanyDocument *doc, SignalManager *man)
{
	on_document_event(geany_object, doc, man, "document-save");
}


static gboolean on_editor_notify(GObject *geany_object, GeanyEditor *editor, SCNotification *nt, SignalManager *man)
{
	gboolean res = FALSE;
	PyObject *py_ed, *py_notif;
	py_ed = (PyObject *) Editor_create_new_from_geany_editor(editor);
	py_notif = (PyObject *) Notification_create_new_from_scintilla_notification(nt);
	g_signal_emit_by_name(man->obj, "editor-notify", py_ed, py_notif, &res);
	Py_XDECREF(py_ed);
	Py_XDECREF(py_notif);
	return res;
}


static void on_geany_startup_complete(GObject *geany_object, SignalManager *man)
{
	g_signal_emit_by_name(man->obj, "geany-startup-complete");
}


static void on_project_close(GObject *geany_object, SignalManager *man)
{
	g_signal_emit_by_name(man->obj, "project-close");
}


static void on_project_dialog_confirmed(GObject *geany_object, GtkWidget *notebook, SignalManager *man)
{
	PyObject *gob = (PyObject *) pygobject_new(G_OBJECT(notebook));
	g_signal_emit_by_name(man->obj, "project-dialog-confirmed", gob);
	Py_XDECREF(gob);
}


static void on_project_dialog_open(GObject *geany_object, GtkWidget *notebook, SignalManager *man)
{
	PyObject *gob = (PyObject *) pygobject_new(G_OBJECT(notebook));
	g_signal_emit_by_name(man->obj, "project-dialog-open", gob);
	Py_XDECREF(gob);
}

static void on_project_dialog_close(GObject *geany_object, GtkWidget *notebook, SignalManager *man)
{
	PyObject *gob = (PyObject *) pygobject_new(G_OBJECT(notebook));
	g_signal_emit_by_name(man->obj, "project-dialog-close", gob);
	Py_XDECREF(gob);
}


static void on_project_open(GObject *geany_object, GKeyFile *config, SignalManager *man)
{
	PyObject *py_proj = (PyObject *) GEANYPY_NEW(Project);
	g_signal_emit_by_name(man->obj, "project-open", py_proj);
	Py_XDECREF(py_proj);
}


static void on_project_save(GObject *geany_object, GKeyFile *config, SignalManager *man)
{
	PyObject *py_proj = (PyObject *) GEANYPY_NEW(Project);
	g_signal_emit_by_name(man->obj, "project-save", py_proj);
	Py_XDECREF(py_proj);
}


static void on_update_editor_menu(GObject *geany_object, const gchar *word, gint pos, GeanyDocument *doc, SignalManager *man)
{
	PyObject *py_doc = (PyObject *) Document_create_new_from_geany_document(doc);
	g_signal_emit_by_name(man->obj, "update-editor-menu", word, pos, py_doc);
	Py_XDECREF(py_doc);
}
