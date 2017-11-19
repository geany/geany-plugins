 /*
 *      bibtexlabels.c
 *
 *      Copyright 2009-2012 Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
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

#include "latex.h"
#include "bibtex.h"
#include "datatypes.h"

BibTeXType glatex_bibtex_types[] = {
	{ "Article", N_("Article (@Article)")},
	{ "Book", N_("Book (@Book)")},
	{ "Booklet", N_("Booklet (@Booklet)")},
	{ "Conference", N_("Conference (@Conference)")},
	{ "Inbook", N_("Inbook (@Inbook)")},
	{ "Incollection", N_("Incollection (@Incollection)")},
	{ "Inproceedings", N_("Inproceedings (@Inproceedings)")},
	{ "Manual", N_("Manual (@Manual)")},
	{ "Mastersthesis", N_("Mastersthesis (@Mastersthesis)")},
	{ "Misc", N_("Misc (@Misc)")},
	{ "PhdThesis", N_("PhdThesis (@PhdThesis)")},
	{ "Proceedings", N_("Proceedings (@Proceedings)")},
	{ "Techreport", N_("Techreport (@Techreport)")},
	{ "Unpublished", N_("Unpublished (@Unpublished)")},
	{ NULL, NULL}
};


const gchar *glatex_label_entry[] = {
	N_("Address"),
	N_("Annote"),
	N_("Author"),
	N_("Booktitle"),
	N_("Chapter"),
	N_("Crossref"),
	N_("Edition"),
	N_("Editor"),
	N_("E-print"),
	N_("HowPublished"),
	N_("Institution"),
	N_("Journal"),
	N_("Key"),
	N_("Month"),
	N_("Note"),
	N_("Number"),
	N_("Organization"),
	N_("Pages"),
	N_("Publisher"),
	N_("School"),
	N_("Series"),
	N_("Title"),
	N_("Type"),
	N_("URL"),
	N_("Volume"),
	N_("Year")};

const gchar *glatex_label_entry_keywords[] = {
	("Address"),
	("Annote"),
	("Author"),
	("Booktitle"),
	("Chapter"),
	("Crossref"),
	("Edition"),
	("Editor"),
	("E-print"),
	("HowPublished"),
	("Institution"),
	("Journal"),
	("Key"),
	("Month"),
	("Note"),
	("Number"),
	("Organization"),
	("Pages"),
	("Publisher"),
	("School"),
	("Series"),
	("Title"),
	("Type"),
	("URL"),
	("Volume"),
	("Year"),
	( NULL) };

const gchar *glatex_label_tooltips[] = {
	N_("Address of publisher"),
	N_("Annotation for annotated bibliography styles"),
	N_("Name(s) of the author(s), separated by 'and' if more than one"),
	N_("Title of the book, if only part of it is being cited"),
	N_("Chapter number"),
	N_("Citation key of the cross-referenced entry"),
	N_("Edition of the book (such as \"first\" or \"second\")"),
	N_("Name(s) of the editor(s), separated by 'and' if more than one"),
	N_("Specification of electronic publication"),
	N_("Publishing method if the method is nonstandard"),
	N_("Institution that was involved in the publishing"),
	N_("Journal or magazine in which the work was published"),
	N_("Hidden field used for specifying or overriding the alphabetical order of entries"),
	N_("Month of publication or creation if unpublished"),
	N_("Miscellaneous extra information"),
	N_("Number of journal, magazine, or tech-report"),
	N_("Sponsor of the conference"),
	N_("Page numbers separated by commas or double-hyphens"),
	N_("Name of publisher"),
	N_("School where thesis was written"),
	N_("Series of books in which the book was published"),
	N_("Title of the work"),
	N_("Type of technical report"),
	N_("Internet address"),
	N_("Number of the volume"),
	N_("Year of publication or creation if unpublished")};
