/*
 *      latexencodings.h
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

#include <gtk/gtk.h>
#include "geanylatex.h"
#include "encodings.h"
#include "latexencodings.h"

LaTeXEncodings latex_encodings[LATEX_ENCODINGS_MAX];

#define fill(Charset, Name, LaTeX, GeanyEnc) \
		latex_encodings[Charset].charset = Charset; \
		latex_encodings[Charset].name = Name; \
		latex_encodings[Charset].latex = LaTeX; \
		latex_encodings[Charset].geany_enc = GeanyEnc;

void glatex_init_encodings_latex(void)
{
	fill(LATEX_ENCODING_UTF_8, _("UTF-8"), "utf8", GEANY_ENCODING_UTF_8);
	fill(LATEX_ENCODING_ASCII, _("US-ASCII"), "ascii", GEANY_ENCODING_ISO_8859_1);
	fill(LATEX_ENCODING_ISO_8859_1, _("ISO-8859-1 (Latin-1)"), "latin1",
	    GEANY_ENCODING_ISO_8859_1);
	fill(LATEX_ENCODING_ISO_8859_2, _("ISO-8859-2 (Latin-2)"), "latin2",
	    GEANY_ENCODING_ISO_8859_2);
	fill(LATEX_ENCODING_ISO_8859_3, _("ISO-8859-3 (Latin-3)"), "latin3",
	    GEANY_ENCODING_ISO_8859_3);
	fill(LATEX_ENCODING_ISO_8859_4, _("ISO-8859-4 (Latin-4)"), "latin4",
	    GEANY_ENCODING_ISO_8859_4);
	fill(LATEX_ENCODING_ISO_8859_5, _("ISO-8859-5 (Latin-5)"), "latin5",
	    GEANY_ENCODING_ISO_8859_5);
	fill(LATEX_ENCODING_ISO_8859_9, _("ISO-8859-9 (Latin-9)"), "latin9",
	    GEANY_ENCODING_ISO_8859_9);
	fill(LATEX_ENCODING_ISO_8859_10, _("ISO-8859-10 (Latin-10)"), "latin10",
		GEANY_ENCODING_ISO_8859_10);
	fill(LATEX_ENCODING_IBM_850, _("IBM 850 code page"), "cp850",
		GEANY_ENCODING_IBM_850);
	fill(LATEX_ENCODING_IBM_852, _("IBM 852 code page"), "cp852",
		GEANY_ENCODING_IBM_852);
	fill(LATEX_ENCODING_NONE, _("Don't set any encoding"), NULL,
		GEANY_ENCODING_NONE);
}
