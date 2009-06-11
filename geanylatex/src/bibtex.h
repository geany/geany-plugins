 /*
 *      bibtex.h
 *
 *      Copyright 2008 Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
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
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#ifndef LATEXBIBTEX_H
#define LATEXBIBTEX_H

#include "geanylatex.h"


/* Define generic stuff */
enum {
	ARTICLE = 0,
	BOOK,
	BOOKLET,
	CONFERENCE,
	INBOOK,
	INCOLLECTION,
	INPROCEEDINGS,
	MANUAL,
	MASTERSTHESIS,
	MISC,
	PHDTHESIS,
	PROCEEDINGS,
	TECHREPORT,
	UNPUBLISHED,
	N_TYPES
};


enum {
	ADDRESS = 0,
	ANNOTE,
	AUTHOR,
	BOOKTITLE,
	CHAPTER,
	CROSSREF,
	EDITION,
	EDITOR,
	EPRINT,
	HOWPUBLISHED,
	INSTITUTION,
	JOURNAL,
	KEY,
	MONTH,
	NOTE,
	NUMBER,
	ORGANIZATION,
	PAGES,
	PUBLISHER,
	SCHOOL,
	SERIES,
	TITLE,
	TYPE,
	URL,
	VOLUME,
	YEAR,
	N_ENTRIES
};

extern gchar *glatex_label_types[];

extern const gchar *glatex_label_tooltips[];

extern const gchar *glatex_label_entry_keywords[];

extern const gchar *glatex_label_entry[];

int glatex_push_bibtex_entry(int style, GeanyDocument *doc);

void glatex_insert_bibtex_entry(G_GNUC_UNUSED GtkMenuItem * menuitem,
								G_GNUC_UNUSED gpointer gdata);

#endif
