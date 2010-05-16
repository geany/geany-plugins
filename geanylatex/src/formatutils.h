/*
 *	  formatutils.h
 *
 *	  Copyright 2009-2010 Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
 *
 *	  This program is free software; you can redistribute it and/or modify
 *	  it under the terms of the GNU General Public License as published by
 *	  the Free Software Foundation; either version 2 of the License, or
 *	  (at your option) any later version.
 *
 *	  This program is distributed in the hope that it will be useful,
 *	  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	  GNU General Public License for more details.
 *
 *	  You should have received a copy of the GNU General Public License
 *	  along with this program; if not, write to the Free Software
 *	  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *	  MA 02110-1301, USA.
 */

#ifndef LATEXFORMATUTILS_H
#define LATEXFORMATUTILS_H

#include "geanylatex.h"

enum {
	LATEX_ITALIC = 0,
	LATEX_BOLD,
	LATEX_UNDERLINE,
	LATEX_SLANTED,
	LATEX_TYPEWRITER,
	LATEX_SMALLCAPS,
	LATEX_EMPHASIS,
	LATEX_CENTER,
	LATEX_LEFT,
	LATEX_RIGHT,
	LATEX_STYLES_END
};

/* Couting from smalles on up to biggest default font size. Keep in mind:
 * LATEX_FONTSIZE_LARGE1 -> large
 * LATEX_FONTSIZE_LARGE2 -> Large
 * LATEX_FONTSIZE_LARGE3 -> LARGE
 * LATEX_FONTSIZE_HUGE1 -> huge
 * LATEX_FONTSIZE_HUGE2 -> Huge */
enum {
	LATEX_FONTSIZE_TINY = 0,
	LATEX_FONTSIZE_SCRIPTSIZE,
	LATEX_FONTSIZE_FOOTNOTESIZE,
	LATEX_FONTSIZE_SMALL,
	LATEX_FONTSIZE_NORMALSIZE,
	LATEX_FONTSIZE_LARGE1,
	LATEX_FONTSIZE_LARGE2,
	LATEX_FONTSIZE_LARGE3,
	LATEX_FONTSIZE_HUGE1,
	LATEX_FONTSIZE_HUGE2,
	LATEX_FONTSIZE_END
};

extern gchar* glatex_format_pattern[];
extern const gchar *glatex_format_labels[];
extern const gchar *glatex_fontsize_labels[];
extern gchar *glatex_fontsize_pattern[];
void glatex_insert_latex_format(GtkMenuItem * menuitem, gpointer gdata);
void glatex_insert_latex_fontsize(GtkMenuItem * menuitem, gpointer gdata);

#endif
