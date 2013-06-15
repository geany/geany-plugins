#include "geanypy.h"


static void
Project_dealloc(Project *self)
{
	g_return_if_fail(self != NULL);
	self->ob_type->tp_free((PyObject *) self);
}


static int
Project_init(Project *self, PyObject *args, PyObject *kwds)
{
	g_return_val_if_fail(self != NULL, -1);
	self->project = geany_data->app->project;
	return 0;
}


static PyObject *
Project_get_property(Project *self, const gchar *prop_name)
{
	g_return_val_if_fail(self != NULL, NULL);
	g_return_val_if_fail(prop_name != NULL, NULL);

	if (!self->project)
		Py_RETURN_NONE;

	if (g_str_equal(prop_name, "base_path") && self->project->base_path)
		return PyString_FromString(self->project->base_path);
	else if (g_str_equal(prop_name, "description") && self->project->description)
		return PyString_FromString(self->project->description);
	else if (g_str_equal(prop_name, "file_name") && self->project->file_name)
		return PyString_FromString(self->project->file_name);
	else if (g_str_equal(prop_name, "file_patterns") && self->project->file_patterns)
	{
		guint i, len;
		PyObject *set;
		len = g_strv_length(self->project->file_patterns);
		set = PyFrozenSet_New(NULL);
		for (i = 0; i < len; i++)
			PySet_Add(set, PyString_FromString(self->project->file_patterns[i]));
		return set;
	}
	else if (g_str_equal(prop_name, "name") && self->project->name)
		return PyString_FromString(self->project->name);
	else if (g_str_equal(prop_name, "type") && self->project->type)
		return Py_BuildValue("i", self->project->type);

	Py_RETURN_NONE;
}
GEANYPY_PROPS_READONLY(Project);


static PyGetSetDef Project_getseters[] = {
	GEANYPY_GETSETDEF(Project, "base_path",
		"Base path of the project directory (maybe relative)."),
	GEANYPY_GETSETDEF(Project, "description",
		"Short description of the project."),
	GEANYPY_GETSETDEF(Project, "file_name",
		"Where the project file is stored."),
	GEANYPY_GETSETDEF(Project, "file_patterns",
		"Sequence of filename extension patterns."),
	GEANYPY_GETSETDEF(Project, "name",
		"The name of the project."),
	GEANYPY_GETSETDEF(Project, "type",
		"Identifier whether it is a pure Geany project or modified/"
		"extended by a plugin."),
	{ NULL }
};


PyTypeObject ProjectType = {
	PyObject_HEAD_INIT(NULL)
	0,											/* ob_size */
	"geany.project.Project",					/* tp_name */
	sizeof(Project),							/* tp_basicsize */
	0,											/* tp_itemsize */
	(destructor) Project_dealloc,				/* tp_dealloc */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* tp_print - tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,	/* tp_flags */
	"Wrapper around a GeanyProject structure.",	/* tp_doc */
	0, 0, 0, 0, 0, 0, 0, 0,						/* tp_traverse - tp_members */
	Project_getseters,							/* tp_getset */
	0, 0, 0, 0, 0,								/* tp_base - tp_dictoffset */
	(initproc) Project_init,					/* tp_init */
	0, 0,										/* tp_alloc - tp_new */
};


static PyMethodDef ProjectModule_methods[] = { { NULL } };


PyMODINIT_FUNC initproject(void)
{
	PyObject *m;

	ProjectType.tp_new = PyType_GenericNew;
	if (PyType_Ready(&ProjectType) < 0)
		return;

	m = Py_InitModule3("project", ProjectModule_methods, "Project information");

	Py_INCREF(&ProjectType);
	PyModule_AddObject(m, "Project", (PyObject *)&ProjectType);
}
