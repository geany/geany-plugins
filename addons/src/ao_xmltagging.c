/*
 *	  ao_xmltagging.c
 *
 *	  Copyright 2010 Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
 *
 *	  This program is free software; you can redistribute it and/or modify
 *	  it under the terms of the GNU General Public License as published by
 *	  the Free Software Foundation; either version 2 of the License, or
 *	  (at your option) any later version.
 *
 *	  This program is distributed in the hope that it will be useful,
 *	  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	  GNU General Public License for more details.
 *
 *	  You should have received a copy of the GNU General Public License
 *	  along with this program; if not, write to the Free Software
 *	  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *	  MA 02110-1301, USA.
 */

#include "geanyplugin.h"
#include "addons.h"
#include "ao_xmltagging.h"


static void enter_key_pressed_in_entry(G_GNUC_UNUSED GtkWidget *widget, gpointer dialog)
{
	gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
}


void ao_xmltagging(void)
{
	GeanyDocument *doc = NULL;

	doc = document_get_current();

	g_return_if_fail(doc != NULL);

	if (sci_has_selection(doc->editor->sci) == TRUE)
	{
		gchar *selection  = NULL;
		gchar *replacement = NULL;
		GtkWidget *dialog = NULL;
		GtkWidget *vbox = NULL;
		GtkWidget *hbox = NULL;
		GtkWidget *label = NULL;
		GtkWidget *textbox = NULL;

		dialog = gtk_dialog_new_with_buttons(_("XML tagging"),
							 GTK_WINDOW(geany->main_widgets->window),
							 GTK_DIALOG_DESTROY_WITH_PARENT, GTK_STOCK_CANCEL,
							 GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
							 NULL);
		vbox = ui_dialog_vbox_new(GTK_DIALOG(dialog));
		gtk_widget_set_name(dialog, "GeanyDialog");
		gtk_box_set_spacing(GTK_BOX(vbox), 10);

		hbox = gtk_hbox_new(FALSE, 10);

		label = gtk_label_new(_("Tag name to be inserted:"));
		textbox = gtk_entry_new();

		gtk_container_add(GTK_CONTAINER(hbox), label);
		gtk_container_add(GTK_CONTAINER(hbox), textbox);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

		gtk_container_add(GTK_CONTAINER(vbox), hbox);

		g_signal_connect(G_OBJECT(textbox), "activate",
			G_CALLBACK(enter_key_pressed_in_entry), dialog);

		gtk_widget_show_all(vbox);

		if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
		{
			const gchar *tag = NULL;

			selection = sci_get_selection_contents(doc->editor->sci);
			sci_start_undo_action(doc->editor->sci);

			tag = g_strdup(gtk_entry_get_text(GTK_ENTRY(textbox)));
			if (NZV(tag))
			{
				gint end = 0;
				const gchar *end_tag;
				/* We try to find a space inside the inserted tag as we
				 * only need to close the tag with part until first space.
				 * */
				while (!g_ascii_isspace(tag[end]) && tag[end] != '\0')
					end++;

				if (end > 0)
				{
					end_tag = g_strndup(tag, end);
				}
				else
				{
					end_tag = tag;
				}
				replacement = g_strconcat("<", tag, ">",
								selection, "</", end_tag, ">", NULL);
			}

			sci_replace_sel(doc->editor->sci, replacement);
			sci_end_undo_action(doc->editor->sci);
			g_free(selection);
			g_free(replacement);
		}
		gtk_widget_destroy(dialog);
	}
}
