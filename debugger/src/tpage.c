/*
 *		tpage.c
 *      
 *      Copyright 2010 Alexander Petukhov <devel(at)apetukhov.ru>
 *      
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *      
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *      
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

/*
 *		Contains function to manipulate target page in debug notebook.
 */

#include <string.h>

#include <gtk/gtk.h>

#include <sys/stat.h>

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif
#include <geanyplugin.h>
#include <gp_gtkcompat.h>

extern GeanyData		*geany_data;

#include "breakpoints.h"
#include "utils.h"
#include "watch_model.h"
#include "wtree.h"
#include "debug.h"
#include "dconfig.h"
#include "tpage.h"
#include "tabs.h"
#include "envtree.h"
#include "gui.h"

/* boxes margins */
#define SPACING 7

/* root boxes border width */
#define ROOT_BORDER_WIDTH 10

/* root boxes border width */
#define BROWSE_BUTTON_WIDTH 65

/* widgets */

/* target */
static GtkWidget *target_label = NULL;
static GtkWidget *target_name = NULL;
static GtkWidget *target_button_browse = NULL;

/* debugger type */
static GtkWidget *debugger_label =	NULL;
static GtkWidget *debugger_cmb =	NULL;

/* argments */
static GtkWidget *args_frame = NULL;
static GtkWidget *args_textview = NULL;

/* environment variables */
static GtkWidget *env_frame = NULL;

/* widgets array for reference management when moving to another container */
static GtkWidget **widgets[] = {
	&target_label, &target_name, &target_button_browse,
	&debugger_label, &debugger_cmb,
	&args_frame,
	&env_frame,
	NULL
};

/*
 * tells config to update when target arguments change 
 */
static void on_arguments_changed(GtkTextBuffer *textbuffer, gpointer user_data)
{
	config_set_debug_changed();
}

/*
 * target browse button clicked handler
 */
static void on_target_browse_clicked(GtkButton *button, gpointer   user_data)
{
	gchar *path;
	const gchar *prevfile;
	GtkWidget *dialog;
	GeanyDocument *doc;

	dialog = gtk_file_chooser_dialog_new (_("Choose target file"),
					  NULL,
					  GTK_FILE_CHOOSER_ACTION_OPEN,
					  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					  GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
					  NULL);
	
	prevfile = gtk_entry_get_text(GTK_ENTRY(target_name));
	path = g_path_get_dirname(prevfile);
	if (strcmp(".", path) == 0 && (doc = document_get_current()) != NULL)
	{
		g_free(path);
		path = g_path_get_dirname(DOC_FILENAME(doc));
	}
	
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER (dialog), path);
	g_free(path);
	
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
	{
		gchar *filename;
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
		gtk_entry_set_text(GTK_ENTRY(target_name), filename);
		g_free (filename);
		
		config_set_debug_changed();
	}
	gtk_widget_destroy (dialog);
}

/*
 * packs widgets into page depending one tabbed mode state 
 */
void tpage_pack_widgets(gboolean tabbed)
{
	/* root box */
	GtkWidget *root = NULL, *oldroot = NULL;
	GList *children = gtk_container_get_children(GTK_CONTAINER(tab_target));
	if (children)
	{
		int i;

		oldroot = (GtkWidget*)children->data;
		
		/* unparent widgets */
		i = 0;
		while (widgets[i])
		{
			g_object_ref(*widgets[i]);
			gtk_container_remove(GTK_CONTAINER(gtk_widget_get_parent(*widgets[i])), *widgets[i]);
			i++;
		}
		
		g_list_free(children);
	}
	
	if (tabbed)
	{
		GtkWidget *hbox, *rbox, *lbox;

#if GTK_CHECK_VERSION(3, 0, 0)
		root = gtk_box_new(GTK_ORIENTATION_VERTICAL, SPACING);
#else
		root = gtk_vbox_new(FALSE, SPACING);
#endif
		gtk_container_set_border_width(GTK_CONTAINER(root), ROOT_BORDER_WIDTH);
	
		/* filename */
#if GTK_CHECK_VERSION(3, 0, 0)
		hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, SPACING);
#else
		hbox = gtk_hbox_new(FALSE, SPACING);
#endif
		gtk_box_pack_start(GTK_BOX(root), hbox, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(hbox), target_label, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(hbox), target_name, TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(hbox), target_button_browse, FALSE, FALSE, 0);
		
		/* lower hbox */
#if GTK_CHECK_VERSION(3, 0, 0)
		hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, SPACING);
		gtk_box_set_homogeneous(GTK_BOX(hbox), TRUE);
#else
		hbox = gtk_hbox_new(TRUE, SPACING);
#endif
		gtk_box_pack_start(GTK_BOX(root), hbox, TRUE, TRUE, 0);

		/* lower left and right vboxes */
#if GTK_CHECK_VERSION(3, 0, 0)
		lbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, SPACING);
		rbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, SPACING);
#else
		lbox = gtk_vbox_new(FALSE, SPACING);
		rbox = gtk_vbox_new(FALSE, SPACING);
#endif
		gtk_box_pack_start(GTK_BOX(hbox), lbox, TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(hbox), rbox, TRUE, TRUE, 0);

		/* environment */
		gtk_box_pack_start(GTK_BOX(lbox), env_frame, TRUE, TRUE, 0);

		/* arguments */
		gtk_box_pack_start(GTK_BOX(rbox), args_frame, TRUE, TRUE, 0);
		/* debugger type */
#if GTK_CHECK_VERSION(3, 0, 0)
		hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, SPACING);
#else
		hbox = gtk_hbox_new(FALSE, SPACING);
#endif
		gtk_box_pack_start(GTK_BOX(hbox), debugger_label, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(hbox), debugger_cmb, TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(rbox), hbox, FALSE, FALSE, 0);
	}
	else
	{
		GtkWidget *lbox, *rbox, *hbox;

#if GTK_CHECK_VERSION(3, 0, 0)
		root = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, SPACING);
		gtk_box_set_homogeneous(GTK_BOX(root), TRUE);
#else
		root = gtk_hbox_new(TRUE, SPACING);
#endif
		gtk_container_set_border_width(GTK_CONTAINER(root), ROOT_BORDER_WIDTH);

#if GTK_CHECK_VERSION(3, 0, 0)
		lbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, SPACING);
		rbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, SPACING);
#else
		lbox = gtk_vbox_new(FALSE, SPACING);
		rbox = gtk_vbox_new(FALSE, SPACING);
#endif
		gtk_box_pack_start(GTK_BOX(root), lbox, TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(root), rbox, TRUE, TRUE, 0);

		/* environment */
		gtk_box_pack_start(GTK_BOX(lbox), env_frame, TRUE, TRUE, 0);

		/* target */
#if GTK_CHECK_VERSION(3, 0, 0)
		hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, SPACING);
#else
		hbox = gtk_hbox_new(FALSE, SPACING);
#endif
		gtk_box_pack_start(GTK_BOX(hbox), target_label, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(hbox), target_name, TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(hbox), target_button_browse, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(rbox), hbox, FALSE, FALSE, 0);
		/* arguments */
		gtk_box_pack_start(GTK_BOX(rbox), args_frame, TRUE, TRUE, 0);
		/* debugger type */
#if GTK_CHECK_VERSION(3, 0, 0)
		hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, SPACING);
#else
		hbox = gtk_hbox_new(FALSE, SPACING);
#endif
		gtk_box_pack_start(GTK_BOX(hbox), debugger_label, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(hbox), debugger_cmb, TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(rbox), hbox, FALSE, FALSE, 0);
	}

	if (oldroot)
	{
		int i = 0;
		while (widgets[i])
		{
			g_object_unref(*widgets[i]);
			i++;
		}
		gtk_container_remove(GTK_CONTAINER(tab_target), oldroot);
	}

#if GTK_CHECK_VERSION(3, 0, 0)
	gtk_box_pack_start(GTK_BOX(tab_target), root, TRUE, TRUE, 0);
#else
	gtk_container_add(GTK_CONTAINER(tab_target), root);
#endif
	gtk_widget_show_all(tab_target);
}

/*
 * create widgets 
 */
static void tpage_create_widgets(void)
{
	GList *modules, *iter;
	GtkWidget *hbox;
	GtkTextBuffer *buffer;
	GtkWidget *tree;

	/* target */
	target_label = gtk_label_new(_("Target:"));
	target_name = gtk_entry_new ();
#if GTK_CHECK_VERSION(3, 0, 0)
	gtk_editable_set_editable(GTK_EDITABLE(target_name), FALSE);
	target_button_browse = create_stock_button("document-open", _("Browse"));
#else
	gtk_entry_set_editable(GTK_ENTRY(target_name), FALSE);
	target_button_browse = create_stock_button(GTK_STOCK_OPEN, _("Browse"));
#endif
	gtk_widget_set_size_request(target_button_browse, BROWSE_BUTTON_WIDTH, 0);
	g_signal_connect(G_OBJECT(target_button_browse), "clicked", G_CALLBACK (on_target_browse_clicked), NULL);

	/* debugger */
	debugger_label = gtk_label_new(_("Debugger:")); 
	debugger_cmb = gtk_combo_box_text_new();
	modules = debug_get_modules();
	for (iter = modules; iter; iter = iter->next)
	{
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(debugger_cmb), (gchar*)iter->data);
	}
	g_list_free(modules);
	gtk_combo_box_set_active(GTK_COMBO_BOX(debugger_cmb), 0);

	/* arguments */
	args_frame = gtk_frame_new(_("Command Line Arguments"));
	hbox = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(hbox), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	args_textview = gtk_text_view_new();
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(args_textview), GTK_WRAP_CHAR);
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(args_textview));
	g_signal_connect(G_OBJECT(buffer), "changed", G_CALLBACK (on_arguments_changed), NULL);
	gtk_container_add(GTK_CONTAINER(hbox), args_textview);
	gtk_container_add(GTK_CONTAINER(args_frame), hbox);

	/* environment */
	env_frame = gtk_frame_new(_("Environment Variables"));
	hbox = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(hbox), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	tree = envtree_init();
	gtk_container_add(GTK_CONTAINER(hbox), tree);
	gtk_container_add(GTK_CONTAINER(env_frame), hbox);
}

/*
 * set target 
 */
void tpage_set_target(const gchar *newvalue)
{
	gtk_entry_set_text(GTK_ENTRY(target_name), newvalue);
}

/*
 * set debugger 
 */
void tpage_set_debugger(const gchar *newvalue)
{
	int module = debug_get_module_index(newvalue);
	if (-1 == module)
	{
		module = 0;
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(debugger_cmb), module);
}

/*
 * set command line 
 */
void tpage_set_commandline(const gchar *newvalue)
{
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(args_textview));
	gtk_text_buffer_set_text(buffer, newvalue, -1);
}

/*
 * add environment variable
 */
void tpage_add_environment(const gchar *name, const gchar *value)
{
	envtree_add_environment(name, value);
}

/*
 * removes all data (clears widgets)
 */
void tpage_clear(void)
{
	GtkTextBuffer *buffer;

	/* target */
	gtk_entry_set_text(GTK_ENTRY(target_name), "");
	
	/* reset debugger type */
	gtk_combo_box_set_active(GTK_COMBO_BOX(debugger_cmb), 0);

	/* arguments */
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(args_textview));
	gtk_text_buffer_set_text(buffer, "", -1);

	/* environment variables */
	envtree_clear();
}

/*
 * get target file names
 */
gchar* tpage_get_target(void)
{
	return g_strdup(gtk_entry_get_text(GTK_ENTRY(target_name)));
}

/*
 * get selected debugger module index
 */
int tpage_get_debug_module_index(void)
{
	return gtk_combo_box_get_active(GTK_COMBO_BOX(debugger_cmb));
}

/*
 * get selected debugger name
 */
gchar* tpage_get_debugger(void)
{
	return gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(debugger_cmb));
}

/*
 * get command line
 */
gchar* tpage_get_commandline(void)
{
	gchar *args;
	gchar **lines;
	GtkTextIter start, end;
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(args_textview));
	
	gtk_text_buffer_get_start_iter(buffer, &start);
	gtk_text_buffer_get_end_iter(buffer, &end);
	
	args = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
	lines = g_strsplit(args, "\n", 0);
	g_free(args);
	args = g_strjoinv(" ", lines);
	g_strfreev(lines);
	
	return args;
}

/*
 * get list of environment variables
 */
GList* tpage_get_environment(void)
{
	return envpage_get_environment();
}

/*
 * create target page
 */
void tpage_init(void)
{
#if GTK_CHECK_VERSION(3, 0, 0)
	tab_target = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
#else
	tab_target = gtk_vbox_new(FALSE, 0);
#endif
	tpage_create_widgets();
}

/*
 * set the page readonly
 */
void tpage_set_readonly(gboolean readonly)
{
	gtk_text_view_set_editable (GTK_TEXT_VIEW (args_textview), !readonly);
	gtk_widget_set_sensitive (target_button_browse, !readonly);
	gtk_widget_set_sensitive (debugger_cmb, !readonly);

	envtree_set_readonly(readonly);
}
