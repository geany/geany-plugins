/*
 * Copyright (C) 2018 Jiri Techet <techet@gmail.com>
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

#include "vi.h"

#include <gtk/gtk.h>

#define SSM(s, m, w, l) scintilla_send_message((s), (m), (w), (l))

static const gchar *fname = NULL;
static ScintillaObject *sci;
static GtkWidget *window;
static GtkWidget *statusbar;


static const gchar *get_mode_name(ViMode vi_mode)
{
	switch (vi_mode)
	{
		case VI_MODE_COMMAND:
			return "NORMAL";
			break;
		case VI_MODE_COMMAND_SINGLE:
			return "(insert)";
			break;
		case VI_MODE_INSERT:
			return "INSERT";
			break;
		case VI_MODE_REPLACE:
			return "REPLACE";
			break;
		case VI_MODE_VISUAL:
			return "VISUAL";
			break;
		case VI_MODE_VISUAL_LINE:
			return "VISUAL LINE";
			break;
		case VI_MODE_VISUAL_BLOCK:
			return "VISUAL BLOCK";
			break;
	}
	return "";
}


static void set_statusbar_text(const gchar *text)
{
	static guint id = 0;
	if (id == 0)
		id = gtk_statusbar_get_context_id(GTK_STATUSBAR(statusbar), "viw");

	gtk_statusbar_pop(GTK_STATUSBAR(statusbar), id);
	gtk_statusbar_push(GTK_STATUSBAR(statusbar), id, text);
}


static void on_mode_change(ViMode mode)
{
	gchar *msg = g_strconcat("-- ", get_mode_name(mode), " --", NULL);
	set_statusbar_text(msg);
	g_free(msg);
}


static gboolean on_save(gboolean force)
{
	gint size;
	gchar *buf;
	gboolean success;

	if (!SSM(sci, SCI_GETMODIFY, 0, 0) && !force)
		return TRUE;

	if (!fname)
	{
		GtkWidget *dialog = gtk_file_chooser_dialog_new ("Save File", GTK_WINDOW(window),
			GTK_FILE_CHOOSER_ACTION_SAVE,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);
		gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
		if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
			fname = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
		gtk_widget_destroy(dialog);
	}

	if (!fname)
		return FALSE;

	size = SSM(sci, SCI_GETLENGTH, 0, 0) + 1;
	buf = g_malloc(size);
	SSM(sci, SCI_GETTEXT, size, (sptr_t)buf);
	success = g_file_set_contents(fname, buf, size, NULL);
	g_free(buf);

	if (success)
		SSM(sci, SCI_SETSAVEPOINT, 0, 0);
	else
		set_statusbar_text("Error saving file");

	return success;
}


static gboolean on_save_all(gboolean force)
{
	return on_save(force);
}


static void on_quit(gboolean force)
{
	if (force || !SSM(sci, SCI_GETMODIFY, 0, 0))
		gtk_main_quit();
	else
		set_statusbar_text("Save the file before exiting or force (!) the command");
}


static gboolean on_wrong_quit(GtkWidget *widget, GdkEvent *event, gpointer parent_window)
{
	GtkWidget *dialog = gtk_message_dialog_new(parent_window,
		GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_YES_NO,
		"Really?");
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
		"Did you really expect you would close a vi-based editor this way?");
	gtk_window_set_title(GTK_WINDOW(dialog), "Huh!");
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	return TRUE;
}


static gboolean on_key_press_cb(GtkWidget *widget, GdkEventKey *ev, gpointer user_data)
{
	return vi_notify_key_press(ev);
}


static void on_sci_notify_cb(G_GNUC_UNUSED GtkWidget *widget, G_GNUC_UNUSED gint scn,
	gpointer scnt, gpointer data)
{
	vi_notify_sci(scnt);
}


// stolen from Geany
static void use_monospaced_font(void)
{
	gint style, size;
	gchar *font_name;
	PangoFontDescription *pfd;

	pfd = pango_font_description_from_string("Monospace 10");
	size = pango_font_description_get_size(pfd) / PANGO_SCALE;
	font_name = g_strdup_printf("!%s", pango_font_description_get_family(pfd));
	pango_font_description_free(pfd);

	for (style = 0; style <= STYLE_MAX; style++)
	{
		SSM(sci, SCI_STYLESETFONT, (uptr_t) style, (sptr_t) font_name);
		SSM(sci, SCI_STYLESETSIZE, (uptr_t) style, size);
	}

	g_free(font_name);
}


static void open_file(const gchar *name)
{
	gchar *buf;
	gsize len;
	if (g_file_get_contents(name, &buf, &len, NULL))
	{
		fname = g_strdup(name);
		SSM(sci, SCI_ADDTEXT, len, (sptr_t)buf);
		SSM(sci, SCI_GOTOPOS, 0, 0);
		SSM(sci, SCI_SETSAVEPOINT, 0, 0);
		SSM(sci, SCI_EMPTYUNDOBUFFER, 0, 0);
		g_free(buf);
	}
	else
		set_statusbar_text("File could not be opened");
}


int main(int argc, char **argv)
{
	GtkWidget *editor, *vbox;
	ViCallback cb;

	gtk_init(&argc, &argv);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), "viw - vi worsened");
	g_signal_connect(G_OBJECT(window), "delete-event", G_CALLBACK(on_wrong_quit), window);

	editor = scintilla_new();
	sci = SCINTILLA(editor);
	SSM(sci, SCI_SETCODEPAGE, SC_CP_UTF8, 0);
	use_monospaced_font();
	//show line number margin
	SSM(sci, SCI_SETMARGINWIDTHN, 0, 40);
	//hide symbol margin
	SSM(sci, SCI_SETMARGINWIDTHN, 1, 0);
	if (argc > 1)
		open_file(argv[1]);

	statusbar = gtk_statusbar_new();
	vbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), editor, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), statusbar, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);
	gtk_widget_set_size_request(editor, 640, 480);

	gtk_widget_show_all(window);
	gtk_widget_grab_focus(editor);

	// the below is the only setup needed to convert normal editor to a vi editor
	cb.on_mode_change = on_mode_change;
	cb.on_save = on_save;
	cb.on_save_all = on_save_all;
	cb.on_quit = on_quit;
	vi_init(window, &cb);
	vi_set_active_sci(sci);
	g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_press_cb), NULL);
	g_signal_connect(editor, "sci-notify", G_CALLBACK(on_sci_notify_cb), NULL);

	gtk_main();

	return 0;
}
