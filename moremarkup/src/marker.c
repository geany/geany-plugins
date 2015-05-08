#include <gtk/gtk.h>
#include <glib-object.h>

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif
#include <geanyplugin.h>
#include "marker.h"
#include "moremarkup.h"
#include <gdk/gdk.h>


inline gint _GDK_COLOR_TO_SCI_COLOR(GdkColor *color) { 
	return (((color->blue << 8) & 0xff0000) | ((color->green) & 0x00ff00) | ((color->red >> 8) & 0x0000ff));
}

MarkerData marker_data; 

void marker_data_init(void) {
	marker_data.text = NULL;
	marker_data.original_text = NULL;
	marker_data.flags = 0;
	/* Default to be set by calling function */
	marker_data.indic_number = 0;
	marker_data.indic_style = 0;
    marker_data.sci_alpha = 255;
	/* Default to be set by calling function */
	marker_data.color = g_malloc(sizeof(GdkColor));
	/* Deprecate this */
	marker_data.last_mark_info = g_malloc(sizeof(MarkerMatchInfo));
}

void marker_data_finalize(void) {
	g_free(marker_data.text);
	g_free(marker_data.color);
	g_free(marker_data.original_text);
	g_free(marker_data.last_mark_info);
}
 
static void _sci_indicator_set(ScintillaObject *sci, gint indic) {
	SSM(sci, SCI_SETINDICATORCURRENT, (uptr_t) indic, 0);
}

static void _sci_indicator_set_style(ScintillaObject *sci, gint indic_number, gint indic_style) {
	SSM(sci, SCI_INDICSETSTYLE, indic_number, indic_style);
}

static void _sci_indicator_clear(ScintillaObject *sci, gint pos, gint len) {
	SSM(sci, SCI_INDICATORCLEARRANGE, (uptr_t) pos, len);
}

static void _sci_indicator_fill(ScintillaObject *sci, gint pos, gint len) {
	SSM(sci, SCI_INDICATORFILLRANGE, (uptr_t) pos, len);
}

static gint _sci_find_text(ScintillaObject *sci, guint flags, struct Sci_TextToFind *ttf)
{
	return (gint) SSM(sci, SCI_FINDTEXT, (uptr_t) flags, (sptr_t) ttf);
}

/* Careful about duplicating since a pointer to lpstrText is also duplicated */
 MarkerMatchInfo *match_info_duplicate(MarkerMatchInfo *info) {
	MarkerMatchInfo *dup = g_slice_copy(sizeof(*info), info);
	return dup;
}

void match_info_free(MarkerMatchInfo *info) {
	g_slice_free1(sizeof(*info), info);
}

void marker_indicator_clear(ScintillaObject *sci, gint indic_number) {
	glong last_pos;
	gpointer *ptr;

	g_return_if_fail(sci != NULL);

	last_pos = sci_get_length(sci);
	
	//g_warning("marker_indicator_clear, last_pos: %i", last_pos);
	if (last_pos > 0) {
		_sci_indicator_set(sci, indic_number);
		_sci_indicator_clear(sci, 0, last_pos);
	}
	//g_warning("marker_indicator_clear: cleared");
}

gint marker_find_text(ScintillaObject *sci, MarkerMatchInfo *info) {
	return _sci_find_text(sci, info->flags, (struct Sci_TextToFind*)info);
}

static GSList *marker_find_range(ScintillaObject *sci, MarkerMatchInfo *info) {
	//g_warning("marker_find_range");
	GSList *matches = NULL;
	GdkColor *temp_color;
	/* if sci is null or text to search is null return null */
	g_return_val_if_fail(sci != NULL && info->lpstrText != NULL, NULL);
	if (info->lpstrText == NULL)
		return NULL;
	while (marker_find_text(sci, info) != -1)
	{
		/* Create a new info object for each match */
		if (info->chrgText.cpMax > info->chrg.cpMax) {
			/* found text is partially out of range */
			match_info_free(info);
			break;
		}
		matches = g_slist_prepend(matches, match_info_duplicate(info));
		info->chrg.cpMin = info->chrgText.cpMax;

		/* avoid rematching with empty matches like "(?=[a-z])" or "^$".
		 * note we cannot assume a match will always be empty or not and then break out, since
		 * matches like "a?(?=b)" will sometimes be empty and sometimes not */
		if (info->chrgText.cpMax == info->chrgText.cpMin) {
			info->chrg.cpMin ++;
		}
	}
	return g_slist_reverse(matches);
}

void marker_indicator_set_on_range(ScintillaObject *sci, MarkerMatchInfo *info) {
	//g_warning("marker_indicator_set_on_range");
	
	g_return_if_fail(sci != NULL);
	if (info->chrgText.cpMin >= info->chrgText.cpMax) //Not sure this matters - we can probably set the range backwards. 
		return;

	_sci_indicator_set(sci, info->indic_number);
	_sci_indicator_set_style(sci, info->indic_number, info->indic_style);
	_sci_indicator_fill(sci, info->chrgText.cpMin, info->chrgText.cpMax - info->chrgText.cpMin); 
	//g_warning("marker_indicator_set_on_range, color: %x , indic %i",  _GDK_COLOR_TO_SCI_COLOR(marker_data.color), indic);
	SSM(sci, SCI_INDICSETFORE, (uptr_t) info->indic_number, info->sci_color);
    /* Apply alpha for box types only */
    if (info->indic_style > 6) {
    SSM(sci, SCI_INDICSETALPHA, (uptr_t) info->indic_number, info->sci_alpha);
    }
}

gint search_mark_all(GeanyDocument *doc, const gchar *search_text, guint flags) {
	//g_warning("search_mark_all");
	
	gint count = 0;
	GSList *match;
	MarkerMatchInfo *info, *_info;
	_info = marker_data.last_mark_info;
	
	//Is doc guaranteed have an editor? 
	g_return_val_if_fail(DOC_VALID(doc), 0);
	
	/* clear previous search indicators */
	/* Adjust based on preferences */
	marker_indicator_clear(doc->editor->sci, marker_data.indic_number);

	if (G_UNLIKELY(EMPTY(search_text)))
		return 0;
	
	_info->chrg.cpMin = 0;
	_info->chrg.cpMax = sci_get_length(doc->editor->sci); 
	/* MatchInfo object text should be preserved */
	_info->lpstrText = search_text; //strdup? 
	_info->flags = flags; 
	_info->indic_number = marker_data.indic_number;
	_info->indic_style = marker_data.indic_style;
	_info->sci_color = _GDK_COLOR_TO_SCI_COLOR(marker_data.color);
	_info->doc = doc;
    _info->sci_alpha = marker_data.sci_alpha;
	
	/* Get a gslist of valid marker matches */
	if ((match = marker_find_range(doc->editor->sci, _info)) == NULL) {
		return 0;
	}
	/* Set indicators on all valid mathces */ 
	do {
		info = (MarkerMatchInfo*)match->data;
		//g_warning("cpMin %i  cpMax %i ", info->chrgText.cpMin, info->chrgText.cpMax);
		if (info->chrgText.cpMin != info->chrgText.cpMax) {
			marker_indicator_set_on_range(doc->editor->sci, info);
		}
		count++;
		match_info_free(info);
	} while((match = g_slist_next(match)) != NULL);

	g_slist_free(match);
	return count; 
}

/*
GList  *get_last_marker_info(void) {
	GList *list =  g_list_last(marker_data.active_markers);
	g_warning("LIST == NULL %i", (list == NULL));
	return list;
}
*/
MarkerMatchInfo *get_last_marker_info(void) {
	return marker_data.last_mark_info;
}

gint on_marker_set(gchar *entry_text, gint indic_number, gint indic_style,  GdkColor *color, gint alpha) {
	//g_warning("on_marker_set");
	GeanyDocument *doc = document_get_current();
	if (doc == NULL) {
		return -1; 
	}
	/* Currently unused */
	marker_data.backwards = FALSE;
	marker_data.search_bar = FALSE;
	/* Copy text entry */
	g_free(marker_data.text);
	g_free(marker_data.original_text); 
	marker_data.text = g_strdup(entry_text); 
	marker_data.original_text = g_strdup(marker_data.text);
	/* Set Indicator */ 
	marker_data.indic_number = indic_number; 
	marker_data.indic_style = indic_style;
    marker_data.sci_alpha = alpha; 
	/* Copy color */ 	
	memcpy(marker_data.color, color, sizeof(GdkColor));
	
	//g_warning("Got text entry: %s", marker_data.text);
	if (EMPTY(marker_data.text)) {
		return -1;
	}
	gint count = search_mark_all(doc, marker_data.text, marker_data.flags);
	
	if (count == 0) {
		ui_set_statusbar(FALSE, _("No matches found for \"%s\"."), marker_data.original_text);
	}
	else {
		ui_set_statusbar(FALSE,
			ngettext("Found %d match for \"%s\".",
					 "Found %d matches for \"%s\".", count),
			count, marker_data.original_text);
	} 
	return count;
}




