/*
 * Copyright 2018 Jiri Techet <techet@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "excmd-prompt.h"
#include "excmd-runner.h"

#include <gdk/gdkkeysyms.h>
#include <string.h>

#if ! GTK_CHECK_VERSION (3, 0, 0) && ! defined (gtk_widget_get_allocated_width)
# define gtk_widget_get_allocated_width(w) (GTK_WIDGET (w)->allocation.width)
#endif
#if ! GTK_CHECK_VERSION (3, 0, 0) && ! defined (gtk_widget_get_allocated_height)
# define gtk_widget_get_allocated_height(w) (GTK_WIDGET (w)->allocation.height)
#endif

#define PROMPT_WIDTH 500

static GtkWidget *prompt;
static GtkWidget *entry;
static CmdContext *ctx;
static GList *history;
static gint history_pos;


static void close_prompt()
{
	gtk_widget_hide(prompt);
}


static gboolean on_prompt_key_press_event(GtkWidget *widget, GdkEventKey *event, gpointer dummy)
{
	switch (event->keyval)
	{
		case GDK_KEY_Escape:
			close_prompt();
			return TRUE;

		case GDK_KEY_Tab:
			/* avoid leaving the entry */
			return TRUE;

		case GDK_KEY_Return:
		case GDK_KEY_KP_Enter:
		case GDK_KEY_ISO_Enter:
		{
			const gchar *text = gtk_entry_get_text(GTK_ENTRY(entry));
			GList *link = g_list_find_custom(history, text, (GCompareFunc)g_strcmp0);

			if (link)
			{
				g_free(link->data);
				history = g_list_remove_link(history, link);
			}
			if (g_strcmp0(text, ":") != 0)
				history = g_list_prepend(history, g_strdup(text));

			excmd_perform(ctx, text);
			close_prompt();

			return TRUE;
		}

		case GDK_KEY_Up:
		case GDK_KEY_KP_Up:
		case GDK_KEY_uparrow:
		{
			gint history_len = g_list_length(history);

			if (history_pos == -1 && history_len > 0)
				history_pos = 0;
			else if (history_pos + 1 < history_len)
				history_pos++;

			if (history_pos != -1 && history_pos < history_len)
			{
				gchar *val = g_list_nth_data(history, history_pos);
				gtk_entry_set_text(GTK_ENTRY(entry), val);
				gtk_editable_set_position(GTK_EDITABLE(entry), strlen(val));
			}

			return TRUE;
		}

		case GDK_KEY_Down:
		case GDK_KEY_KP_Down:
		case GDK_KEY_downarrow:
		{
			const gchar *val;

			if (history_pos == -1)
				return TRUE;

			history_pos--;

			val = history_pos == -1 ? ":" : g_list_nth_data(history, history_pos);
			gtk_entry_set_text(GTK_ENTRY(entry), val);
			gtk_editable_set_position(GTK_EDITABLE(entry), strlen(val));

			return TRUE;
		}
	}

	history_pos = -1;

	return FALSE;
}


static void on_entry_text_notify(GObject *object, GParamSpec *pspec, gpointer dummy)
{
	const gchar* cmd = gtk_entry_get_text(GTK_ENTRY(entry));

	if (cmd == NULL || strlen(cmd) == 0)
		close_prompt();
}


static void on_prompt_show(GtkWidget *widget, gpointer dummy)
{
	gtk_widget_grab_focus(entry);
}


void ex_prompt_init(GtkWidget *parent_window, CmdContext *c)
{
	GtkWidget *frame;

	ctx = c;

	history = NULL;

	/* prompt */
	prompt = g_object_new(GTK_TYPE_WINDOW,
			"decorated", FALSE,
			"default-width", PROMPT_WIDTH,
			"transient-for", parent_window,
			"window-position", GTK_WIN_POS_CENTER_ON_PARENT,
			"type-hint", GDK_WINDOW_TYPE_HINT_DIALOG,
			"skip-taskbar-hint", TRUE,
			"skip-pager-hint", TRUE,
			NULL);
	g_signal_connect(prompt, "focus-out-event", G_CALLBACK(close_prompt), NULL);
	g_signal_connect(prompt, "show", G_CALLBACK(on_prompt_show), NULL);
	g_signal_connect(prompt, "key-press-event", G_CALLBACK(on_prompt_key_press_event), NULL);

	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
	gtk_container_add(GTK_CONTAINER(prompt), frame);
  
	entry = gtk_entry_new();
	gtk_container_add(GTK_CONTAINER(frame), entry);

	g_signal_connect(entry, "notify::text", G_CALLBACK(on_entry_text_notify), NULL);

	gtk_widget_show_all(frame);
}


static void position_prompt(void)
{
	gint sci_x, sci_y;
	gint sci_width = gtk_widget_get_allocated_width(GTK_WIDGET(ctx->sci));
	gint prompt_width = PROMPT_WIDTH > sci_width ? sci_width : PROMPT_WIDTH;
	gint prompt_height = gtk_widget_get_allocated_height(GTK_WIDGET(prompt));
	gdk_window_get_origin(gtk_widget_get_window(GTK_WIDGET(ctx->sci)), &sci_x, &sci_y);
	gtk_window_resize(GTK_WINDOW(prompt), prompt_width, prompt_height);
	gtk_window_move(GTK_WINDOW(prompt), sci_x + (sci_width - prompt_width) / 2, sci_y);
}


void ex_prompt_show(const gchar *val)
{
	history_pos = -1;
	gtk_widget_show(prompt);
	position_prompt();
	gtk_entry_set_text(GTK_ENTRY(entry), val);
	gtk_editable_set_position(GTK_EDITABLE(entry), strlen(val));
}


void ex_prompt_cleanup(void)
{
	gtk_widget_destroy(prompt);
	g_list_free_full(history, g_free);
}
