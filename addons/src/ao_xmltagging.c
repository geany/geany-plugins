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

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif
#include <geanyplugin.h>

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
		GtkWidget *dialog = NULL;
		GtkWidget *vbox = NULL;
		GtkWidget *hbox = NULL;
		GtkWidget *label = NULL;
		GtkWidget *textbox = NULL;
		GtkWidget *textline = NULL;

		dialog = gtk_dialog_new_with_buttons(_("XML tagging"),
							 GTK_WINDOW(geany->main_widgets->window),
							 GTK_DIALOG_DESTROY_WITH_PARENT, "_Cancel",
							 GTK_RESPONSE_CANCEL, "_OK", GTK_RESPONSE_ACCEPT,
							 NULL);
		vbox = ui_dialog_vbox_new(GTK_DIALOG(dialog));
		gtk_widget_set_name(dialog, "GeanyDialog");
		gtk_box_set_spacing(GTK_BOX(vbox), 10);

		hbox = gtk_hbox_new(FALSE, 10);

		label = gtk_label_new(_("Tag name to be inserted:"));
		textbox = gtk_entry_new();

		textline = gtk_label_new(
			_("%s will be replaced with your current selection. Please keep care on your selection"));

		gtk_container_add(GTK_CONTAINER(hbox), label);
		gtk_container_add(GTK_CONTAINER(hbox), textbox);
		g_object_set(label, "xalign",  0.0, NULL);
        g_object_set(label, "yalign",  0.5, NULL);

		gtk_container_add(GTK_CONTAINER(vbox), hbox);
		gtk_container_add(GTK_CONTAINER(vbox), textline);

		g_signal_connect(G_OBJECT(textbox), "activate",
			G_CALLBACK(enter_key_pressed_in_entry), dialog);

		gtk_widget_show_all(vbox);

		if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
		{
			GString *tag = NULL;
			gchar *selection  = NULL;
			gchar *replacement = NULL;

			/* Getting the selection and setting the undo flag */
			selection = sci_get_selection_contents(doc->editor->sci);
			sci_start_undo_action(doc->editor->sci);

			/* Getting the tag */
			tag = g_string_new(gtk_entry_get_text(GTK_ENTRY(textbox)));

			if (tag->len > 0)
			{
				gsize end = 0;
				gchar *end_tag;

				/* First we check for %s and replace it with selection*/
				utils_string_replace_all(tag, "%s", selection);

				/* We try to find a space inside the inserted tag as we
				 * only need to close the tag with part until first space.
				 * */
				while (end < tag->len && !g_ascii_isspace(tag->str[end]) )
					end++;

				if (end > 0)
				{
					end_tag = g_strndup(tag->str, end);
				}
				else
				{
					end_tag = tag->str;
				}
				replacement = g_strconcat("<", tag->str, ">",
								selection, "</", end_tag, ">", NULL);
				g_free(end_tag);
			}

			sci_replace_sel(doc->editor->sci, replacement);
			sci_end_undo_action(doc->editor->sci);
			g_free(selection);
			g_free(replacement);
			g_string_free(tag, TRUE);
		}
		gtk_widget_destroy(dialog);
	}
}
