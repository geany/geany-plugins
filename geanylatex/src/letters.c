/*
 * 		letters.c
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

#include <gtk/gtk.h>
#include "support.h"
#include "datatypes.h"
#include "letters.h"

enum
{
	GREEK_LETTERS = 0,
	GERMAN_LETTERS,
	MISC_LETTERS,
	ARROW_CHAR,
	RELATIONAL_SIGNS,
	BINARY_OPERATIONS,
	LETTERS_END
};

CategoryName glatex_cat_names[] = {
	{ GREEK_LETTERS, N_("Greek letters"), TRUE},
	{ GERMAN_LETTERS, N_("German umlauts"), TRUE},
	{ MISC_LETTERS, N_("Misc"), FALSE},
	{ ARROW_CHAR, N_("Arrow characters"), FALSE},
	{ RELATIONAL_SIGNS, N_("Relational"), FALSE},
	{ BINARY_OPERATIONS, N_("Binary operation"), FALSE},
	{ 0, NULL, FALSE}
};

/* Entries need to be sorted by categorie (1st field) or some random
 * features will occure.
 * AAABBBCCC is valid
 * AAACCCBBB is valid
 * ACABCBACB is _not_ valid and will course trouble */
SubMenuTemplate glatex_char_array[] = {
	/* Greek characters */
	{GREEK_LETTERS, "Α", "\\Alpha" },
	{GREEK_LETTERS, "α", "\\alpha" },
	{GREEK_LETTERS, "Β", "\\Beta" },
	{GREEK_LETTERS, "β", "\\beta" },
	{GREEK_LETTERS, "Γ", "\\Gamma" },
	{GREEK_LETTERS, "γ", "\\gamma" },
	{GREEK_LETTERS, "Δ", "\\Delta" },
	{GREEK_LETTERS, "δ", "\\Delta" },
	{GREEK_LETTERS, "δ", "\\delta" },
	{GREEK_LETTERS, "Ε", "\\Epsilon" },
	{GREEK_LETTERS, "ε", "\\epsilon" },
	{GREEK_LETTERS, "Ζ", "\\Zeta" },
	{GREEK_LETTERS, "ζ", "\\zeta" },
	{GREEK_LETTERS, "Η", "\\Eta" },
	{GREEK_LETTERS, "η", "\\eta" },
	{GREEK_LETTERS, "Θ", "\\Theta" },
	{GREEK_LETTERS, "θ", "\\theta" },
	{GREEK_LETTERS, "Ι", "\\Iota" },
	{GREEK_LETTERS, "ι", "\\iota" },
	{GREEK_LETTERS, "Κ", "\\Kappa" },
	{GREEK_LETTERS, "κ", "\\kappa" },
	{GREEK_LETTERS, "Λ", "\\Lambda" },
	{GREEK_LETTERS, "λ", "\\lambda" },
	{GREEK_LETTERS, "Μ", "\\Mu" },
	{GREEK_LETTERS, "μ", "\\mu" },
	{GREEK_LETTERS, "Ν", "\\Nu" },
	{GREEK_LETTERS, "ν", "\\nu" },
	{GREEK_LETTERS, "Ξ", "\\Xi" },
	{GREEK_LETTERS, "ξ", "\\xi" },
	{GREEK_LETTERS, "Ο", "\\Omicron" },
	{GREEK_LETTERS, "ο", "\\omicron" },
	{GREEK_LETTERS, "Π", "\\Pi" },
	{GREEK_LETTERS, "π", "\\pi" },
	{GREEK_LETTERS, "Ρ", "\\Rho" },
	{GREEK_LETTERS, "ρ", "\\rho" },
	{GREEK_LETTERS, "Σ", "\\Sigma" },
	{GREEK_LETTERS, "ς", "\\sigmaf" },
	{GREEK_LETTERS, "σ", "\\sigma" },
	{GREEK_LETTERS, "Τ", "\\Tau" },
	{GREEK_LETTERS, "τ", "\\tau" },
	{GREEK_LETTERS, "Υ", "\\Upsilon" },
	{GREEK_LETTERS, "υ", "\\upsilon" },
	{GREEK_LETTERS, "Φ", "\\Phi" },
	{GREEK_LETTERS, "φ", "\\phi" },
	{GREEK_LETTERS, "Χ", "\\Chi" },
	{GREEK_LETTERS, "χ", "\\chi" },
	{GREEK_LETTERS, "Ψ", "\\Psi" },
	{GREEK_LETTERS, "ψ", "\\psi" },
	{GREEK_LETTERS, "Ω", "\\Omega" },
	{GREEK_LETTERS, "ω", "\\omega" },
	{GREEK_LETTERS, "ϑ", "\\thetasym" },
	{GREEK_LETTERS, "ϒ", "\\upsih" },
	{GREEK_LETTERS, "ϖ", "\\piv" },

	/* German Umlaute */
	{GERMAN_LETTERS, "ä","\"a"},
	{GERMAN_LETTERS, "ü","\"u"},
	{GERMAN_LETTERS, "ö","\"o"},
	{GERMAN_LETTERS, "ß","\"s"},

	/* Czech characters */
	{MISC_LETTERS, "ě","\\v{e}"},
	{MISC_LETTERS, "š","\\v{s}"},
	{MISC_LETTERS, "č","\\v[c}"},
	{MISC_LETTERS, "ř","\\v{r}"},
	{MISC_LETTERS, "ž","\\v{z}"},
	{MISC_LETTERS, "ů","\\r{u}"},
	{MISC_LETTERS, "Ě","\\v{E}"},
	{MISC_LETTERS, "Š","\\v{S}"},
	{MISC_LETTERS, "Č","\\v{C}"},
	{MISC_LETTERS, "Ř","\\v{R}"},
	{MISC_LETTERS, "Ž","\\v{Z}"},

	/* Misc */
	{MISC_LETTERS, "\\","\\backslash"},
	{MISC_LETTERS, "€", "\\euro"},
	{MISC_LETTERS, "»", "\\frqq"},
	{MISC_LETTERS, "«", "\\flqq"},
	{MISC_LETTERS, "›", "\\frq"},
	{MISC_LETTERS, "‹", "\\flq"},
	{ARROW_CHAR, "←", "\\leftarrow" },
	{ARROW_CHAR, "↑", "\\uparrow" },
	{ARROW_CHAR, "→", "\\rightarrow" },
	{ARROW_CHAR, "↓", "\\downarrow" },
	{ARROW_CHAR, "↔", "\\leftrightarrow" },
	{ARROW_CHAR, "⇐", "\\Leftarrow" },
	{ARROW_CHAR, "⇑", "\\Uparrow" },
	{ARROW_CHAR, "⇒", "\\Rightarrow" },
	{ARROW_CHAR, "⇓", "\\Downarrow" },
	{ARROW_CHAR, "⇔", "\\Leftrightarrow" },
	{RELATIONAL_SIGNS, "\342\211\244", "\\leq"},
	{RELATIONAL_SIGNS, "\342\211\245", "\\geq"},
	{RELATIONAL_SIGNS, "\342\210\216", "\\qed"},
	{RELATIONAL_SIGNS, "\342\211\241", "\\equiv"},
	{RELATIONAL_SIGNS, "\342\212\247", "\\models"},
	{RELATIONAL_SIGNS, "\342\211\272", "\\prec"},
	{RELATIONAL_SIGNS, "\342\211\273", "\\succ"},
	{RELATIONAL_SIGNS, "\342\211\274", "\\sim"},
	{RELATIONAL_SIGNS, "\342\237\202", "\\perp"},
	{RELATIONAL_SIGNS, "\342\252\257", "\\preceq"},
	{RELATIONAL_SIGNS, "\342\252\260", "\\succeq"},
	{RELATIONAL_SIGNS, "\342\211\203", "\\simeq"},
	{RELATIONAL_SIGNS, "\342\210\243", "\\mid"},
	{RELATIONAL_SIGNS, "\342\211\252", "\\ll"},
	{RELATIONAL_SIGNS, "\342\211\253", "\\gg"},
	{RELATIONAL_SIGNS, "\342\211\215", "\\asymp"},
	{RELATIONAL_SIGNS, "\342\210\245", "\\parallel"},
	{RELATIONAL_SIGNS, "\342\212\202", "\\subset"},
	{RELATIONAL_SIGNS, "\342\212\203", "\\supset"},
	{RELATIONAL_SIGNS, "\342\211\210", "\\approx"},
	{RELATIONAL_SIGNS, "\342\213\210", "\\bowtie"},
	{RELATIONAL_SIGNS, "\342\212\206", "\\subseteq"},
	{RELATIONAL_SIGNS, "\342\212\207", "\\supseteq"},
	{RELATIONAL_SIGNS, "\342\211\205", "\\cong"},
	{RELATIONAL_SIGNS, "\342\250\235", "\\Join"},
	{RELATIONAL_SIGNS, "\342\212\217", "\\sqsubset"},
	{RELATIONAL_SIGNS, "\342\212\220", "\\sqsupset"},
	{RELATIONAL_SIGNS, "\342\211\240", "\\neq"},
	{RELATIONAL_SIGNS, "\342\214\243", "\\smile"},
	{RELATIONAL_SIGNS, "\342\212\221", "\\sqsubseteq"},
	{RELATIONAL_SIGNS, "\342\212\222", "\\sqsupseteq"},
	{RELATIONAL_SIGNS, "\342\211\220", "\\doteq"},
	{RELATIONAL_SIGNS, "\342\214\242", "\\frown"},
	{RELATIONAL_SIGNS, "\342\210\210", "\\in"},
	{RELATIONAL_SIGNS, "\342\210\213", "\\ni"},
	{RELATIONAL_SIGNS, "\342\210\235", "\\propto"},
	{RELATIONAL_SIGNS, "\342\212\242", "\\vdash"},
	{RELATIONAL_SIGNS, "\342\212\243", "\\dashv"},
	{BINARY_OPERATIONS, "\302\261", "\\pm"},
	{BINARY_OPERATIONS, "\342\210\223", "\\mp"},
	{BINARY_OPERATIONS, "\303\227", "\\times"},
	{BINARY_OPERATIONS, "\303\267", "\\div"},
	{BINARY_OPERATIONS, "\342\210\227", "\\ast"},
	{BINARY_OPERATIONS, "\342\213\206", "\\star"},
	{BINARY_OPERATIONS, "\342\210\230", "\\circ"},
	{BINARY_OPERATIONS, "\342\210\231", "\\bullet"},
	{BINARY_OPERATIONS, "\342\213\205", "\\cdot"},
	{BINARY_OPERATIONS, "\342\210\251", "\\cap"},
	{BINARY_OPERATIONS, "\342\210\252", "\\cup"},
	{BINARY_OPERATIONS, "\342\212\216", "\\uplus"},
	{BINARY_OPERATIONS, "\342\212\223", "\\sqcap"},
	{BINARY_OPERATIONS, "\342\210\250", "\\vee"},
	{BINARY_OPERATIONS, "\342\210\247", "\\wedge"},
	{BINARY_OPERATIONS, "\342\210\226", "\\setminus"},
	{BINARY_OPERATIONS, "\342\211\200", "\\wr"},
	{BINARY_OPERATIONS, "\342\213\204", "\\diamond"},
	{BINARY_OPERATIONS, "\342\226\263", "\\bigtriangleup"},
	{BINARY_OPERATIONS, "\342\226\275", "\\bigtriangledown"},
	{BINARY_OPERATIONS, "\342\227\201", "\\triangleleft"},
	{BINARY_OPERATIONS, "\342\226\267", "\\triangleright"},
	{BINARY_OPERATIONS, "", "\\lhd"},
	{BINARY_OPERATIONS, "", "\\rhd"},
	{BINARY_OPERATIONS, "", "\\unlhd"},
	{BINARY_OPERATIONS, "", "\\unrhd"},
	{BINARY_OPERATIONS, "\342\212\225", "\\oplus"},
	{BINARY_OPERATIONS, "\342\212\226", "\\ominus"},
	{BINARY_OPERATIONS, "\342\212\227", "\\otimes"},
	{BINARY_OPERATIONS, "\342\210\205", "\\oslash"},
	{BINARY_OPERATIONS, "\342\212\231", "\\odot"},
	{BINARY_OPERATIONS, "\342\227\213", "\\bigcirc"},
	{BINARY_OPERATIONS, "\342\200\240", "\\dagger"},
	{BINARY_OPERATIONS, "\342\200\241", "\\ddagger"},
	{BINARY_OPERATIONS, "\342\250\277", "\\amalg"},
	{0, NULL, NULL},

};

gchar *glatex_get_entity(const gchar *letter)
{
	if (! utils_str_equal(letter, "\\"))
	{
		guint i, len;
    	len = G_N_ELEMENTS(glatex_char_array);
		for (i = 0; i < len; i++)
		{
			if (utils_str_equal(glatex_char_array[i].label, letter))
			{
				return glatex_char_array[i].latex;
			}
		}
	}

	/* if the char is not in the list */
	return NULL;
}
