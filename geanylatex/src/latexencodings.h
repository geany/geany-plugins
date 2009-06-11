/*
 *      latexencodings.h
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

#ifndef LATEXEXCODINGS_H
#define LATEXEXCODINGS_H


typedef enum
{
	LATEX_ENCODING_UTF_8 = 0,
	LATEX_ENCODING_ASCII,
	LATEX_ENCODING_ISO_8859_1,
	LATEX_ENCODING_ISO_8859_2,
	LATEX_ENCODING_ISO_8859_3,
	LATEX_ENCODING_ISO_8859_4,
	LATEX_ENCODING_ISO_8859_5,
	LATEX_ENCODING_ISO_8859_9,
	LATEX_ENCODING_ISO_8859_10,
	LATEX_ENCODING_IBM_850,
	LATEX_ENCODING_IBM_852,
	LATEX_ENCODING_NONE,
	LATEX_ENCODINGS_MAX
} LaTeXEncodingIndex;


typedef struct
{
	LaTeXEncodingIndex charset;
	gchar *name;
	gchar *latex;
	gint geany_enc;
} LaTeXEncodings;

extern LaTeXEncodings latex_encodings[LATEX_ENCODINGS_MAX];

void glatex_init_encodings_latex(void);

#endif
