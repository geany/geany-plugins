#if defined(HAVE_CONFIG_H) && !defined(GEANYPY_WINDOWS)
# include "config.h"
#endif

#include "geanypy.h"


static PyObject *
Encodings_convert_to_utf8(PyObject *module, PyObject *args, PyObject *kwargs)
{
    gchar *buffer = NULL, *used_encoding = NULL, *new_buffer = NULL;
    gssize size = -1; /* bug alert: this is gsize in Geany for some reason */
    PyObject *result;
    static gchar *kwlist[] = { "buffer", "size", NULL };

    if (PyArg_ParseTupleAndKeywords(args, kwargs, "s|l", kwlist, &buffer, &size))
    {
        new_buffer = encodings_convert_to_utf8(buffer, size, &used_encoding);
        if (new_buffer != NULL)
        {
            result = Py_BuildValue("ss", new_buffer, used_encoding);
            g_free(new_buffer);
            g_free(used_encoding);
            return result;
        }
    }

    Py_RETURN_NONE;
}


static PyObject *
Encodings_convert_to_utf8_from_charset(PyObject *module, PyObject *args, PyObject *kwargs)
{
    gchar *buffer = NULL, *charset = NULL, *new_buffer = NULL;
    gssize size = -1;
    gint fast = 0;
    PyObject *result;
    static gchar *kwlist[] = { "buffer", "size", "charset", "fast", NULL };

    if (PyArg_ParseTupleAndKeywords(args, kwargs, "ss|li", kwlist, &buffer,
		&charset, &size, &fast))
    {
        new_buffer = encodings_convert_to_utf8_from_charset(buffer, size, charset, fast);
        if (new_buffer != NULL)
        {
            result = Py_BuildValue("s", new_buffer);
            g_free(new_buffer);
            return result;
        }
    }

    Py_RETURN_NONE;
}


static PyObject *
Encodings_get_charset_from_index(PyObject *module, PyObject *args, PyObject *kwargs)
{
    gint idx = 0;
    const gchar *charset = NULL;
    static gchar *kwlist[] = { "index", NULL };

    if (PyArg_ParseTupleAndKeywords(args, kwargs, "i", kwlist, &idx))
    {
        charset = encodings_get_charset_from_index(idx);
        if (charset != NULL)
            return Py_BuildValue("s", charset);
    }

    Py_RETURN_NONE;
}


static const gchar *encoding_names[] = {
	"ISO_8859_1",
	"ISO_8859_2",
	"ISO_8859_3",
	"ISO_8859_4",
	"ISO_8859_5",
	"ISO_8859_6",
	"ISO_8859_7",
	"ISO_8859_8",
	"ISO_8859_8_I",
	"ISO_8859_9",
	"ISO_8859_10",
	"ISO_8859_13",
	"ISO_8859_14",
	"ISO_8859_15",
	"ISO_8859_16",

	"UTF_7",
	"UTF_8",
	"UTF_16LE",
	"UTF_16BE",
	"UCS_2LE",
	"UCS_2BE",
	"UTF_32LE",
	"UTF_32BE",

	"ARMSCII_8",
	"BIG5",
	"BIG5_HKSCS",
	"CP_866",

	"EUC_JP",
	"EUC_KR",
	"EUC_TW",

	"GB18030",
	"GB2312",
	"GBK",
	"GEOSTD8",
	"HZ",

	"IBM_850",
	"IBM_852",
	"IBM_855",
	"IBM_857",
	"IBM_862",
	"IBM_864",

	"ISO_2022_JP",
	"ISO_2022_KR",
	"ISO_IR_111",
	"JOHAB",
	"KOI8_R",
	"KOI8_U",

	"SHIFT_JIS",
	"TCVN",
	"TIS_620",
	"UHC",
	"VISCII",

	"WINDOWS_1250",
	"WINDOWS_1251",
	"WINDOWS_1252",
	"WINDOWS_1253",
	"WINDOWS_1254",
	"WINDOWS_1255",
	"WINDOWS_1256",
	"WINDOWS_1257",
	"WINDOWS_1258",

	"NONE",
	"CP_932",

	"MAX"
};


static PyObject *
Encodings_get_list(PyObject *module)
{
	int i;
	PyObject *list;
	list = PyList_New(0);
	for (i = 0; i < GEANY_ENCODINGS_MAX; i++)
		PyList_Append(list, PyString_FromString(encoding_names[i]));
	return list;
}


static
PyMethodDef EncodingsModule_methods[] = {
    {
		"convert_to_utf8",
		(PyCFunction)Encodings_convert_to_utf8, METH_KEYWORDS,
		"Tries to convert the supplied buffer to UTF-8 encoding.  Returns "
		"the converted buffer and the encoding that was used."
	},
    {
		"convert_to_utf8_from_charset",
		(PyCFunction)Encodings_convert_to_utf8_from_charset, METH_KEYWORDS,
		"Tries to convert the supplied buffer to UTF-8 from the supplied "
		"charset.  If the fast parameter is not False (default), additional "
		"checks to validate the converted string are performed."
	},
    {
		"get_charset_from_index",
		(PyCFunction)Encodings_get_charset_from_index, METH_KEYWORDS,
		"Gets the character set name of the specified index."
	},
	{
		"get_list",
		(PyCFunction) Encodings_get_list, METH_NOARGS,
		"Gets a list of all supported encodings."
	},
    { NULL }
};


PyMODINIT_FUNC
initencoding(void)
{
	int i;
    PyObject *m;

    m = Py_InitModule3("encoding", EncodingsModule_methods,
			"Encoding conversion functions.");

	for (i = 0; i < GEANY_ENCODINGS_MAX; i++)
		PyModule_AddIntConstant(m, encoding_names[i], (glong) i);
}
