/*
 *      formatpatterns.c
 *
 *      Copyright 2009-2011 Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
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
	"\\texttt",
	"\\textsc",
	"\\textsl",
	"\\emph",
	"\\centering",
	"\\raggedleft",
	"\\raggedright",
};

const gchar *glatex_format_labels[] = {
	N_("Italic"),
	N_("Bold"),
	N_("Underline"),
	N_("Typewriter"),
	N_("Small Caps"),
	N_("Slanted"),
	N_("Emphasis"),
	N_("Centered"),
	N_("Left side oriented"),
	N_("Right side oriented")
};

gchar *glatex_fontsize_pattern[] = {
	"\\tiny",
	"\\scriptsize",
	"\\footnotesize",
	"\\small",
	"\\normalsize",
	"\\large",
	"\\Large",
	"\\LARGE",
	"\\huge",
	"\\Huge"
};

const gchar *glatex_fontsize_labels[] = {
	N_("tiny"),
	N_("scriptsize"),
	N_("footnotesize"),
	N_("small"),
	N_("normalsize"),
	N_("large"),
	N_("Large"),
	N_("LARGE"),
	N_("huge"),
	N_("Huge")
};
