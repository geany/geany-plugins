/*
 *		markers.c
 *      
 *      Copyright 2010 Alexander Petukhov <Alexander(dot)Petukhov(at)mail(dot)ru>
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
 * 		Contains functions for manipulating margin and background markers.
 */

#include "geanyplugin.h"
extern GeanyFunctions	*geany_functions;
extern GeanyData		*geany_data;

#include "breakpoint.h"
#include "breakpoints.h"

/* markers identifiers */
#define MARKER_FIRST								12
#define MARKER_BREAKPOINT_ENABLED				MARKER_FIRST
#define MARKER_BREAKPOINT_DISABLED				(MARKER_FIRST + 1)
#define MARKER_CURRENT_INSTRUCTION_BACKGROUND	(MARKER_FIRST + 2)
#define MARKER_CURRENT_INSTRUCTION_ARROW		(MARKER_FIRST + 3)
#define MARKER_FRAME								(MARKER_FIRST + 4)

/* markers colors */
#define RGB(R,G,B)	(R | (G << 8) | (B << 16))
#define RED			RGB(255,0,0)
#define GREEN			RGB(0,255,0)
#define BLUE			RGB(0,0,255)
#define YELLOW			RGB(255,255,0)
#define BLACK			RGB(0,0,0)
#define WHITE			RGB(255,255,255)

#define LIGHT_YELLOW	RGB(200,200,0)

/*
 * sets markers for a scintilla document
 */
void markers_set_for_document(ScintillaObject *sci)
{
	/* enabled breakpoint */
	scintilla_send_message(sci, SCI_MARKERDEFINE, MARKER_BREAKPOINT_ENABLED, SC_MARK_SMALLRECT);
	scintilla_send_message(sci, SCI_MARKERSETBACK, MARKER_BREAKPOINT_ENABLED, RED);
	scintilla_send_message(sci, SCI_MARKERSETFORE, MARKER_BREAKPOINT_ENABLED, RED);

	/* disabled breakpoint */ 
	scintilla_send_message(sci, SCI_MARKERDEFINE, MARKER_BREAKPOINT_DISABLED, SC_MARK_SMALLRECT);
	scintilla_send_message(sci, SCI_MARKERSETFORE, MARKER_BREAKPOINT_DISABLED, RED);

	/* currect instruction background */
	scintilla_send_message(sci, SCI_MARKERDEFINE, MARKER_CURRENT_INSTRUCTION_BACKGROUND, SC_MARK_BACKGROUND);
	scintilla_send_message(sci, SCI_MARKERSETBACK, MARKER_CURRENT_INSTRUCTION_BACKGROUND, YELLOW);
	scintilla_send_message(sci, SCI_MARKERSETFORE, MARKER_CURRENT_INSTRUCTION_BACKGROUND, YELLOW);
	scintilla_send_message(sci, SCI_MARKERSETALPHA, MARKER_CURRENT_INSTRUCTION_BACKGROUND, 75);

	/* currect instruction arrow */
	scintilla_send_message(sci, SCI_MARKERDEFINE, MARKER_CURRENT_INSTRUCTION_ARROW, SC_MARK_SHORTARROW);
	scintilla_send_message(sci, SCI_MARKERSETBACK, MARKER_CURRENT_INSTRUCTION_ARROW, YELLOW);
	scintilla_send_message(sci, SCI_MARKERSETFORE, MARKER_CURRENT_INSTRUCTION_ARROW, BLACK);
	scintilla_send_message(sci, SCI_MARKERSETALPHA, MARKER_CURRENT_INSTRUCTION_ARROW, 75);

	/* frame marker */
	scintilla_send_message(sci, SCI_MARKERDEFINE, MARKER_FRAME, SC_MARK_SHORTARROW);
	scintilla_send_message(sci, SCI_MARKERSETBACK, MARKER_FRAME, LIGHT_YELLOW);
	scintilla_send_message(sci, SCI_MARKERSETFORE, MARKER_FRAME, LIGHT_YELLOW);
}

/*
 * inits markers staff
 */
void markers_init()
{
	/* set markers in all currently opened documents */
	int i;
	foreach_document(i)
		markers_set_for_document(document_index(i)->editor->sci);
}

/*
 * add breakpoint marker
 * enabled or disabled, based on bp->enabled value
 * arguments:
 * 		bp - breakpoint to add marker for  
 */
void markers_add_breakpoint(breakpoint* bp)
{
	GeanyDocument *doc = document_find_by_filename(bp->file);
	if (doc)
	{
		sci_set_marker_at_line(doc->editor->sci, bp->line - 1,
			bp->enabled ? MARKER_BREAKPOINT_ENABLED : MARKER_BREAKPOINT_DISABLED);
	}
}

/*
 * removes breakpoints marker
 */
void markers_remove_breakpoint(breakpoint* bp)
{
	GeanyDocument *doc = document_find_by_filename(bp->file);
	if (doc)
	{
		/* delete enabled marker */
		sci_delete_marker_at_line(doc->editor->sci, bp->line - 1, MARKER_BREAKPOINT_ENABLED);
		/* delete disabled marker */
		sci_delete_marker_at_line(doc->editor->sci, bp->line - 1, MARKER_BREAKPOINT_DISABLED);
	}
}

/*
 * adds disabled breakpoint marker
 */
void markers_add_breakpoint_disabled(char* file, int line)
{
	GeanyDocument *doc = document_find_by_filename(file);
	sci_set_marker_at_line(doc->editor->sci, line - 1, MARKER_BREAKPOINT_DISABLED);
}

/*
 * removes disabled breakpoint marker
 */
void markers_remove_breakpoint_disabled(char* file, int line)
{
	GeanyDocument *doc = document_find_by_filename(file);
	sci_delete_marker_at_line(doc->editor->sci, line - 1, MARKER_BREAKPOINT_DISABLED);
}

/*
 * adds current instruction marker
 */
void markers_add_current_instruction(char* file, int line)
{
	GeanyDocument *doc = document_find_by_filename(file);
	sci_set_marker_at_line(doc->editor->sci, line - 1, MARKER_CURRENT_INSTRUCTION_ARROW);
	sci_set_marker_at_line(doc->editor->sci, line - 1, MARKER_CURRENT_INSTRUCTION_BACKGROUND);
}

/*
 * removes current instruction marker
 */
void markers_remove_current_instruction(char* file, int line)
{
	GeanyDocument *doc = document_find_by_filename(file);
	sci_delete_marker_at_line(doc->editor->sci, line - 1, MARKER_CURRENT_INSTRUCTION_ARROW);
	sci_delete_marker_at_line(doc->editor->sci, line - 1, MARKER_CURRENT_INSTRUCTION_BACKGROUND);
	scintilla_send_message(doc->editor->sci, SCI_SETFOCUS, TRUE, 0);
}

