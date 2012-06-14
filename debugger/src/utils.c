/*
 *		utils.c
 *      
 *      Copyright 2010 Alexander Petukhov <devel(at)apetukhov.ru>
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

/*
 *		Miscellaneous functions.
 */

#include <stdlib.h>
#include <memory.h>
#include <stdio.h>
#include <ctype.h>
#include <gtk/gtk.h>

#include "breakpoint.h"
#include "debug_module.h"

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif
#include <geanyplugin.h>
extern GeanyFunctions	*geany_functions;

#include "utils.h"

/*
 * opens position in a editor 
 */
void editor_open_position(const gchar *filename, int line)
{
	GeanyDocument* doc = NULL;
	gboolean already_open = (doc = document_get_current()) && !strcmp(DOC_FILENAME(doc), filename);

	if (!already_open)
		doc = document_open_file(filename, FALSE, NULL, NULL);

	if (doc)
	{
		/* temporarily set debug caret policy */
		scintilla_send_message(doc->editor->sci, SCI_SETYCARETPOLICY, CARET_SLOP | CARET_JUMPS | CARET_EVEN, 3);

		sci_goto_line(doc->editor->sci, line - 1, TRUE);

		/* revert to default edit caret policy */
		scintilla_send_message(doc->editor->sci, SCI_SETYCARETPOLICY, CARET_EVEN, 0);

		scintilla_send_message(doc->editor->sci, SCI_SETFOCUS, TRUE, 0);
	}
	else
	{
		dialogs_show_msgbox(GTK_MESSAGE_ERROR, _("Can't find a source file \"%s\""), filename);
	}
}

/*
 * get word at "position" in Scintilla document
 */
GString* get_word_at_position(ScintillaObject *sci, int position)
{
	GString *word = g_string_new("");

	gchar gc;

	/* first, move to the beginning of a word */
	do
	{
		gc = sci_get_char_at(sci, position - 1);
		if (isalpha(gc) || '.' == gc || '_' == gc)
		{
			position--;
			continue;
		}
		else if ('>' == gc)
		{
			if('-' == sci_get_char_at(sci, position - 2))
			{
				position -= 2;
				continue;
			}
		}
		break;		
	}
	while(TRUE);

	/* move to the end of a word */
	do
	{
		gc = sci_get_char_at(sci, position);
		if (isalpha(gc) || '.' == gc || '_' == gc)
		{
			word = g_string_append_c(word, gc);
			position++;
			continue;
		}
		else if ('-' == gc)
		{
			gchar next = sci_get_char_at(sci, position + 1);
			if('>' == next)
			{
				word = g_string_append(word, "->");
				position += 2;
				continue;
			}
		}
		break;		
	}
	while (TRUE);
	
	return word;
}
