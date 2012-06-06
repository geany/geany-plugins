/*
 *		markers.c
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
 * 		Contains functions for manipulating margin and background markers.
 */

#include <string.h>

#include "geanyplugin.h"
extern GeanyFunctions	*geany_functions;
extern GeanyData		*geany_data;

#include "markers.h"
#include "breakpoints.h"

#include "xpm/breakpoint.xpm"
#include "xpm/breakpoint_disabled.xpm"
#include "xpm/breakpoint_condition.xpm"

#include "xpm/frame.xpm"
#include "xpm/frame_current.xpm"

/* markers identifiers */
#define M_FIRST									12
#define M_BP_ENABLED						M_FIRST
#define M_BP_DISABLED						(M_FIRST + 1)
#define M_BP_CONDITIONAL					(M_FIRST + 2)
#define M_CI_BACKGROUND					(M_FIRST + 3)
#define M_CI_ARROW							(M_FIRST + 4)
#define M_FRAME								(M_FIRST + 5)

#define MARKER_PRESENT(mask, marker) (mask && (0x01 << marker))

/* markers colors */
#define RGB(R,G,B)	(R | (G << 8) | (B << 16))
#define RED				RGB(255,0,0)
#define GREEN			RGB(0,255,0)
#define BLUE				RGB(0,0,255)
#define YELLOW			RGB(255,255,0)
#define BLACK			RGB(0,0,0)
#define WHITE			RGB(255,255,255)
#define PINK				RGB(255,192,203)

#define BP_BACKGROUND	RGB(255,246,33)


#define LIGHT_YELLOW	RGB(200,200,0)

/*
 * sets markers for a scintilla document
 */
void markers_set_for_document(ScintillaObject *sci)
{
	/* enabled breakpoint */
	scintilla_send_message(sci, SCI_MARKERDEFINEPIXMAP, M_BP_ENABLED, (long)breakpoint_xpm);
	
	/* disabled breakpoint */ 
	scintilla_send_message(sci, SCI_MARKERDEFINEPIXMAP, M_BP_DISABLED, (long)breakpoint_disabled_xpm);

	/* conditional breakpoint */
	scintilla_send_message(sci, SCI_MARKERDEFINEPIXMAP, M_BP_CONDITIONAL, (long)breakpoint_condition_xpm);

	/* currect instruction background */
	scintilla_send_message(sci, SCI_MARKERDEFINE, M_CI_BACKGROUND, SC_MARK_BACKGROUND);
	scintilla_send_message(sci, SCI_MARKERSETBACK, M_CI_BACKGROUND, YELLOW);
	scintilla_send_message(sci, SCI_MARKERSETFORE, M_CI_BACKGROUND, YELLOW);
	scintilla_send_message(sci, SCI_MARKERSETALPHA, M_CI_BACKGROUND, 75);

	/* currect instruction arrow */
	scintilla_send_message(sci, SCI_MARKERDEFINEPIXMAP, M_CI_ARROW, (long)frame_current_xpm);

	/* frame marker current */
	scintilla_send_message(sci, SCI_MARKERDEFINEPIXMAP, M_FRAME, (long)frame_xpm);
}

/*
 * inits markers staff
 */
void markers_init(void)
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
		if (!bp->enabled)
		{
			sci_set_marker_at_line(doc->editor->sci, bp->line - 1, M_BP_DISABLED);
		}
		else if (strlen(bp->condition) || bp->hitscount)
		{
				sci_set_marker_at_line(doc->editor->sci, bp->line - 1, M_BP_CONDITIONAL);
		}
		else
		{
				sci_set_marker_at_line(doc->editor->sci, bp->line - 1, M_BP_ENABLED);
		}
	}
}

/*
 * removes breakpoints marker
 */
void markers_remove_breakpoint(breakpoint *bp)
{
	static int breakpoint_markers[] = {
		M_BP_ENABLED,
		M_BP_DISABLED,
		M_BP_CONDITIONAL
	};

	GeanyDocument *doc = document_find_by_filename(bp->file);
	if (doc)
	{
		int markers = scintilla_send_message(doc->editor->sci, SCI_MARKERGET, bp->line - 1, (long)NULL);
		int markers_count = sizeof(breakpoint_markers) / sizeof(breakpoint_markers[0]);
		int i = 0;
		for (; i < markers_count; i++)
		{
			int marker = breakpoint_markers[i];
			if (markers & (0x01 << marker))
			{
				sci_delete_marker_at_line(doc->editor->sci, bp->line - 1, marker);
			}
		}
	}
}

/*
 * adds current instruction marker
 */
void markers_add_current_instruction(char* file, int line)
{
	GeanyDocument *doc = document_find_by_filename(file);
	if (doc)
	{
		sci_set_marker_at_line(doc->editor->sci, line - 1, M_CI_ARROW);
		sci_set_marker_at_line(doc->editor->sci, line - 1, M_CI_BACKGROUND);
	}
}

/*
 * removes current instruction marker
 */
void markers_remove_current_instruction(char* file, int line)
{
	GeanyDocument *doc = document_find_by_filename(file);
	if (doc)
	{
		sci_delete_marker_at_line(doc->editor->sci, line - 1, M_CI_ARROW);
		sci_delete_marker_at_line(doc->editor->sci, line - 1, M_CI_BACKGROUND);
		scintilla_send_message(doc->editor->sci, SCI_SETFOCUS, TRUE, 0);
	}
}

/*
 * adds frame marker
 */
void markers_add_frame(char* file, int line)
{
	GeanyDocument *doc = document_find_by_filename(file);
	if (doc)
	{
		sci_set_marker_at_line(doc->editor->sci, line - 1, M_FRAME);
	}
}

/*
 * removes frame marker
 */
void markers_remove_frame(char* file, int line)
{
	GeanyDocument *doc = document_find_by_filename(file);
	if (doc)
	{
		sci_delete_marker_at_line(doc->editor->sci, line - 1, M_FRAME);
		scintilla_send_message(doc->editor->sci, SCI_SETFOCUS, TRUE, 0);
	}
}

/*
 * removes all markers from GeanyDocument
 */
void markers_remove_all(GeanyDocument *doc)
{
	static int markers[] = { M_BP_ENABLED, M_BP_DISABLED, M_BP_CONDITIONAL, M_CI_BACKGROUND, M_CI_ARROW, M_FRAME };
	int i = 0, size = sizeof(markers) / sizeof(int);
	for (; i < size; i++)
	{
		scintilla_send_message(doc->editor->sci, SCI_MARKERDELETEALL, markers[i], 0);
	}
}
