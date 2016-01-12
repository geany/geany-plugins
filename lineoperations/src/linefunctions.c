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


// isspace()
gboolean is_sp(gchar c)
{
	return (c == ' ' || c == '\t' || c == '\f' ||
			c == '\v' || c == '\r' || c == '\n');
}


// is_whitespace_line(gchar* line) checks if line is a whitespace line
gboolean is_whitespace_line(gchar* line)
{
	gint i;

	for(i = 0; line[i] != '\0'; i++)
		if(!is_sp(line[i]))
			return FALSE;
	return TRUE;
}


// comparison function to be used in qsort
static gint compare_asc(const void * a, const void * b)
{
	return strcmp(*(const gchar **) a, *(const gchar **) b);
}


// comparison function to be used in qsort
static gint compare_desc(const void * a, const void * b)
{
	return strcmp(*(const gchar **) b, *(const gchar **) a);
}


// Remove Duplicate Lines, sorted
void rmdupst(GeanyDocument *doc) {
	gint  total_num_chars;  // number of characters in the document
	gint  total_num_lines;  // number of lines in the document
	gchar **lines;          // array to hold all lines in the document
	gchar *new_file;        // *final* string to replace current document
	gint  nfposn;           // keeps track of the position of new_file string
	gint  i;                // iterator
	gint  j;                // iterator
	gint  k;                // iterator

	total_num_chars = sci_get_length(doc->editor->sci);
	total_num_lines = sci_get_line_count(doc->editor->sci);
	lines           = g_malloc(sizeof(gchar *) * total_num_lines);
	new_file        = g_malloc(sizeof(gchar) * (total_num_chars+1));
	nfposn          = 0;
	k               = 0;



	// copy *all* lines into **lines array
	for(i = 0; i < total_num_lines; i++)
		lines[i] = sci_get_line(doc->editor->sci, i);

	qsort(lines, total_num_lines, sizeof(gchar *), compare_asc);

	if(total_num_lines > 0)	// copy the first line into *new_file
		for(j = 0; lines[0][j] != '\0'; j++)
			new_file[nfposn++]  = lines[0][j];

	for(i = 0; i < total_num_lines; i++)	// copy 1st occurance of each line
									// into *new_file
		if(strcmp(lines[k], lines[i]) != 0)
		{
			for(j = 0; lines[i][j] != '\0'; j++)
				new_file[nfposn++]  = lines[i][j];
			k = i;
		}

	new_file[nfposn] = '\0';
	sci_set_text(doc->editor->sci, new_file);	// set new document



	// free used memory
	for(i = 0; i < total_num_lines; i++)
		g_free(lines[i]);
	g_free(lines);
	g_free(new_file);
}


// Remove Duplicate Lines, ordered
void rmdupln(GeanyDocument *doc) {
	gint  total_num_chars;  // number of characters in the document
	gint  total_num_lines;  // number of lines in the document
	gchar **lines;          // array to hold all lines in the document
	gchar *new_file;        // *final* string to replace current document
	gint  nfposn;           // keeps track of the position of new_file string
	gint  i;                // iterator
	gint  j;                // iterator
	gboolean *to_remove;    // flag to 'mark' which lines to remove

	total_num_chars = sci_get_length(doc->editor->sci);
	total_num_lines = sci_get_line_count(doc->editor->sci);
	lines           = g_malloc(sizeof(gchar *) * total_num_lines);
	new_file        = g_malloc(sizeof(gchar) * (total_num_chars+1));
	nfposn          = 0;
	to_remove       = NULL;



	// copy *all* lines into **lines array
	for(i = 0; i < total_num_lines; i++)
		lines[i] = sci_get_line(doc->editor->sci, i);

	// allocate and set *to_remove to all FALSE
	to_remove = g_malloc(sizeof(gboolean) * total_num_lines);
	for(i = 0; i < (total_num_lines); i++)
		to_remove[i] = FALSE;

	// find which lines are duplicate
	for(i = 0; i < total_num_lines; i++)
		// make sure that the line is not already duplicate
		if(!to_remove[i])
			for(j = (i+1); j < total_num_lines; j++)
				if(!to_remove[j])
					if(strcmp(lines[i], lines[j]) == 0) {
						to_remove[j] = TRUE;   // this line is duplicate,
						                       // mark to remove
						//to_remove[i] = TRUE; //remove all occurrances
					}

	// copy line into 'new_file' if it is not FALSE(not duplicate)
	for(i = 0; i < total_num_lines; i++)
	{
		if(!to_remove[i])
			for(j = 0; lines[i][j] != '\0'; j++)
				new_file[nfposn++]  = lines[i][j];
		g_free(lines[i]);
	}

	new_file[nfposn] = '\0';
	sci_set_text(doc->editor->sci, new_file);	// set new document



	// each line is freed in above for-loop
	g_free(lines);
	g_free(new_file);
	g_free(to_remove);
}



void rmunqln(GeanyDocument *doc) {
	gint total_num_chars; // number of characters in the document
	gint total_num_lines; // number of lines in the document
	gchar **lines;        // array to hold all lines in the document
	gchar *new_file;      // *final* string to replace current document
	gint nfposn;          // keeps track of the position of new_file string
	gint i;               // iterator
	gint j;               // iterator
	gboolean *to_remove;  // to 'mark' which lines to remove

	total_num_chars = sci_get_length(doc->editor->sci);
	total_num_lines = sci_get_line_count(doc->editor->sci);
	lines           = g_malloc(sizeof(gchar *) * total_num_lines);
	new_file        = g_malloc(sizeof(gchar) * (total_num_chars+1));
	nfposn          = 0;
	to_remove       = NULL;


	// copy *all* lines into **lines array
	for(i = 0; i < total_num_lines; i++)
		lines[i] = sci_get_line(doc->editor->sci, i);

	// allocate and set *to_remove to all TRUE
	to_remove = g_malloc(sizeof(gboolean) * total_num_lines);
	for(i = 0; i < (total_num_lines); i++)
		to_remove[i] = TRUE;

	// set all unique rows to FALSE
	// to_remove[i] corresponds to lines[i]
	for(i = 0; i < total_num_lines; i++)
		if(to_remove[i])
			for(j = (i+1); j < total_num_lines; j++)
				if(to_remove[j])
					if(strcmp(lines[i], lines[j]) == 0)
					{
						to_remove[i] = FALSE;
						to_remove[j] = FALSE;
					}


	// copy line into 'new_file' if it is not FALSE(unique)
	for(i = 0; i < total_num_lines; i++)
	{
		if(!to_remove[i])
			for(j = 0; lines[i][j] != '\0'; j++)
				new_file[nfposn++]  = lines[i][j];
		g_free(lines[i]);
	}

	new_file[nfposn] = '\0';
	sci_set_text(doc->editor->sci, new_file);	// set new document



	// each line is freed in above for-loop
	g_free(lines);
	g_free(new_file);
	g_free(to_remove);
}



void rmemtyln(GeanyDocument *doc) {
	gint  total_num_chars;  // number of characters in the document
	gint  total_num_lines;  // number of lines in the document
	gchar *line;            // temporary line
	gint  linelen;          // length of *line
	gchar *new_file;        // *final* string to replace current document
	gint  nfposn;           // position to the last character in new_file
	gint  i;                // temporary iterator number
	gint j;                 // iterator


	total_num_chars = sci_get_length(doc->editor->sci);
	total_num_lines = sci_get_line_count(doc->editor->sci);
	/*
	 * Allocate space for the new document (same amount as open document.
	 * If the document contains lines with only whitespace,
	 * not all of this space will be used.)
	*/
	new_file   = g_malloc(sizeof(gchar) * (total_num_chars+1));
	nfposn     = 0;



	for(i = 0; i < total_num_lines; i++)    // loop through opened doc char by char
	{
		linelen = sci_get_line_length(doc->editor->sci, i);

		if(linelen == 2)
		{
			line = sci_get_line(doc->editor->sci, i);

			if(line[0] != '\r')
				// copy current line into *new_file
				for(j = 0; line[j] != '\0'; j++)
					new_file[nfposn++] = line[j];
		}
		else if(linelen != 1)
		{
			line = sci_get_line(doc->editor->sci, i);

			// copy current line into *new_file
			for(j = 0; line[j] != '\0'; j++)
				new_file[nfposn++] = line[j];
		}
	}
	new_file[nfposn] = '\0';
	sci_set_text(doc->editor->sci, new_file);	// set new document



	g_free(new_file);
}



// Remove Whitespace Lines
void rmwhspln(GeanyDocument *doc) {
	gint  total_num_chars;  // number of characters in the document
	gint  total_num_lines;  // number of lines in the document
	gchar *line;            // temporary line
	gint  linelen;          // length of *line
	gchar *new_file;        // *final* string to replace current document
	gint  nfposn;           // position to the last character in new_file
	gint  i;                // temporary iterator number
	gint  j;                // iterator


	total_num_chars = sci_get_length(doc->editor->sci);
	total_num_lines = sci_get_line_count(doc->editor->sci);
	/*
	 * Allocate space for the new document (same amount as open document.
	 * If the document contains lines with only whitespace,
	 * not all of this space will be used.)
	*/
	new_file = g_malloc(sizeof(gchar) * (total_num_chars+1));
	nfposn   = 0;

	for(i = 0; i < total_num_lines; i++)    // loop through opened doc
	{
		linelen = sci_get_line_length(doc->editor->sci, i);

		if(linelen == 2)
		{
			line = sci_get_line(doc->editor->sci, i);

			if(line[0] != '\r')
				// copy current line into *new_file
				for(j = 0; line[j] != '\0'; j++)
					new_file[nfposn++] = line[j];
		}
		else if(linelen != 1)
		{
			line = sci_get_line(doc->editor->sci, i);

			if(!is_whitespace_line(line))
				// copy current line into *new_file
				for(j = 0; line[j] != '\0'; j++)
					new_file[nfposn++] = line[j];
		}

	}
	new_file[nfposn] = '\0';
	sci_set_text(doc->editor->sci, new_file);	// set new document

	g_free(new_file);
}


// Sort Lines Ascending and Descending
void sortlines(GeanyDocument *doc, gboolean asc) {
	gint  total_num_chars;  // number of characters in the document
	gint  total_num_lines;  // number of lines in the document
	gchar **lines;          // array to hold all lines in the document
	gchar *new_file;        // *final* string to replace current document
	gint  nfposn;           // keeps track of the position of new_file string
	gint  i;                // iterator
	gint  j;                // iterator

	total_num_chars = sci_get_length(doc->editor->sci);
	total_num_lines = sci_get_line_count(doc->editor->sci);
	lines           = g_malloc(sizeof(gchar *) * total_num_lines);
	new_file        = g_malloc(sizeof(gchar) * (total_num_chars+1));
	nfposn          = 0;



	// copy *all* lines into **lines array
	for(i = 0; i < total_num_lines; i++)
		lines[i] = sci_get_line(doc->editor->sci, i);

	// sort **lines array
	if(asc)
		qsort(lines, total_num_lines, sizeof(gchar *), compare_asc);
	else
		qsort(lines, total_num_lines, sizeof(gchar *), compare_desc);

	// copy **lines array into *new_file
	for(i = 0; i < total_num_lines; i++)
	{
		for(j = 0; lines[i][j] != '\0'; j++)
			new_file[nfposn++] = lines[i][j];

		g_free(lines[i]);
	}
	new_file[nfposn] = '\0';
	sci_set_text(doc->editor->sci, new_file);	// set new document



	// each line is freed in above for-loop
	g_free(lines);
	g_free(new_file);
}
