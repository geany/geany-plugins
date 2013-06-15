#include "geanypy.h"


static void
NotifyHeader_dealloc(NotifyHeader *self)
{
	self->ob_type->tp_free((PyObject *) self);
}


static int
NotifyHeader_init(NotifyHeader *self, PyObject *args, PyObject *kwds)
{
	self->notif = NULL;
	return 0;
}


static PyObject *
NotifyHeader_get_property(NotifyHeader *self, const gchar *prop_name)
{
	g_return_val_if_fail(self != NULL, NULL);
	g_return_val_if_fail(prop_name != NULL, NULL);

	if (!self->notif)
	{
		PyErr_SetString(PyExc_RuntimeError,
			"NotifyHeader instance not initialized properly");
		return NULL;
	}

	if (g_str_equal(prop_name, "hwnd_from"))
		return PyLong_FromVoidPtr(self->notif->nmhdr.hwndFrom);
	else if (g_str_equal(prop_name, "id_from"))
		return PyLong_FromLong(self->notif->nmhdr.idFrom);
	else if (g_str_equal(prop_name, "code"))
		return PyInt_FromLong((glong) self->notif->nmhdr.code);

	Py_RETURN_NONE;
}
GEANYPY_PROPS_READONLY(NotifyHeader);


static PyGetSetDef NotifyHeader_getseters[] = {
	GEANYPY_GETSETDEF(NotifyHeader, "hwnd_from", ""),
	GEANYPY_GETSETDEF(NotifyHeader, "id_from", ""),
	GEANYPY_GETSETDEF(NotifyHeader, "code", ""),
	{ NULL }
};


PyTypeObject NotifyHeaderType = {
	PyObject_HEAD_INIT(NULL)
	0,												/* ob_size */
	"geany.scintilla.NotifyHeader",					/* tp_name */
	sizeof(NotifyHeader),							/* tp_basicsize */
	0,												/* tp_itemsize */
	(destructor) NotifyHeader_dealloc,				/* tp_dealloc */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* tp_print - tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,		/* tp_flags */
	"Wrapper around a NotifyHeader structure.",		/* tp_doc */
	0, 0, 0, 0, 0, 0, 0, 0,							/* tp_traverse - tp_members */
	NotifyHeader_getseters,							/* tp_getset */
	0, 0, 0, 0, 0,									/* tp_base - tp_dictoffset */
	(initproc) NotifyHeader_init,					/* tp_init */
	0, 0,											/* tp_alloc - tp_new */

};


NotifyHeader *NotifyHeader_create_new_from_scintilla_notification(SCNotification *notif)
{
	NotifyHeader *self;
	self = (NotifyHeader *) PyObject_CallObject((PyObject *) &NotifyHeaderType, NULL);
	self->notif = notif;
	return self;
}
