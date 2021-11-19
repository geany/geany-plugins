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

static gchar cmd_first_char;
static GPtrArray *history;
static GPtrArray *cmd_history;
static GPtrArray *search_history;
static gint history_pos;
/* text entered by user used to filter history */
static gchar *entered_text;
/* ignore change of text inside entry when set from history */
static gboolean ignore_change;


static void close_prompt()
{
	gtk_widget_hide(prompt);
	if (entered_text)
	{
		g_free(entered_text);
		entered_text = NULL;
	}
}


static void set_prompt_text(const gchar *val)
{
	gchar *text = g_strconcat(" ", val, NULL);
	text[0] = cmd_first_char;
	ignore_change = TRUE;
	gtk_entry_set_text(GTK_ENTRY(entry), text);
	gtk_editable_set_position(GTK_EDITABLE(entry), strlen(text));
	g_free(text);
}


static gboolean on_prompt_key_press_event(GtkWidget *widget, GdkEventKey *event, gpointer dummy)
{
	const gchar *text = gtk_entry_get_text(GTK_ENTRY(entry));
	guint modif_mask = gtk_accelerator_get_default_mod_mask();

	ignore_change = FALSE;

	if ((event->state & modif_mask) == 0)
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
				guint i;

				for (i = 0; i < history->len; i++)
					if (g_str_equal(text + 1, history->pdata[i]))
						g_ptr_array_remove_index(history, i);
				if (strlen(text) > 1)
					g_ptr_array_add(history, g_strdup(text + 1));
				if (history->len > 20) // default vim history size
					g_ptr_array_remove_index(history, 0);

				excmd_perform(ctx, text);
				close_prompt();

				return TRUE;
			}

			case GDK_KEY_Up:
			case GDK_KEY_KP_Up:
			case GDK_KEY_uparrow:
			{
				gint pos = -1;

				if (history_pos == -1 && history->len > 0)
					pos = history->len - 1;
				else if (history_pos > 0)
					pos = history_pos - 1;

				while (pos >= 0)
				{
					if (g_str_has_prefix(history->pdata[pos], entered_text))
						break;
					pos--;
				}

				if (pos != -1)
				{
					set_prompt_text(history->pdata[pos]);
					history_pos = pos;
				}

				return TRUE;
			}

			case GDK_KEY_Down:
			case GDK_KEY_KP_Down:
			case GDK_KEY_downarrow:
			{
				gint pos;

				if (history_pos == -1)
					return TRUE;

				pos = history_pos + 1;

				while (pos < history->len)
				{
					if (g_str_has_prefix(history->pdata[pos], entered_text))
						break;
					pos++;
				}

				if (pos >= history->len)
					pos = -1;

				set_prompt_text(pos == -1 ? "" : history->pdata[pos]);
				history_pos = pos;

				return TRUE;
			}

			case GDK_KEY_Home:
				gtk_editable_set_position(GTK_EDITABLE(entry), 1);
				return TRUE;
			case GDK_KEY_End:
				gtk_editable_set_position(GTK_EDITABLE(entry), strlen(text));
				return TRUE;
		}
	}
	else if ((event->state & modif_mask) == GDK_CONTROL_MASK)
	{
		switch (event->keyval)
		{
			case GDK_KEY_c:
				close_prompt();
				return TRUE;

			case GDK_KEY_b:
				gtk_editable_set_position(GTK_EDITABLE(entry), 1);
				return TRUE;
			case GDK_KEY_e:
				gtk_editable_set_position(GTK_EDITABLE(entry), strlen(text));
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
	else if (!ignore_change)
	{
		g_free(entered_text);
		entered_text = g_strdup(cmd + 1);
	}
}


static void on_prompt_show(GtkWidget *widget, gpointer dummy)
{
	gtk_widget_grab_focus(entry);
}


void ex_prompt_init(GtkWidget *parent_window, CmdContext *c)
{
	GtkWidget *frame;

	ctx = c;

	cmd_history = g_ptr_array_new_with_free_func(g_free);
	search_history = g_ptr_array_new_with_free_func(g_free);

	/* prompt */
	prompt = g_object_new(GTK_TYPE_WINDOW,
			"decorated", FALSE,
			"default-width", PROMPT_WIDTH,
			"default-height", 1,
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
	entered_text = g_strdup(val + 1);
	cmd_first_char = val[0];
	history = cmd_first_char == ':' ? cmd_history : search_history;
	gtk_widget_show(prompt);
	position_prompt();
	gtk_entry_set_text(GTK_ENTRY(entry), val);
	gtk_editable_set_position(GTK_EDITABLE(entry), strlen(val));
}


void ex_prompt_cleanup(void)
{
	gtk_widget_destroy(prompt);
	g_ptr_array_free(cmd_history, TRUE);
	g_ptr_array_free(search_history, TRUE);
}
