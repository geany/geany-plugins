/*
 * gdb-ui-break.c - Breakpoint management for a GTK-based GDB user interface.
 * Copyright 2008 Jeff Pohlmeyer <yetanothergeek(at)gmail(dot)com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA. *
 */


#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include "gdb-io.h"
#include "gdb-ui.h"
#include "support.h"

enum
{
	dlgRespClose,
	dlgRespDeleteConfirmed,
	dlgRespDeleteCancelled,
	dlgRespEditConfirmed,
	dlgRespEditCancelled,
	dlgRespAddConfirmed,
	dlgRespAddCancelled
};



typedef enum _BreakColumn
{
	bcNumber,
	bcEnabled,
	bcWhat,
	bcFile,
	bcLine,
	bcFunc,
	bcTimes,
	bcCond,
	bcIgnore,
	bcAccess,
	bcData,
	bcNumCols
} BreakColumn;



typedef struct _BkPtDlgData
{
	GtkWidget *dlg;
	GdbBreakPointInfo *bpi;
} BkPtDlgData;


static void break_dlg(const GSList * frame_list);


static gboolean is_watchlist = FALSE;
static gint prev_resp = dlgRespClose;




static void
break_select_cb(GtkTreeSelection * selection, gpointer data)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	GdbBreakPointInfo *bpi = NULL;
	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gtk_tree_model_get(model, &iter, bcData, &bpi, -1);
		if (bpi)
		{
			BkPtDlgData *bpd = data;
			bpd->bpi = bpi;
		}
	}
}



static gboolean
confirm(const gchar * question)
{
	gboolean rv = FALSE;
	GtkWidget *dlg =
		gtk_message_dialog_new(GTK_WINDOW(gdbui_setup.main_window), GTK_MESSAGE_QUESTION,
				       GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				       GTK_BUTTONS_YES_NO, "%s", question);
	gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_YES);
	rv = gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_YES;
	gtk_widget_destroy(dlg);
	return rv;
}



static void
delete_click(GtkWidget * w, gpointer p)
{
	BkPtDlgData *bpd = p;

	if (! bpd->bpi)
	{
		GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(gdbui_setup.main_window),
			GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
			_("No %s selected"), is_watchlist ? _("watchpoint") : _("breakpoint"));
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
		return;
	}

	if (confirm(is_watchlist ? _("Delete selected watchpoint?") : _("Delete selected breakpoint?")))
	{
		gdbui_enable(FALSE);
		gdbio_delete_break(break_dlg, bpd->bpi->number);
		prev_resp = dlgRespDeleteConfirmed;
		gtk_dialog_response(GTK_DIALOG(bpd->dlg), dlgRespDeleteConfirmed);
	}
	else
	{
		gtk_dialog_response(GTK_DIALOG(bpd->dlg), dlgRespDeleteCancelled);
	}
}




static void
edit_click(GtkWidget * w, gpointer p)
{
	BkPtDlgData *bpd = p;
	GtkWidget *enabled_chk;
	GtkWidget *after_entry;
	GtkWidget *condition_entry;
	GtkWidget *hbox;
	gint resp = 0;
	gboolean changed = FALSE;

	GtkWidget *dlg =
		gtk_dialog_new_with_buttons(is_watchlist ? _("Edit watchpoint") : _("Edit breakpoint"),
					    GTK_WINDOW(gdbui_setup.main_window),
					    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					    GTK_STOCK_OK, GTK_RESPONSE_OK,
					    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL);

	GtkBox *vbox = GTK_BOX(GTK_DIALOG(dlg)->vbox);

	gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);

	enabled_chk = gtk_check_button_new_with_label(_("Enabled"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enabled_chk), bpd->bpi->enabled[0] == 'y');
	gtk_box_pack_start(vbox, enabled_chk, FALSE, FALSE, 0);
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(vbox, hbox, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new(_(" Break after ")), FALSE, FALSE, 0);
	after_entry = gtk_entry_new();
	if (bpd->bpi->ignore)
	{
		gtk_entry_set_text(GTK_ENTRY(after_entry), bpd->bpi->ignore);
	}
	gtk_box_pack_start(GTK_BOX(hbox), after_entry, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new(_(" times. ")), FALSE, FALSE, 0);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(vbox, hbox, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new(_(" Break when ")), FALSE, FALSE, 0);
	condition_entry = gtk_entry_new();
	if (bpd->bpi->cond)
	{
		gtk_entry_set_text(GTK_ENTRY(condition_entry), bpd->bpi->cond);
	}
	gtk_box_pack_start(GTK_BOX(hbox), condition_entry, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new(_(" is true. ")), FALSE, FALSE, 0);

	gtk_widget_show_all(dlg);
	gtk_entry_set_activates_default(GTK_ENTRY(condition_entry), TRUE);
	gtk_entry_set_activates_default(GTK_ENTRY(after_entry), TRUE);

	resp = gtk_dialog_run(GTK_DIALOG(dlg));
	if (resp == GTK_RESPONSE_OK)
	{
		const gchar *cond = gtk_entry_get_text(GTK_ENTRY(condition_entry));
		const gchar *ignore = gtk_entry_get_text(GTK_ENTRY(after_entry));
		gboolean enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enabled_chk));

		if (!g_str_equal(cond, bpd->bpi->cond ? bpd->bpi->cond : ""))
		{
			gdbio_break_cond(bpd->bpi->number, cond);
			changed = TRUE;
		}
		if (!g_str_equal(ignore, bpd->bpi->ignore ? bpd->bpi->ignore : ""))
		{
			gdbio_ignore_break(bpd->bpi->number, ignore);
			changed = TRUE;
		}
		switch (enabled)
		{
			case TRUE:
				{
					if (bpd->bpi->enabled[0] != 'y')
					{
						gdbio_enable_break(bpd->bpi->number, TRUE);
						changed = TRUE;
					}
					break;
				}
			case FALSE:
				{
					if (bpd->bpi->enabled[0] == 'y')
					{
						gdbio_enable_break(bpd->bpi->number, FALSE);
						changed = TRUE;
					}
					break;
				}
		}
	}
	if (!changed)
	{
		resp = GTK_RESPONSE_CANCEL;

	}
	else
	{
		gdbui_enable(FALSE);
	}
	gtk_widget_destroy(dlg);
	gtk_dialog_response(GTK_DIALOG(bpd->dlg),
			    resp == GTK_RESPONSE_OK ? dlgRespEditConfirmed : dlgRespEditCancelled);
}


static void
add_click(GtkWidget * w, gpointer p)
{
	BkPtDlgData *bpd = p;
	LocationInfo *abi =
		gdbui_location_dlg(is_watchlist ? _("Add watchpoint") : _("Add breakpoint"),
				   is_watchlist);
	if (abi)
	{
		if (is_watchlist)
		{
			gdbio_add_watch(break_dlg, abi->filename ? abi->filename : "", abi->symbol);
		}
		else
		{
			gdbio_add_break(break_dlg, abi->filename, abi->line_num);
		}
	}
	if (bpd)
	{
		if (abi)
		{
			gdbui_enable(FALSE);
		}
		gtk_dialog_response(GTK_DIALOG(bpd->dlg),
				    abi ? dlgRespAddConfirmed : dlgRespAddCancelled);
	}
	gdbui_free_location_info(abi);
}



static gboolean
list_keypress(GtkWidget * w, GdkEventKey * ev, gpointer user_data)
{
	BkPtDlgData *bpd = user_data;
	if (ev->type == GDK_KEY_PRESS)
	{
		switch (ev->keyval)
		{
			case GDK_Return:
				{
					gtk_dialog_response(GTK_DIALOG(bpd->dlg), dlgRespClose);
					break;
				}
			case GDK_Delete:
				{
					delete_click(w, bpd);
					break;
				}
			case GDK_Insert:
				{
					add_click(w, bpd);
					break;
				}
			case GDK_F2:
				{
					edit_click(w, bpd);
					break;
				}
		}
	}
	return FALSE;
}


/*
"read watchpoint" //read
"hw watchpoint"  //write
"acc watchpoint" //read/write
*/

static const gchar *
access_txt(GdbBreakPointInfo * bpi)
{
	if (bpi && bpi->type)
		switch (bpi->type[0])
		{
			case 'h':
				return "write";
			case 'r':
				return "read";
			case 'a':
				return "rd/wr";
		}
	return NULL;
}



static gboolean
has_items(const GSList * frame_list)
{
	const GSList *p;
	if (prev_resp == dlgRespDeleteConfirmed)
	{
		prev_resp = dlgRespClose;
		return TRUE;
	}
	for (p = frame_list; p; p = p->next)
	{
		GdbBreakPointInfo *bpi = p->data;
		if (g_str_equal(bpi->number, "1"))
		{
			continue;
		}
		if ((bpi) && (strstr(bpi->type, is_watchlist ? _("watchpoint") : _("breakpoint"))))
		{
			return TRUE;
		}
	}
	return FALSE;
}



static void
break_dlg(const GSList * frame_list)
{
	gdbui_enable(TRUE);
	if (has_items(frame_list))
	{
		const GSList *p;
		GtkTreeIter iter;
		GtkListStore *store;
		GtkWidget *listview;
		GtkTreeViewColumn *column;
		GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
		GtkTreeSelection *selection;
		GtkWidget *scroll;
		GtkWidget *delete_btn;
		GtkWidget *add_btn;
		GtkWidget *edit_btn;
		GtkWidget *close_btn;
		gboolean HaveWhat = FALSE;
		gboolean HaveFile = FALSE;
		gboolean HaveLine = FALSE;
		gboolean HaveFunc = FALSE;
		gboolean HaveAccess = FALSE;
		gint resp = 0;
		const gchar *access_type;
		BkPtDlgData bpd = { NULL, NULL };
		gint rowcount = 0;
		store = gtk_list_store_new(bcNumCols, G_TYPE_STRING,	//bcNumber
					   G_TYPE_STRING,	//bcEnabled
					   G_TYPE_STRING,	//bcWhat
					   G_TYPE_STRING,	//bcFile
					   G_TYPE_STRING,	//bcLine
					   G_TYPE_STRING,	//bcFunc
					   G_TYPE_STRING,	//bcTimes
					   G_TYPE_STRING,	//bcIgnore
					   G_TYPE_STRING,	//bcCond
					   G_TYPE_STRING,	//bcAccess
					   G_TYPE_POINTER	//bcData
			);
		for (p = frame_list; p; p = p->next)
		{
			GdbBreakPointInfo *bpi = p->data;
			if (bpi)
			{
				gboolean iswatch = !g_str_equal(_("breakpoint"), bpi->type);
				if (is_watchlist != iswatch)
				{
					continue;
				}
				if (g_str_equal(bpi->number, "1"))
				{
					continue;
				}
				access_type = access_txt(bpi);

				gtk_list_store_append(store, &iter);
				gtk_list_store_set(store, &iter,
						   bcNumber, bpi->number,
						   bcEnabled, bpi->enabled,
						   bcWhat, bpi->what,
						   bcFile, bpi->file,
						   bcLine, bpi->line,
						   bcFunc, bpi->func,
						   bcTimes, bpi->times,
						   bcIgnore, bpi->ignore ? bpi->ignore : "0",
						   bcCond, bpi->cond,
						   bcAccess, access_type, bcData, bpi, -1);
				HaveWhat = HaveWhat || (bpi->what && *bpi->what);
				HaveFile = HaveFile || (bpi->file && *bpi->file);
				HaveLine = HaveLine || (bpi->line && *bpi->line);
				HaveFunc = HaveFunc || (bpi->func && *bpi->func);
				HaveAccess = HaveAccess || access_type;
				rowcount++;
			}
		}
		listview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));

		g_signal_connect(G_OBJECT(listview), "key-press-event", G_CALLBACK(list_keypress),
				 &bpd);
		column = gtk_tree_view_column_new_with_attributes("#", renderer, "text", bcNumber,
								  NULL);
		gtk_tree_view_append_column(GTK_TREE_VIEW(listview), column);

		column = gtk_tree_view_column_new_with_attributes("on", renderer, "text", bcEnabled,
								  NULL);
		gtk_tree_view_append_column(GTK_TREE_VIEW(listview), column);

		if (HaveWhat)
		{
			column = gtk_tree_view_column_new_with_attributes("what", renderer, "text",
									  bcWhat, NULL);
			gtk_tree_view_append_column(GTK_TREE_VIEW(listview), column);
		}

		if (HaveFile)
		{
			column = gtk_tree_view_column_new_with_attributes("filename", renderer,
									  "text", bcFile, NULL);
			gtk_tree_view_append_column(GTK_TREE_VIEW(listview), column);
		}

		if (HaveLine)
		{
			column = gtk_tree_view_column_new_with_attributes("line", renderer, "text",
									  bcLine, NULL);
			gtk_tree_view_append_column(GTK_TREE_VIEW(listview), column);
		}

		if (HaveFunc)
		{
			column = gtk_tree_view_column_new_with_attributes("function", renderer,
									  "text", bcFunc, NULL);
			gtk_tree_view_append_column(GTK_TREE_VIEW(listview), column);
		}


		if (HaveAccess)
		{
			column = gtk_tree_view_column_new_with_attributes("trap", renderer, "text",
									  bcAccess, NULL);
			gtk_tree_view_append_column(GTK_TREE_VIEW(listview), column);
		}


		column = gtk_tree_view_column_new_with_attributes("times", renderer, "text",
								  bcTimes, NULL);
		gtk_tree_view_append_column(GTK_TREE_VIEW(listview), column);

		column = gtk_tree_view_column_new_with_attributes("skip", renderer, "text",
								  bcIgnore, NULL);
		gtk_tree_view_append_column(GTK_TREE_VIEW(listview), column);

		column = gtk_tree_view_column_new_with_attributes("condition", renderer, "text",
								  bcCond, NULL);
		gtk_tree_view_append_column(GTK_TREE_VIEW(listview), column);

		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(listview));
		gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
		g_signal_connect(G_OBJECT(selection), "changed", G_CALLBACK(break_select_cb), &bpd);

		bpd.dlg = gdbui_new_dialog(is_watchlist ? _("Watchpoints") : _("Breakpoints"));

		scroll = gtk_scrolled_window_new(NULL, NULL);
		gtk_widget_set_usize(scroll,
				     (gdk_screen_get_width(gdk_screen_get_default()) / 3) * 2,
				     (gdk_screen_get_height(gdk_screen_get_default()) / 3) * 1);
		gtk_container_add(GTK_CONTAINER(scroll), listview);

		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(bpd.dlg)->vbox), scroll, FALSE, FALSE, 0);
		delete_btn = gtk_button_new_from_stock(GTK_STOCK_DELETE);

		g_signal_connect(G_OBJECT(delete_btn), "clicked", G_CALLBACK(delete_click), &bpd);
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(bpd.dlg)->action_area), delete_btn);
		edit_btn = gtk_button_new_from_stock(GTK_STOCK_EDIT);

		g_signal_connect(G_OBJECT(edit_btn), "clicked", G_CALLBACK(edit_click), &bpd);
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(bpd.dlg)->action_area), edit_btn);
		add_btn = gtk_button_new_from_stock(GTK_STOCK_ADD);

		g_signal_connect(G_OBJECT(add_btn), "clicked", G_CALLBACK(add_click), &bpd);
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(bpd.dlg)->action_area), add_btn);



		close_btn =
			gtk_dialog_add_button(GTK_DIALOG(bpd.dlg), GTK_STOCK_CLOSE, dlgRespClose);
		gtk_widget_set_sensitive(delete_btn, rowcount > 0);
		gtk_widget_set_sensitive(edit_btn, rowcount > 0);
		gtk_dialog_set_default_response(GTK_DIALOG(bpd.dlg), dlgRespClose);
		gtk_widget_show_all(bpd.dlg);
		do
		{
			resp = gtk_dialog_run(GTK_DIALOG(bpd.dlg));
			switch (resp)
			{
				case dlgRespDeleteConfirmed:
					{
						resp = dlgRespClose;
						gtk_widget_destroy(bpd.dlg);
						break;
					}
				case dlgRespDeleteCancelled:
					{
						break;
					}
				case dlgRespEditConfirmed:
					{
						resp = dlgRespClose;
						gtk_widget_destroy(bpd.dlg);
						gdbio_wait(100);
						gdbui_enable(FALSE);
						gdbio_show_breaks(break_dlg);
						break;
					}
				case dlgRespEditCancelled:
					{
						break;
					}
				case dlgRespAddConfirmed:
					{
						resp = dlgRespClose;
						gtk_widget_destroy(bpd.dlg);
						break;
					}
				case dlgRespAddCancelled:
					{
						break;
					}
				case dlgRespClose:
				default:
					{
						gtk_widget_destroy(bpd.dlg);
						resp = dlgRespClose;
						break;
					}
			}
		}
		while (resp != dlgRespClose);
		gtk_window_present(GTK_WINDOW(gdbui_setup.main_window));
	}
	else
	{
		add_click(NULL, NULL);
	}

}



void
gdbui_break_dlg(gboolean is_watch)
{
	is_watchlist = is_watch;
	gdbio_show_breaks(break_dlg);
}
