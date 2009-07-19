/*
 *      codenavigation.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2007-2009 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2007-2009 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
 *
 * $Id: codenavigation.c 3522 2009-01-28 17:55:58Z eht16 $
 */

/**
 * Code navigation plugin - plugin which adds facilities for navigating between files.
 * 2009 Lionel Fuentes.
 */

/* First */
#include "geany.h"		/* for the GeanyApp data type */

/* Other includes */
#include "support.h"	/* for the _() translation macro (see also po/POTFILES.in) */
#include "editor.h"		/* for the declaration of the GeanyEditor struct, not strictly necessary
						   as it will be also included by plugindata.h */
#include "document.h"	/* for the declaration of the GeanyDocument struct */
#include "ui_utils.h"
#include "Scintilla.h"	/* for the SCNotification struct */

#include "keybindings.h"
#include "filetypes.h"
#include <gdk/gdkkeysyms.h>
#include <string.h>

/* Last */
#include "plugindata.h"		/* this defines the plugin API */
#include "geanyfunctions.h"	/* this wraps geany_functions function pointers */

/* These items are set by Geany before plugin_init() is called. */
GeanyPlugin		*geany_plugin;
GeanyData		*geany_data;
GeanyFunctions	*geany_functions;

/* #define CODE_NAVIGATION_DEBUG */
#define CODE_NAVIGATION_VERSION "0.1"

#ifdef CODE_NAVIGATION_DEBUG
#define log_debug g_print
#else
inline void log_debug(const gchar* s, ...) {}
#endif

/* Check that the running Geany supports the plugin API used below, and check
 * for binary compatibility. */
PLUGIN_VERSION_CHECK(112)

/* All plugins must set name, description, version and author. */
PLUGIN_SET_INFO(_("Code navigation"), _("This plugin adds features to facilitate navigation between source files.\n"
										"As for the moment, it implements :\n"
										"- switching between a .cpp file and the corresponding .h file\n"
										"- opening a file by typing its name"), CODE_NAVIGATION_VERSION, _("Lionel Fuentes"))

static GtkWidget* switch_menu_item = NULL;
static GtkWidget* goto_file_menu_item = NULL;

typedef struct
{
	GArray* head_extensions;	/* e.g. : "h", "hpp", ... */
	GArray* impl_extensions; /* e.g. : "cpp", "cxx", ... */
} LanguageExtensions;

/* Array of LanguageExtensions */
static GArray* languages_extensions;

/* Keybindings */
enum
{
	PLUGIN_KEYS_SWITCH,
	PLUGIN_KEYS_GOTO_FILE,
	PLUGIN_KEYS_NUMBER	/* Dummy value, just to keep track of the number of different values */
};

PLUGIN_KEY_GROUP(code_navigation, PLUGIN_KEYS_NUMBER)

/* Utility function to check if a string is in a given string array */
static gboolean
str_is_in_garray(const gchar* str, GArray* garray)
{
	guint i;

	for(i=0 ; i < garray->len ; i++)
	{
		if(g_strcmp0(str, g_array_index(garray, gchar*, i)) == 0)
			return TRUE;
	}
	return FALSE;
}

/* Utility function, which returns a newly-allocated string containing the
 * extension of the file path which is given, or NULL if it did not found any extension.
 */
static gchar*
get_extension(gchar* path)
{
	gchar* extension = NULL;
	gchar* pc = path;
	while(*pc != '\0')
	{
		if(*pc == '.')
			extension = pc+1;
		pc++;
	}

	if(extension == NULL || (*extension) == '\0')
		return NULL;
	else
		return g_strdup(extension);
}

/* Copy a path and remove the extension
 */
static gchar*
copy_and_remove_extension(gchar* path)
{
	gchar* str = NULL;
	gchar* pc = NULL;
	gchar* dot_pos = NULL;

	if(path == NULL || path[0] == '\0')
		return NULL;

	str = g_strdup(path);
	pc = str;
	while(*pc != '\0')
	{
		if(*pc == '.')
			dot_pos = pc;
		pc++;
	}

	if(dot_pos != NULL)
		*dot_pos = '\0';

	return str;
}

/* Callback when the menu item is clicked. */
static void
switch_menu_item_activate(guint key_id)
{
	GeanyDocument* current_doc = document_get_current();
	GeanyDocument* new_doc = NULL;
	guint nb_documents = geany->documents_array->len;

	gchar* extension = NULL;	/* e.g. : "hpp" */

	GArray* p_extensions_to_test = NULL;	/* e.g. : {"cpp", "cxx", ...} */

	GArray* filenames_to_test = NULL;	/* e.g. : {"f.cpp", "f.cxx", ...} */

	gchar* dirname = NULL;
	gchar* basename = NULL;
	gchar* basename_no_extension = NULL;

	guint i=0;
	guint j=0;

	gchar* p_str = NULL;	/* Local variables, used as temporaty buffers */
	gchar* p_str2 = NULL;

	log_debug("DEBUG : switch menu activate\n");

	log_debug("DEBUG : current_doc->file_name == %s\n", current_doc->file_name);

	if(current_doc != NULL && current_doc->file_name != NULL && current_doc->file_name[0] != '\0')
	{
		/* Get the basename, e.g. : "/home/me/file.cpp" -> "file.cpp" */
		basename = g_path_get_basename(current_doc->file_name);

		if(strlen(basename) < 2)
			goto free_mem;

		log_debug("DEBUG : basename == %s\n", basename);

		/* Get the extension , e.g. : "cpp" */
		extension = get_extension(basename);

		if(extension == NULL || strlen(extension) == 0)
			goto free_mem;

		log_debug("DEBUG : extension == %s\n", extension);

		/* Get the basename without any extension */
		basename_no_extension = copy_and_remove_extension(basename);
		if(basename_no_extension == NULL || strlen(basename_no_extension) == 0)
			goto free_mem;

		/* Identify the language and whether the file is a header or an implementation. */
		/* For each recognized language : */
		for(i=0 ; i < languages_extensions->len ; i++)
		{
			LanguageExtensions* p_lang = &(g_array_index(languages_extensions, LanguageExtensions, i));

			/* Test the headers : */
			if(str_is_in_garray(extension, p_lang->head_extensions))
			{
				p_extensions_to_test = p_lang->impl_extensions;
				break;
			}

			/* Test the implementations : */
			else if(str_is_in_garray(extension, p_lang->impl_extensions))
			{
				p_extensions_to_test = p_lang->head_extensions;
				break;
			}
		}

		if(p_extensions_to_test == NULL)
			goto free_mem;

		log_debug("DEBUG : extension known !\n");
		log_debug("DEBUG : p_extensions_to_test : ");
		for(i=0 ; i < p_extensions_to_test->len ; i++)
			log_debug("\"%s\", ", g_array_index(p_extensions_to_test, gchar*, i));
		log_debug("\n");

		/* Build an array of filenames to test : */
		filenames_to_test = g_array_sized_new(FALSE, FALSE, sizeof(gchar*), p_extensions_to_test->len);
		for(i=0 ; i < p_extensions_to_test->len ; i++)
		{
			p_str = g_strdup_printf("%s.%s", basename_no_extension, g_array_index(p_extensions_to_test, gchar*, i));
			g_array_append_val(filenames_to_test, p_str);
			log_debug("DEBUG : filenames_to_test[%d] == \"%s\"", i, g_array_index(filenames_to_test, gchar*, i));
		}

		/* First : look for a corresponding file in the opened files.
		 * If found, open it. */
		for(i=0 ; i < nb_documents ; i++)
		{
			new_doc = document_index(i);

			for(j=0 ; j < p_extensions_to_test->len ; j++)
			{
				p_str = g_path_get_basename(new_doc->file_name);

				log_debug("DEBUG : comparing \"%s\" and \"%s\"\n", g_array_index(filenames_to_test, gchar*, j), p_str);
				if(g_strcmp0(g_array_index(filenames_to_test, gchar*, j), p_str) == 0)
				{
					log_debug("DEBUG : FOUND ! i == %d\n", i);
					g_free(p_str);

					p_str = g_locale_from_utf8(new_doc->file_name, -1, NULL, NULL, NULL);
					document_open_file(p_str, FALSE, NULL, NULL);
					g_free(p_str);
					goto free_mem;
				}

				g_free(p_str);
			}
		}


		/* Second : if not found, look for a corresponding file in the same directory.
		 * If found, open it.
		 */
		/* -> compute dirname */
		dirname = g_path_get_dirname(current_doc->real_path);
		if(dirname == NULL)
			goto free_mem;

		log_debug("DEBUG : dirname == \"%s\"", dirname);

		/* -> try all the extensions we should test */
		for(i=0 ; i < p_extensions_to_test->len ; i++)
		{
			p_str = g_strdup_printf(	"%s" G_DIR_SEPARATOR_S "%s.%s",
										dirname, basename_no_extension, g_array_index(p_extensions_to_test, gchar*, i));

			p_str2 = g_locale_from_utf8(p_str, -1, NULL, NULL, NULL);
			g_free(p_str);

			log_debug("DEBUG : trying to open the file \"%s\"\n", p_str2);

			/* Try without read-only and in read-only mode */
			if(	document_open_file(p_str2, FALSE, NULL, NULL) != NULL ||
				document_open_file(p_str2, TRUE, NULL, NULL) != NULL)
			{
				g_free(p_str2);
				goto free_mem;
			}
			g_free(p_str2);
		}

		/* Third : if not found, ask the user if he wants to create it or not. */
		{
			p_str = g_strdup_printf("%s.%s", basename_no_extension, g_array_index(p_extensions_to_test, gchar*, 0));

			GtkWidget* dialog = gtk_message_dialog_new(	GTK_WINDOW(geany_data->main_widgets->window),
														GTK_DIALOG_MODAL,
														GTK_MESSAGE_QUESTION,
														GTK_BUTTONS_OK_CANCEL,
														_("%s not found, create it ?"), p_str);
			gtk_window_set_title(GTK_WINDOW(dialog), "Geany");
			if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
			{
				document_new_file(p_str, current_doc->file_type, NULL);
				document_set_text_changed(document_get_current(), TRUE);
			}

			log_debug("DESTROY");

			gtk_widget_destroy(dialog);

			g_free(p_str);
		}

		/* Free the memory */
free_mem:
		if(filenames_to_test != NULL)
		{
			for(i=0 ; i < filenames_to_test->len ; i++)
				g_free(g_array_index(filenames_to_test, gchar*, i));
			g_array_free(filenames_to_test, TRUE);
		}

		g_free(dirname);
		g_free(basename_no_extension);
		g_free(extension);
		g_free(basename);
	}
}

/* Callback when the menu item is clicked. */
static void
goto_file_menu_item_activate(guint key_id)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new(
		GTK_WINDOW(geany->main_widgets->window),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_INFO,
		GTK_BUTTONS_OK,
		"Open by name : TODO");

	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
		_("(From the %s plugin)"), geany_plugin->info->name);

	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}


/* Called by Geany to initialize the plugin.
 * Note: data is the same as geany_data. */
void plugin_init(GeanyData *data)
{
	log_debug("DEBUG : plugin_init : POUET\n");

	LanguageExtensions le;

	gchar* p_str = NULL;

	/* Get a pointer to the "Edit" menu */
	GtkWidget* edit_menu = ui_lookup_widget(geany->main_widgets->window, "edit1_menu");

	/* Add items to the Edit menu : */

	/* - add a separator */
	GtkWidget* separator = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(edit_menu), separator);
	gtk_widget_show(separator);

	/* - add the "Switch header/implementation" menu item */
	switch_menu_item = gtk_menu_item_new_with_mnemonic(_("Switch header/implementation"));
	gtk_widget_show(switch_menu_item);
	gtk_container_add(GTK_CONTAINER(edit_menu), switch_menu_item);

	/* - add the "Open file by name" menu item */
	goto_file_menu_item = gtk_menu_item_new_with_mnemonic(_("Goto file..."));
	gtk_widget_show(goto_file_menu_item);
	gtk_container_add(GTK_CONTAINER(edit_menu), goto_file_menu_item);

	/* make the menu items sensitive only when documents are open */
	ui_add_document_sensitive(switch_menu_item);
	ui_add_document_sensitive(goto_file_menu_item);

	/* Initialize the key bindings : */
	plugin_keys[PLUGIN_KEYS_SWITCH].key = GDK_s;
	plugin_keys[PLUGIN_KEYS_SWITCH].mods = GDK_MOD1_MASK | GDK_SHIFT_MASK;
	plugin_keys[PLUGIN_KEYS_SWITCH].name = _("switch_header_impl");
	plugin_keys[PLUGIN_KEYS_SWITCH].label = _("Switch header/implementation");
	plugin_keys[PLUGIN_KEYS_SWITCH].callback = (GeanyKeyCallback)(&switch_menu_item_activate);
	plugin_keys[PLUGIN_KEYS_SWITCH].menu_item = switch_menu_item;

	plugin_keys[PLUGIN_KEYS_GOTO_FILE].key = GDK_g;
	plugin_keys[PLUGIN_KEYS_GOTO_FILE].mods = GDK_MOD1_MASK | GDK_SHIFT_MASK;
	plugin_keys[PLUGIN_KEYS_GOTO_FILE].name = _("goto_file");
	plugin_keys[PLUGIN_KEYS_GOTO_FILE].label = _("Goto file...");
	plugin_keys[PLUGIN_KEYS_GOTO_FILE].callback = (GeanyKeyCallback)(&goto_file_menu_item_activate);
	plugin_keys[PLUGIN_KEYS_GOTO_FILE].menu_item = goto_file_menu_item;

	/* Initialize the extensions array.
	 * TODO : we should let the user configure this. */
	languages_extensions = g_array_new(FALSE, FALSE, sizeof(LanguageExtensions));

	/* C/C++ */
	le.head_extensions = g_array_new(FALSE, FALSE, sizeof(gchar*));
	le.impl_extensions = g_array_new(FALSE, FALSE, sizeof(gchar*));

	p_str = g_strdup("h");   g_array_append_val(le.head_extensions, p_str);
	p_str = g_strdup("hpp"); g_array_append_val(le.head_extensions, p_str);
	p_str = g_strdup("hxx"); g_array_append_val(le.head_extensions, p_str);
	p_str = g_strdup("h++"); g_array_append_val(le.head_extensions, p_str);
	p_str = g_strdup("hh");  g_array_append_val(le.head_extensions, p_str);

	p_str = g_strdup("cpp"); g_array_append_val(le.impl_extensions, p_str);
	p_str = g_strdup("cxx"); g_array_append_val(le.impl_extensions, p_str);
	p_str = g_strdup("c++"); g_array_append_val(le.impl_extensions, p_str);
	p_str = g_strdup("cc");  g_array_append_val(le.impl_extensions, p_str);
	p_str = g_strdup("C"); g_array_append_val(le.impl_extensions, p_str);
	p_str = g_strdup("c"); g_array_append_val(le.impl_extensions, p_str);

	g_array_append_val(languages_extensions, le);


	/* GLSL */
	le.head_extensions = g_array_new(FALSE, FALSE, sizeof(gchar*));
	le.impl_extensions = g_array_new(FALSE, FALSE, sizeof(gchar*));

	p_str = g_strdup("vert");   g_array_append_val(le.head_extensions, p_str);

	p_str = g_strdup("frag"); g_array_append_val(le.impl_extensions, p_str);

	g_array_append_val(languages_extensions, le);


	/* Ada */
	le.head_extensions = g_array_new(FALSE, FALSE, sizeof(gchar*));
	le.impl_extensions = g_array_new(FALSE, FALSE, sizeof(gchar*));

	p_str = g_strdup("ads");   g_array_append_val(le.head_extensions, p_str);

	p_str = g_strdup("adb"); g_array_append_val(le.impl_extensions, p_str);

	g_array_append_val(languages_extensions, le);
}


/* Callback connected in plugin_configure(). */
static void
on_configure_response(GtkDialog *dialog, gint response, gpointer user_data)
{
	/* catch OK or Apply clicked */
	if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY)
	{
		/* maybe the plugin should write here the settings into a file
		 * (e.g. using GLib's GKeyFile API)
		 * all plugin specific files should be created in:
		 * geany->app->configdir G_DIR_SEPARATOR_S plugins G_DIR_SEPARATOR_S pluginname G_DIR_SEPARATOR_S
		 * e.g. this could be: ~/.config/geany/plugins/Demo/, please use geany->app->configdir */
	}
}

/* Callback connected in plugin_configure() to the click on the "Add" button for the
 * "Switch headers/implementation" frame */
static void
on_configure_add_language(GtkWidget* widget, gpointer data)
{
	GtkWidget *list_view = (GtkWidget*)data;
	GtkListStore *list_store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(list_view)));
	GtkTreeIter iter;
	GtkTreePath *path;
	GtkTreeViewColumn* column = NULL;
	gint nb_lines;

	/* Add a line */
	gtk_list_store_append(list_store, &iter);

	/* and give the focus to it */
	nb_lines = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(list_store), NULL);

	path = gtk_tree_path_new_from_indices(nb_lines-1, -1);

	column = gtk_tree_view_get_column(GTK_TREE_VIEW(list_view), 0);
	gtk_tree_view_set_cursor(GTK_TREE_VIEW(list_view), path, column, TRUE);
	gtk_widget_grab_focus(list_view);

	gtk_tree_path_free(path);
}

/* Same thing for the "Remove" button */
static void
on_configure_remove_language(GtkWidget* widget, gpointer data)
{
	//GtkWidget* list_view = (GtkWidget*)data;
}

/* Called by Geany to show the plugin's configure dialog. This function is always called after
 * plugin_init() was called.
 */
GtkWidget *plugin_configure(GtkDialog *dialog)
{
	GtkWidget *label, *vbox, *list_view, *switch_frame_vbox;
	GtkWidget *switch_frame, *switch_frame_hbox, *add_button, *remove_button;
	GtkListStore *list_store;
	GtkTreeViewColumn *column;
	GtkCellRenderer *cell_renderer;
	GtkTreeIter iter;
	guint i, j, k;
	gchar** str_array = NULL;
	gchar* str = NULL;

	typedef enum { COLUMN_HEAD, COLUMN_IMPL, NB_COLUMNS } Column;

	vbox = gtk_vbox_new(FALSE, 6);

	/* Add a label */
	label = gtk_label_new(_("Code Navigation plug-in configuration"));
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, TRUE, 0);

	/* Add a frame for the switch header/implementation feature */
	switch_frame = gtk_frame_new(_("Switch header / implementation"));
	gtk_box_pack_start(GTK_BOX(vbox), switch_frame, FALSE, TRUE, 0);

	/* Add a vbox into the frame */
	switch_frame_vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(switch_frame), switch_frame_vbox);

	/* Add a list containing the extensions for each language (headers / implementations) */
	/* - create the GtkListStore */
	list_store = gtk_list_store_new(NB_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);
	for(i=0 ; i < languages_extensions->len ; i++)
	{
		LanguageExtensions* le = &(g_array_index(languages_extensions, LanguageExtensions, i));
		GArray* extensions = NULL;
		Column col;

		if(le->head_extensions->len == 0 || le->impl_extensions->len == 0)
			continue;

		/* Fill the GtkListStore with comma-separated strings */
		/* loop : "headers", then "implementations" */
		col = COLUMN_HEAD;
		extensions = le->head_extensions;
		for(j=0 ; j<2 ; j++)
		{
			/* Copy extensions to str_array and then join the strings, separated with commas. */
			str_array = g_malloc((extensions->len+1) * sizeof(gchar*));

			log_debug("DEBUG : extensions->len == %d", extensions->len);
			log_debug("DEBUG : head_extensions->len == %d", le->head_extensions->len);
			log_debug("DEBUG : impl_extensions->len == %d", le->impl_extensions->len);
			for(k=0 ; k < extensions->len ; k++)
			{
				str_array[k] = g_strdup(g_array_index(extensions, gchar*, k));
				log_debug("DEBUG : str_array[%d] == %s", k, str_array[k]);
			}
			str_array[k] = NULL;

			str = g_strjoinv(",", str_array);

			log_debug("DEBUG : str == \"%s\"", str);

			if(j == 0)
				gtk_list_store_append(list_store, &iter);
			gtk_list_store_set(list_store, &iter, col, str, -1);

			g_free(str);

			for(k=0 ; k < extensions->len ; k++)
				g_free(str_array[k]);

			g_free(str_array);

			/* Next iteration : "implementations" */
			col = COLUMN_IMPL;
			extensions = le->impl_extensions;
		}
	}

	/* - create the GtkTreeView */
	list_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));

	/* - add the columns */
	/* -> headers : */
	cell_renderer = gtk_cell_renderer_text_new();
	g_object_set(G_OBJECT(cell_renderer), "editable", TRUE, NULL);
	column = gtk_tree_view_column_new_with_attributes(	_("Headers extensions"), cell_renderer,
														"text", COLUMN_HEAD,
														NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list_view), column);

	/* -> implementations : */
	cell_renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(	_("Implementations extensions"), cell_renderer,
														"text", COLUMN_IMPL,
														NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list_view), column);

	/* - finally add the GtkTreeView to the frame's vbox */
	gtk_box_pack_start(GTK_BOX(switch_frame_vbox), list_view, FALSE, TRUE, 0);

	/* Add an hbox for the frame's button(s) */
	switch_frame_hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(switch_frame_vbox), switch_frame_hbox, FALSE, FALSE, 0);

	/* Add the "add" button to the frame's hbox */
	add_button = gtk_button_new_from_stock(GTK_STOCK_ADD);
	g_signal_connect(G_OBJECT(add_button), "clicked", G_CALLBACK(on_configure_add_language), list_view);
	gtk_box_pack_start(GTK_BOX(switch_frame_hbox), add_button, FALSE, FALSE, 0);

	/* Add the "remove" button to the frame's hbox */
	remove_button = gtk_button_new_from_stock(GTK_STOCK_REMOVE);
	g_signal_connect(G_OBJECT(remove_button), "clicked", G_CALLBACK(on_configure_remove_language), list_view);
	gtk_box_pack_start(GTK_BOX(switch_frame_hbox), remove_button, FALSE, FALSE, 0);

	/* Show all */
	gtk_widget_show_all(vbox);

	/* Connect a callback for when the user clicks a dialog button */
	g_signal_connect(dialog, "response", G_CALLBACK(on_configure_response), NULL);
	return vbox;
}


/* Called by Geany before unloading the plugin.
 * Here any UI changes should be removed, memory freed and any other finalization done.
 * Be sure to leave Geany as it was before plugin_init(). */
void plugin_cleanup(void)
{
	guint i=0, j=0;
	LanguageExtensions* p_lang = NULL;

	log_debug("DEBUG : plugin_cleanup\n");

	/* For each language : */
	for(i=0 ; i < languages_extensions->len ; i++)
	{
		p_lang = &(g_array_index(languages_extensions, LanguageExtensions, i));

		/* Free the headers' extensions array */
		for(j=0 ; j < p_lang->head_extensions->len ; j++)
			g_free(g_array_index(p_lang->head_extensions, gchar*, j));

		g_array_free(p_lang->head_extensions, TRUE);

		/* Free the implementations' extensions array */
		for(j=0 ; j < p_lang->impl_extensions->len ; j++)
			g_free(g_array_index(p_lang->impl_extensions, gchar*, j));

		g_array_free(p_lang->impl_extensions, TRUE);
	}

	g_array_free(languages_extensions, TRUE);

	/* remove the menu item added in plugin_init() */
	gtk_widget_destroy(switch_menu_item);
	gtk_widget_destroy(goto_file_menu_item);
}
