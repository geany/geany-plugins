 /*
 *      bibtex.h
 *
 *      Copyright 2008-2010 Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
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
	GLATEX_BIBTEX_ARTICLE = 0,
	GLATEX_BIBTEX_BOOK,
	GLATEX_BIBTEX_BOOKLET,
	GLATEX_BIBTEX_CONFERENCE,
	GLATEX_BIBTEX_INBOOK,
	GLATEX_BIBTEX_INCOLLECTION,
	GLATEX_BIBTEX_INPROCEEDINGS,
	GLATEX_BIBTEX_MANUAL,
	GLATEX_BIBTEX_MASTERSTHESIS,
	GLATEX_BIBTEX_MISC,
	GLATEX_BIBTEX_PHDTHESIS,
	GLATEX_BIBTEX_PROCEEDINGS,
	GLATEX_BIBTEX_TECHREPORT,
	GLATEX_BIBTEX_UNPUBLISHED,
	GLATEX_BIBTEX_N_TYPES
};

enum {
	GLATEX_BIBTEX_ADDRESS = 0,
	GLATEX_BIBTEX_ANNOTE,
	GLATEX_BIBTEX_AUTHOR,
	GLATEX_BIBTEX_BOOKTITLE,
	GLATEX_BIBTEX_CHAPTER,
	GLATEX_BIBTEX_CROSSREF,
	GLATEX_BIBTEX_EDITION,
	GLATEX_BIBTEX_EDITOR,
	GLATEX_BIBTEX_EPRINT,
	GLATEX_BIBTEX_HOWPUBLISHED,
	GLATEX_BIBTEX_INSTITUTION,
	GLATEX_BIBTEX_JOURNAL,
	GLATEX_BIBTEX_KEY,
	GLATEX_BIBTEX_MONTH,
	GLATEX_BIBTEX_NOTE,
	GLATEX_BIBTEX_NUMBER,
	GLATEX_BIBTEX_ORGANIZATION,
	GLATEX_BIBTEX_PAGES,
	GLATEX_BIBTEX_PUBLISHER,
	GLATEX_BIBTEX_SCHOOL,
	GLATEX_BIBTEX_SERIES,
	GLATEX_BIBTEX_TITLE,
	GLATEX_BIBTEX_TYPE,
	GLATEX_BIBTEX_URL,
	GLATEX_BIBTEX_VOLUME,
	GLATEX_BIBTEX_YEAR,
	GLATEX_BIBTEX_N_ENTRIES
};

extern gchar *glatex_label_types[];

extern const gchar *glatex_label_tooltips[];

extern const gchar *glatex_label_entry_keywords[];

extern const gchar *glatex_label_entry[];

int glatex_push_bibtex_entry(int style, GeanyDocument *doc);

void glatex_insert_bibtex_entry(G_GNUC_UNUSED GtkMenuItem * menuitem, gpointer gdata);

void glatex_bibtex_write_entry(GPtrArray *entry, gint doctype);

GPtrArray *glatex_bibtex_init_empty_entry();
#endif
