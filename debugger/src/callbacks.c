/*
 *      callbacks.c
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
 * 		Contains callbacks for the user actions as well
 * 		as for the Geany events  
 */

#include <string.h>

#include "geanyplugin.h"
#include "breakpoints.h"
#include "debug.h"
#include "keys.h"
#include "tpage.h"
#include "stree.h"
#include "markers.h"
#include "utils.h"
#include "bptree.h"
#include "btnpanel.h"
#include "dconfig.h"
#include "tabs.h"

extern GeanyFunctions *geany_functions;

/*
 * 	Set breakpoint and stack markers for a file
 */
void set_markers_for_file(const gchar* file)
{
	GList *breaks;
	if ( (breaks = breaks_get_for_document(file)) )
	{
		GList *iter = breaks;
		while (iter)
		{
			breakpoint *bp = (breakpoint*)iter->data;
			markers_add_breakpoint(bp);
			
			iter = iter->next;
		}
		g_list_free(breaks);
	}

	/* set frames markers if exists */
	if (DBS_STOPPED == debug_get_state())
	{
		GList *iter = debug_get_stack();
		if (iter)
		{
			frame *f = (frame*)iter->data;
			if (f->have_source && !strcmp(f->file, file))
			{
				markers_add_current_instruction(f->file, f->line);
			}

			iter = iter->next;
			while (iter)
			{
				f = (frame*)iter->data;
				if (f->have_source && !strcmp(f->file, file))
				{
					markers_add_frame(f->file, f->line);
				}
				iter = iter->next;
			}
		}
	}
}

/*
 * 	Following group of callbacks are used for
 * 	checking of existance of the config file
 * 	and changing buttons state in the target page
 */

/*
 * 	Occures before document is going to be saved
 */
static gboolean _unexisting_file = FALSE;
void on_document_before_save(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
	if (!doc->real_path)
	{
		/* we can fall here if we are saving new document
		 (that doesn't exists on a filesystem) or if we're "Saving As" */

		_unexisting_file = TRUE;
	}
}

/*
 * 	Occures on saving document
 */
void on_document_save(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
	if (_unexisting_file)
	{
		/* if we are saving as - remove all markers at first */
		markers_remove_all(doc);

		/* next, lets try to find and insert markers for the file, current document is being saved to*/
		set_markers_for_file(doc->file_name);

		/* if debug is active - tell the debug module that a file was opened */
		if (DBS_IDLE != debug_get_state())
			debug_on_file_open(doc);
		
		_unexisting_file = FALSE;
	}
}

/*
 * 	Occures on new document creating
 */
void on_document_new(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
}

/*
 * 	Occures on document opening.
 * 	Used to set breaks markers 
 */
void on_document_open(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
	const gchar* file = DOC_FILENAME(doc);
	/*set markers*/
	markers_set_for_document(doc->editor->sci);

	/*set dwell interval*/
	scintilla_send_message(doc->editor->sci, SCI_SETMOUSEDWELLTIME, 500, 0);

	/* set tab size for calltips */
	scintilla_send_message(doc->editor->sci, SCI_CALLTIPUSESTYLE, 20, (long)NULL);

	/* set breakpoint and frame markers */
	set_markers_for_file(file);

	/* if debug is active - tell the debug module that a file was opened */
	if (DBS_IDLE != debug_get_state())
		debug_on_file_open(doc);
}

/*
 * 	Occures on notify from editor.
 * 	Handles margin click to set/remove breakpoint 
 */
gboolean on_editor_notify(
	GObject *object, GeanyEditor *editor,
	SCNotification *nt, gpointer data)
{
	if (!editor->document->real_path)
	{
		/* no other way to handle removing a file from outside of geany */
		markers_remove_all(editor->document);
	}
	
	switch (nt->nmhdr.code)
	{
		case SCN_MARGINCLICK:
		{
			if (1 != nt->margin)
				break;
			
			char* file = editor->document->file_name;
			int line = sci_get_line_from_position(editor->sci, nt->position) + 1;

			break_state	bs = breaks_get_state(file, line);
			if (BS_NOT_SET == bs)
				breaks_add(file, line, NULL, TRUE, 0);
			else if (BS_ENABLED == bs)
				breaks_remove(file, line);
			else if (BS_DISABLED == bs)
				breaks_switch(file, line);
			
			scintilla_send_message(editor->sci, SCI_SETFOCUS, TRUE, 0);
			
			return TRUE;
		}
		case SCN_DWELLSTART:
		{
			if (DBS_STOPPED != debug_get_state ())
				break;
			
			/* get a word under the cursor */
			GString *word = get_word_at_position(editor->sci, nt->position);

			if (word->len)
			{
				gchar *calltip = NULL;//debug_get_calltip_for_expression(word->str);
				if (calltip)
				{
					scintilla_send_message (editor->sci, SCI_CALLTIPSHOW, nt->position, (long)calltip);
				}
			}
				
			g_string_free(word, TRUE);
			
			break;
		}
		case SCN_DWELLEND:
		{
			if (DBS_STOPPED != debug_get_state ())
				break;

			scintilla_send_message (editor->sci, SCI_CALLTIPCANCEL, 0, 0);
			break;
		}
		case SCN_MODIFYATTEMPTRO:
		{
			dialogs_show_msgbox(GTK_MESSAGE_INFO, _("To edit source files stop debugging session"));
			break;
		}
		case SCN_MODIFIED:
		{
			if(((SC_MOD_INSERTTEXT & nt->modificationType) || (SC_MOD_DELETETEXT && nt->modificationType)) && nt->linesAdded)
			{
				int line = sci_get_line_from_position(editor->sci, nt->position) + 1;

				GList *breaks = breaks_get_for_document(editor->document->file_name);
				if (breaks)
				{
					GList *iter = breaks;
					while (iter)
					{
						breakpoint *bp = (breakpoint*)iter->data;

						if (nt->linesAdded > 0 && bp->line >= line)
						{
							breaks_move_to_line(bp->file, bp->line, bp->line + nt->linesAdded);
							bptree_update_breakpoint(bp);
						}
						else if (nt->linesAdded < 0 && bp->line >= line)
						{
							if (bp->line < line - nt->linesAdded)
							{
								breaks_remove(bp->file, bp->line);
							}
							else
							{
								breaks_move_to_line(bp->file, bp->line, bp->line + nt->linesAdded);
								bptree_update_breakpoint(bp);
							}
						}
						iter = iter->next;
					}
					
					config_set_debug_changed();

					g_list_free(breaks);
				}
			}
			break;
		}
	}

	return FALSE;
}

/*
 * 	Occures when key is pressed.
 * 	Handles debug Run/Stop/... and add/remove breakpoint activities  
 */
gboolean keys_callback(guint key_id)
{
	switch (key_id)
	{
		case KEY_RUN:
			debug_run();
			break;
		case KEY_STOP:
			debug_stop();
			break;
		case KEY_RESTART:
			debug_restart();
			break;
		case KEY_STEP_OVER:
			debug_step_over();
			break;
		case KEY_STEP_INTO:
			debug_step_into();
			break;
		case KEY_STEP_OUT:
			debug_step_out();
			break;
		case KEY_EXECUTE_UNTIL:
		{
			GeanyDocument *doc = document_get_current();
			if (doc)
			{
				int line = sci_get_current_line(doc->editor->sci) + 1;
				debug_execute_until(DOC_FILENAME(doc), line);
			}
			break;
		}
		case KEY_BREAKPOINT:
		{
			GeanyDocument *doc = document_get_current();
			if (doc)
			{
				int line = sci_get_current_line(doc->editor->sci) + 1;
				break_state	bs = breaks_get_state(DOC_FILENAME(doc), line);
				if (BS_NOT_SET == bs)
					breaks_add(DOC_FILENAME(doc), line, NULL, TRUE, 0);
				else if (BS_ENABLED == bs)
					breaks_remove(DOC_FILENAME(doc), line);
				else if (BS_DISABLED == bs)
					breaks_switch(DOC_FILENAME(doc), line);
				
				scintilla_send_message(doc->editor->sci, SCI_SETFOCUS, TRUE, 0);
			}
			break;
		}
		case KEY_CURRENT_INSTRUCTION:
		{
			if (DBS_STOPPED == debug_get_state() && debug_current_instruction_have_sources())
			{
				debug_jump_to_current_instruction();
				gtk_widget_set_sensitive(tab_call_stack, FALSE);
				stree_select_first();
				gtk_widget_set_sensitive(tab_call_stack, TRUE);
			}
		}
	}
	
	return TRUE;
} 
