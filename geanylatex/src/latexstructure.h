/*
 *      latexstructure.h
 *
 *      Copyright 2009-2010 Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
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

#ifndef LATEXSTRUCTURE_H
#define LATEXSTRUCTURE_H

#include "geanylatex.h"

enum {
	GLATEX_STRUCTURE_PART = 0,
	GLATEX_STRUCTURE_CHAPTER,
	GLATEX_STRUCTURE_SECTION,
	GLATEX_STRUCTURE_SUBSECTION,
	GLATEX_STRUCTURE_SUBSUBSECTION,
	GLATEX_STRUCTURE_PARAGRAPH,
	GLATEX_STRUCTURE_SUBPARAGRAPH,
	GLATEX_STRUCTURE_SUBSUBPARAGRAPH,
	GLATEX_STRUCTURE_N_LEVEL};

extern const gchar *glatex_structure_values[];

void glatex_structure_lvlup(void);
void glatex_structure_lvldown(void);
gint glatex_structure_rotate(gboolean direction, gint start);
#endif
