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
void
rmdupst(GeanyDocument *doc, gchar **lines, gint num_lines, gchar *new_file)
{
	gchar *nf_end  = new_file;    /* points to last char of new_file */
	gchar *lineptr = (gchar *)""; /* temporary line pointer */
	gint  i        = 0;           /* iterator */

	/* sort **lines ascending */
	qsort(lines, num_lines, sizeof(gchar *), compare_asc);

	/* loop through **lines, join first occurances into one str (new_file) */
	for(i = 0; i < num_lines; i++)
	{
		if(strcmp(lines[i], lineptr) != 0)
		{
			lineptr  = lines[i];
			nf_end   = g_stpcpy(nf_end, lines[i]);
		}
	}
}


/* Remove Duplicate Lines, ordered */
void
rmdupln(GeanyDocument *doc, gchar **lines, gint num_lines, gchar *new_file)
{
	gchar *nf_end  = new_file;  /* points to last char of new_file */
	gint  i        = 0;         /* iterator */
	gint  j        = 0;         /* iterator */
	gboolean *to_remove = NULL; /* flag to 'mark' which lines to remove */


	/* allocate and set *to_remove to all FALSE
	 * to_remove[i] represents whether lines[i] should be removed  */
	to_remove = g_malloc(sizeof(gboolean) * num_lines);
	for(i = 0; i < (num_lines); i++)
		to_remove[i] = FALSE;

	/* find which **lines are duplicate, and mark them as duplicate */
	for(i = 0; i < num_lines; i++)	/* loop through **lines */
	{
		/* make sure that the line is not already duplicate */
		if(!to_remove[i])
		{
			/* find the rest of same lines */
			for(j = (i+1); j < num_lines; j++)
			{
				if(!to_remove[j] && strcmp(lines[i], lines[j]) == 0)
					to_remove[j] = TRUE;   /* line is duplicate, mark to remove */
			}
		}
	}

	/* copy **lines into 'new_file' if it is not FALSE(not duplicate) */
	for(i = 0; i < num_lines; i++)
		if(!to_remove[i])
			nf_end   = g_stpcpy(nf_end, lines[i]);

	/* free used memory */
	g_free(to_remove);
}


/* Remove Unique Lines */
void
rmunqln(GeanyDocument *doc, gchar **lines, gint num_lines, gchar *new_file)
{
	gchar *nf_end = new_file;   /* points to last char of new_file */
	gint i        = 0;          /* iterator */
	gint j        = 0;          /* iterator */
	gboolean *to_remove = NULL; /* to 'mark' which lines to remove */


	/* allocate and set *to_remove to all TRUE
	 * to_remove[i] represents whether lines[i] should be removed */
	to_remove = g_malloc(sizeof(gboolean) * num_lines);
	for(i = 0; i < num_lines; i++)
		to_remove[i] = TRUE;

	/* find all unique lines and set them to FALSE (not to be removed) */
	for(i = 0; i < num_lines; i++)
		/* make sure that the line is not already determined to be unique */
		if(to_remove[i])
			for(j = (i+1); j < num_lines; j++)
				if(to_remove[j] && strcmp(lines[i], lines[j]) == 0)
				{
					to_remove[i] = FALSE;
					to_remove[j] = FALSE;
				}

	/* copy **lines into 'new_file' if it is not FALSE(not duplicate) */
	for(i = 0; i < num_lines; i++)
		if(!to_remove[i])
			nf_end = g_stpcpy(nf_end, lines[i]);

	/* free used memory */
	g_free(to_remove);
}


/* Remove Empty Lines */
void
rmemtyln(GeanyDocument *doc, gint line_num, gint end_line_num)
{

	while(line_num <= end_line_num)    /* loop through lines */
	{
		/* check if the first posn of the line is also the end of line posn */
		if(sci_get_position_from_line(doc->editor->sci, i) ==
		   sci_get_line_end_position(doc->editor->sci, i))
		{
			scintilla_send_message(doc->editor->sci,
					   SCI_DELETERANGE,
					   sci_get_position_from_line(doc->editor->sci, line_num),
					   sci_get_line_length(doc->editor->sci, line_num));
			line_num--;
			end_line_num--;
		}
	}
}


/* Remove Whitespace Lines */
void
rmwhspln(GeanyDocument *doc, gint line_num, gint end_line_num)
{
	gint indent;                       /* indent position */

	while(line_num <= end_line_num)    /* loop through lines */
	{
		indent = scintilla_send_message(doc->editor->sci,
									SCI_GETLINEINDENTPOSITION,
									i, 0);

		/* check if the posn of indentation is also the end of line posn */
		if(indent -
		   sci_get_position_from_line(doc->editor->sci, i) ==
		   sci_get_line_end_position(doc->editor->sci, i)-
		   sci_get_position_from_line(doc->editor->sci, i))
		{
			scintilla_send_message(doc->editor->sci,
					   SCI_DELETERANGE,
					   sci_get_position_from_line(doc->editor->sci, line_num),
					   sci_get_line_length(doc->editor->sci, line_num));
			line_num--;
			end_line_num--;
		}

	}
}


/* Sort Lines Ascending */
void sortlinesasc(GeanyDocument *doc, gchar **lines, gint num_lines, gchar *new_file) {
	gchar *nf_end = new_file;          /* points to last char of new_file */
	gint i;

	qsort(lines, num_lines, sizeof(gchar *), compare_asc);

	/* join **lines into one string (new_file) */
	for(i = 0; i < num_lines; i++)
		nf_end = g_stpcpy(nf_end, lines[i]);
}


/* Sort Lines Descending */
void sortlinesdesc(GeanyDocument *doc, gchar **lines, gint num_lines, gchar *new_file) {
	gchar *nf_end = new_file;          /* points to last char of new_file */
	gint i;

	qsort(lines, num_lines, sizeof(gchar *), compare_desc);

	/* join **lines into one string (new_file) */
	for(i = 0; i < num_lines; i++)
		nf_end = g_stpcpy(nf_end, lines[i]);
}
