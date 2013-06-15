#include "geanypy.h"


#define RET_BOOL(memb) \
	{ \
		if (memb) \
			Py_RETURN_TRUE; \
		else \
			Py_RETURN_FALSE; \
	}


static void
InterfacePrefs_dealloc(InterfacePrefs *self)
{
	g_return_if_fail(self != NULL);
	self->ob_type->tp_free((PyObject *) self);
}


static int
InterfacePrefs_init(InterfacePrefs *self)
{
	g_return_val_if_fail(self != NULL, -1);
	self->iface_prefs = geany_data->interface_prefs;
	return 0;
}


static PyObject *
InterfacePrefs_get_property(InterfacePrefs *self, const gchar *prop_name)
{
	g_return_val_if_fail(self != NULL, NULL);
	g_return_val_if_fail(prop_name != NULL, NULL);

	if (!self->iface_prefs)
	{
		PyErr_SetString(PyExc_RuntimeError,
			"InterfacePrefs instance not initialized properly");
		return NULL;
	}

	if (g_str_equal(prop_name, "compiler_tab_autoscroll"))
		RET_BOOL(self->iface_prefs->compiler_tab_autoscroll)
	else if (g_str_equal(prop_name, "editor_font"))
		return PyString_FromString(self->iface_prefs->editor_font);
	else if (g_str_equal(prop_name, "highlighting_invert_all"))
		RET_BOOL(self->iface_prefs->highlighting_invert_all)
	else if (g_str_equal(prop_name, "msgwin_compiler_visible"))
		RET_BOOL(self->iface_prefs->msgwin_compiler_visible)
	else if (g_str_equal(prop_name, "msgwin_font"))
		return PyString_FromString(self->iface_prefs->msgwin_font);
	else if (g_str_equal(prop_name, "msgwin_messages_visible"))
		RET_BOOL(self->iface_prefs->msgwin_messages_visible)
	else if (g_str_equal(prop_name, "msgwin_scribble_visible"))
		RET_BOOL(self->iface_prefs->msgwin_scribble_visible)
	else if (g_str_equal(prop_name, "msgwin_status_visible"))
		RET_BOOL(self->iface_prefs->msgwin_status_visible)
	else if (g_str_equal(prop_name, "notebook_double_click_hides_widgets"))
		RET_BOOL(self->iface_prefs->notebook_double_click_hides_widgets)
	else if (g_str_equal(prop_name, "show_notebook_tabs"))
		RET_BOOL(self->iface_prefs->show_notebook_tabs)
	else if (g_str_equal(prop_name, "show_symbol_list_expanders"))
		RET_BOOL(self->iface_prefs->show_symbol_list_expanders)
	else if (g_str_equal(prop_name, "sidebar_openfiles_visible"))
		RET_BOOL(self->iface_prefs->sidebar_openfiles_visible)
	else if (g_str_equal(prop_name, "sidebar_pos"))
		return PyInt_FromLong((glong) self->iface_prefs->sidebar_pos);
	else if (g_str_equal(prop_name, "sidebar_symbol_visible"))
		RET_BOOL(self->iface_prefs->sidebar_symbol_visible)
	else if (g_str_equal(prop_name, "statusbar_visible"))
		RET_BOOL(self->iface_prefs->statusbar_visible)
	else if (g_str_equal(prop_name, "tab_pos_editor"))
		return PyInt_FromLong((glong) self->iface_prefs->tab_pos_editor);
	else if (g_str_equal(prop_name, "tab_pos_msgwin"))
		return PyInt_FromLong((glong) self->iface_prefs->tab_pos_msgwin);
	else if (g_str_equal(prop_name, "tab_pos_sidebar"))
		return PyInt_FromLong((glong) self->iface_prefs->tab_pos_sidebar);
	else if (g_str_equal(prop_name, "tagbar_font"))
		return PyString_FromString(self->iface_prefs->tagbar_font);
	else if (g_str_equal(prop_name, "use_native_windows_dialogs"))
		RET_BOOL(self->iface_prefs->use_native_windows_dialogs)

	Py_RETURN_NONE;
}
GEANYPY_PROPS_READONLY(InterfacePrefs);


static PyGetSetDef InterfacePrefs_getseters[] = {
	GEANYPY_GETSETDEF(InterfacePrefs, "compiler_tab_autoscroll",
		"Wether compiler messages window is automatically scrolled to "
		"show new messages."),
	GEANYPY_GETSETDEF(InterfacePrefs, "editor_font",
		"Font used in the editor window."),
	GEANYPY_GETSETDEF(InterfacePrefs, "highlighting_invert_all",
		"Whether highlighting colors are inverted."),
	GEANYPY_GETSETDEF(InterfacePrefs, "msgwin_compiler_visible",
		"Wether message window's  compiler tab is visible."),
	GEANYPY_GETSETDEF(InterfacePrefs, "msgwin_font",
		"Font used in the message window."),
	GEANYPY_GETSETDEF(InterfacePrefs, "msgwin_messages_visible",
		"Whether message window's messages tab is visible."),
	GEANYPY_GETSETDEF(InterfacePrefs, "msgwin_scribble_visible",
		"Whether message window's scribble tab is visible."),
	GEANYPY_GETSETDEF(InterfacePrefs, "msgwin_status_visible",
		"Whether message window's status tab is visible."),
	GEANYPY_GETSETDEF(InterfacePrefs, "notebook_double_click_hides_widgets",
		"Whether a double-click on the notebook tabs hides all other windows."),
	GEANYPY_GETSETDEF(InterfacePrefs, "show_notebook_tabs",
		"Whether editor tabs are visible."),
	GEANYPY_GETSETDEF(InterfacePrefs, "show_symbol_list_expanders",
		"Whether to show expanders in the symbol list."),
	GEANYPY_GETSETDEF(InterfacePrefs, "sidebar_openfiles_visible",
		"Whether the open files list is visible."),
	GEANYPY_GETSETDEF(InterfacePrefs, "sidebar_pos",
		"Position of the sidebar (left or right)."),
	GEANYPY_GETSETDEF(InterfacePrefs, "sidebar_symbol_visible",
		"Whether the status bar is visible."),
	GEANYPY_GETSETDEF(InterfacePrefs, "statusbar_visible",
		"Whether the statusbar is visible."),
	GEANYPY_GETSETDEF(InterfacePrefs, "tab_pos_editor",
		"Position of the editor's tabs."),
	GEANYPY_GETSETDEF(InterfacePrefs, "tab_pos_msgwin",
		"Position of the message window tabs."),
	GEANYPY_GETSETDEF(InterfacePrefs, "tab_pos_sidebar",
		"Position of the sidebar's tabs."),
	GEANYPY_GETSETDEF(InterfacePrefs, "tagbar_font",
		"Symbol sidebar font."),
	GEANYPY_GETSETDEF(InterfacePrefs, "use_native_windows_dialogs",
		"Whether to use native Windows dialogs (only on Win32)."),
	{ NULL }
};


PyTypeObject InterfacePrefsType = {
	PyObject_HEAD_INIT(NULL)
	0,											/* ob_size */
	"geany.ui_utils.InterfacePrefs",			/* tp_name */
	sizeof(InterfacePrefs),						/* tp_basicsize */
	0,											/* tp_itemsize */
	(destructor) InterfacePrefs_dealloc,		/* tp_dealloc */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* tp_print - tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,	/* tp_flags */
	"Wrapper around a GeanyInterfacePrefs structure.",		/* tp_doc  */
	0, 0, 0, 0, 0, 0, 0, 0,						/* tp_traverse - tp_members */
	InterfacePrefs_getseters,					/* tp_getset */
	0, 0, 0, 0, 0,								/* tp_base - tp_dictoffset */
	(initproc) InterfacePrefs_init,				/* tp_init */
	0, 0,										/* tp_alloc - tp_new */
};
