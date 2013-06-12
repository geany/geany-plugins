#include "geanypy.h"


static PyObject *
Navqueue_goto_line(PyObject *module, PyObject *args, PyObject *kwargs)
{
	gint line = 1;
	PyObject *py_old = NULL, *py_new = NULL;
	Document *py_doc_old, *py_doc_new;
	GeanyDocument *old_doc, *new_doc;
	static gchar *kwlist[] = { "old_doc", "new_doc", "line", NULL };

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "OOi", kwlist, &py_old,
		&py_new, &line))
	{
		if (!py_old || py_old == Py_None)
			old_doc = NULL;
		else
		{
			py_doc_old = (Document *) py_old;
			old_doc = py_doc_old->doc;
		}

		if (!py_new || py_new == Py_None)
			Py_RETURN_NONE;
		else
		{
			py_doc_new = (Document *) py_new;
			new_doc = py_doc_new->doc;
		}

		if ( (old_doc != NULL && !DOC_VALID(old_doc)) || !DOC_VALID(new_doc) )
			Py_RETURN_NONE;
		if (navqueue_goto_line(old_doc, new_doc, line))
			Py_RETURN_TRUE;
		else
			Py_RETURN_FALSE;
	}

	Py_RETURN_NONE;
}


#ifdef ENABLE_PRIVATE
static PyObject *
Navqueue_go_back(PyObject *module)
{
	navqueue_go_back();
	Py_RETURN_NONE;
}


static PyObject *
Navqueue_go_forward(PyObject *module)
{
	navqueue_go_forward();
	Py_RETURN_NONE;
}
#endif


static
PyMethodDef NavqueueModule_methods[] = {
	{ "goto_line", (PyCFunction) Navqueue_goto_line, METH_KEYWORDS,
		"Adds old file position and new file position to the navqueue, "
		"then goes to the new position." },
#ifdef ENABLE_PRIVATE
	{ "go_back", (PyCFunction) Navqueue_go_back, METH_NOARGS,
		"Navigate backwards." },
	{ "go_forward", (PyCFunction) Navqueue_go_forward, METH_NOARGS,
		"Navigate forward." },
#endif
	{ NULL }
};


PyMODINIT_FUNC initnavqueue(void)
{
	Py_InitModule3("navqueue", NavqueueModule_methods, "Simple code navigation.");
}
