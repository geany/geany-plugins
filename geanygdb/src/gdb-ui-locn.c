
/*
 * gdb-ui-locn.c - Breakpoint insertion dialog for a GTK-based GDB user interface.
 *
 * See the file "gdb-ui.h" for license information.
 *
 */


#include <string.h>
#include <gtk/gtk.h>
#include "gdb-io.h"
#include "gdb-ui.h"
#include "support.h"



void
gdbui_free_location_info(LocationInfo * li)
{
	if (li)
	{
		g_free(li->filename);
		g_free(li->line_num);
		g_free(li->symbol);
		g_free(li);
	}
}


LocationInfo *
gdbui_location_dlg(gchar * title, gboolean is_watch)
{
	GtkWidget *file_entry = NULL;
	GtkWidget *line_entry;
	GtkWidget *hbox;
	GtkWidget *opt_r = NULL;
	GtkWidget *opt_w = NULL;
	GtkWidget *opt_rw = NULL;
	GtkWidget *dlg;
	GtkWidget *btn;
	GtkWidget *img;
	GtkBox *vbox;
	gint resp = 0;
	LocationInfo *abi = NULL;
	LocationInfo *rv = NULL;
	if (gdbui_setup.location_query)
	{
		abi = gdbui_setup.location_query();
	}
	dlg = gdbui_new_dialog(title);
	vbox = GTK_BOX(GTK_DIALOG(dlg)->vbox);
	btn = gtk_dialog_add_button(GTK_DIALOG(dlg), _("Clea_r"), GTK_RESPONSE_APPLY);
	img = gtk_image_new_from_stock(GTK_STOCK_CLEAR, GTK_ICON_SIZE_BUTTON);

	gtk_button_set_image(GTK_BUTTON(btn), img);


	btn = gtk_dialog_add_button(GTK_DIALOG(dlg), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	btn = gtk_dialog_add_button(GTK_DIALOG(dlg), GTK_STOCK_OK, GTK_RESPONSE_OK);

	gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);

	if (!is_watch)
	{
		hbox = gtk_hbox_new(FALSE, 0);
		gtk_box_pack_start(vbox, hbox, TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new(_("Filename: ")), FALSE, FALSE, 0);
		file_entry = gtk_entry_new();
		if (abi && abi->filename)
		{
			gtk_entry_set_text(GTK_ENTRY(file_entry), abi->filename);
		}
		gtk_entry_set_activates_default(GTK_ENTRY(file_entry), TRUE);
		gtk_box_pack_start(GTK_BOX(hbox), file_entry, TRUE, TRUE, 0);
	}

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(vbox, hbox, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox),
			   gtk_label_new(is_watch ? _("Variable to watch:") :
					 _("Line number or function name: ")), FALSE, FALSE, 0);
	line_entry = gtk_entry_new();
	if (abi)
	{
		switch (is_watch)
		{
			case TRUE:
				{
					if (abi->symbol)
					{
						gtk_entry_set_text(GTK_ENTRY(line_entry),
								   abi->symbol);
					}
					break;
				}
			case FALSE:
				{
					if (abi->line_num)
					{
						gtk_entry_set_text(GTK_ENTRY(line_entry),
								   abi->line_num);
					}
					else
					{
						if (abi->symbol)
						{
							gtk_entry_set_text(GTK_ENTRY(line_entry),
									   abi->symbol);
						}
					}
					break;
				}
		}
	}

	gtk_entry_set_activates_default(GTK_ENTRY(line_entry), TRUE);
	gtk_box_pack_start(GTK_BOX(hbox), line_entry, TRUE, TRUE, is_watch ? 4 : 0);

	if (is_watch)
	{
		gtk_box_pack_start(vbox, gtk_hseparator_new(), FALSE, FALSE, 0);
		hbox = gtk_label_new(_("Access trigger:"));
		gtk_misc_set_alignment(GTK_MISC(hbox), 0.0f, 0.0f);
		gtk_box_pack_start(vbox, hbox, FALSE, FALSE, 0);
		hbox = gtk_hbox_new(FALSE, 0);
		gtk_box_pack_start(vbox, hbox, TRUE, TRUE, 0);
		opt_r = gtk_radio_button_new_with_label(NULL, "read");
		gtk_box_pack_start(GTK_BOX(hbox), opt_r, FALSE, FALSE, 0);
		opt_w = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(opt_r),
								    "write");
		gtk_box_pack_start(GTK_BOX(hbox), opt_w, FALSE, FALSE, 0);
		opt_rw = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(opt_r),
								     "both");
		gtk_box_pack_start(GTK_BOX(hbox), opt_rw, FALSE, FALSE, 0);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(opt_w), TRUE);
	}

	gtk_widget_show_all(dlg);
	do
	{
		gtk_widget_grab_focus(line_entry);
		resp = gtk_dialog_run(GTK_DIALOG(dlg));
		if (resp == GTK_RESPONSE_OK)
		{
			const gchar *locn = gtk_entry_get_text(GTK_ENTRY(line_entry));
			if (locn && *locn)
			{
				rv = g_new0(LocationInfo, 1);
				if (is_watch)
				{
					gchar *opt = "";
					if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(opt_r)))
					{
						opt = "-r";
					}
					else
					{
						if (gtk_toggle_button_get_active
						    (GTK_TOGGLE_BUTTON(opt_rw)))
						{
							opt = "-a";
						}
					}
					rv->filename = g_strdup(opt);
					rv->symbol = g_strdup(locn);
				}
				else
				{
					const gchar *filename =
						gtk_entry_get_text(GTK_ENTRY(file_entry));
					rv->filename = g_strdup(filename);
					rv->line_num = g_strdup(locn);
				}
				break;
			}
		}
		else
		{
			if (resp == GTK_RESPONSE_APPLY)
			{
				gtk_entry_set_text(GTK_ENTRY(line_entry), "");
				gtk_entry_set_text(GTK_ENTRY(file_entry), "");
			}
		}
	}
	while ((resp == GTK_RESPONSE_OK) || (resp == GTK_RESPONSE_APPLY));
	gtk_widget_destroy(dlg);

	gdbui_free_location_info(abi);
	return (rv);
}
