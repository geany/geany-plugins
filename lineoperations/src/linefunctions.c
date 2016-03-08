/*
 *      linefunctions.c - Line operations, remove duplicate lines, empty lines, lines with only whitespace, sort lines.
 *
 *      Copyright 2015 Sylvan Mostert <smostert.dev@gmail.com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License along
 *      with this program; if not, write to the Free Software Foundation, Inc.,
 *      51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/


#include "linefunctions.h"


/* altered from geany/src/editor.c, ensure new line at file end */
static void ensure_final_newline(GeanyEditor *editor, gint max_lines)
{
	gint end_document       = sci_get_position_from_line(editor->sci, max_lines);
	gboolean append_newline = end_document >
					sci_get_position_from_line(editor->sci, max_lines - 1);

	if (append_newline)
	{
		const gchar *eol = editor_get_eol_char(editor);
		sci_insert_text(editor->sci, end_document, eol);
	}
}


/* comparison function to be used in qsort */
static gint compare_asc(const void * a, const void * b)
{
	return strcmp(*(const gchar **) a, *(const gchar **) b);
}


/* comparison function to be used in qsort */
static gint compare_desc(const void * a, const void * b)
{
	return strcmp(*(const gchar **) b, *(const gchar **) a);
}


/* Remove Duplicate Lines, sorted */
void rmdupst(GeanyDocument *doc) {
	gint  total_num_chars;  /* number of characters in the document */
	gint  total_num_lines;  /* number of lines in the document */
	gchar **lines;          /* array to hold all lines in the document */
	gchar *new_file;        /* *final* string to replace current document */
	gchar *nf_end;          /* points to end of new_file */
	gchar *lineptr;         /* temporary line pointer */
	gint  i;                /* iterator */

	total_num_chars = sci_get_length(doc->editor->sci);
	total_num_lines = sci_get_line_count(doc->editor->sci);
	lines           = g_malloc(sizeof(gchar *) * total_num_lines);
	new_file        = g_malloc(sizeof(gchar)   * (total_num_chars+1));
	new_file[0]     = '\0';
	nf_end          = new_file;
	lineptr         = (gchar *)"";


	/* if file is not empty, ensure that the file ends with newline */
	if(total_num_lines != 1)
		ensure_final_newline(doc->editor, total_num_lines);

	/* copy *all* lines into **lines array */
	for(i = 0; i < total_num_lines; i++)
		lines[i] = sci_get_line(doc->editor->sci, i);

	/* sort **lines ascending */
	qsort(lines, total_num_lines, sizeof(gchar *), compare_asc);

	/* loop through **lines, join first occurances into one str (new_file) */
	for(i = 0; i < total_num_lines; i++)
		if(strcmp(lines[i], lineptr) != 0)
		{
			lineptr  = lines[i];
			nf_end   = g_stpcpy(nf_end, lines[i]);
		}

	/* set new document */
	sci_set_text(doc->editor->sci, new_file);

	/* free used memory */
	for(i = 0; i < total_num_lines; i++)
		g_free(lines[i]);
	g_free(lines);
	g_free(new_file);
}


/* Remove Duplicate Lines, ordered */
void rmdupln(GeanyDocument *doc) {
	gint  total_num_chars;  /* number of characters in the document */
	gint  total_num_lines;  /* number of lines in the document */
	gchar **lines;          /* array to hold all lines in the document */
	gchar *new_file;        /* *final* string to replace current document */
	gchar *nf_end;          /* points to end of new_file */
	gint  i;                /* iterator */
	gint  j;                /* iterator */
	gboolean *to_remove;    /* flag to 'mark' which lines to remove */

	total_num_chars = sci_get_length(doc->editor->sci);
	total_num_lines = sci_get_line_count(doc->editor->sci);
	lines           = g_malloc(sizeof(gchar *) * total_num_lines);
	new_file        = g_malloc(sizeof(gchar) * (total_num_chars+1));
	new_file[0]     = '\0';
	nf_end          = new_file;


	/* if file is not empty, ensure that the file ends with newline */
	if(total_num_lines != 1)
		ensure_final_newline(doc->editor, total_num_lines);

	/* copy *all* lines into **lines array */
	for(i = 0; i < total_num_lines; i++)
		lines[i] = sci_get_line(doc->editor->sci, i);

	/* allocate and set *to_remove to all FALSE
	 * to_remove[i] represents whether lines[i] should be removed  */
	to_remove = g_malloc(sizeof(gboolean) * total_num_lines);
	for(i = 0; i < (total_num_lines); i++)
		to_remove[i] = FALSE;

	/* find which **lines are duplicate, and mark them as duplicate */
	for(i = 0; i < total_num_lines; i++)	/* loop through **lines */
		/* make sure that the line is not already duplicate */
		if(!to_remove[i])
			/* find the rest of same lines */
			for(j = (i+1); j < total_num_lines; j++)
				if(!to_remove[j] && strcmp(lines[i], lines[j]) == 0)
					to_remove[j] = TRUE;   /* line is duplicate, mark to remove */

	/* copy **lines into 'new_file' if it is not FALSE(not duplicate) */
	for(i = 0; i < total_num_lines; i++)
		if(!to_remove[i])
			nf_end   = g_stpcpy(nf_end, lines[i]);

	/* set new document */
	sci_set_text(doc->editor->sci, new_file);

	/* free used memory */
	for(i = 0; i < total_num_lines; i++)
		g_free(lines[i]);
	g_free(lines);
	g_free(new_file);
	g_free(to_remove);
}


/* Remove Unique Lines */
void rmunqln(GeanyDocument *doc) {
	gint total_num_chars;   /* number of characters in the document */
	gint total_num_lines;   /* number of lines in the document */
	gchar **lines;          /* array to hold all lines in the document */
	gchar *new_file;        /* *final* string to replace current document */
	gchar *nf_end;          /* points to end of new_file */
	gint i;                 /* iterator */
	gint j;                 /* iterator */
	gboolean *to_remove;    /* to 'mark' which lines to remove */

	total_num_chars = sci_get_length(doc->editor->sci);
	total_num_lines = sci_get_line_count(doc->editor->sci);
	lines           = g_malloc(sizeof(gchar *) * total_num_lines);
	new_file        = g_malloc(sizeof(gchar) * (total_num_chars+1));
	new_file[0]     = '\0';
	nf_end          = new_file;


	/* if file is not empty, ensure that the file ends with newline */
	if(total_num_lines != 1)
		ensure_final_newline(doc->editor, total_num_lines);

	/* copy *all* lines into **lines array */
	for(i = 0; i < total_num_lines; i++)
		lines[i] = sci_get_line(doc->editor->sci, i);


	/* allocate and set *to_remove to all TRUE
	 * to_remove[i] represents whether lines[i] should be removed */
	to_remove = g_malloc(sizeof(gboolean) * total_num_lines);
	for(i = 0; i < total_num_lines; i++)
		to_remove[i] = TRUE;

	/* find all unique lines and set them to FALSE (not to be removed) */
	for(i = 0; i < total_num_lines; i++)
		/* make sure that the line is not already determined to be unique */
		if(to_remove[i])
			for(j = (i+1); j < total_num_lines; j++)
				if(to_remove[j] && strcmp(lines[i], lines[j]) == 0)
				{
					to_remove[i] = FALSE;
					to_remove[j] = FALSE;
				}

	/* copy **lines into 'new_file' if it is not FALSE(not duplicate) */
	for(i = 0; i < total_num_lines; i++)
		if(!to_remove[i])
			nf_end = g_stpcpy(nf_end, lines[i]);

	/* set new document */
	sci_set_text(doc->editor->sci, new_file);

	/* free used memory */
	for(i = 0; i < total_num_lines; i++)
		g_free(lines[i]);
	g_free(lines);
	g_free(new_file);
	g_free(to_remove);
}


/* Remove Empty Lines */
void rmemtyln(GeanyDocument *doc) {
	gint  total_num_lines;    /* number of lines in the document */
	gint  i;                  /* iterator */

	total_num_lines = sci_get_line_count(doc->editor->sci);

	sci_start_undo_action(doc->editor->sci);

	for(i = 0; i < total_num_lines; i++)    /* loop through opened doc */
	{
		if(sci_get_position_from_line(doc->editor->sci, i) ==
		   sci_get_line_end_position(doc->editor->sci, i))
		{
			scintilla_send_message(doc->editor->sci,
								   SCI_DELETERANGE,
								   sci_get_position_from_line(doc->editor->sci, i),
								   sci_get_line_length(doc->editor->sci, i));
			total_num_lines--;
			i--;
		}
	}

	sci_end_undo_action(doc->editor->sci);
}


/* Remove Whitespace Lines */
void rmwhspln(GeanyDocument *doc) {
	gint total_num_lines;  /* number of lines in the document */
	gint indent;
	gint i;                /* iterator */

	total_num_lines = sci_get_line_count(doc->editor->sci);

	sci_start_undo_action(doc->editor->sci);

	for(i = 0; i < total_num_lines; i++)    /* loop through opened doc */
	{
		indent = scintilla_send_message(doc->editor->sci,
									SCI_GETLINEINDENTPOSITION,
									i, 0);

		if(indent -
		   sci_get_position_from_line(doc->editor->sci, i) ==
		   sci_get_line_end_position(doc->editor->sci, i)-
		   sci_get_position_from_line(doc->editor->sci, i))
		{
			scintilla_send_message(doc->editor->sci,
								   SCI_DELETERANGE,
								   sci_get_position_from_line(doc->editor->sci, i),
								   sci_get_line_length(doc->editor->sci, i));
			total_num_lines--;
			i--;
		}

	}


	sci_end_undo_action(doc->editor->sci);
}


/* Sort Lines Ascending and Descending */
void sortlines(GeanyDocument *doc, gboolean asc) {
	gint  total_num_lines;  /* number of lines in the document */
	gchar **lines;          /* array to hold all lines in the document */
	gchar *new_file;        /* *final* string to replace current document */
	gint  i;                /* iterator */

	total_num_lines = sci_get_line_count(doc->editor->sci);
	lines           = g_malloc(sizeof(gchar *) * (total_num_lines+1));

	/* if file is not empty, ensure that the file ends with newline */
	if(total_num_lines != 1)
		ensure_final_newline(doc->editor, total_num_lines);

	/* copy *all* lines into **lines array */
	for(i = 0; i < total_num_lines; i++)
		lines[i] = sci_get_line(doc->editor->sci, i);

	/* sort **lines array */
	if(asc)
		qsort(lines, total_num_lines, sizeof(gchar *), compare_asc);
	else
		qsort(lines, total_num_lines, sizeof(gchar *), compare_desc);

	/* join **lines into one string (new_file) */
	lines[total_num_lines] = NULL;
	new_file = g_strjoinv("", lines);

	/* set new document */
	sci_set_text(doc->editor->sci, new_file);

	/* free used memory */
	for(i = 0; i < total_num_lines; i++)
		g_free(lines[i]);
	g_free(lines);
	g_free(new_file);
}
