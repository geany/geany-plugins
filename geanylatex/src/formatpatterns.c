/*
 *      formatpatterns.c
 *
 *      Copyright 2009 Frank Lanitz <frank@Jupiter>
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

#include "geanylatex.h"

gchar* glatex_format_pattern[] = {
	"\\textit",
	"\\textbf",
	"\\underline",
	"\\textsl",
	"\\texttt",
	"\\textsc",
	"\\emph",
	"\\centering",
	"\\raggedleft",
	"\\raggedright",
};

const gchar *glatex_format_labels[] = {
	N_("Italic"),
	N_("Boldfont"),
	N_("Underline"),
	N_("Slanted"),
	N_("Typewriter"),
	N_("Small Caps"),
	N_("Emphasis"),
	N_("Centered"),
	N_("Left side oriented"),
	N_("Right side oriented")
};
