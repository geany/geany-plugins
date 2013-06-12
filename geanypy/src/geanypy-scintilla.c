#include "geanypy.h"


/* Bail-out when ScintillaObject being wrapped is NULL. */
#define SCI_RET_IF_FAIL(obj) { \
	if (!self->sci) { \
		PyErr_SetString(PyExc_RuntimeError, \
			"Scintilla instance not initialized properly."); \
		Py_RETURN_NONE; } }


static void
Scintilla_dealloc(Scintilla *self)
{
	self->ob_type->tp_free((PyObject *) self);
}


static int
Scintilla_init(Scintilla *self)
{
	self->sci = NULL;
	return 0;
}


static PyObject *
Scintilla_get_property(Scintilla *self, const gchar *prop_name)
{
	g_return_val_if_fail(self != NULL, NULL);
	g_return_val_if_fail(prop_name != NULL, NULL);

	if (!self->sci)
	{
		PyErr_SetString(PyExc_RuntimeError,
			"Scintilla instance not initialized properly");
		return NULL;
	}

	if (g_str_equal(prop_name, "widget"))
		return pygobject_new(G_OBJECT(self->sci));

	Py_RETURN_NONE;
}
GEANYPY_PROPS_READONLY(Scintilla);


static PyObject *
Scintilla_delete_marker_at_line(Scintilla *self, PyObject *args, PyObject *kwargs)
{
	gint line_num, marker;
	static gchar *kwlist[] = { "line_number", "marker", NULL };

	SCI_RET_IF_FAIL(self);

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "ii", kwlist, &line_num, &marker))
			sci_delete_marker_at_line(self->sci, line_num, marker);
	Py_RETURN_NONE;
}


static PyObject *
Scintilla_end_undo_action(Scintilla *self)
{
	SCI_RET_IF_FAIL(self);
	sci_end_undo_action(self->sci);
	Py_RETURN_NONE;
}


static PyObject *
Scintilla_ensure_line_is_visible(Scintilla *self, PyObject *args, PyObject *kwargs)
{
	gint line = -1;
	static gchar *kwlist[] = { "line", NULL };

	SCI_RET_IF_FAIL(self);

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "|i", kwlist, &line))
	{
		if (line == -1)
			line = sci_get_current_line(self->sci);
		sci_ensure_line_is_visible(self->sci, line);
	}

	Py_RETURN_NONE;
}


static PyObject *
Scintilla_find_matching_brace(Scintilla *self, PyObject *args, PyObject *kwargs)
{
	gint pos, match_pos;
	static gchar *kwlist[] = { "pos", NULL };

	SCI_RET_IF_FAIL(self);

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "i", kwlist, &pos))
	{
		match_pos = sci_find_matching_brace(self->sci, pos);
		return Py_BuildValue("i", match_pos);
	}
	Py_RETURN_NONE;
}


static PyObject *
Scintilla_find_text(Scintilla *self, PyObject *args, PyObject *kwargs)
{
	gint pos = -1, flags = 0;
	glong start_chr = 0, end_chr = 0;
	gchar *search_text;
	struct Sci_TextToFind ttf = { { 0 } };
	static gchar *kwlist[] = { "text", "flags", "start_char", "end_char", NULL };

	SCI_RET_IF_FAIL(self);

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "s|ill", kwlist,
			&search_text, &flags, &start_chr, &end_chr))
	{
		ttf.chrg.cpMin = start_chr;
		ttf.chrg.cpMax = end_chr;
		ttf.lpstrText = search_text;
		pos = sci_find_text(self->sci, flags, &ttf);
		if (pos > -1)
			return Py_BuildValue("ll", ttf.chrgText.cpMin, ttf.chrgText.cpMax);
	}

	Py_RETURN_NONE;
}


static PyObject *
Scintilla_get_char_at(Scintilla *self, PyObject *args, PyObject *kwargs)
{
	gint pos;
	gchar chr;
	static gchar *kwlist[] = { "pos", NULL };

	SCI_RET_IF_FAIL(self);

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "i", kwlist, &pos))
	{
		chr = sci_get_char_at(self->sci, pos);
		return PyString_FromFormat("%c", chr);
	}

	Py_RETURN_NONE;
}


static PyObject *
Scintilla_get_col_from_position(Scintilla *self, PyObject *args, PyObject *kwargs)
{
	gint pos, col;
	static gchar *kwlist[] = { "pos", NULL };

	SCI_RET_IF_FAIL(self);

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "i", kwlist, &pos))
	{
		col = sci_get_col_from_position(self->sci, pos);
		return Py_BuildValue("i", col);
	}

	Py_RETURN_NONE;
}


static PyObject *
Scintilla_get_contents(Scintilla *self, PyObject *args, PyObject *kwargs)
{
	gint len = -1;
	gchar *text;
	PyObject *py_text;
	static gchar *kwlist[] = { "len", NULL };

	SCI_RET_IF_FAIL(self);

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "|i", kwlist, &len))
	{
		if (len == -1)
			len = sci_get_length(self->sci) + 1;
		text = sci_get_contents(self->sci, len);
		if (text == NULL)
			Py_RETURN_NONE;
		py_text = PyString_FromString(text);
		g_free(text);
		return py_text;
	}

	Py_RETURN_NONE;
}


static PyObject *
Scintilla_get_contents_range(Scintilla *self, PyObject *args, PyObject *kwargs)
{
	gint start = -1, end = -1;
	gchar *text;
	PyObject *py_text;
	static gchar *kwlist[] = { "start", "end", NULL };

	SCI_RET_IF_FAIL(self);

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "|ii", kwlist, &start, &end))
	{
		if (start == -1)
			start = 0;
		if (end == -1)
			end = sci_get_length(self->sci) + 1;
		text = sci_get_contents_range(self->sci, start, end);
		if (text == NULL)
			Py_RETURN_NONE;
		py_text = PyString_FromString(text);
		g_free(text);
		return py_text;
	}

	Py_RETURN_NONE;
}


static PyObject *
Scintilla_get_current_line(Scintilla *self)
{
	gint line;
	SCI_RET_IF_FAIL(self);
	line = sci_get_current_line(self->sci);
	return Py_BuildValue("i", line);
}

static PyObject *
Scintilla_get_current_position(Scintilla *self)
{
	gint pos;
	SCI_RET_IF_FAIL(self);
	pos = sci_get_current_position(self->sci);
	return Py_BuildValue("i", pos);
}


static PyObject *
Scintilla_get_length(Scintilla *self)
{
	gint len;
	SCI_RET_IF_FAIL(self);
	len = sci_get_length(self->sci);
	return Py_BuildValue("i", len);
}


static PyObject *
Scintilla_get_line(Scintilla *self, PyObject *args, PyObject *kwargs)
{
	gint line_num = -1;
	gchar *text;
	PyObject *py_text;
	static gchar *kwlist[] = { "line_num", NULL };

	SCI_RET_IF_FAIL(self);

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "|i", kwlist, &line_num))
	{
		if (line_num == -1)
			line_num = sci_get_current_line(self->sci);
		text = sci_get_line(self->sci, line_num);
		if (text == NULL)
			Py_RETURN_NONE;
		py_text = PyString_FromString(text);
		g_free(text);
		return py_text;
	}

	Py_RETURN_NONE;
}


static PyObject *
Scintilla_get_line_count(Scintilla *self)
{
	gint line_count;
	SCI_RET_IF_FAIL(self);
	line_count = sci_get_line_count(self->sci);
	return Py_BuildValue("i", line_count);
}


static PyObject *
Scintilla_get_line_end_position(Scintilla *self, PyObject *args, PyObject *kwargs)
{
	gint line = -1, line_end_pos;
	static gchar *kwlist[] = { "line", NULL };

	SCI_RET_IF_FAIL(self);

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "|i", kwlist, &line))
	{
		if (line == -1)
			line = sci_get_current_line(self->sci);
		line_end_pos = sci_get_line_end_position(self->sci, line);
		return Py_BuildValue("i", line_end_pos);
	}

	Py_RETURN_NONE;
}


static PyObject *
Scintilla_get_line_from_position(Scintilla *self, PyObject *args, PyObject *kwargs)
{
	gint line, pos;
	static gchar *kwlist[] = { "pos", NULL };

	SCI_RET_IF_FAIL(self);

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "|i", kwlist, &pos))
	{
		if (pos == -1)
			pos = sci_get_current_position(self->sci);
		line = sci_get_line_from_position(self->sci, pos);
		return Py_BuildValue("i", line);
	}

	Py_RETURN_NONE;
}


static PyObject *
Scintilla_get_line_indentation(Scintilla *self, PyObject *args, PyObject *kwargs)
{
	gint line = -1, width;
	static gchar *kwlist[] = { "line", NULL };

	SCI_RET_IF_FAIL(self);

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "|i", kwlist, &line))
	{
		if (line == -1)
			line = sci_get_current_line(self->sci);
		width = sci_get_line_indentation(self->sci, line);
		return Py_BuildValue("i", width);
	}

	Py_RETURN_NONE;
}


static PyObject *
Scintilla_get_line_is_visible(Scintilla *self, PyObject *args, PyObject *kwargs)
{
	gint line = -1;
	gboolean visible;
	static gchar *kwlist[] = { "line", NULL };

	SCI_RET_IF_FAIL(self);

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "|i", kwlist, &line))
	{
		if (line == -1)
			line = sci_get_current_line(self->sci);
		visible = sci_get_line_is_visible(self->sci, line);
		if (visible)
			Py_RETURN_TRUE;
		else
			Py_RETURN_FALSE;
	}

	Py_RETURN_NONE;
}


static PyObject *
Scintilla_get_line_length(Scintilla *self, PyObject *args, PyObject *kwargs)
{
	gint line = -1, length;
	static gchar *kwlist[] = { "line", NULL };

	SCI_RET_IF_FAIL(self);

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "|i", kwlist, &line))
	{
		if (line == -1)
			line = sci_get_current_line(self->sci);
		length = sci_get_line_length(self->sci, line);
		return Py_BuildValue("i", length);
	}

	Py_RETURN_NONE;
}



static PyObject *
Scintilla_get_position_from_line(Scintilla *self, PyObject *args, PyObject *kwargs)
{
	gint line = -1, pos;
	static gchar *kwlist[] = { "line", NULL };

	SCI_RET_IF_FAIL(self);

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "|i", kwlist, &line))
	{
		if (line == -1)
			line = sci_get_current_line(self->sci);
		pos = sci_get_position_from_line(self->sci, line);
		return Py_BuildValue("i", pos);
	}

	Py_RETURN_NONE;
}


static PyObject *
Scintilla_get_selected_text_length(Scintilla *self)
{
	gint len;
	SCI_RET_IF_FAIL(self);
	len = sci_get_selected_text_length(self->sci);
	return Py_BuildValue("i", len);
}


static PyObject *
Scintilla_get_selection_contents(Scintilla *self)
{
	gchar *text;
	PyObject *py_text;
	SCI_RET_IF_FAIL(self);
	text = sci_get_selection_contents(self->sci);
	if (text == NULL)
		Py_RETURN_NONE;
	py_text = PyString_FromString(text);
	g_free(text);
	return py_text;
}


static PyObject *
Scintilla_get_selection_end(Scintilla *self)
{
	gint pos;
	SCI_RET_IF_FAIL(self);
	pos = sci_get_selection_end(self->sci);
	return Py_BuildValue("i", pos);
}


static PyObject *
Scintilla_get_selection_mode(Scintilla *self)
{
	gint mode;
	SCI_RET_IF_FAIL(self);
	mode = sci_get_selection_mode(self->sci);
	return Py_BuildValue("i", mode);
}


static PyObject *
Scintilla_get_selection_start(Scintilla *self)
{
	gint pos;
	SCI_RET_IF_FAIL(self);
	pos = sci_get_selection_start(self->sci);
	return Py_BuildValue("i", pos);
}


static PyObject *
Scintilla_get_style_at(Scintilla *self, PyObject *args, PyObject *kwargs)
{
	gint pos = -1, style;
	static gchar *kwlist[] = { "pos", NULL };

	SCI_RET_IF_FAIL(self);

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "|i", kwlist, &pos))
	{
		if (pos == -1)
			pos = sci_get_current_position(self->sci);
		style = sci_get_style_at(self->sci, pos);
		return Py_BuildValue("i", style);
	}

	Py_RETURN_NONE;
}


static PyObject *
Scintilla_get_tab_width(Scintilla *self)
{
	gint width;
	SCI_RET_IF_FAIL(self);
	width = sci_get_tab_width(self->sci);
	return Py_BuildValue("i", width);
}


static PyObject *
Scintilla_goto_line(Scintilla *self, PyObject *args, PyObject *kwargs)
{
	gint line, unfold;
	static gchar *kwlist[] = { "line", "unfold", NULL };

	SCI_RET_IF_FAIL(self);

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "ii", kwlist, &line, &unfold))
		sci_goto_line(self->sci, line, (gboolean) unfold);

	Py_RETURN_NONE;
}


static PyObject *
Scintilla_has_selection(Scintilla *self)
{
	SCI_RET_IF_FAIL(self);
	if (sci_has_selection(self->sci))
		Py_RETURN_TRUE;
	else
		Py_RETURN_FALSE;
}


static PyObject *
Scintilla_indicator_clear(Scintilla *self, PyObject *args, PyObject *kwargs)
{
	gint pos, len;
	static gchar *kwlist[] = { "pos", "len", NULL };

	SCI_RET_IF_FAIL(self);

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "ii", kwlist, &pos, &len))
		sci_indicator_clear(self->sci, pos, len);

	Py_RETURN_NONE;
}


static PyObject *
Scintilla_indicator_set(Scintilla *self, PyObject *args, PyObject *kwargs)
{
	gint indic;
	static gchar *kwlist[] = { "indic", NULL };

	SCI_RET_IF_FAIL(self);
	if (PyArg_ParseTupleAndKeywords(args, kwargs, "i", kwlist, &indic))
		sci_indicator_set(self->sci, indic);

	Py_RETURN_NONE;
}


static PyObject *
Scintilla_insert_text(Scintilla *self, PyObject *args, PyObject *kwargs)
{
	gint pos = -1;
	gchar *text;
	static gchar *kwlist[] = { "text", "pos", NULL };

	SCI_RET_IF_FAIL(self);

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "s|i", kwlist, &text, &pos))
	{
		if (pos == -1)
			pos = sci_get_current_position(self->sci);
		if (text != NULL)
			sci_insert_text(self->sci, pos, text);
	}

	Py_RETURN_NONE;
}


static PyObject *
Scintilla_is_marker_set_at_line(Scintilla *self, PyObject *args, PyObject *kwargs)
{
	gboolean result;
	gint line, marker;
	static gchar *kwlist[] = { "line", "marker", NULL };

	SCI_RET_IF_FAIL(self);

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "ii", kwlist, &line, &marker))
	{
		result = sci_is_marker_set_at_line(self->sci, line, marker);
		if (result)
			Py_RETURN_TRUE;
		else
			Py_RETURN_FALSE;
	}

	Py_RETURN_NONE;
}


static PyObject *
Scintilla_replace_sel(Scintilla *self, PyObject *args, PyObject *kwargs)
{
	gchar *text;
	static gchar *kwlist[] = { "text", NULL };
	SCI_RET_IF_FAIL(self);
	if (PyArg_ParseTupleAndKeywords(args, kwargs, "s", kwlist, &text))
		sci_replace_sel(self->sci, text);
	Py_RETURN_NONE;
}


static PyObject *
Scintilla_scroll_caret(Scintilla *self)
{
	SCI_RET_IF_FAIL(self);
	sci_scroll_caret(self->sci);
	Py_RETURN_NONE;
}


static PyObject *
Scintilla_send_command(Scintilla *self, PyObject *args, PyObject *kwargs)
{
	gint cmd;
	static gchar *kwlist[] = { "cmd", NULL };

	SCI_RET_IF_FAIL(self);

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "i", kwlist, &cmd))
		sci_send_command(self->sci, cmd);

	Py_RETURN_NONE;
}


static PyObject *
Scintilla_set_current_position(Scintilla *self, PyObject *args, PyObject *kwargs)
{
	gint pos, stc = FALSE;
	static gchar *kwlist[] = { "pos", "scroll_to_caret", NULL };

	SCI_RET_IF_FAIL(self);

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "i|i", kwlist, &pos, &stc))
		sci_set_current_position(self->sci, pos, stc);

	Py_RETURN_NONE;
}


static PyObject *
Scintilla_set_font(Scintilla *self, PyObject *args, PyObject *kwargs)
{
	gint style, size;
	gchar *font;
	static gchar *kwlist[] = { "style", "font", "size", NULL };

	SCI_RET_IF_FAIL(self);

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "isi", kwlist, &style, &font, &size))
		sci_set_font(self->sci, style, font, size);

	Py_RETURN_NONE;
}


static PyObject *
Scintilla_set_line_indentation(Scintilla *self, PyObject *args, PyObject *kwargs)
{
	gint line, indent;
	static gchar *kwlist[] = { "line", "indent", NULL };

	SCI_RET_IF_FAIL(self);

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "ii", kwlist, &line, &indent))
		sci_set_line_indentation(self->sci, line, indent);

	Py_RETURN_NONE;
}


static PyObject *
Scintilla_set_marker_at_line(Scintilla *self, PyObject *args, PyObject *kwargs)
{
	gint line, marker;
	static gchar *kwlist[] = { "line", "marker", NULL };

	SCI_RET_IF_FAIL(self);

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "ii", kwlist, &line, &marker))
		sci_set_marker_at_line(self->sci, line, marker);

	Py_RETURN_NONE;
}


static PyObject *
Scintilla_set_selection_end(Scintilla *self, PyObject *args, PyObject *kwargs)
{
	gint pos;
	static gchar *kwlist[] = { "pos", NULL };

	SCI_RET_IF_FAIL(self);

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "i", kwlist, &pos))
		sci_set_selection_end(self->sci, pos);

	Py_RETURN_NONE;
}


static PyObject *
Scintilla_set_selection_mode(Scintilla *self, PyObject *args, PyObject *kwargs)
{
	gint mode;
	static gchar *kwlist[] = { "mode", NULL };

	SCI_RET_IF_FAIL(self);

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "i", kwlist, &mode))
		sci_set_selection_mode(self->sci, mode);

	Py_RETURN_NONE;
}


static PyObject *
Scintilla_set_selection_start(Scintilla *self, PyObject *args, PyObject *kwargs)
{
	gint pos;
	static gchar *kwlist[] = { "pos", NULL };

	SCI_RET_IF_FAIL(self);

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "i", kwlist, &pos))
		sci_set_selection_start(self->sci, pos);

	Py_RETURN_NONE;
}


static PyObject *
Scintilla_set_text(Scintilla *self, PyObject *args, PyObject *kwargs)
{
	gchar *text;
	static gchar *kwlist[] = { "text", NULL };

	SCI_RET_IF_FAIL(self);

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "s", kwlist, &text))
		sci_set_text(self->sci, text);

	Py_RETURN_NONE;
}


static PyObject *
Scintilla_start_undo_action(Scintilla *self)
{
	SCI_RET_IF_FAIL(self);
	sci_start_undo_action(self->sci);
	Py_RETURN_NONE;
}


static PyObject *
Scintilla_send_message(Scintilla *self, PyObject *args, PyObject *kwargs)
{
	gint msg;
	glong uptr = 0, sptr = 0, ret;
	static gchar *kwlist[] = { "msg", "lparam", "wparam", NULL };

	SCI_RET_IF_FAIL(self);

	if (PyArg_ParseTupleAndKeywords(args, kwargs, "i|ll", kwlist, &msg, &uptr, &sptr))
	{
		ret = scintilla_send_message(self->sci, msg, uptr, sptr);
		return Py_BuildValue("l", ret);
	}

	Py_RETURN_NONE;
}


static PyMethodDef Scintilla_methods[] = {
	{ "delete_marker_at_line", (PyCFunction) Scintilla_delete_marker_at_line, METH_KEYWORDS,
		"Deletes a line marker." },
	{ "end_undo_action", (PyCFunction) Scintilla_end_undo_action, METH_NOARGS,
		"Ends grouping a set of edits together as one Undo action." },
	{ "ensure_line_is_visible", (PyCFunction) Scintilla_ensure_line_is_visible, METH_KEYWORDS,
		"Makes line visible (folding may have hidden it)." },
	{ "find_matching_brace", (PyCFunction) Scintilla_find_matching_brace, METH_KEYWORDS,
		"Finds a matching brace at pos." },
	{ "find_text", (PyCFunction) Scintilla_find_text, METH_KEYWORDS,
		"Finds text in the document." },
	{ "get_char_at", (PyCFunction) Scintilla_get_char_at, METH_KEYWORDS,
		"Gets the character at a position." },
	{ "get_col_from_position", (PyCFunction) Scintilla_get_col_from_position, METH_KEYWORDS,
		"Gets the column number relative to the start of the line that "
		"pos is on." },
	{ "get_contents", (PyCFunction) Scintilla_get_contents, METH_KEYWORDS,
		"Gets all text inside a given text length." },
	{ "get_contents_range", (PyCFunction) Scintilla_get_contents_range, METH_KEYWORDS,
		"Gets text between start and end." },
	{ "get_current_line", (PyCFunction) Scintilla_get_current_line, METH_NOARGS,
		"Gets current line number." },
	{ "get_current_position", (PyCFunction) Scintilla_get_current_position, METH_NOARGS,
		"Gets the cursor position." },
	{ "get_length", (PyCFunction) Scintilla_get_length, METH_NOARGS,
		"Gets the length of all text." },
	{ "get_line", (PyCFunction) Scintilla_get_line, METH_KEYWORDS,
		"Gets line contents." },
	{ "get_line_count", (PyCFunction) Scintilla_get_line_count, METH_NOARGS,
		"Gets the total number of lines." },
	{ "get_line_end_position", (PyCFunction) Scintilla_get_line_end_position, METH_KEYWORDS,
		"Gets the position at the end of a line." },
	{ "get_line_from_position", (PyCFunction) Scintilla_get_line_from_position, METH_KEYWORDS,
		"Gets the line number from pos." },
	{ "get_line_indentation", (PyCFunction) Scintilla_get_line_indentation, METH_KEYWORDS,
		"Gets the indentation width of a line." },
	{ "get_line_is_visible", (PyCFunction) Scintilla_get_line_is_visible, METH_KEYWORDS,
		"Checks if a line is visible (folding may have hidden it)." },
	{ "get_line_length", (PyCFunction) Scintilla_get_line_length, METH_KEYWORDS,
		"Gets line length." },
	{ "get_position_from_line", (PyCFunction) Scintilla_get_position_from_line, METH_KEYWORDS,
		"Gets the position for the start of line." },
	{ "get_selected_text_length", (PyCFunction) Scintilla_get_selected_text_length, METH_NOARGS,
		"Gets selected text length."},
	{ "get_selection_contents", (PyCFunction) Scintilla_get_selection_contents, METH_NOARGS,
		"Gets selected text." },
	{ "get_selection_end", (PyCFunction) Scintilla_get_selection_end, METH_NOARGS,
		"Gets the selection end position." },
	{ "get_selection_mode", (PyCFunction) Scintilla_get_selection_mode, METH_NOARGS,
		"Gets the selection mode." },
	{ "get_selection_start", (PyCFunction) Scintilla_get_selection_start, METH_NOARGS,
		"Gets the selection start position." },
	{ "get_style_at", (PyCFunction) Scintilla_get_style_at, METH_KEYWORDS,
		"Gets the style ID at pos." },
	{ "get_tab_width", (PyCFunction) Scintilla_get_tab_width, METH_NOARGS,
		"Gets display tab width (this is not indent width, see IndentPrefs)." },
	{ "goto_line", (PyCFunction) Scintilla_goto_line, METH_KEYWORDS,
		"Jumps to the specified line in the document." },
	{ "has_selection", (PyCFunction) Scintilla_has_selection, METH_NOARGS,
		"Checks if there's a selection." },
	{ "indicator_clear", (PyCFunction) Scintilla_indicator_clear, METH_KEYWORDS,
		"Clears the currently set indicator from a range of text." },
	{ "indicator_set", (PyCFunction) Scintilla_indicator_set, METH_KEYWORDS,
		"Sets the current indicator." },
	{ "insert_text", (PyCFunction) Scintilla_insert_text, METH_KEYWORDS,
		"Inserts text at pos." },
	{ "is_marker_set_at_line", (PyCFunction) Scintilla_is_marker_set_at_line, METH_KEYWORDS,
		"Checks if a line has a marker set." },
	{ "replace_sel", (PyCFunction) Scintilla_replace_sel, METH_KEYWORDS,
		"Replaces selection." },
	{ "scroll_caret", (PyCFunction) Scintilla_scroll_caret, METH_NOARGS,
		"Scrolls the cursor in view." },
	{ "send_command", (PyCFunction) Scintilla_send_command, METH_KEYWORDS,
		"Sends Scintilla commands without any parameters (see send_message function)." },
	{ "set_current_position", (PyCFunction) Scintilla_set_current_position, METH_KEYWORDS,
		"Sets the cursor position." },
	{ "set_font", (PyCFunction) Scintilla_set_font, METH_KEYWORDS,
		"Sets the font and size for a particular style." },
	{ "set_line_indentation", (PyCFunction) Scintilla_set_line_indentation, METH_KEYWORDS,
		"Sets the indentation of a line." },
	{ "set_marker_at_line", (PyCFunction) Scintilla_set_marker_at_line, METH_KEYWORDS,
		"Sets a line marker." },
	{ "set_selection_end", (PyCFunction) Scintilla_set_selection_end, METH_KEYWORDS,
		"Sets the selection end position." },
	{ "set_selection_mode", (PyCFunction) Scintilla_set_selection_mode, METH_KEYWORDS,
		"Sets selection mode." },
	{ "set_selection_start", (PyCFunction) Scintilla_set_selection_start, METH_KEYWORDS,
		"Sets the selection start position." },
	{ "set_text", (PyCFunction) Scintilla_set_text, METH_KEYWORDS,
		"Sets all text." },
	{ "start_undo_action", (PyCFunction) Scintilla_start_undo_action, METH_NOARGS,
		"Begins grouping a set of edits together as one Undo action." },
	{ "send_message", (PyCFunction) Scintilla_send_message, METH_KEYWORDS,
		"Send a message to the Scintilla widget." },
	{ NULL }
};


static PyGetSetDef Scintilla_getseters[] = {
	GEANYPY_GETSETDEF(Scintilla, "widget",
		"Gets the ScintillaObject as a GTK+ widget."),
	{ NULL }
};


static PyTypeObject ScintillaType = {
	PyObject_HEAD_INIT(NULL)
	0,												/* ob_size */
	"geany.scintilla.Scintilla",					/* tp_name */
	sizeof(Scintilla),								/* tp_basicsize */
	0,												/* tp_itemsize */
	(destructor) Scintilla_dealloc,					/* tp_dealloc */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* tp_print - tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,		/* tp_flags */
	"Wrapper around a ScintillaObject structure.",	/* tp_doc */
	0, 0, 0, 0, 0, 0,								/* tp_traverse - tp_iternext */
	Scintilla_methods,								/* tp_methods */
	0,												/* tp_members */
	Scintilla_getseters,							/* tp_getset */
	0, 0, 0, 0, 0,									/* tp_base - tp_dictoffset */
	(initproc) Scintilla_init,						/* tp_init */
	0, 0,											/* tp_alloc - tp_new */
};

static PyMethodDef ScintillaModule_methods[] = { { NULL } };


PyMODINIT_FUNC initscintilla(void)
{
	PyObject *m;

	ScintillaType.tp_new = PyType_GenericNew;
	if (PyType_Ready(&ScintillaType) < 0)
		return;

	NotificationType.tp_new = PyType_GenericNew;
	if (PyType_Ready(&NotificationType) < 0)
		return;

	NotifyHeaderType.tp_new = PyType_GenericNew;
	if (PyType_Ready(&NotifyHeaderType) < 0)
		return;

	m = Py_InitModule("scintilla", ScintillaModule_methods);

	Py_INCREF(&ScintillaType);
	PyModule_AddObject(m, "Scintilla", (PyObject *)&ScintillaType);

	Py_INCREF(&NotificationType);
	PyModule_AddObject(m, "Notification", (PyObject *)&NotificationType);

	Py_INCREF(&NotifyHeaderType);
	PyModule_AddObject(m, "NotifyHeader", (PyObject *)&NotifyHeaderType);


	PyModule_AddIntConstant(m, "FLAG_WHOLE_WORD", SCFIND_WHOLEWORD);
	PyModule_AddIntConstant(m, "FLAG_MATCH_CASE", SCFIND_MATCHCASE);
	PyModule_AddIntConstant(m, "FLAG_WORD_START", SCFIND_WORDSTART);
	PyModule_AddIntConstant(m, "FLAG_REGEXP", SCFIND_REGEXP);
	PyModule_AddIntConstant(m, "FLAG_POSIX", SCFIND_POSIX);

	PyModule_AddIntConstant(m, "UPDATE_CONTENT", SC_UPDATE_CONTENT);
	PyModule_AddIntConstant(m, "UPDATE_SELECTION", SC_UPDATE_SELECTION);
	PyModule_AddIntConstant(m, "UPDATE_V_SCROLL", SC_UPDATE_V_SCROLL);
	PyModule_AddIntConstant(m, "UPDATE_H_SCROLL", SC_UPDATE_H_SCROLL);

	PyModule_AddIntConstant(m, "MOD_INSERT_TEXT", SC_MOD_INSERTTEXT);
	PyModule_AddIntConstant(m, "MOD_DELETE_TEXT", SC_MOD_DELETETEXT);
	PyModule_AddIntConstant(m, "MOD_CHANGE_STYLE", SC_MOD_CHANGESTYLE);
	PyModule_AddIntConstant(m, "MOD_CHANGE_FOLD", SC_MOD_CHANGEFOLD);
	PyModule_AddIntConstant(m, "PERFORMED_USER", SC_PERFORMED_USER);
	PyModule_AddIntConstant(m, "PERFORMED_UNDO", SC_PERFORMED_UNDO);
	PyModule_AddIntConstant(m, "PERFORMED_REDO", SC_PERFORMED_REDO);
	PyModule_AddIntConstant(m, "MULTI_STEP_UNDO_REDO", SC_MULTISTEPUNDOREDO);
	PyModule_AddIntConstant(m, "LAST_STEP_IN_UNDO_REDO", SC_LASTSTEPINUNDOREDO);
	PyModule_AddIntConstant(m, "MOD_CHANGE_MARKER", SC_MOD_CHANGEMARKER);
	PyModule_AddIntConstant(m, "MOD_BEFORE_INSERT", SC_MOD_BEFOREINSERT);
	PyModule_AddIntConstant(m, "MOD_BEFORE_DELETE", SC_MOD_BEFOREDELETE);
	PyModule_AddIntConstant(m, "MOD_CHANGE_INDICATOR", SC_MOD_CHANGEINDICATOR);
	PyModule_AddIntConstant(m, "MOD_CHANGE_LINE_STATE", SC_MOD_CHANGELINESTATE);
	PyModule_AddIntConstant(m, "MOD_LEXER_STATE", SC_MOD_LEXERSTATE);
	PyModule_AddIntConstant(m, "MOD_CHANGE_MARGIN", SC_MOD_CHANGEMARGIN);
	PyModule_AddIntConstant(m, "MOD_CHANGE_ANNOTATION", SC_MOD_CHANGEANNOTATION);
	PyModule_AddIntConstant(m, "MULTILINE_UNDO_REDO", SC_MULTILINEUNDOREDO);
	PyModule_AddIntConstant(m, "START_ACTION", SC_STARTACTION);
	PyModule_AddIntConstant(m, "MOD_CONTAINER", SC_MOD_CONTAINER);
	PyModule_AddIntConstant(m, "MOD_EVENT_MASK_ALL", SC_MODEVENTMASKALL);

	PyModule_AddIntConstant(m, "STYLE_NEEDED", SCN_STYLENEEDED);
	PyModule_AddIntConstant(m, "CHAR_ADDED", SCN_CHARADDED);
	PyModule_AddIntConstant(m, "SAVE_POINT_REACHED", SCN_SAVEPOINTREACHED);
	PyModule_AddIntConstant(m, "SAVE_POINT_LEFT", SCN_SAVEPOINTLEFT);
	PyModule_AddIntConstant(m, "MODIFY_ATTEMPT_RO", SCN_MODIFYATTEMPTRO);
	PyModule_AddIntConstant(m, "KEY", SCN_KEY);
	PyModule_AddIntConstant(m, "DOUBLE_CLICK", SCN_DOUBLECLICK);
	PyModule_AddIntConstant(m, "UPDATE_UI", SCN_UPDATEUI);
	PyModule_AddIntConstant(m, "MODIFIED", SCN_MODIFIED);
	PyModule_AddIntConstant(m, "MACRO_RECORD", SCN_MACRORECORD);
	PyModule_AddIntConstant(m, "MARGIN_CLICK", SCN_MARGINCLICK);
	PyModule_AddIntConstant(m, "NEED_SHOWN", SCN_NEEDSHOWN);
	PyModule_AddIntConstant(m, "PAINTED", SCN_PAINTED);
	PyModule_AddIntConstant(m, "USER_LIST_SELECTION", SCN_USERLISTSELECTION);
	PyModule_AddIntConstant(m, "URI_DROPPED", SCN_URIDROPPED);
	PyModule_AddIntConstant(m, "DWELL_START", SCN_DWELLSTART);
	PyModule_AddIntConstant(m, "DWELL_END", SCN_DWELLEND);
	PyModule_AddIntConstant(m, "ZOOM", SCN_ZOOM);
	PyModule_AddIntConstant(m, "HOT_SPOT_CLICK", SCN_HOTSPOTCLICK);
	PyModule_AddIntConstant(m, "HOT_SPOT_DOUBLE_CLICK", SCN_HOTSPOTDOUBLECLICK);
	PyModule_AddIntConstant(m, "CALL_TIP_CLICK", SCN_CALLTIPCLICK);
	PyModule_AddIntConstant(m, "AUTO_C_SELECTION", SCN_AUTOCSELECTION);
	PyModule_AddIntConstant(m, "INDICATOR_CLICK", SCN_INDICATORCLICK);
	PyModule_AddIntConstant(m, "INDICATOR_RELEASE", SCN_INDICATORRELEASE);
	PyModule_AddIntConstant(m, "AUTOC_CANCELLED", SCN_AUTOCCANCELLED);
	PyModule_AddIntConstant(m, "AUTOC_CHAR_DELETED", SCN_AUTOCCHARDELETED);
	PyModule_AddIntConstant(m, "HOT_SPOT_RELEASE_CLICK", SCN_HOTSPOTRELEASECLICK);
}


Scintilla *Scintilla_create_new_from_scintilla(ScintillaObject *sci)
{
	Scintilla *self;
	self = (Scintilla *) PyObject_CallObject((PyObject *) &ScintillaType, NULL);
	self->sci = sci;
	return self;
}
