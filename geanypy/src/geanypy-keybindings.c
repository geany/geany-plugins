/*
 * plugin.c
 *
 * Copyright 2015 Thomas Martitz <kugel@rockbox.org>
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


#include "geanypy.h"
#include "geanypy-keybindings.h"

#include <glib.h>

static gboolean call_key(gpointer *unused, guint key_id, gpointer data)
{
	PyObject *callback = data;
	PyObject *args;

	args = Py_BuildValue("(i)", key_id);
	PyObject_CallObject(callback, args);
	Py_DECREF(args);
}


/* plugin.py provides an OOP-style wrapper around this so call it like:
 * class Foo(geany.Plugin):
 *   def __init__(self):
 *     self.set_key_group(...)
 */
static PyObject *
Keybindings_set_key_group(PyObject *self, PyObject *args, PyObject *kwargs)
{
	static gchar *kwlist[] = { "plugin", "section_name", "count", "callback", NULL };
	int count = 0;
	const gchar *section_name = NULL;
	GeanyKeyGroup *group = NULL;
	PyObject *py_callback = NULL;
	PyObject *py_ret = Py_None;
	PyObject *py_plugin;
	gboolean has_cb = FALSE;

	Py_INCREF(Py_None);

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "Osi|O", kwlist,
		&py_plugin, &section_name, &count, &py_callback))
	{
		GeanyPlugin *plugin = plugin_get(py_plugin);
		g_return_val_if_fail(plugin != NULL, Py_None);

		has_cb = PyCallable_Check(py_callback);
		if (has_cb)
		{
			Py_INCREF(py_callback);
			group = plugin_set_key_group_full(plugin, section_name, count,
			                                  (GeanyKeyGroupFunc) call_key, py_callback,
			                                  (GDestroyNotify) Py_DecRef);
		}
		else
			group = plugin_set_key_group(plugin, section_name, count, NULL);
	}

	if (group)
	{
		Py_DECREF(py_ret);
		py_ret = KeyGroup_new_with_geany_key_group(group, has_cb);
	}

	return py_ret;
}


static PyObject *
KeyGroup_add_key_item(KeyGroup *self, PyObject *args, PyObject *kwargs)
{
	static gchar *kwlist[] = { "name", "label", "callback", "key_id", "key", "mod" , "menu_item", NULL };
	int id = -1;
	int key = 0, mod = 0;
	const gchar *name = NULL, *label = NULL;
	PyObject *py_menu_item = NULL;
	PyObject *py_callback  = NULL;
	GeanyKeyBinding *item = NULL;

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "ss|OiiiO", kwlist,
		&name, &label, &py_callback, &id, &key, &mod, &py_menu_item))
	{
		if (id == -1)
			id = self->item_index;

		GtkWidget *menu_item = (py_menu_item == NULL || py_menu_item == Py_None)
									? NULL : GTK_WIDGET(pygobject_get(py_menu_item));
		if (PyCallable_Check(py_callback))
		{
			Py_INCREF(py_callback);
			item = keybindings_set_item_full(self->kb_group, id, (guint) key,
			                                 (GdkModifierType) mod, name, label, menu_item,
			                                 (GeanyKeyBindingFunc) call_key, py_callback,
			                                 (GDestroyNotify) Py_DecRef);
		}
		else
		{
			if (!self->has_cb)
				g_warning("Either KeyGroup or the Keybinding must have a callback\n");
			else
				item = keybindings_set_item(self->kb_group, id, NULL, (guint) key,
				                            (GdkModifierType) mod, name, label, menu_item);
		}
		Py_XDECREF(py_menu_item);

		self->item_index = id + 1;
	}

	if (item)
	{
		/* Return a tuple containing the key group and the opaque GeanyKeyBinding pointer.
		 * This is in preparation of allowing chained calls like
		 * set_kb_group(X, 3).add_key_item().add_key_item().add_key_item()
		 * without losing access to the keybinding pointer (might become necessary for newer
		 * Geany APIs).
		 * Note that the plain tuple doesn't support the above yet, we've got to subclass it,
		 * but we are prepared without breaking sub-plugins */
		PyObject *ret = PyTuple_Pack(2, self, PyCapsule_New(item, "GeanyKeyBinding", NULL));
		return ret;
	}
	Py_RETURN_NONE;
}


static PyMethodDef
KeyGroup_methods[] = {
	{ "add_key_item",				(PyCFunction)KeyGroup_add_key_item,	METH_KEYWORDS,
		"Adds an action to the plugin's key group" },
	{ NULL }
};

static PyMethodDef
Keybindings_methods[] = {
	{ "set_key_group",				(PyCFunction)Keybindings_set_key_group,	METH_KEYWORDS,
		"Sets up a GeanyKeybindingGroup for this plugin." },
	{ NULL }
};


static PyGetSetDef
KeyGroup_getseters[] = {
	{ NULL },
};


static void
KeyGroup_dealloc(KeyGroup *self)
{
	g_return_if_fail(self != NULL);
	self->ob_type->tp_free((PyObject *) self);
}


static PyTypeObject KeyGroupType = {
	PyObject_HEAD_INIT(NULL)
	0,											/* ob_size */
	"geany.keybindings.KeyGroup",					/* tp_name */
	sizeof(KeyGroup),								/* tp_basicsize */
	0,											/* tp_itemsize */
	(destructor) KeyGroup_dealloc,				/* tp_dealloc */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* tp_print - tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,	/* tp_flags */
	"Wrapper around a GeanyKeyGroup structure."	,/* tp_doc */
	0, 0, 0, 0, 0, 0,							/* tp_traverse - tp_iternext */
	KeyGroup_methods,							/* tp_methods */
	0,											/* tp_members */
	KeyGroup_getseters,							/* tp_getset */
	0, 0, 0, 0, 0,								/* tp_base - tp_dictoffset */
	0, 0, (newfunc) PyType_GenericNew,			/* tp_init - tp_alloc, tp_new */
};


PyMODINIT_FUNC initkeybindings(void)
{
	PyObject *m;

	if (PyType_Ready(&KeyGroupType) < 0)
		return;

	m = Py_InitModule3("keybindings", Keybindings_methods, "Keybindings support.");

	Py_INCREF(&KeyGroupType);
	PyModule_AddObject(m, "KeyGroup", (PyObject *)&KeyGroupType);
}

PyObject *KeyGroup_new_with_geany_key_group(GeanyKeyGroup *group, gboolean has_cb)
{
	KeyGroup *ret = PyObject_New(KeyGroup, &KeyGroupType);

	ret->kb_group = group;
	ret->has_cb = has_cb;
	ret->item_index = 0;

	return (PyObject *) ret;
}

