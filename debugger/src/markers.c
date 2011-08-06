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
#define M_FIRST									12
#define M_BP_ENABLED						M_FIRST
#define M_BP_DISABLED						(M_FIRST + 1)
#define M_CI_BACKGROUND					(M_FIRST + 2)
#define M_BP_ENABLED_CONDITIONAL		(M_FIRST + 3)
#define M_BP_ENABLED_HITS					(M_FIRST + 4)
#define M_BP_ENABLED_HITS_CONDITIONAL	(M_FIRST + 5)
#define M_BP_DISABLED_CONDITIONAL		(M_FIRST + 6)
#define M_BP_DISABLED_HITS					(M_FIRST + 7)
#define M_BP_DISABLED_HITS_CONDITIONAL	(M_FIRST + 8)
#define M_CI_ARROW							(M_FIRST + 9)
#define M_FRAME								(M_FIRST + 10)

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

#define LIGHT_YELLOW	RGB(200,200,0)

/*
 * sets markers for a scintilla document
 */
void markers_set_for_document(ScintillaObject *sci)
{
	/* enabled breakpoint */
	scintilla_send_message(sci, SCI_MARKERDEFINE, M_BP_ENABLED, SC_MARK_ROUNDRECT);
	scintilla_send_message(sci, SCI_MARKERSETBACK, M_BP_ENABLED, RED);
	scintilla_send_message(sci, SCI_MARKERSETFORE, M_BP_ENABLED, RED);
	
	/* enabled breakpoint - condition */
	scintilla_send_message(sci, SCI_MARKERDEFINE, M_BP_ENABLED_CONDITIONAL, SC_MARK_ROUNDRECT);
	scintilla_send_message(sci, SCI_MARKERSETBACK, M_BP_ENABLED_CONDITIONAL, BLUE);
	scintilla_send_message(sci, SCI_MARKERSETFORE, M_BP_ENABLED_CONDITIONAL, BLUE);

	/* enabled breakpoint - hits */
	scintilla_send_message(sci, SCI_MARKERDEFINE, M_BP_ENABLED_HITS, SC_MARK_ROUNDRECT);
	scintilla_send_message(sci, SCI_MARKERSETBACK, M_BP_ENABLED_HITS, YELLOW);
	scintilla_send_message(sci, SCI_MARKERSETFORE, M_BP_ENABLED_HITS, YELLOW);

	/* enabled breakpoint - hits, condition */
	scintilla_send_message(sci, SCI_MARKERDEFINE, M_BP_ENABLED_HITS_CONDITIONAL, SC_MARK_ROUNDRECT);
	scintilla_send_message(sci, SCI_MARKERSETBACK, M_BP_ENABLED_HITS_CONDITIONAL, GREEN);
	scintilla_send_message(sci, SCI_MARKERSETFORE, M_BP_ENABLED_HITS_CONDITIONAL, GREEN);

	/* disabled breakpoint */ 
	scintilla_send_message(sci, SCI_MARKERDEFINE, M_BP_DISABLED, SC_MARK_ROUNDRECT);
	scintilla_send_message(sci, SCI_MARKERSETFORE, M_BP_DISABLED, RED);

	/* disabled breakpoint - condition */
	scintilla_send_message(sci, SCI_MARKERDEFINE, M_BP_DISABLED_CONDITIONAL, SC_MARK_ROUNDRECT);
	scintilla_send_message(sci, SCI_MARKERSETFORE, M_BP_DISABLED_CONDITIONAL, BLUE);

	/* disabled breakpoint - hits */
	scintilla_send_message(sci, SCI_MARKERDEFINE, M_BP_DISABLED_HITS, SC_MARK_ROUNDRECT);
	scintilla_send_message(sci, SCI_MARKERSETFORE, M_BP_DISABLED_HITS, YELLOW);

	/* disabled breakpoint - hits, condition */
	scintilla_send_message(sci, SCI_MARKERDEFINE, M_BP_DISABLED_HITS_CONDITIONAL, SC_MARK_ROUNDRECT);
	scintilla_send_message(sci, SCI_MARKERSETFORE, M_BP_DISABLED_HITS_CONDITIONAL, GREEN);

	/* currect instruction background */
	scintilla_send_message(sci, SCI_MARKERDEFINE, M_CI_BACKGROUND, SC_MARK_BACKGROUND);
	scintilla_send_message(sci, SCI_MARKERSETBACK, M_CI_BACKGROUND, YELLOW);
	scintilla_send_message(sci, SCI_MARKERSETFORE, M_CI_BACKGROUND, YELLOW);
	scintilla_send_message(sci, SCI_MARKERSETALPHA, M_CI_BACKGROUND, 75);

	/* currect instruction arrow */
	scintilla_send_message(sci, SCI_MARKERDEFINE, M_CI_ARROW, SC_MARK_SHORTARROW);
	scintilla_send_message(sci, SCI_MARKERSETBACK, M_CI_ARROW, YELLOW);
	scintilla_send_message(sci, SCI_MARKERSETFORE, M_CI_ARROW, BLACK);
	scintilla_send_message(sci, SCI_MARKERSETALPHA, M_CI_ARROW, 75);

	/* frame marker current */
	scintilla_send_message(sci, SCI_MARKERDEFINE, M_FRAME, SC_MARK_SHORTARROW);
	scintilla_send_message(sci, SCI_MARKERSETBACK, M_FRAME, WHITE);
	scintilla_send_message(sci, SCI_MARKERSETFORE, M_FRAME, BLACK);
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
		int marker;
		if (bp->enabled)
		{
			if (strlen(bp->condition))
			{
				marker = bp->hitscount ? M_BP_ENABLED_HITS_CONDITIONAL : M_BP_ENABLED_CONDITIONAL;
			}
			else if (bp->hitscount)
			{
				marker = M_BP_ENABLED_HITS;
			}
			else
			{
				marker = M_BP_ENABLED;
			}
		}
		else
		{
			if (strlen(bp->condition))
			{
				marker = bp->hitscount ? M_BP_DISABLED_HITS_CONDITIONAL : M_BP_DISABLED_CONDITIONAL;
			}
			else if (bp->hitscount)
			{
				marker = M_BP_DISABLED_HITS;
			}
			else
			{
				marker = M_BP_DISABLED;
			}
		}
		sci_set_marker_at_line(doc->editor->sci, bp->line - 1, marker);
	}
}

/*
 * removes breakpoints marker
 */
void markers_remove_breakpoint(breakpoint *bp)
{
	static int breakpoint_markers[] = {
		M_BP_ENABLED,
		M_BP_ENABLED_CONDITIONAL,
		M_BP_ENABLED_HITS,
		M_BP_ENABLED_HITS_CONDITIONAL,
		M_BP_DISABLED,
		M_BP_DISABLED_CONDITIONAL,
		M_BP_DISABLED_HITS,
		M_BP_DISABLED_HITS_CONDITIONAL
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
