#include "geanypy.h"


static PyObject *
Dialogs_show_input(PyObject *self, PyObject *args, PyObject *kwargs)
{
    const gchar *title=NULL, *label_text=NULL, *default_text=NULL, *result=NULL;
    PyObject *py_win_obj = NULL;
    PyGObject *py_win_gobj;
    GtkWindow *win;
	static gchar *kwlist[] = { "title", "parent", "label_text", "default_text", NULL };

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "|zOzz", kwlist,
		&title, &py_win_obj, &label_text, &default_text))
	{
        if (title == NULL)
            title = "";

        if (py_win_obj != NULL && py_win_obj != Py_None)
        {
            py_win_gobj = (PyGObject *) py_win_obj;
            win = GTK_WINDOW((GObject *) py_win_gobj->obj);
        }
        else
            win = GTK_WINDOW(geany->main_widgets->window);

        result = dialogs_show_input(title, win, label_text, default_text);
        if (result != NULL)
            return PyString_FromString(result);
    }

    Py_RETURN_NONE;
}


static PyObject *
Dialogs_show_input_numeric(PyObject *self, PyObject *args, PyObject *kwargs)
{
    const gchar *title=NULL, *label_text = NULL;
    gdouble value = 0.0, min = 0.0, max = 0.0, step = 0.0;
    static gchar *kwlist[] = { "title", "label_text", "value", "minimum",
		"maximum", "step", NULL };

    if (PyArg_ParseTupleAndKeywords(args, kwargs, "|zzdddd", kwlist,
		&title, &label_text, &value, &min, &max, &step))
    {
        if (title == NULL)
            title = "";

        if (label_text == NULL)
            label_text = "";

        if (dialogs_show_input_numeric(title, label_text, &value, min, max, step))
            return PyFloat_FromDouble(value);
    }

    Py_RETURN_NONE;
}


static PyObject *
Dialogs_show_msgbox(PyObject *self, PyObject *args, PyObject *kwargs)
{
    gchar *text = NULL;
    gint msgtype = (gint) GTK_MESSAGE_INFO;
    static gchar *kwlist[] = { "text", "msgtype", NULL };

    if (PyArg_ParseTupleAndKeywords(args, kwargs, "s|i", kwlist, &text, &msgtype))
    {
        if (text != NULL)
        {
            dialogs_show_msgbox((GtkMessageType) msgtype, "%s", text);
            Py_RETURN_TRUE;
        }
    }
    Py_RETURN_NONE;
}


static PyObject *
Dialogs_show_question(PyObject *self, PyObject *args, PyObject *kwargs)
{
    gchar *text = NULL;
    static gchar *kwlist[] = { "text", NULL };

    if (PyArg_ParseTupleAndKeywords(args, kwargs, "s", kwlist, &text))
    {
        if (text != NULL)
        {
            if (dialogs_show_question("%s", text))
                Py_RETURN_TRUE;
            else
                Py_RETURN_FALSE;
        }
    }
    Py_RETURN_NONE;
}


static PyObject *
Dialogs_show_save_as(PyObject *self)
{
    if (dialogs_show_save_as())
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}


static
PyMethodDef DialogsModule_methods[] = {
    { "show_input",			(PyCFunction) Dialogs_show_input,			METH_KEYWORDS,
		"Asks the user for input." },
    { "show_input_numeric",	(PyCFunction) Dialogs_show_input_numeric,	METH_KEYWORDS,
		"Shows an input box to enter a numerical value." },
    { "show_msgbox",		(PyCFunction) Dialogs_show_msgbox,			METH_KEYWORDS,
		"Shows a message box of the specified type.  See "
		"gtk.MESSAGE_TYPE_* constants."},
    { "show_question",		(PyCFunction) Dialogs_show_question,		METH_KEYWORDS,
		"Shows a question message box with Yes/No buttons." },
    { "show_save_as",		(PyCFunction) Dialogs_show_save_as,			METH_NOARGS,
		"Shows the Save As dialog for the current notebook." },
    { NULL }
};


PyMODINIT_FUNC initdialogs(void)
{
    Py_InitModule3("dialogs", DialogsModule_methods,
		"Wrappers around Geany's dialog functions.");
}
