/*
 * gsdlg.c - Simple GTK dialog wrapper
 *
 * Copyright 2007-2008 Jeff Pohlmeyer <yetanothergeek(at)gmail(dot)com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef GSDLG_ALL_IN_ONE
#include "gsdlg.h"
#endif

#include <glib/gi18n.h>
#include <stdlib.h>

#ifdef G_OS_WIN32
#define realpath(src,dst) _fullpath((dst),(src),_MAX_PATH)
#endif


#define TextKey "gsdlg_TextKey_bc4871f4e3478ab5234e28432460a6b8"
#define DataKey "gsdlg_DataKey_bc4871f4e3478ab5234e28432460a6b8"


static void destroy_slist_and_data(gpointer list)
{
	GSList*p;
	for (p=list; p ; p=p->next) {
		if (p->data) g_free(p->data);
		p->data=NULL;
	}
	g_slist_free(list);
}



void gsdlg_textarea(GtkDialog *dlg, GsDlgStr key, GsDlgStr value, GsDlgStr label)
{
	GtkWidget*tv;
	GtkWidget*sw;
	GtkWidget*frm;
	g_return_if_fail(dlg);
	tv=gtk_text_view_new();
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(tv),GTK_WRAP_WORD_CHAR);
	gtk_text_view_set_accepts_tab(GTK_TEXT_VIEW(tv),FALSE);
	if (value) {
		GtkTextBuffer *tb = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv));
		gtk_text_buffer_set_text (tb, value, -1);
	}
	sw=gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_usize(sw, gdk_screen_get_width(gdk_screen_get_default())/3,
												   gdk_screen_get_height(gdk_screen_get_default())/10);
	gtk_container_add(GTK_CONTAINER(sw),tv);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	frm=gtk_frame_new(label);
	gtk_frame_set_shadow_type(GTK_FRAME(frm), GTK_SHADOW_ETCHED_IN);
	gtk_container_add(GTK_CONTAINER(frm),sw);
	gtk_container_add(GTK_CONTAINER(dlg->vbox),frm);
	g_object_set_data_full(G_OBJECT(tv), TextKey, g_strdup(key), g_free);
}



static void make_modal(GtkWidget *w, GtkWidget*p)
{
	gtk_window_set_modal(GTK_WINDOW(w), TRUE);
	gtk_window_set_transient_for(GTK_WINDOW(w),GTK_WINDOW(p));
}



static void  file_dlg_map(GtkWidget *w, gpointer user_data)
{
	GtkWidget* entry=gtk_window_get_focus (GTK_WINDOW(w));
	if (entry && GTK_IS_ENTRY(entry)) {
		gtk_entry_set_text(GTK_ENTRY(entry), (gchar*)user_data);
	}
}



static void file_btn_clicked(GtkButton *button, gpointer user_data)
{
	GtkWidget*dlg;
	const gchar *fn=NULL;
	gchar *bn=NULL;
	gint resp;
	dlg=gtk_file_chooser_dialog_new(_("Open file"),
				gsdlg_toplevel,	GTK_FILE_CHOOSER_ACTION_OPEN,
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,	NULL);
	gtk_window_set_title(GTK_WINDOW(dlg), _("Select file"));
	make_modal(dlg, gtk_widget_get_toplevel(GTK_WIDGET(user_data)));
	fn=gtk_entry_get_text(GTK_ENTRY(user_data));
	if (fn && *fn) {
		if (g_file_test(fn,G_FILE_TEST_IS_REGULAR)) {
			gchar *rp=realpath(fn,NULL);
			gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dlg), rp);
			if (rp) free(rp);
		} else {
			if (g_file_test(fn,G_FILE_TEST_IS_DIR)) {
				gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), fn);
			} else {
				gchar *dn=g_path_get_dirname(fn);
				if (g_file_test(dn,G_FILE_TEST_IS_DIR)) {
					gchar *rp=realpath(dn,NULL);
					gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), rp);
					if (rp) free(rp);
					bn=g_path_get_basename(fn);
					g_signal_connect(G_OBJECT(dlg), "map", G_CALLBACK(file_dlg_map), bn);
				}
				g_free(dn);
			}
		}
	}
	resp=gtk_dialog_run(GTK_DIALOG(dlg));
	if (resp == GTK_RESPONSE_ACCEPT) {
		gchar *fcfn=gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
		if (fcfn) {
			gtk_entry_set_text(GTK_ENTRY(user_data), fcfn);
			g_free(fcfn);
		}
	}
	gtk_widget_destroy(dlg);
	if (bn) { g_free(bn); }
}



void gsdlg_file(GtkDialog *dlg, GsDlgStr key, GsDlgStr value, GsDlgStr label)
{
	GtkWidget *hbox;
	GtkWidget *input;
	GtkWidget*frm;
	GtkWidget*btn;
	g_return_if_fail(dlg);
	input=gtk_entry_new();
	if (value) { gtk_entry_set_text(GTK_ENTRY(input),value); }
	btn=gtk_button_new_with_label(_("Browse..."));
	g_signal_connect(G_OBJECT(btn), "clicked", G_CALLBACK(file_btn_clicked), input);
	hbox=gtk_hbox_new(FALSE,FALSE);
	gtk_box_pack_start(GTK_BOX(hbox), input,TRUE,TRUE,1);
	gtk_box_pack_start(GTK_BOX(hbox), btn,FALSE,FALSE,1);
	frm=gtk_frame_new(label);
	gtk_frame_set_shadow_type(GTK_FRAME(frm), GTK_SHADOW_ETCHED_IN);
	gtk_container_add(GTK_CONTAINER(frm),hbox);
	gtk_container_add(GTK_CONTAINER(dlg->vbox),frm);
	g_object_set_data_full(G_OBJECT(input), TextKey, g_strdup(key), g_free);
}



static void color_btn_clicked(GtkButton *button, gpointer user_data)
{
	GtkWidget*dlg;
	const gchar *cn=NULL;
	gint resp;
	GdkColor rgb;

	dlg=gtk_color_selection_dialog_new (_("Select Color"));
	make_modal(dlg, gtk_widget_get_toplevel(GTK_WIDGET(user_data)));
	cn=gtk_entry_get_text(GTK_ENTRY(user_data));
	if (cn && *cn && gdk_color_parse(cn,&rgb)) {
		gtk_color_selection_set_current_color(
			GTK_COLOR_SELECTION(GTK_COLOR_SELECTION_DIALOG(dlg)->colorsel), &rgb);
	}
	resp=gtk_dialog_run(GTK_DIALOG(dlg));
	if (resp == GTK_RESPONSE_OK) {
		gchar *rv=NULL;
		gtk_color_selection_get_current_color(
			GTK_COLOR_SELECTION(GTK_COLOR_SELECTION_DIALOG(dlg)->colorsel), &rgb);
		rv=g_strdup_printf("#%2.2X%2.2X%2.2X",rgb.red/256,rgb.green/256,rgb.blue/256);
		gtk_entry_set_text(GTK_ENTRY(user_data), rv);
		g_free(rv);
	}
	gtk_widget_destroy(dlg);
}



void gsdlg_color(GtkDialog *dlg, GsDlgStr key, GsDlgStr value, GsDlgStr prompt)
{
	GtkWidget *hbox;
	GtkWidget *input;
	GtkWidget *label;
	GtkWidget*btn;
	g_return_if_fail(dlg);
	input=gtk_entry_new();
	if (value) { gtk_entry_set_text(GTK_ENTRY(input),value); }
	btn=gtk_button_new_with_label(_("Choose..."));
	g_signal_connect(G_OBJECT(btn), "clicked", G_CALLBACK(color_btn_clicked), input);
	hbox=gtk_hbox_new(FALSE,FALSE);
	if (prompt) {
		label=gtk_label_new(prompt);
		gtk_box_pack_start(GTK_BOX(hbox), label,FALSE,FALSE,1);
	}
	gtk_box_pack_start(GTK_BOX(hbox), input,TRUE,TRUE,1);
	gtk_box_pack_start(GTK_BOX(hbox), btn,FALSE,FALSE,1);
	gtk_container_add(GTK_CONTAINER(dlg->vbox),hbox);
	g_object_set_data_full(G_OBJECT(input), TextKey, g_strdup(key), g_free);
}



static void font_btn_clicked(GtkButton *button, gpointer user_data)
{
	GtkWidget*dlg;
	const gchar *fn=NULL;
	gint resp;
	fn=gtk_entry_get_text(GTK_ENTRY(user_data));
	dlg=gtk_font_selection_dialog_new(_("Select Font"));
	make_modal(dlg, gtk_widget_get_toplevel(GTK_WIDGET(user_data)));
	if (fn && *fn) {
		gtk_font_selection_dialog_set_font_name(GTK_FONT_SELECTION_DIALOG(dlg),fn);
	}
	resp=gtk_dialog_run(GTK_DIALOG(dlg));
	if (resp==GTK_RESPONSE_OK) {
		gchar *gfn=gtk_font_selection_dialog_get_font_name(GTK_FONT_SELECTION_DIALOG(dlg));
		if (gfn) {
			gtk_entry_set_text(GTK_ENTRY(user_data), gfn);
			g_free(gfn);
		}
	}
	gtk_widget_destroy(dlg);
}



void gsdlg_font(GtkDialog *dlg, GsDlgStr key, GsDlgStr value, GsDlgStr prompt)
{
	GtkWidget *hbox;
	GtkWidget *input;
	GtkWidget *label;
	GtkWidget*btn;
	g_return_if_fail(dlg);
	input=gtk_entry_new();
	if (value) { gtk_entry_set_text(GTK_ENTRY(input),value); }
	btn=gtk_button_new_with_label(_("Select..."));
	g_signal_connect(G_OBJECT(btn), "clicked", G_CALLBACK(font_btn_clicked), input);
	hbox=gtk_hbox_new(FALSE,FALSE);
	if (prompt) {
		label=gtk_label_new(prompt);
		gtk_box_pack_start(GTK_BOX(hbox), label,FALSE,FALSE,1);
	}
	gtk_box_pack_start(GTK_BOX(hbox), input,TRUE,TRUE,1);
	gtk_box_pack_start(GTK_BOX(hbox), btn,FALSE,FALSE,1);
	gtk_container_add(GTK_CONTAINER(dlg->vbox),hbox);
	g_object_set_data_full(G_OBJECT(input), TextKey, g_strdup(key), g_free);
}



static void gsdlg_entry(GtkDialog *dlg, GsDlgStr key, GsDlgStr value, GsDlgStr prompt, gboolean masked)
{
	GtkWidget *hbox;
	GtkWidget *input;
	GtkWidget *label;
	g_return_if_fail(dlg);
	input=gtk_entry_new();
	if (value) { gtk_entry_set_text(GTK_ENTRY(input),value); }
	label=gtk_label_new(prompt);
	hbox=gtk_hbox_new(FALSE,FALSE);
	gtk_box_pack_start(GTK_BOX(hbox), label,FALSE,FALSE,1);
	gtk_box_pack_start(GTK_BOX(hbox), input,TRUE,TRUE,1);
	gtk_entry_set_visibility(GTK_ENTRY(input), !masked);
	gtk_container_add(GTK_CONTAINER(dlg->vbox),hbox);
	g_object_set_data_full(G_OBJECT(input), TextKey, g_strdup(key), g_free);
}


void gsdlg_text(GtkDialog *dlg, GsDlgStr key, GsDlgStr value, GsDlgStr label)
{
	gsdlg_entry(dlg, key, value, label, FALSE);
}



void gsdlg_password(GtkDialog *dlg, GsDlgStr key, GsDlgStr value, GsDlgStr label)
{
	gsdlg_entry(dlg, key, value, label, TRUE);
}


void gsdlg_checkbox(GtkDialog *dlg, GsDlgStr key, gboolean value, GsDlgStr label)
{
	GtkWidget *cb=NULL;
	g_return_if_fail(dlg);
	cb=gtk_check_button_new_with_label(label);
	g_object_set_data_full(G_OBJECT(cb),TextKey,g_strdup(key), g_free);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb),value);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dlg)->vbox),cb);
}



typedef struct _KeySearchData {
	const gchar *key;
	GType type;
	GtkWidget*value;
} KeySearchData;



static void find_widget_by_key_cb(GtkWidget *w, gpointer p)
{
	KeySearchData*kv=p;
	if (kv->value) {return;}

	if (G_OBJECT_TYPE(G_OBJECT(w)) == kv->type) {
		gchar*key=g_object_get_data(G_OBJECT(w), TextKey);
		if (key && g_str_equal(kv->key,key)) { kv->value=w;	}
	}
}


static GtkWidget *find_widget_by_key(GtkDialog *dlg, GType type, GsDlgStr key)
{
	KeySearchData kv={NULL,0,NULL};
	g_return_val_if_fail(dlg, NULL);
	kv.key=key;
	kv.type=type;
	gtk_container_foreach(GTK_CONTAINER(GTK_DIALOG(dlg)->vbox),find_widget_by_key_cb,&kv);
	return kv.value;
}


static void select_radio(GtkWidget*parent, GsDlgStr value)
{
	GList *kids=gtk_container_get_children(GTK_CONTAINER(parent));
	if (!kids) { return;}
	if (kids->data && GTK_IS_RADIO_BUTTON(kids->data)) {
		GList *kid=kids;
		for (kid=kids; kid; kid=kid->next) {
			if (kid->data && GTK_IS_RADIO_BUTTON(kid->data)) {
				gchar*kval=g_object_get_data(G_OBJECT(kid->data),TextKey);
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(kid->data),
																				kval && g_str_equal(kval,value));
			}
		}
	}
}



void gsdlg_group(GtkDialog *dlg, GsDlgStr key, GsDlgStr value, GsDlgStr label)
{
	GtkWidget*frm;
	GtkWidget*vbox;

	g_return_if_fail(dlg);
	frm=find_widget_by_key(dlg,GTK_TYPE_FRAME,key);
	if (frm) {
		vbox=gtk_bin_get_child(GTK_BIN(frm));
		gtk_frame_set_label(GTK_FRAME(frm), label);
	} else {
		frm=gtk_frame_new(label);
		vbox=gtk_vbox_new(FALSE,FALSE);
		gtk_container_add(GTK_CONTAINER(frm),vbox);
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dlg)->vbox),frm);
	} 

	/* Frame holds keyname, vbox holds default value */
	g_object_set_data_full(G_OBJECT(frm),  TextKey, g_strdup(key), g_free);
	g_object_set_data_full(G_OBJECT(vbox), TextKey, g_strdup(value), g_free);
	select_radio(vbox,value);
}




void gsdlg_radio(GtkDialog *dlg, GsDlgStr key, GsDlgStr value, GsDlgStr label)
{
	GtkWidget *frm=NULL;
	GtkWidget *vbox=NULL;
	GList *kids=NULL;
	GtkWidget *rb=NULL;
	gchar *defval;
	g_return_if_fail(dlg);
	frm=find_widget_by_key(dlg,GTK_TYPE_FRAME,key);
	if (frm) {
		vbox=gtk_bin_get_child(GTK_BIN(frm));
		if (vbox) {
		kids=gtk_container_get_children(GTK_CONTAINER(vbox));
		}
	} else {
		gsdlg_group(dlg,key,value,NULL);
		frm=find_widget_by_key(dlg,GTK_TYPE_FRAME,key);
		vbox=gtk_bin_get_child(GTK_BIN(frm));
	}
	if (kids) {
		rb=gtk_radio_button_new_with_label_from_widget (kids->data,label);
		g_list_free(kids);
	} else {
		rb=gtk_radio_button_new_with_label(NULL, label);
	}
	g_object_set_data_full(G_OBJECT(rb),TextKey, g_strdup(value), g_free);
	gtk_container_add(GTK_CONTAINER(vbox), rb);
	defval=g_object_get_data(G_OBJECT(vbox),TextKey);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rb),
																value && defval && g_str_equal(defval,value));
}




static void select_combo(GtkWidget*parent, GsDlgStr value)
{
	gint n=0;
	GSList*values=g_object_get_data(G_OBJECT(parent), DataKey);
	GSList*v;
	for (v=values; v; v=v->next) {
		if (v->data && g_str_equal(v->data, value)) {break;}
		n++;
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(parent), n);
}


typedef struct _ComboWidgets {
	GtkWidget *label;
	GtkWidget *combo;
} ComboWidgets;


void gsdlg_select(GtkDialog *dlg, GsDlgStr key, GsDlgStr value, GsDlgStr label)
{
	GtkWidget *hbox=NULL;
	ComboWidgets*cw=NULL;
	g_return_if_fail(dlg);
	hbox=find_widget_by_key(dlg,GTK_TYPE_HBOX, key);
	if (hbox) {
		cw=g_object_get_data(G_OBJECT(hbox), DataKey);
		gtk_label_set(GTK_LABEL(cw->label), label);
	} else {
		hbox=gtk_hbox_new(FALSE,FALSE);
		cw=g_malloc0(sizeof(ComboWidgets));
		g_object_set_data_full(G_OBJECT(hbox),DataKey,cw,g_free);
		cw->combo=gtk_combo_box_new_text();
		cw->label=gtk_label_new(label);
		gtk_box_pack_start(GTK_BOX(hbox), cw->label,FALSE,FALSE,4);
		gtk_box_pack_start(GTK_BOX(hbox), cw->combo,TRUE,TRUE,1);
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dlg)->vbox),hbox);
	}
	/* Hbox holds keyname, combo holds default value */
	g_object_set_data_full(G_OBJECT(hbox),  TextKey, g_strdup(key), g_free);
	g_object_set_data_full(G_OBJECT(cw->combo), TextKey, g_strdup(value), g_free);
	select_combo(cw->combo,value);
}



void gsdlg_option(GtkDialog *dlg, GsDlgStr key, GsDlgStr value, GsDlgStr label)
{
	GtkWidget *hbox=NULL;
	GSList *values=NULL;
	ComboWidgets*cw=NULL;
	gchar*defval=NULL;
	g_return_if_fail(dlg);

	hbox=find_widget_by_key(dlg,GTK_TYPE_HBOX, key);
	if (!hbox) {
		gsdlg_select(dlg,key,value,NULL);
		hbox=find_widget_by_key(dlg,GTK_TYPE_HBOX, key);
	}
	cw=g_object_get_data(G_OBJECT(hbox), DataKey);
	values=g_object_steal_data(G_OBJECT(cw->combo), DataKey);
	values=g_slist_append(values, g_strdup(value));
	g_object_set_data_full(G_OBJECT(cw->combo), DataKey, values, destroy_slist_and_data);
	gtk_combo_box_append_text(GTK_COMBO_BOX(cw->combo), label);
	defval=g_object_get_data(G_OBJECT(cw->combo), TextKey);
	if (value && defval && g_str_equal(value,defval)) {
		select_combo(cw->combo,value);
	}
}




void gsdlg_label(GtkDialog *dlg, GsDlgStr text)
{
	GtkWidget *lab=NULL;
	g_return_if_fail(dlg);
	lab=gtk_label_new(text);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dlg)->vbox), lab);
	gtk_misc_set_alignment(GTK_MISC(lab), 0.0f, 0.0f);
}



void gsdlg_hr(GtkDialog *dlg)
{
	g_return_if_fail(dlg);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dlg)->vbox), gtk_hseparator_new()); 
}



void gsdlg_heading(GtkDialog *dlg, GsDlgStr text)
{
	g_return_if_fail(dlg);
	gsdlg_hr(dlg);
	gsdlg_label(dlg,text);
}

GSDLG_API GtkWindow* gsdlg_toplevel=NULL;



GtkDialog *gsdlg_new(GsDlgStr title, GsDlgStr*btns)
{
	gint i;
	GtkDialog *dlg;
	dlg=GTK_DIALOG(gtk_dialog_new());
	if (gsdlg_toplevel) {
		gtk_window_set_destroy_with_parent(GTK_WINDOW(dlg), TRUE);
		gtk_window_set_transient_for(GTK_WINDOW(dlg),gsdlg_toplevel);
		gtk_window_set_modal(GTK_WINDOW(dlg), TRUE);
	}
	for (i=0; btns[i]; i++) {
		gtk_dialog_add_button(GTK_DIALOG(dlg),btns[i],i);
	}
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dlg)->vbox), 4);
	gtk_container_set_border_width(GTK_CONTAINER(dlg), 4);
	gtk_window_set_title(GTK_WINDOW(dlg),title);
	return dlg;
}



static void widgets_foreach(GtkWidget *w, gpointer p)
{
	gchar*key=g_object_get_data(G_OBJECT(w),TextKey);
	if (key && *key) {
		const gchar*value=NULL;
		if (GTK_IS_ENTRY(w)) {
			value=gtk_entry_get_text(GTK_ENTRY(w));
		} else if (GTK_IS_RADIO_BUTTON(w)) {
			if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))){
				value=key;
				key=g_object_get_data(
						G_OBJECT(gtk_widget_get_parent(gtk_widget_get_parent(w))), TextKey);
			}
		} else if (GTK_IS_CHECK_BUTTON(w)) {
			value=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))?"1":"0";
		} else if (GTK_IS_COMBO_BOX(w)) {
			GSList*values=g_object_get_data(G_OBJECT(w),DataKey);
			key=g_object_get_data(G_OBJECT(gtk_widget_get_parent(w)), TextKey);
			if (values) {
				gint n=gtk_combo_box_get_active(GTK_COMBO_BOX(w));
				if (n>=0) { value=g_slist_nth_data(values, n); }
			}
		} else if (GTK_IS_TEXT_VIEW(w)) {
			GtkTextBuffer *tb = gtk_text_view_get_buffer(GTK_TEXT_VIEW(w));
			GtkTextIter a,z;
			gtk_text_buffer_get_start_iter(tb,&a);
			gtk_text_buffer_get_end_iter(tb,&z);
			value=gtk_text_buffer_get_text (tb,&a,&z,TRUE);
		}
		if (value&&*value) {
			GHashTable*h=p;
			g_hash_table_insert(h,g_strdup(key),g_strdup(value));
		}
	}
	if (GTK_IS_CONTAINER(w)) {
		gtk_container_foreach(GTK_CONTAINER(w),widgets_foreach,p);
	}
}


static GsDlgRunHook gsdlg_run_hook=NULL;


#ifndef DIALOG_LIB 
void gsdlg_set_run_hook(GsDlgRunHook cb)
{
  gsdlg_run_hook=cb;
}
#endif


GHashTable* gsdlg_run(GtkDialog *dlg, gint *btn, gpointer user_data)
{
	GHashTable* results=NULL;
	gint dummy;
	g_return_val_if_fail(dlg, NULL);
	gtk_widget_show_all(GTK_WIDGET(dlg));
	if (!btn) { btn=&dummy; }
	if (gsdlg_run_hook) { gsdlg_run_hook(TRUE,user_data); }
	*btn=gtk_dialog_run(GTK_DIALOG(dlg));
	if (gsdlg_run_hook) { gsdlg_run_hook(FALSE,user_data); }
	if (*btn<0) *btn=-1;
	results=g_hash_table_new_full(g_str_hash,g_str_equal,g_free,g_free);
	gtk_container_foreach(GTK_CONTAINER(GTK_DIALOG(dlg)->vbox),widgets_foreach,results);
	gtk_widget_hide(GTK_WIDGET(dlg));
	return results;
}



