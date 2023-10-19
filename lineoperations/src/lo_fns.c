/*
 *      lo_fns.c - Line operations, remove duplicate lines, empty lines,
 *                 lines with only whitespace, sort lines.
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


#ifdef HAVE_CONFIG_H
    #include "config.h" /* for the gettext domain */
#endif

#include "lo_fns.h"
#include "lo_prefs.h"


/* Get sort function based on user preferences */
lo_strcmpfns
getcmpfns(void)
{
	if(lo_info->use_collation_compare)
	{
		return g_utf8_collate;
	}
	else
	{
		return g_strcmp0;
	}
}


/* comparison function to be used in qsort */
static gint
compare_asc(const void *a, const void *b)
{
	lo_strcmpfns lo_strcmp = getcmpfns();
	return lo_strcmp(*(const gchar **)a, *(const gchar **)b);
}


/* comparison function to be used in qsort */
static gint
compare_desc(const void *a, const void *b)
{
	lo_strcmpfns lo_strcmp = getcmpfns();
	return lo_strcmp(*(const gchar **)b, *(const gchar **)a);
}


/* Remove Duplicate Lines, sorted */
gint
rmdupst(gchar **lines, gint num_lines, gchar *new_file)
{
	gchar *nf_end  = new_file;     /* points to last char of new_file */
	gchar *lineptr = (gchar *)" "; /* temporary line pointer */
	gint  i        = 0;            /* iterator */
	gint  changed  = 0;            /* number of lines removed */
	lo_strcmpfns lo_strcmp = getcmpfns();

	/* sort **lines ascending */
	qsort(lines, num_lines, sizeof(gchar *), compare_asc);

	/* loop through **lines, join first occurances into one str (new_file) */
	for (i = 0; i < num_lines; i++)
	{
		if (lo_strcmp(lines[i], lineptr) != 0)
		{
			changed++;     /* number of lines kept */
			lineptr = lines[i];
			nf_end  = g_stpcpy(nf_end, lines[i]);
		}
	}

	/* return the number of lines deleted */
	return -(num_lines - changed);
}


/* Remove Duplicate Lines, ordered */
gint
rmdupln(gchar **lines, gint num_lines, gchar *new_file)
{
	gchar *nf_end  = new_file;  /* points to last char of new_file */
	gint  i        = 0;         /* iterator */
	gint  j        = 0;         /* iterator */
	gboolean *to_remove = NULL; /* flag to 'mark' which lines to remove */
	gint  changed  = 0;         /* number of lines removed */
	lo_strcmpfns lo_strcmp = getcmpfns();


	/* allocate and set *to_remove to all FALSE
	 * to_remove[i] represents whether lines[i] should be removed  */
	to_remove = g_malloc(sizeof(gboolean) * num_lines);
	for (i = 0; i < (num_lines); i++)
		to_remove[i] = FALSE;

	/* find which **lines are duplicate, and mark them as duplicate */
	for (i = 0; i < num_lines; i++)  /* loop through **lines */
	{
		/* make sure that the line is not already duplicate */
		if (!to_remove[i])
		{
			/* find the rest of same lines */
			for (j = (i + 1); j < num_lines; j++)
			{
				if (!to_remove[j] && lo_strcmp(lines[i], lines[j]) == 0)
					to_remove[j] = TRUE; /* line is duplicate, mark to remove */
			}
		}
	}

	/* copy **lines into 'new_file' if it is not FALSE (not duplicate) */
	for (i = 0; i < num_lines; i++)
		if (!to_remove[i])
		{
			changed++;     /* number of lines kept */
			nf_end = g_stpcpy(nf_end, lines[i]);
		}

	/* free used memory */
	g_free(to_remove);

	/* return the number of lines deleted */
	return -(num_lines - changed);
}


/* Remove Unique Lines */
gint
rmunqln(gchar **lines, gint num_lines, gchar *new_file)
{
	gchar *nf_end = new_file;   /* points to last char of new_file */
	gint  i       = 0;          /* iterator */
	gint  j       = 0;          /* iterator */
	gboolean *to_remove = NULL; /* to 'mark' which lines to remove */
	gint  changed = 0;          /* number of lines removed */
	lo_strcmpfns lo_strcmp = getcmpfns();


	/* allocate and set *to_remove to all TRUE
	 * to_remove[i] represents whether lines[i] should be removed */
	to_remove = g_malloc(sizeof(gboolean) * num_lines);
	for (i = 0; i < num_lines; i++)
		to_remove[i] = TRUE;

	/* find all unique lines and set them to FALSE (not to be removed) */
	for (i = 0; i < num_lines; i++)
		/* make sure that the line is not already determined to be unique */
		if (to_remove[i])
			for (j = (i + 1); j < num_lines; j++)
				if (to_remove[j] && lo_strcmp(lines[i], lines[j]) == 0)
				{
					to_remove[i] = FALSE;
					to_remove[j] = FALSE;
				}

	/* copy **lines into 'new_file' if it is not FALSE(not duplicate) */
	for (i = 0; i < num_lines; i++)
		if (!to_remove[i])
		{
			changed++;     /* number of lines kept */
			nf_end = g_stpcpy(nf_end, lines[i]);
		}

	/* free used memory */
	g_free(to_remove);

	/* return the number of lines deleted */
	return -(num_lines - changed);
}


/* Keep Unique Lines */
gint
kpunqln(gchar **lines, gint num_lines, gchar *new_file)
{
	gchar *nf_end = new_file;   /* points to last char of new_file */
	gint  i       = 0;          /* iterator */
	gint  j       = 0;          /* iterator */
	gboolean *to_remove = NULL; /* to 'mark' which lines to remove */
	gint  changed = 0;          /* number of lines removed */
	lo_strcmpfns lo_strcmp = getcmpfns();


	/* allocate and set *to_remove to all FALSE
	 * to_remove[i] represents whether lines[i] should be removed */
	to_remove = g_malloc(sizeof(gboolean) * num_lines);
	for (i = 0; i < num_lines; i++)
		to_remove[i] = FALSE;

	/* find all non unique lines and set them to TRUE (to be removed) */
	for (i = 0; i < num_lines; i++)
		/* make sure that the line is not already determined to be non unique */
		if (!to_remove[i])
			for (j = (i + 1); j < num_lines; j++)
				if (!to_remove[j] && lo_strcmp(lines[i], lines[j]) == 0)
				{
					to_remove[i] = TRUE;
					to_remove[j] = TRUE;
				}

	/* copy **lines into 'new_file' if it is not FALSE(not duplicate) */
	for (i = 0; i < num_lines; i++)
		if (!to_remove[i])
		{
			changed++;     /* number of lines kept */
			nf_end = g_stpcpy(nf_end, lines[i]);
		}

	/* free used memory */
	g_free(to_remove);

	/* return the number of lines deleted */
	return -(num_lines - changed);
}


/* Remove Empty Lines */
gint
rmemtyln(ScintillaObject *sci, gint line_num, gint end_line_num)
{
	gint changed = 0;     /* number of lines removed */

	while (line_num <= end_line_num)    /* loop through lines */
	{
		/* check if the first posn of the line is also the end of line posn */
		if (sci_get_position_from_line(sci, line_num) ==
			sci_get_line_end_position (sci, line_num))
		{
			scintilla_send_message(sci,
								   SCI_DELETERANGE,
								   sci_get_position_from_line(sci, line_num),
								   sci_get_line_length(sci, line_num));

			line_num--;
			end_line_num--;
			changed++;
		}
		line_num++;
	}

	/* return the number of lines deleted */
	return -changed;
}


/* Remove Whitespace Lines */
gint
rmwhspln(ScintillaObject *sci, gint line_num, gint end_line_num)
{
	gint indent;                       /* indent position */
	gint changed = 0;                  /* number of lines removed */

	while (line_num <= end_line_num)    /* loop through lines */
	{
		indent = scintilla_send_message(sci,
										SCI_GETLINEINDENTPOSITION,
										line_num, 0);

		/* check if the posn of indentation is also the end of line posn */
		if (indent -
				sci_get_position_from_line(sci, line_num) ==
				sci_get_line_end_position (sci, line_num) -
				sci_get_position_from_line(sci, line_num))
		{
			scintilla_send_message(sci,
								   SCI_DELETERANGE,
								   sci_get_position_from_line(sci, line_num),
								   sci_get_line_length(sci, line_num));

			line_num--;
			end_line_num--;
			changed++;
		}
		line_num++;
	}

	/* return the number of lines deleted */
	return -changed;
}


/* Sort Lines Ascending */
gint
sortlnsasc(gchar **lines, gint num_lines, gchar *new_file)
{
	gchar *nf_end = new_file;          /* points to last char of new_file */
	gint i;

	qsort(lines, num_lines, sizeof(gchar *), compare_asc);

	/* join **lines into one string (new_file) */
	for (i = 0; i < num_lines; i++)
		nf_end = g_stpcpy(nf_end, lines[i]);

	return num_lines;
}


/* Sort Lines Descending */
gint
sortlndesc(gchar **lines, gint num_lines, gchar *new_file)
{
	gchar *nf_end = new_file;          /* points to last char of new_file */
	gint i;

	qsort(lines, num_lines, sizeof(gchar *), compare_desc);

	/* join **lines into one string (new_file) */
	for (i = 0; i < num_lines; i++)
		nf_end = g_stpcpy(nf_end, lines[i]);

	return num_lines;
}


/* Remove Every Nth Line */
gint
rmnthln(ScintillaObject *sci, gint line_num, gint end_line_num)
{
	gboolean ok;
	gdouble n;
	gint count;
	gint changed = 0;     /* number of lines removed */

	ok = dialogs_show_input_numeric(_("Remove every Nth line"),
									_("Value of N"), &n, 1, 1000, 1);
	if (ok == FALSE)
	{
		return 0;
	}

	count = n;
	while (line_num <= end_line_num)    /* loop through lines */
	{
		count--;

		/* check if this is the nth line. */
		if (count == 0)
		{
			scintilla_send_message(sci,
								   SCI_DELETERANGE,
								   sci_get_position_from_line(sci, line_num),
								   sci_get_line_length(sci, line_num));

			line_num--;
			end_line_num--;
			changed++;
			count = n;
		}
		line_num++;
	}

	/* return the number of lines deleted */
	return -changed;
}
