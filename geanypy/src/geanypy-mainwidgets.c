#include "geanypy.h"


static void
MainWidgets_dealloc(MainWidgets *self)
{
	self->ob_type->tp_free((PyObject *) self);
}


static int
MainWidgets_init(MainWidgets *self)
{
	self->main_widgets = geany_data->main_widgets;
	return 0;
}


static PyObject *
MainWidgets_get_property(MainWidgets *self, const gchar *prop_name)
{
	g_return_val_if_fail(self != NULL, NULL);
	g_return_val_if_fail(prop_name != NULL, NULL);

	if (!self->main_widgets)
	{
		PyErr_SetString(PyExc_RuntimeError,
			"MainWidgets instance not initialized properly");
		return NULL;
	}

	if (g_str_equal(prop_name, "editor_menu"))
		return pygobject_new(G_OBJECT(self->main_widgets->editor_menu));
	else if (g_str_equal(prop_name, "message_window_notebook"))
		return pygobject_new(G_OBJECT(self->main_widgets->message_window_notebook));
	else if (g_str_equal(prop_name, "notebook"))
		return pygobject_new(G_OBJECT(self->main_widgets->notebook));
	else if (g_str_equal(prop_name, "progressbar"))
		return pygobject_new(G_OBJECT(self->main_widgets->progressbar));
	else if (g_str_equal(prop_name, "project_menu"))
		return pygobject_new(G_OBJECT(self->main_widgets->project_menu));
	else if (g_str_equal(prop_name, "sidebar_notebook"))
		return pygobject_new(G_OBJECT(self->main_widgets->sidebar_notebook));
	else if (g_str_equal(prop_name, "toolbar"))
		return pygobject_new(G_OBJECT(self->main_widgets->toolbar));
	else if (g_str_equal(prop_name, "tools_menu"))
		return pygobject_new(G_OBJECT(self->main_widgets->tools_menu));
	else if (g_str_equal(prop_name, "window"))
		return pygobject_new(G_OBJECT(self->main_widgets->window));

	Py_RETURN_NONE;
}
GEANYPY_PROPS_READONLY(MainWidgets);


static PyGetSetDef MainWidgets_getseters[] = {
	GEANYPY_GETSETDEF(MainWidgets, "editor_menu",
		"Popup context menu on the editor widget."),
	GEANYPY_GETSETDEF(MainWidgets, "message_window_notebook",
		"Notebook in the bottom message window."),
	GEANYPY_GETSETDEF(MainWidgets, "notebook",
		"The central documents notebook."),
	GEANYPY_GETSETDEF(MainWidgets, "progressbar",
		"Progress bar in the bottom status bar."),
	GEANYPY_GETSETDEF(MainWidgets, "project_menu",
		"The Project menu in the main menu bar."),
	GEANYPY_GETSETDEF(MainWidgets, "sidebar_notebook",
		"The notebook in the sidebar."),
	GEANYPY_GETSETDEF(MainWidgets, "toolbar",
		"Main toolbar."),
	GEANYPY_GETSETDEF(MainWidgets, "tools_menu",
		"The Tools menu in the main menu bar (recommended for plugins)."),
	GEANYPY_GETSETDEF(MainWidgets, "window",
		"Main window."),
	{ NULL },
};


PyTypeObject MainWidgetsType = {
	PyObject_HEAD_INIT(NULL)
	0,													/* ob_size */
	"geany.ui_utils.MainWidgets",						/* tp_name */
	sizeof(MainWidgets),								/* tp_basicsize */
	0,													/* tp_itemsize */
	(destructor) MainWidgets_dealloc,					/* tp_dealloc */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,			/* tp_print - tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,			/* tp_flags */
	"Wrapper around the GeanyMainWidgets structure.",	/* tp_doc */
	0, 0, 0, 0, 0, 0, 0, 0,								/* tp_traverse - tp_members */
	MainWidgets_getseters,								/* tp_getset */
	0, 0, 0, 0, 0,										/* tp_base - tp_dictoffset */
	(initproc) MainWidgets_init,						/* tp_init */
	0, 0,												/* tp_alloc - tp_new */

};
