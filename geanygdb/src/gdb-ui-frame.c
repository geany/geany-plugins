/*
 * gdb-ui-frame.c - Stack frame dialogs for a GTK-based GDB user interface.
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

#include "gdb-io.h"
#include "gdb-ui.h"
#include "support.h"


static gpointer
qpop(GQueue ** q)
{
	gpointer p = NULL;
	if (*q)
	{
		p = g_queue_pop_head(*q);
		if (g_queue_get_length(*q) == 0)
		{
			g_queue_free(*q);
			*q = NULL;
		}
	}
	return p;
}


static void
qpush(GQueue ** q, gpointer p)
{
	if (p)
	{
		if (!*q)
		{
			*q = g_queue_new();
		}
		g_queue_push_head(*q, p);
	}
}



static gpointer
qtop(GQueue * q)
{
	return q ? g_queue_peek_head(q) : NULL;
}





static void object_func(const GdbVar * obj, const GSList * list);
static void show_frame_click(GtkWidget * btn, gpointer user_data);

static GQueue *obj_name_queue = NULL;
static void
push_name(const gchar * p)
{
	qpush(&obj_name_queue, g_strdup(p));
}
static void
pop_name()
{
	g_free(qpop(&obj_name_queue));
}
static gchar *
top_name()
{
	return qtop(obj_name_queue);
}

static gchar current_frame[32] = { 0 };


typedef enum _StackDlgResp
{
	respMoreInfo = 1,
	respGoBack,
	respDone
} StackDlgResp;


typedef struct _StackWidgets
{
	GtkWidget *dlg;
	GtkWidget *args_label;
	GtkWidget *path_label;
	GtkWidget *code_label;
	GdbFrameInfo *frame;
} StackWidgets;


typedef struct _VarWidgets
{
	GdbVar *v;
	GtkWidget *info_btn;
	GtkWidget *dlg;
} VarWidgets;


typedef enum _FrameColumn
{
	fcLevelStr,
	fcFile,
	fcFunc,
	fcAddrStr,
	fcFrame,
	fcNumCols
} FrameColumn;


//#define MORE_INFO GTK_STOCK_GO_FORWARD

#define MORE_INFO GTK_STOCK_ZOOM_IN

static GtkWidget *
new_info_btn()
{
	GtkWidget *rv;
	rv = gtk_button_new_with_mnemonic(_("_Examine"));
	gtk_button_set_image(GTK_BUTTON(rv),
			     gtk_image_new_from_stock(MORE_INFO, GTK_ICON_SIZE_BUTTON));
#if GTK_CHECK_VERSION(2, 10, 0)
	// gtk_button_set_image_position(GTK_BUTTON(rv), GTK_POS_RIGHT);
#endif
	return rv;
}

GtkWidget *
gdbui_new_dialog(gchar * title)
{
	GtkWidget *dlg = gtk_dialog_new();
	gtk_window_set_transient_for(GTK_WINDOW(dlg), GTK_WINDOW(gdbui_setup.main_window));
	gtk_window_set_position(GTK_WINDOW(dlg), GTK_WIN_POS_CENTER);
	gtk_window_set_title(GTK_WINDOW(dlg), title);
	return dlg;
}



static void
monospace(GtkWidget * label, gchar * line, gchar * text)
{
	gchar *esc = g_markup_escape_text(text, -1);
	gchar *mu;
	mu = line ?
		g_strdup_printf("<span font_desc=\"%s\"><b>%s</b>  %s</span>",
				gdbui_setup.options.mono_font, line,
				esc) : g_strdup_printf("<span font_desc=\"%s\">%s</span>",
						       gdbui_setup.options.mono_font, esc);
	gtk_label_set_markup(GTK_LABEL(label), mu);
	g_free(mu);
	g_free(esc);
}



static void
locals_select_cb(GtkTreeSelection * selection, gpointer data)
{
	GdbVar *v = NULL;
	VarWidgets *vw = data;
	GtkTreeIter iter;
	GtkTreeModel *model;
	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gtk_tree_model_get(model, &iter, 1, &v, -1);
		if (v)
		{
			vw->v = v;
			gtk_widget_set_sensitive(vw->info_btn, !g_str_equal(v->numchild, "0"));
		}
	}
}



static GtkWidget *
make_list(const GSList * list, gchar * title, VarWidgets * vw)
{
	GtkTreeIter iter;
	GtkTreeViewColumn *column;
	GtkWidget *listview;
	GtkWidget *scroll;
	GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
	GtkListStore *store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_POINTER);
	GtkTreeSelection *selection;
	gint width, height;
	gint len = g_slist_length((GSList *) list);
	const GSList *p;
	g_object_set(G_OBJECT(renderer), "font", gdbui_setup.options.mono_font, NULL);
	for (p = list; p; p = p->next)
	{
		GdbVar *v = p->data;
		gchar *txt = g_strdup_printf(" %s %s = %s", v->type, v->name, v->value);
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, 0, txt, 1, v, -1);
	}

	listview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	column = gtk_tree_view_column_new_with_attributes(title, renderer, "text", 0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(listview), column);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(listview));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
	g_signal_connect(G_OBJECT(selection), "changed", G_CALLBACK(locals_select_cb), vw);

	gtk_cell_renderer_get_size(GTK_CELL_RENDERER(renderer), listview,
				   NULL, NULL, NULL, &width, &height);

	scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_usize(scroll, (gdk_screen_get_width(gdk_screen_get_default()) / 2) * 1,
			     height * (len < 8 ? len + 3 : 11));
	gtk_container_add(GTK_CONTAINER(scroll), listview);
	gtk_widget_set_sensitive(listview, list != NULL);
	return scroll;

}



static void
info_click(GtkWidget * btn, gpointer user_data)
{
	VarWidgets *vw = user_data;
	if (vw->v && vw->v->name)
	{
		push_name(vw->v->name);
		gdbio_show_object(object_func, vw->v->name);
	}
	gtk_dialog_response(GTK_DIALOG(vw->dlg), respMoreInfo);
}


#define strval(s) s?s:""


static gchar *
fmt_val(gchar * value)
{
	gchar buf[64];
	if (!value)
		return g_strdup("");
	if (strlen(value) < 64)
	{
		return g_strdup(value);
	}
	strncpy(buf, value, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';
	return g_strdup_printf("%s...%s", buf, strchr(buf, '"') ? "\"" : "");
}

static void
object_func(const GdbVar * obj, const GSList * list)
{
	VarWidgets vw;
	GtkBox *vbox;
	GtkWidget *view;
	GtkWidget *header;
	GtkWidget *btn;
	gchar *heading;
	gchar *value = fmt_val(obj->value);
	gint resp;

	heading = g_strdup_printf("\n%s %s = %s\n", strval(obj->type), strval(obj->name), value);
	g_free(value);
	memset(&vw, 0, sizeof(vw));
	vw.dlg = gdbui_new_dialog(_("Object info"));
	vbox = GTK_BOX(GTK_DIALOG(vw.dlg)->vbox);
	header = gtk_label_new(NULL);
	monospace(header, NULL, heading);
	g_free(heading);
	gtk_box_pack_start(vbox, header, FALSE, FALSE, 0);

	view = make_list(list, strchr(strval(obj->type), '[') ? _("Elements") : _("Fields"), &vw);

	gtk_box_pack_start(vbox, view, TRUE, TRUE, 0);



//  btn=gtk_dialog_add_button(GTK_DIALOG(vw.dlg)," << _Back ",respGoBack);
	btn = gtk_dialog_add_button(GTK_DIALOG(vw.dlg), GTK_STOCK_GO_BACK, respGoBack);
	gdbui_set_tip(btn, _("Return to previous dialog."));
	gtk_dialog_set_default_response(GTK_DIALOG(vw.dlg), respGoBack);


	vw.info_btn = new_info_btn();
	gdbui_set_tip(vw.info_btn, _("Display additional information about the selected item."));
	g_signal_connect(G_OBJECT(vw.info_btn), "clicked", G_CALLBACK(info_click), &vw);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(vw.dlg)->action_area), vw.info_btn, FALSE, FALSE, 0);

	gtk_dialog_add_button(GTK_DIALOG(vw.dlg), GTK_STOCK_CLOSE, respDone);

	gtk_widget_show_all(vw.dlg);
	resp = gtk_dialog_run(GTK_DIALOG(vw.dlg));
	gtk_widget_destroy(vw.dlg);

	switch (resp)
	{
		case respGoBack:
			{
				pop_name();
				if (top_name())
				{
					gdbio_show_object(object_func, top_name());
				}
				else
				{
					show_frame_click(NULL, NULL);
				}
				break;
			}
		case respMoreInfo:
			break;
		case respDone:
			break;
	}
}




static void
locals_func(const GdbFrameInfo * frame, const GSList * locals)
{
	GtkWidget *view;
	GtkBox *vbox;
	VarWidgets vw;
	gint resp;
	gchar *heading;
	GtkWidget *header;
	GtkWidget *btn;

	memset(&vw, 0, sizeof(vw));
	vw.dlg = gdbui_new_dialog(_("Frame info"));
	vbox = GTK_BOX(GTK_DIALOG(vw.dlg)->vbox);

	heading = g_strdup_printf(_("\nFrame #%s in %s() at %s:%s\n"),
				  strval(frame->level), strval(frame->func),
				  basename(strval(frame->filename)), strval(frame->line));
	header = gtk_label_new(NULL);
	monospace(header, NULL, heading);
	g_free(heading);
	gtk_box_pack_start(vbox, header, FALSE, FALSE, 0);


	gtk_box_pack_start(vbox, gtk_hseparator_new(), FALSE, FALSE, 0);
	view = make_list(frame->args, _("Function arguments"), &vw);
	gtk_box_pack_start(vbox, view, TRUE, TRUE, 0);

	gtk_box_pack_start(vbox, gtk_hseparator_new(), FALSE, FALSE, 0);
	view = make_list(locals, _("Local variables"), &vw);
	gtk_box_pack_start(vbox, view, TRUE, TRUE, 0);

//  btn=gtk_dialog_add_button(GTK_DIALOG(vw.dlg)," << _Back ",respGoBack);
	btn = gtk_dialog_add_button(GTK_DIALOG(vw.dlg), GTK_STOCK_GO_BACK, respGoBack);
	gdbui_set_tip(btn, _("Return to stack list dialog."));

	gtk_dialog_set_default_response(GTK_DIALOG(vw.dlg), respGoBack);

	vw.info_btn = new_info_btn();
	gdbui_set_tip(vw.info_btn, _("Display additional information about the selected item."));
	g_signal_connect(G_OBJECT(vw.info_btn), "clicked", G_CALLBACK(info_click), &vw);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(vw.dlg)->action_area), vw.info_btn, FALSE, FALSE, 0);

	gtk_dialog_add_button(GTK_DIALOG(vw.dlg), GTK_STOCK_CLOSE, respDone);

	gtk_widget_show_all(vw.dlg);
	if (locals && !frame->args)
	{
		gtk_widget_grab_focus(GTK_BIN(view)->child);
	}
	resp = gtk_dialog_run(GTK_DIALOG(vw.dlg));
	gtk_widget_destroy(vw.dlg);
	if (resp == respGoBack)
		gdbio_show_stack(gdbui_stack_dlg);
}



static void
show_frame_click(GtkWidget * btn, gpointer user_data)
{
	StackWidgets *sw = user_data;
	if (sw)
	{
		gtk_dialog_response(GTK_DIALOG(sw->dlg), respDone);
	}
	gdbio_show_locals(locals_func, current_frame);
}



static void
stack_select_cb(GtkTreeSelection * selection, gpointer data)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	GdbFrameInfo *frame = NULL;
	gchar *buf = NULL;
	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		StackWidgets *sw = data;
		gtk_tree_model_get(model, &iter, fcFrame, &frame, -1);
		if (frame)
		{
			gchar *path;
			sw->frame = frame;
			strncpy(current_frame, frame->level, sizeof(current_frame) - 1);
			path = g_strdup_printf("%s:%s", frame->filename, frame->line);
			monospace(sw->path_label, NULL, path);
			g_free(path);

			if (frame->args)
			{
				gchar **arglist = g_new0(gchar *, g_slist_length(frame->args) + 3);
				gint i = 1;
				GSList *p;
				gchar *all;
				arglist[0] = g_strdup_printf("%s (", frame->func);
				for (p = frame->args; p; p = p->next)
				{
					GdbVar *arg = p->data;
					if (arg)
					{
						arglist[i] =
							g_strdup_printf(" %s=%1.64s%s%s", arg->name,
									arg->value,
									strlen(arg->value) >
									64 ? "...\"" : "",
									p->next ? "," : "");
						i++;
					}
				}
				arglist[i] = g_strdup(")");
				all = g_strjoinv("\n", arglist);
				monospace(sw->args_label, NULL, all);
				g_free(all);
				g_strfreev(arglist);
			}
			else
			{
				gchar *tmp = g_strdup_printf("%s ()", frame->func);
				monospace(sw->args_label, NULL, tmp);
				g_free(tmp);
			}

			if (g_file_test(frame->filename, G_FILE_TEST_IS_REGULAR))
			{
				FILE *fh = fopen(frame->filename, "r");
				if (fh)
				{
					gsize len = 0;
					gint i;
					gint line = gdbio_atoi(frame->line);
					for (i = 1; i <= line; i++)
					{
						gint r = getline(&buf, &len, fh);
						if (r < 0)
						{
							free(buf);
							buf = NULL;
							break;
						}
					}
					fclose(fh);
				}
			}
			g_strstrip(buf);
			monospace(sw->code_label, frame->line, buf);
			if (buf)
			{
				g_free(buf);
			}
		}
	}
}



void
gdbui_stack_dlg(const GSList * frame_list)
{
	const GSList *p;
	GtkTreeIter iter;
	GtkListStore *store;
	GtkWidget *listview;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
	GtkTreeSelection *selection;
	GtkWidget *scroll;
	GtkWidget *locals_btn;
	gint most_args = 0;
	StackWidgets sw;
	memset(&sw, 0, sizeof(sw));
	store = gtk_list_store_new(fcNumCols, G_TYPE_STRING,	/* fcLevelStr */
				   G_TYPE_STRING,	/* fcFile */
				   G_TYPE_STRING,	/* fcFunc */
				   G_TYPE_STRING,	/* fcAddrStr */
				   G_TYPE_POINTER	/* fcFrame */
		);

	for (p = frame_list; p; p = p->next)
	{
		GdbFrameInfo *f = p->data;
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
				   fcLevelStr, f->level,
				   fcFile, basename(f->filename),
				   fcFunc, f->func, fcAddrStr, f->addr, fcFrame, f, -1);
		if (f->args)
		{
			gint argc = g_slist_length(f->args);
			if (argc > most_args)
			{
				most_args = argc;
			}
		}
	}
	listview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));

	g_object_set(G_OBJECT(renderer), "font", gdbui_setup.options.mono_font, NULL);
	column = gtk_tree_view_column_new_with_attributes("#", renderer, "text", fcLevelStr, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(listview), column);

	column = gtk_tree_view_column_new_with_attributes("filename", renderer, "text", fcFile,
							  NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(listview), column);

	column = gtk_tree_view_column_new_with_attributes("function", renderer, "text", fcFunc,
							  NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(listview), column);

	column = gtk_tree_view_column_new_with_attributes("address", renderer, "text", fcAddrStr,
							  NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(listview), column);

	sw.path_label = gtk_label_new(NULL);

	sw.args_label = gtk_label_new(NULL);
	if (most_args)
	{
		gchar *argtxt;
		gint i;
		most_args += 1;
		argtxt = g_malloc0(most_args * 4);
		for (i = 0; i < most_args; i++)
		{
			strcat(argtxt, ".\n");
		}
		monospace(sw.args_label, NULL, argtxt);
		g_free(argtxt);
	}
	else
	{
		monospace(sw.args_label, NULL, " \n \n");
	}

	sw.code_label = gtk_label_new(NULL);

	gtk_misc_set_alignment(GTK_MISC(sw.path_label), 0.0f, 0.0f);
	gtk_misc_set_alignment(GTK_MISC(sw.args_label), 0.0f, 0.0f);
	gtk_misc_set_alignment(GTK_MISC(sw.code_label), 0.0f, 0.0f);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(listview));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
	g_signal_connect(G_OBJECT(selection), "changed", G_CALLBACK(stack_select_cb), &sw);

	sw.dlg = gdbui_new_dialog(_("Stack trace"));

	scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_usize(scroll, (gdk_screen_get_width(gdk_screen_get_default()) / 3) * 2,
			     (gdk_screen_get_height(gdk_screen_get_default()) / 3) * 1);
	gtk_container_add(GTK_CONTAINER(scroll), listview);

	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(sw.dlg)->vbox), scroll, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(sw.dlg)->vbox), sw.path_label, FALSE, FALSE, 4);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(sw.dlg)->vbox), gtk_hseparator_new(), FALSE, FALSE,
			   0);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(sw.dlg)->vbox), sw.args_label, TRUE, TRUE, 4);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(sw.dlg)->vbox), gtk_hseparator_new(), FALSE, FALSE,
			   0);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(sw.dlg)->vbox), sw.code_label, FALSE, FALSE, 4);

	locals_btn = new_info_btn();
	gdbui_set_tip(locals_btn, _("Display additional information about the selected frame."));
	g_signal_connect(G_OBJECT(locals_btn), "clicked", G_CALLBACK(show_frame_click), &sw);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(sw.dlg)->action_area), locals_btn, FALSE, FALSE, 0);

	gtk_dialog_add_button(GTK_DIALOG(sw.dlg), GTK_STOCK_CLOSE, GTK_RESPONSE_OK);
	gtk_widget_realize(sw.args_label);
	gtk_widget_show_all(sw.dlg);
	gtk_dialog_run(GTK_DIALOG(sw.dlg));
	gtk_widget_destroy(sw.dlg);
	gtk_window_present(GTK_WINDOW(gdbui_setup.main_window));
}
