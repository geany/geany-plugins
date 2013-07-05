/*
 *  tooltip.c
 *
 *  Copyright 2012 Dimitar Toshkov Zhekov <dimitar.zhekov@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>

#include "common.h"

static void tooltip_trigger(void)
{
	GdkDisplay *display = gdk_display_get_default();
#if GTK_CHECK_VERSION(3, 0, 0)
	GdkDeviceManager *manager = gdk_display_get_device_manager(display);
	GdkDevice *device = gdk_device_manager_get_client_pointer(manager);
	GdkWindow *window = gdk_device_get_window_at_position(device, NULL, NULL);
#else
	GdkWindow *window = gdk_display_get_window_at_pointer(display, NULL, NULL);
#endif
	GeanyDocument *doc = document_get_current();

	if (doc && window)
	{
		GtkWidget *event_widget;

		gdk_window_get_user_data(window, (void **) &event_widget);
		/* if you know a better working way, do not hesistate to tell me */
		if (event_widget &&
			gtk_widget_is_ancestor(event_widget, GTK_WIDGET(doc->editor->sci)))
		{
			gtk_tooltip_trigger_tooltip_query(display);
		}
	}
}

static gchar *output = NULL;
static gint last_pos = -1;
static gint peek_pos = -1;
static gboolean show;

static void tooltip_set(gchar *text)
{
	show = text != NULL;
	g_free(output);
	output = text;
	last_pos = peek_pos;

	if (show)
	{
		if (pref_tooltips_length && strlen(output) > (size_t) pref_tooltips_length + 3)
			strcpy(output + pref_tooltips_length, "...");

		tooltip_trigger();
	}
}

static gint scid_gen = 0;

void on_tooltip_error(GArray *nodes)
{
	if (atoi(parse_grab_token(nodes)) == scid_gen)
	{
		if (pref_tooltips_fail_action == 1)
			tooltip_set(parse_get_error(nodes));
		else
		{
			tooltip_set(NULL);
			if (pref_tooltips_fail_action)
				plugin_blink();
		}
	}
}

static char *input = NULL;

void on_tooltip_value(GArray *nodes)
{
	if (atoi(parse_grab_token(nodes)) == scid_gen)
	{
		tooltip_set(parse_get_display_from_7bit(parse_lead_value(nodes),
			parse_mode_get(input, MODE_HBIT), parse_mode_get(input, MODE_MEMBER)));
	}
}

static guint query_id = 0;

static gboolean tooltip_launch(gpointer gdata)
{
	GeanyDocument *doc = document_get_current();

	if (doc && utils_source_document(doc) && doc->editor == gdata &&
		(debug_state() & DS_SENDABLE))
	{
		ScintillaObject *sci = doc->editor->sci;
		gchar *expr = sci_get_selection_mode(sci) == SC_SEL_STREAM &&
			peek_pos >= sci_get_selection_start(sci) &&
			peek_pos < sci_get_selection_end(sci) ?
			editor_get_default_selection(doc->editor, FALSE, NULL) :
			editor_get_word_at_pos(doc->editor, peek_pos, NULL);

		if ((expr = utils_verify_selection(expr)) != NULL)
		{
			g_free(input);
			input = debug_send_evaluate('3', scid_gen, expr);
			g_free(expr);
		}
		else
			tooltip_set(NULL);
	}
	else
		tooltip_set(NULL);

	query_id = 0;
	return FALSE;
}

static gboolean on_query_tooltip(G_GNUC_UNUSED GtkWidget *widget, gint x, gint y,
	gboolean keyboard_mode, GtkTooltip *tooltip, GeanyEditor *editor)
{
	gint pos = keyboard_mode ? sci_get_current_position(editor->sci) :
		scintilla_send_message(editor->sci, SCI_POSITIONFROMPOINT, x, y);

	if (pos >= 0)
	{
		if (pos == last_pos)
		{
			gtk_tooltip_set_text(tooltip, output);
			return show;
		}
		else if (pos != peek_pos)
		{
			if (query_id)
				g_source_remove(query_id);
			else
				scid_gen++;

			peek_pos = pos;
			query_id = plugin_timeout_add(geany_plugin, pref_tooltips_send_delay * 10,
				tooltip_launch, editor);
		}
	}

	return FALSE;
}

void tooltip_attach(GeanyEditor *editor)
{
	if (option_editor_tooltips)
	{
		gtk_widget_set_has_tooltip(GTK_WIDGET(editor->sci), TRUE);
		g_signal_connect(editor->sci, "query-tooltip", G_CALLBACK(on_query_tooltip), editor);
	}
}

void tooltip_remove(GeanyEditor *editor)
{
	GtkWidget *widget = GTK_WIDGET(editor->sci);

	if (gtk_widget_get_has_tooltip(widget))
	{
		gulong query_tooltip_id = g_signal_handler_find(widget, G_SIGNAL_MATCH_ID,
			g_signal_lookup("query-tooltip", GTK_TYPE_WIDGET), 0, NULL, NULL, NULL);

		if (query_tooltip_id)
			g_signal_handler_disconnect(widget, query_tooltip_id);
		gtk_widget_set_has_tooltip(widget, FALSE);
	}
}

void tooltip_clear(void)
{
	scid_gen = 0;
	last_pos = -1;
	peek_pos = -1;
}

gboolean tooltip_update(void)
{
	if (option_editor_tooltips)
	{
		last_pos = -1;
		peek_pos = -1;
		tooltip_trigger();
	}
	return TRUE;
}

void tooltip_finalize(void)
{
	g_free(output);
	g_free(input);
}
