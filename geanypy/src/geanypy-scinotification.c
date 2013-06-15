#include "geanypy.h"


static void
Notification_dealloc(Notification *self)
{
	Py_XDECREF(self->hdr);
	self->ob_type->tp_free((PyObject *) self);
}


static int
Notification_init(Notification *self, PyObject *args, PyObject *kwds)
{
	self->notif = NULL;
	self->hdr = NULL;
	return 0;
}


static PyObject *
Notification_get_property(Notification *self, const gchar *prop_name)
{
	g_return_val_if_fail(self != NULL, NULL);
	g_return_val_if_fail(prop_name != NULL, NULL);

	if (!self->notif)
	{
		PyErr_SetString(PyExc_RuntimeError,
			"Notification instance not initialized properly");
		return NULL;
	}

	if (g_str_equal(prop_name, "nmhdr"))
	{
		Py_INCREF(self->hdr);
		return (PyObject *) self->hdr;
	}
	else if (g_str_equal(prop_name, "position"))
		return PyInt_FromLong((glong) self->notif->position);
	else if (g_str_equal(prop_name, "ch"))
		return Py_BuildValue("c", self->notif->ch);
	else if (g_str_equal(prop_name, "modifiers"))
		return PyInt_FromLong((glong) self->notif->modifiers);
	else if (g_str_equal(prop_name, "modification_type"))
		return PyInt_FromLong((glong) self->notif->modificationType);
	else if (g_str_equal(prop_name, "text"))
		return PyString_FromString(self->notif->text);
	else if (g_str_equal(prop_name, "length"))
		return PyInt_FromLong((glong) self->notif->length);
	else if (g_str_equal(prop_name, "lines_added"))
		return PyInt_FromLong((glong) self->notif->linesAdded);
	else if (g_str_equal(prop_name, "message"))
		return PyInt_FromLong((glong) self->notif->message);
	else if (g_str_equal(prop_name, "w_param"))
		return PyLong_FromLong(self->notif->wParam);
	else if (g_str_equal(prop_name, "l_param"))
		return PyLong_FromLong(self->notif->lParam);
	else if (g_str_equal(prop_name, "line"))
		return PyInt_FromLong((glong) self->notif->line);
	else if (g_str_equal(prop_name, "fold_level_now"))
		return PyInt_FromLong((glong) self->notif->foldLevelNow);
	else if (g_str_equal(prop_name, "fold_level_prev"))
		return PyInt_FromLong((glong) self->notif->foldLevelPrev);
	else if (g_str_equal(prop_name, "margin"))
		return PyInt_FromLong((glong) self->notif->margin);
	else if (g_str_equal(prop_name, "list_type"))
		return PyInt_FromLong((glong) self->notif->listType);
	else if (g_str_equal(prop_name, "x"))
		return PyInt_FromLong((glong) self->notif->x);
	else if (g_str_equal(prop_name, "y"))
		return PyInt_FromLong((glong) self->notif->y);
	else if (g_str_equal(prop_name, "token"))
		return PyInt_FromLong((glong) self->notif->token);
	else if (g_str_equal(prop_name, "annotation_lines_added"))
		return PyInt_FromLong((glong) self->notif->annotationLinesAdded);
	else if (g_str_equal(prop_name, "updated"))
		return PyInt_FromLong((glong) self->notif->updated);

	Py_RETURN_NONE;
}
GEANYPY_PROPS_READONLY(Notification);


static PyGetSetDef Notification_getseters[] = {
	GEANYPY_GETSETDEF(Notification, "nmhdr", ""),
	GEANYPY_GETSETDEF(Notification, "position", ""),
	GEANYPY_GETSETDEF(Notification, "ch", ""),
	GEANYPY_GETSETDEF(Notification, "modifiers", ""),
	GEANYPY_GETSETDEF(Notification, "modification_type", ""),
	GEANYPY_GETSETDEF(Notification, "text", ""),
	GEANYPY_GETSETDEF(Notification, "length", ""),
	GEANYPY_GETSETDEF(Notification, "lines_added", ""),
	GEANYPY_GETSETDEF(Notification, "message", ""),
	GEANYPY_GETSETDEF(Notification, "w_param", ""),
	GEANYPY_GETSETDEF(Notification, "l_param", ""),
	GEANYPY_GETSETDEF(Notification, "line", ""),
	GEANYPY_GETSETDEF(Notification, "fold_level_now", ""),
	GEANYPY_GETSETDEF(Notification, "fold_level_prev", ""),
	GEANYPY_GETSETDEF(Notification, "margin", ""),
	GEANYPY_GETSETDEF(Notification, "list_type", ""),
	GEANYPY_GETSETDEF(Notification, "x", ""),
	GEANYPY_GETSETDEF(Notification, "y", ""),
	GEANYPY_GETSETDEF(Notification, "token", ""),
	GEANYPY_GETSETDEF(Notification, "annotation_lines_added", ""),
	GEANYPY_GETSETDEF(Notification, "updated", ""),
	{ NULL }
};


PyTypeObject NotificationType = {
	PyObject_HEAD_INIT(NULL)
	0,												/* ob_size */
	"geany.scintilla.Notification",					/* tp_name */
	sizeof(Notification),								/* tp_basicsize */
	0,												/* tp_itemsize */
	(destructor) Notification_dealloc,				/* tp_dealloc */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* tp_print - tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,		/* tp_flags */
	"Wrapper around a SCNotification structure.",	/* tp_doc */
	0, 0, 0, 0, 0, 0, 0, 0,							/* tp_traverse - tp_members */
	Notification_getseters,							/* tp_getset */
	0, 0, 0, 0, 0,									/* tp_base - tp_dictoffset */
	(initproc) Notification_init,					/* tp_init */
	0, 0,											/* tp_alloc - tp_new */

};


Notification *Notification_create_new_from_scintilla_notification(SCNotification *notif)
{
	Notification *self;
	self = (Notification *) PyObject_CallObject((PyObject *) &NotificationType, NULL);
	self->notif = notif;
	self->hdr = NotifyHeader_create_new_from_scintilla_notification(self->notif);
	return self;
}
