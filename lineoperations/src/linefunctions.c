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


// isspace() without newline comparisons
gboolean issp(gchar c)
{
	return (c == ' ' || c == '\t' || c == '\f' || c == '\v');
}


// iswhitespaceline(gchar* line) checks if line is a whitespace line
gboolean iswhitespaceline(gchar* line)
{
	gint i;

	for(i = 0; line[i] != '\0'; i++)
		if(!issp(line[i]))
			return FALSE;
	return TRUE;
}


// comparison function to be used in qsort
static gint compareasc(const void * a, const void * b)
{
	return strcmp(*(const gchar **) a, *(const gchar **) b);
}


// comparison function to be used in qsort
static gint comparedesc(const void * a, const void * b)
{
	return strcmp(*(const gchar **) b, *(const gchar **) a);
}


// Remove Duplicate Lines, sorted
void rmdupst(GeanyDocument *doc) {
	gint  totalnumchars;    // number of characters in the document
	gint  totalnumlines;    // number of lines in the document
	gchar **lines;          // array to hold all lines in the document
	gint  numlines;         // number of lines in **lines array
	gchar *newfile;         // *final* string to replace current document
	gint  nfposn;           // keeps track of the position of newfile string
	gint  i;                // iterator
	gint  j;                // iterator
	gint  k;                // iterator

	totalnumchars = sci_get_length(doc->editor->sci);
	totalnumlines = sci_get_line_count(doc->editor->sci);
	lines         = g_malloc(sizeof(gchar *) * totalnumlines);
	newfile       = g_malloc(sizeof(gchar) * (totalnumchars+1));
	numlines      = 0;
	nfposn        = 0;
	k             = 0;

	if(newfile && lines)    // verify memory allocation worked
	{
		// put contents of document into **lines array
		for(i = 0; i < totalnumlines; i++)
			lines[numlines++] = sci_get_line(doc->editor->sci, i);

		qsort(lines, numlines, sizeof(gchar *), compareasc);

		if(numlines > 0)	// copy the first line into *newfile
			for(j = 0; lines[0][j] != '\0'; j++)
				newfile[nfposn++] = lines[0][j];

		for(i = 0; i < numlines; i++)	// copy 1st occurance of each line
										// into *newfile
			if(strcmp(lines[k], lines[i]) != 0)
			{
				for(j = 0; lines[i][j] != '\0'; j++)
					newfile[nfposn++] = lines[i][j];
				k = i;
			}
		newfile[nfposn] = '\0';

	}

	// set the *newfile as current document
	sci_set_text(doc->editor->sci, newfile);

	// free used memory
	for(i = 0; i < numlines; i++)
		if(lines[i]) g_free(lines[i]);
	if(lines)   g_free(lines);
	if(newfile) g_free(newfile);
}


// Remove Duplicate Lines, ordered
void rmdupln(GeanyDocument *doc) {
	gint  totalnumchars;    // number of characters in the document
	gint  totalnumlines;    // number of lines in the document
	gchar **lines;          // array to hold all lines in the document
	gint  numlines;         // number of lines in lines
	gchar *newfile;         // *final* string to replace current document
	gint  nfposn;           // keeps track of the position of newfile string
	gint  i;                // iterator
	gint  j;                // iterator
	gboolean *toremove;     // flag to 'mark' which lines to remove

	totalnumchars = sci_get_length(doc->editor->sci);
	totalnumlines = sci_get_line_count(doc->editor->sci);
	lines         = g_malloc(sizeof(gchar *) * totalnumlines);
	newfile       = g_malloc(sizeof(gchar) * (totalnumchars+1));
	numlines      = 0;
	nfposn        = 0;
	toremove      = NULL;

	if(newfile && lines)
	{
		// put contents of document into **lines array
		for(i = 0; i < totalnumlines; i++)
			lines[numlines++] = sci_get_line(doc->editor->sci, i);

		// allocate and set *toremove to all FALSE
		toremove = g_malloc(sizeof(gboolean) * numlines);
		for(i = 0; i < (numlines); i++)
			toremove[i] = FALSE;

		// find which lines are duplicate
		for(i = 0; i < numlines; i++)
			// make sure that the line is not already duplicate
			if(!toremove[i])
				for(j = (i+1); j < numlines; j++)
					if(!toremove[j])
						if(strcmp(lines[i], lines[j]) == 0) {
							toremove[j] = TRUE;
							//toremove[i] = TRUE;//remove all occurrances
						}

		// copy line into 'newfile' if it is not FALSE(unique)
		for(i = 0; i < numlines; i++)
		{
			if(!toremove[i])
				for(j = 0; lines[i][j] != '\0'; j++)
					newfile[nfposn++] = lines[i][j];
			if(lines[i]) g_free(lines[i]);
		}
		newfile[nfposn] = '\0';
	}
	newfile[nfposn] = '\0';
	sci_set_text(doc->editor->sci, newfile);

	// each line is freed in above for-loop
	if(lines)    g_free(lines);
	if(newfile)  g_free(newfile);
	if(toremove) g_free(toremove);
}



void rmunqln(GeanyDocument *doc) {
	gint totalnumchars; // number of characters in the document
	gint totalnumlines; // number of lines in the document
	gchar **lines;      // array to hold all lines in the document
	gint numlines;      // number of lines in lines
	gchar *newfile;     // *final* string to replace current document
	gint nfposn;        // keeps track of the position of newfile string
	gint i;             // iterator
	gint j;             // iterator
	gboolean *toremove; // to 'mark' which lines to remove

	totalnumchars = sci_get_length(doc->editor->sci);
	totalnumlines = sci_get_line_count(doc->editor->sci);
	lines         = g_malloc(sizeof(gchar *) * totalnumlines);
	newfile       = g_malloc(sizeof(gchar) * (totalnumchars+1));
	numlines      = 0;
	nfposn        = 0;
	toremove      = NULL;

	if(newfile && lines)
	{
		// copy *all* lines into **lines array
		for(i = 0; i < totalnumlines; i++)
			lines[numlines++] = sci_get_line(doc->editor->sci, i);

		toremove = g_malloc(sizeof(gboolean) * numlines);
		for(i = 0; i < (numlines); i++)
			toremove[i] = TRUE;

		// set all unique rows to FALSE
		// toremove[i] corresponds to lines[i]
		for(i = 0; i < numlines; i++)
			if(toremove[i])
				for(j = (i+1); j < numlines; j++)
					if(toremove[j])
						if(strcmp(lines[i], lines[j]) == 0)
						{
							toremove[i] = FALSE;
							toremove[j] = FALSE;
						}


		// copy line into 'newfile' if it is not FALSE(unique)
		for(i = 0; i < numlines; i++)
		{
			if(!toremove[i])
				for(j = 0; lines[i][j] != '\0'; j++)
					newfile[nfposn++] = lines[i][j];
			if(lines[i]) g_free(lines[i]);
		}
		newfile[nfposn] = '\0';
	}
	newfile[nfposn] = '\0';
	sci_set_text(doc->editor->sci, newfile);

	// each line is freed in above for-loop
	if(lines)    g_free(lines);
	if(newfile)  g_free(newfile);
	if(toremove) g_free(toremove);
}



void rmemtyln(GeanyDocument *doc) {
	gint  totalnumchars;    // number of characters in the document
	gint  totalnumlines;    // number of lines in the document
	gchar *line;            // temporary line
	gint  linelen;          // length of *line
	gchar *newfile;         // pointer for new document
	gint  nfposn;           // position to the last character in newfile
	gint  i;                // temporary iterator number
	gint j;                 // iterator


	totalnumchars = sci_get_length(doc->editor->sci);
	totalnumlines = sci_get_line_count(doc->editor->sci);
	/*
	 * Allocate space for the new document (same amount as open document.
	 * If the document contains lines with only whitespace,
	 * not all of this space will be used.)
	*/
	newfile   = g_malloc(sizeof(gchar) * (totalnumchars+1));
	nfposn    = 0;

	if(newfile) // make sure newfile memory allocation has not failed (NULL)
	{
		for(i = 0; i < totalnumlines; i++)    // loop through opened doc char by char
		{
			linelen = sci_get_line_length(doc->editor->sci, i);

			if(linelen == 2)
			{
				line = sci_get_line(doc->editor->sci, i);

				if(line[0] != '\r')
					for(j = 0; line[j] != '\0'; j++)
						newfile[nfposn++] = line[j];
			}
			else if(linelen != 1) {
				line = sci_get_line(doc->editor->sci, i);

				for(j = 0; line[j] != '\0'; j++)
					newfile[nfposn++] = line[j];
			}
		}
		newfile[nfposn] = '\0';


		// set old file with new file
		sci_set_text(doc->editor->sci, newfile);

		g_free(newfile);
	}
}




void rmwhspln(GeanyDocument *doc) {
	gint  totalnumchars;    // number of characters in the document
	gint  totalnumlines;    // number of lines in the document
	gchar *line;            // temporary line
	gint  linelen;          // length of *line
	gchar *newfile;         // pointer for new document
	gint  nfposn;           // position to the last character in newfile
	gint  i;                // temporary iterator number
	gint j;                 // iterator


	totalnumchars = sci_get_length(doc->editor->sci);
	totalnumlines = sci_get_line_count(doc->editor->sci);
	/*
	 * Allocate space for the new document (same amount as open document.
	 * If the document contains lines with only whitespace,
	 * not all of this space will be used.)
	*/
	newfile   = g_malloc(sizeof(gchar) * (totalnumchars+1));
	nfposn    = 0;

	if(newfile) // make sure newfile memory allocation has not failed (NULL)
	{
		for(i = 0; i < totalnumlines; i++)    // loop through opened doc char by char
		{
			linelen = sci_get_line_length(doc->editor->sci, i);

			if(linelen == 2)
			{
				line = sci_get_line(doc->editor->sci, i);

				if(line[0] != '\r')
					for(j = 0; line[j] != '\0'; j++)
						newfile[nfposn++] = line[j];
			}
			else  if(linelen != 1) {
				line = sci_get_line(doc->editor->sci, i);
				if(!iswhitespaceline(line))
					for(j = 0; line[j] != '\0'; j++)
						newfile[nfposn++] = line[j];
			}

		}
		newfile[nfposn] = '\0';


		// set old file with new file
		sci_set_text(doc->editor->sci, newfile);

		g_free(newfile);
	}
}



void sortlines(GeanyDocument *doc, gboolean asc) {
	gint  totalnumchars;    // number of characters in the document
	gint  totalnumlines;    // number of lines in the document
	gchar **lines;          // array to hold all lines in the document
	gint  numlines;         // number of lines in **lines array
	gchar *newfile;         // *final* string to replace current document
	gint  nfposn;           // keeps track of the position of newfile string
	gint  i;                // iterator
	gint  j;                // iterator

	totalnumchars = sci_get_length(doc->editor->sci);
	totalnumlines = sci_get_line_count(doc->editor->sci);
	lines         = g_malloc(sizeof(gchar *) * totalnumlines);
	newfile       = g_malloc(sizeof(gchar) * (totalnumchars+1));
	numlines      = 0;
	nfposn        = 0;

	if(newfile && lines)    // verify memory allocation worked
	{
		for(i = 0; i < totalnumlines; i++)
			lines[numlines++] = sci_get_line(doc->editor->sci, i);

		if(asc)
			qsort(lines, numlines, sizeof(gchar *), compareasc);
		else
			qsort(lines, numlines, sizeof(gchar *), comparedesc);


		for(i = 0; i < numlines; i++)
		{
			for(j = 0; lines[i][j] != '\0'; j++)
				newfile[nfposn++] = lines[i][j];

			if(lines[i]) g_free(lines[i]);
		}
		newfile[nfposn] = '\0';

	}

	sci_set_text(doc->editor->sci, newfile);


	// each line is freed in above for-loop
	if(lines)   g_free(lines);
	if(newfile) g_free(newfile);
}
