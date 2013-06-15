#include "geanypy.h"


static PyObject *
Msgwin_clear_tab(PyObject *module, PyObject *args, PyObject *kwargs)
{
    gint tab_num = 0;
    static gchar *kwlist[] = { "tabnum", NULL };

    if (PyArg_ParseTupleAndKeywords(args, kwargs, "i", kwlist, &tab_num))
        msgwin_clear_tab(tab_num);

    Py_RETURN_NONE;
}


static PyObject *
Msgwin_compiler_add(PyObject *module, PyObject *args, PyObject *kwargs)
{
    gint msg_color = COLOR_BLACK;
    gchar *msg = NULL;
    static gchar *kwlist[] = { "msg", "msg_color", NULL };

    if (PyArg_ParseTupleAndKeywords(args, kwargs, "s|i", kwlist, &msg, &msg_color))
        msgwin_compiler_add(msg_color, "%s", msg);

    Py_RETURN_NONE;
}


static PyObject *
Msgwin_msg_add(PyObject *module, PyObject *args, PyObject *kwargs)
{
    gint msg_color = COLOR_BLACK, line = -1;
    PyObject *obj = NULL;
    Document *py_doc = NULL;
    GeanyDocument *doc = NULL;
    gchar *msg = NULL;
    static gchar *kwlist[] = { "msg", "msg_color", "line", "doc", NULL };

    if (PyArg_ParseTupleAndKeywords(args, kwargs, "s|iiO", kwlist, &msg,
		&msg_color, &line, &obj))
    {
        if (obj == NULL || obj == Py_None)
            doc = NULL;
        else
        {
            py_doc = (Document *) obj;
            doc = py_doc->doc;
        }
        msgwin_msg_add(msg_color, line, doc, "%s", msg);
    }

    Py_RETURN_NONE;
}


static PyObject *
Msgwin_set_messages_dir(PyObject *module, PyObject *args, PyObject *kwargs)
{
    gchar *msgdir = NULL;
    static gchar *kwlist[] = { "messages_dir", NULL };

    if (PyArg_ParseTupleAndKeywords(args, kwargs, "s", kwlist, &msgdir))
    {
        if (msgdir != NULL)
            msgwin_set_messages_dir(msgdir);
    }

    Py_RETURN_NONE;
}


static PyObject *
Msgwin_status_add(PyObject *module, PyObject *args, PyObject *kwargs)
{
    gchar *msg = NULL;
    static gchar *kwlist[] = { "msg", NULL };

    if (PyArg_ParseTupleAndKeywords(args, kwargs, "s", kwlist, &msg))
    {
        if (msg != NULL)
            msgwin_status_add("%s", msg);
    }

    Py_RETURN_NONE;
}


static PyObject *
Msgwin_switch_tab(PyObject *module, PyObject *args, PyObject *kwargs)
{
    gint tabnum = 0, show = 0;
	static gchar *kwlist[] = { "tabnum", "show", NULL };

    if (PyArg_ParseTupleAndKeywords(args, kwargs, "i|i", kwlist, &tabnum, &show))
        msgwin_switch_tab(tabnum, show);

    Py_RETURN_NONE;
}


static
PyMethodDef MsgwinModule_methods[] = {
    { "clear_tab", (PyCFunction) Msgwin_clear_tab, METH_KEYWORDS },
    { "compiler_add", (PyCFunction) Msgwin_compiler_add, METH_KEYWORDS },
    { "msg_add", (PyCFunction) Msgwin_msg_add, METH_KEYWORDS },
    { "set_messages_dir", (PyCFunction) Msgwin_set_messages_dir, METH_KEYWORDS },
    { "status_add", (PyCFunction) Msgwin_status_add, METH_KEYWORDS },
    { "switch_tab", (PyCFunction) Msgwin_switch_tab, METH_KEYWORDS },
    { NULL }
};


PyMODINIT_FUNC
initmsgwin(void)
{
    PyObject *m;

    m = Py_InitModule3("msgwindow", MsgwinModule_methods,
			"Message windows information and management.");

	PyModule_AddIntConstant(m, "COLOR_RED", COLOR_RED);
	PyModule_AddIntConstant(m, "COLOR_DARK_RED", COLOR_DARK_RED);
	PyModule_AddIntConstant(m, "COLOR_BLACK", COLOR_BLACK);
	PyModule_AddIntConstant(m, "COLOR_BLUE", COLOR_BLUE);

    PyModule_AddIntConstant(m, "TAB_STATUS", MSG_STATUS);
    PyModule_AddIntConstant(m, "TAB_COMPILER", MSG_COMPILER);
    PyModule_AddIntConstant(m, "TAB_MESSAGE", MSG_MESSAGE);
    PyModule_AddIntConstant(m, "TAB_SCRIBBLE", MSG_SCRATCH);
    PyModule_AddIntConstant(m, "TAB_TERMINAL", MSG_VTE);
}
