#include "geanypy.h"


#define GOB_CHECK(pyobj, arg) \
	{ \
		if (!pyobj || pyobj == Py_None || !pygobject_check(pyobj, PyGObject_Type)) \
		{ \
			PyErr_SetString(PyExc_ValueError, \
				"argument " #arg " must inherit from a gobject.GObject type"); \
			return NULL; \
		} \
	}

#define GOB_TYPE_CHECK(gob, gob_type, arg) \
	{ \
		if (!gob || !G_IS_OBJECT(gob) || \
			!g_type_is_a(G_TYPE_FROM_INSTANCE(gob), gob_type)) \
		{ \
			PyErr_SetString(PyExc_ValueError, \
				"argument " #arg " must inherit from a " #gob_type " type"); \
			return NULL; \
		} \
	}


static PyTypeObject *PyGObject_Type = NULL;


static PyObject *
UiUtils_hookup_widget(PyObject *module, PyObject *args, PyObject *kwargs)
{
	PyObject *py_owner = NULL, *py_widget = NULL;
	const gchar *widget_name = NULL;
	GObject *owner = NULL, *widget = NULL;
	static gchar *kwlist[] = { "owner", "widget", "widget_name", NULL };

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "OOs", kwlist,
		&py_owner, &py_widget, &widget_name))
	{
		GOB_CHECK(py_owner, 1);
		GOB_CHECK(py_widget, 2);
		owner = pygobject_get(py_owner);
		widget = pygobject_get(py_widget);
		ui_hookup_widget(owner, widget, widget_name);
	}

	Py_RETURN_NONE;
}


static PyObject *
UiUtils_lookup_widget(PyObject *module, PyObject *args, PyObject *kwargs)
{
	PyObject *py_widget = NULL;
	const gchar *widget_name = NULL;
	GObject *widget = NULL;
	GtkWidget *found_widget = NULL;
	static gchar *kwlist[] = { "widget", "widget_name", NULL };

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "Os", kwlist,
		&py_widget, &widget_name))
	{
		GOB_CHECK(py_widget, 1);
		widget = pygobject_get(py_widget);
		GOB_TYPE_CHECK(widget, GTK_TYPE_WIDGET, 1);
		found_widget = ui_lookup_widget(GTK_WIDGET(widget), widget_name);
		if (GTK_IS_WIDGET(found_widget))
			return pygobject_new(G_OBJECT(found_widget));
	}

	Py_RETURN_NONE;
}


static PyObject *
UiUtils_add_document_sensitive(PyObject *module, PyObject *args, PyObject *kwargs)
{
	PyObject *py_widget = NULL;
	GObject *widget = NULL;
	static gchar *kwlist[] = { "widget", NULL };

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "O", kwlist, &py_widget))
	{
		GOB_CHECK(py_widget, 1);
		widget = pygobject_get(py_widget);
		GOB_TYPE_CHECK(widget, GTK_TYPE_WIDGET, 1);
		ui_add_document_sensitive(GTK_WIDGET(widget));
	}

	Py_RETURN_NONE;
}


static PyObject *
UiUtils_button_new_with_image(PyObject *module, PyObject *args, PyObject *kwargs)
{
	const gchar *stock_id = NULL, *text = NULL;
	GtkWidget *button = NULL;
	static gchar *kwlist[] = { "stock_id", "text", NULL };

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "ss", kwlist, &stock_id, &text))
	{
		button = ui_button_new_with_image(stock_id, text);
		if (GTK_IS_WIDGET(button))
			return pygobject_new(G_OBJECT(button));
	}

	Py_RETURN_NONE;
}


static PyObject *
UiUtils_combo_box_add_to_history(PyObject *module, PyObject *args, PyObject *kwargs)
{
	PyObject *py_cbo = NULL;
	const gchar *text = NULL;
	gint hist_len = 0;
	GObject *widget = NULL;
	static gchar *kwlist[] = { "combo_entry", "text", "history_len", NULL };

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "Osi", kwlist,
		&py_cbo, &text, &hist_len))
	{
		GOB_CHECK(py_cbo, 1);
		widget = pygobject_get(py_cbo);
		GOB_TYPE_CHECK(widget, GTK_TYPE_COMBO_BOX_TEXT, 1);
		ui_combo_box_add_to_history(GTK_COMBO_BOX_TEXT(widget), text, hist_len);
	}

	Py_RETURN_NONE;
}


static PyObject *
UiUtils_dialog_vbox_new(PyObject *module, PyObject *args, PyObject *kwargs)
{
	PyObject *py_dlg;
	GObject *dlg;
	GtkWidget *widget;
	static gchar *kwlist[] = { "dialog", NULL };

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "O", kwlist, &py_dlg))
	{
		GOB_CHECK(py_dlg, 1);
		dlg = pygobject_get(py_dlg);
		GOB_TYPE_CHECK(dlg, GTK_TYPE_DIALOG, 1);
		widget = ui_dialog_vbox_new(GTK_DIALOG(dlg));
		if (GTK_IS_WIDGET(widget))
			return pygobject_new(G_OBJECT(widget));
	}

	Py_RETURN_NONE;
}


static PyObject *
UiUtils_entry_add_clear_icon(PyObject *module, PyObject *args, PyObject *kwargs)
{
	PyObject *py_ent = NULL;
	GObject *ent;
	static gchar *kwlist[] = { "entry", NULL };

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "O", kwlist, &py_ent))
	{
		GOB_CHECK(py_ent, 1);
		ent = pygobject_get(py_ent);
		GOB_TYPE_CHECK(ent, GTK_TYPE_ENTRY, 1);
		ui_entry_add_clear_icon(GTK_ENTRY(ent));
	}

	Py_RETURN_NONE;
}


static PyObject *
UiUtils_frame_new_with_alignment(PyObject *module, PyObject *args, PyObject *kwargs)
{
	const gchar *text = NULL;
	static gchar *kwlist[] = { "label_text", NULL };
	GtkWidget *alignment = NULL, *frame = NULL;
	PyObject *py_al = NULL, *py_fr = NULL, *ret;

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "s", kwlist, &text))
	{
		frame = ui_frame_new_with_alignment(text, &alignment);
		py_al = (PyObject *) pygobject_new(G_OBJECT(frame));
		py_fr = (PyObject *) pygobject_new(G_OBJECT(alignment));
		ret = Py_BuildValue("OO", py_al, py_fr);
		Py_DECREF(py_al);
		Py_DECREF(py_fr);
		return ret;
	}

	Py_RETURN_NONE;
}


static PyObject *
UiUtils_get_gtk_settings_integer(PyObject *module, PyObject *args, PyObject *kwargs)
{
	const gchar *prop_name = NULL;
	gint default_value = 0;
	static gchar *kwlist[] = { "property_name", "default_value", NULL };

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "si", kwlist, &prop_name,
		&default_value))
	{
		return PyInt_FromLong(
				(glong) ui_get_gtk_settings_integer(prop_name, default_value));
	}

	Py_RETURN_NONE;
}


static PyObject *
UiUtils_image_menu_item_new(PyObject *module, PyObject *args, PyObject *kwargs)
{
	const gchar *stock_id = NULL, *label = NULL;
	GtkWidget *ret = NULL;
	static gchar *kwlist[] = { "stock_id", "label", NULL };

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "ss", kwlist,
		&stock_id, &label))
	{
		ret = ui_image_menu_item_new(stock_id, label);
		if (GTK_IS_WIDGET(ret))
			return pygobject_new(G_OBJECT(ret));
	}

	Py_RETURN_NONE;
}


static PyObject *
UiUtils_is_keyval_enter_or_return(PyObject *module, PyObject *args, PyObject *kwargs)
{
	guint kv;
	static gchar *kwlist[] = { "keyval", NULL };

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "I", kwlist, &kv))
	{
		if (ui_is_keyval_enter_or_return(kv))
			Py_RETURN_TRUE;
		else
			Py_RETURN_FALSE;
	}

	Py_RETURN_NONE;
}


/* FIXME: ui_menu_add_document_items() and ui_menu_add_document_items_sorted()
 * skipped because I don't know how to pass GCallbacks between Python and C,
 * if it's even possible. */


 static PyObject *
 UiUtils_path_box_new(PyObject *module, PyObject *args, PyObject *kwargs)
 {
	 gint act;
	 PyObject *py_ent = NULL;
	 GObject *ent = NULL;
	 GtkWidget *pbox = NULL;
	 const gchar *title = NULL;
	 static gchar *kwlist[] = { "title", "action", "entry", NULL };

	 if (PyArg_ParseTupleAndKeywords(args, kwargs, "ziO", kwlist,
		&title, &act, &py_ent))
	{
		GOB_CHECK(py_ent, 3);
		ent = pygobject_get(py_ent);
		GOB_TYPE_CHECK(ent, GTK_TYPE_ENTRY, 3);
		pbox = ui_path_box_new(title, (GtkFileChooserAction) act, GTK_ENTRY(ent));
		if (GTK_IS_WIDGET(pbox))
			return pygobject_new(G_OBJECT(pbox));
	}


	 Py_RETURN_NONE;
 }


 static PyObject *
 UiUtils_progress_bar_start(PyObject *module, PyObject *args, PyObject *kwargs)
 {
	 const gchar *text = NULL;
	 static gchar *kwlist[] = { "text", NULL };

	 if (PyArg_ParseTupleAndKeywords(args, kwargs, "z", kwlist, &text))
		 ui_progress_bar_start(text);

	 Py_RETURN_NONE;
 }


 static PyObject *
 UiUtils_progress_bar_stop(PyObject *module)
 {
	 ui_progress_bar_stop();
	 Py_RETURN_NONE;
 }


 static PyObject *
 UiUtils_set_statusbar(PyObject *module, PyObject *args, PyObject *kwargs)
 {
	 gint log = 0;
	 const gchar *text = NULL;
	 static gchar *kwlist[] = { "text", "log", NULL };

	 if (PyArg_ParseTupleAndKeywords(args, kwargs, "s|i", kwlist, &text, &log))
		 ui_set_statusbar((gboolean) log, "%s", text);

	 Py_RETURN_NONE;
 }


 /* FIXME: ui_table_add_row() skipped since it's probably not useful and
  * not well documented. */


static PyObject *
UiUtils_widget_modify_font_from_string(PyObject *module, PyObject *args, PyObject *kwargs)
{
	PyObject *py_widget = NULL;
	GObject *widget = NULL;
	const gchar *font_str = NULL;
	static gchar *kwlist[] = { "widget", "font_str", NULL };

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "Os", kwlist, &py_widget, &font_str))
	{
		GOB_CHECK(py_widget, 1);
		widget = pygobject_get(py_widget);
		GOB_TYPE_CHECK(widget, GTK_TYPE_WIDGET, 1);
		ui_widget_modify_font_from_string(GTK_WIDGET(widget), font_str);
	}

	Py_RETURN_NONE;
}


/* Deprecated in Geany 0.21 in favour of gtk_widget_set_tooltip_text() */
#if 0
static PyObject *
UiUtils_widget_set_tooltip_text(PyObject *module, PyObject *args, PyObject *kwargs)
{
	PyObject *py_widget = NULL;
	GObject *widget = NULL;
	const gchar *text = NULL;
	static gchar *kwlist[] = { "widget", "text", NULL };

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "Os", kwlist, &py_widget, &text))
	{
		GOB_CHECK(py_widget, 1);
		widget = pygobject_get(py_widget);
		GOB_TYPE_CHECK(widget, GTK_TYPE_WIDGET, 1);
		ui_widget_set_tooltip_text(GTK_WIDGET(widget), text);
	}

	Py_RETURN_NONE;
}
#endif


static PyMethodDef UiUtilsModule_methods[] = {
	{ "hookup_widget", (PyCFunction) UiUtils_hookup_widget, METH_KEYWORDS,
		"Sets a name to lookup widget from owner." },
	{ "lookup_widget", (PyCFunction) UiUtils_lookup_widget, METH_KEYWORDS,
		"Returns a widget from a name in a component, usually created "
		"by Glade.  Call it with the toplevel widget in the component "
		"(ie. a window or dialog), or alternatively any widget in the "
		"component, and the name of the widget you want returned."},
	{ "add_document_sensitive", (PyCFunction) UiUtils_add_document_sensitive, METH_KEYWORDS,
		"Adds a widget to the list of widgets that should be set "
		"sensitive or insensitive depending if any documents are open." },
	{ "button_new_with_image", (PyCFunction) UiUtils_button_new_with_image, METH_KEYWORDS,
		"Creates a gtk.Button with custom text and a stock image similar "
		"to gtk.Button() initializer." },
	{ "combo_box_add_to_history", (PyCFunction) UiUtils_combo_box_add_to_history, METH_KEYWORDS,
		"Prepends text to the dropdown list, removing a duplicate element "
		"in the list if found.  Also ensures there are less than the "
		"specified number of elements in the history." },
	{ "dialog_vbox_new", (PyCFunction) UiUtils_dialog_vbox_new, METH_KEYWORDS,
		"Makes a fixed border for dialogs without increasing the button "
		"box border size." },
	{ "entry_add_clear_icon", (PyCFunction) UiUtils_entry_add_clear_icon, METH_KEYWORDS,
		"Adds a small clear icon to the right end of the passed in entry. "
		"A callback to clear the contents of the gtk.Entry is automatically "
		"added." },
	{ "frame_new_with_alignement", (PyCFunction) UiUtils_frame_new_with_alignment, METH_KEYWORDS,
		"Creates a GNOME HIG-style frame with no border and indented "
		"child alignment.  Returns a tuple with the gtk.Frame as the first "
		"element and the gtk.Alignment as the second element." },
	{ "get_gtk_settings_integer", (PyCFunction) UiUtils_get_gtk_settings_integer, METH_KEYWORDS,
		"Reads an integer from the GTK default settings registry." },
	{ "image_menu_item_new", (PyCFunction) UiUtils_image_menu_item_new, METH_KEYWORDS,
		"Creates a gtk.ImageMenuItem with a stock image and a custom label." },
	{ "is_keyval_enter_or_return", (PyCFunction) UiUtils_is_keyval_enter_or_return, METH_KEYWORDS,
		"Checks whether the passed in keyval is the Enter or Returns key. " },
	{ "path_box_new", (PyCFunction) UiUtils_path_box_new, METH_KEYWORDS,
		"Creates a gtk.HBox with entry packed into it and an open button "
		"which runs a file chooser, replacing entry text (if successful) "
		"with the path returned from the gtk.FileChooser." },
	{ "progress_bar_start", (PyCFunction) UiUtils_progress_bar_start, METH_KEYWORDS,
		"Starts a constantly pulsing progressbar in the right corner of "
		"the statusbar (if the status bar is visible)." },
	{ "progress_bar_stop", (PyCFunction) UiUtils_progress_bar_stop, METH_NOARGS,
		"Stops a running progress bar and hides the widget again." },
	{ "set_statusbar", (PyCFunction) UiUtils_set_statusbar, METH_KEYWORDS,
		"Displays text on the statusbar." },
	{ "widget_modify_font_from_string", (PyCFunction) UiUtils_widget_modify_font_from_string, METH_KEYWORDS,
		"Modifies the font of a widget using modify_font() automatically "
		"parsing the Pango font description string in font_str." },
	{ NULL },
};


PyMODINIT_FUNC initui_utils(void)
{
	PyObject *m;

	init_pygobject();
	init_pygtk();
	m = PyImport_ImportModule("gobject");

	if (m)
	{
		PyGObject_Type = (PyTypeObject *) PyObject_GetAttrString(m, "GObject");
		Py_XDECREF(m);
	}

	InterfacePrefsType.tp_new = PyType_GenericNew;
	if (PyType_Ready(&InterfacePrefsType) < 0)
		return;

	MainWidgetsType.tp_new = PyType_GenericNew;
	if (PyType_Ready(&MainWidgetsType) < 0)
		return;

	m = Py_InitModule3("ui_utils", UiUtilsModule_methods,
			"User interface information and utilities.");

	Py_INCREF(&InterfacePrefsType);
	PyModule_AddObject(m, "InterfacePrefs", (PyObject *) &InterfacePrefsType);

	Py_INCREF(&MainWidgetsType);
	PyModule_AddObject(m, "MainWidgets", (PyObject *) &MainWidgetsType);
}
