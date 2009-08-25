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

//#define CODE_NAVIGATION_DEBUG
#define CODE_NAVIGATION_VERSION "0.1"

#ifdef CODE_NAVIGATION_DEBUG
#define log_debug g_print
#else
static void log_debug(const gchar* s, ...) {}
#endif

/* Check that the running Geany supports the plugin API used below, and check
 * for binary compatibility. */
PLUGIN_VERSION_CHECK(112)

/* All plugins must set name, description, version and author. */
PLUGIN_SET_INFO(_("Code navigation"),
	_(	"This plugin adds features to facilitate navigation between source files.\n"
		"As for the moment, it implements :\n"
		"- switching between a .cpp file and the corresponding .h file\n"
		"- [opening a file by typing its name -> TODO]"), CODE_NAVIGATION_VERSION, _("Lionel Fuentes"))

static GtkWidget* switch_menu_item = NULL;
static GtkWidget* goto_file_menu_item = NULL;
static GtkWidget* file_menu_item_separator = NULL;

typedef struct
{
	const gchar* language_name;
	GSList* head_extensions;	/* e.g. : "h", "hpp", ... */
	GSList* impl_extensions; /* e.g. : "cpp", "cxx", ... */
} LanguageExtensions;

/* List of LanguageExtensions */
static GSList* languages_extensions;

/* Keybindings */
enum
{
	PLUGIN_KEYS_SWITCH,
	PLUGIN_KEYS_GOTO_FILE,
	PLUGIN_KEYS_NUMBER	/* Dummy value, just to keep track of the number of different values */
};

PLUGIN_KEY_GROUP(code_navigation, PLUGIN_KEYS_NUMBER)

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

	GSList* p_extensions_to_test = NULL;	/* e.g. : ["cpp", "cxx", ...] */

	GSList* filenames_to_test = NULL;	/* e.g. : ["f.cpp", "f.cxx", ...] */

	GSList* p_lang = NULL;
	GSList* p_ext = NULL;
	GSList* p_filename = NULL;
	gint i=0;

	gchar* dirname = NULL;
	gchar* basename = NULL;
	gchar* basename_no_extension = NULL;

	gchar* p_str = NULL;	/* Local variables, used as temporaty buffers */
	gchar* p_str2 = NULL;

	log_debug("DEBUG : switch menu activate\n");

	log_debug("DEBUG : current_doc->file_name == %s\n", current_doc->file_name);

	log_debug("DEBUG : geany->documents_array->len == %d\n", geany->documents_array->len);

	if(current_doc != NULL && current_doc->file_name != NULL && current_doc->file_name[0] != '\0')
	{
		/* Get the basename, e.g. : "/home/me/file.cpp" -> "file.cpp" */
		basename = g_path_get_basename(current_doc->file_name);

		if(g_utf8_strlen(basename, -1) < 2)
			goto free_mem;

		log_debug("DEBUG : basename == %s\n", basename);

		/* Get the extension , e.g. : "cpp" */
		extension = get_extension(basename);

		if(extension == NULL || g_utf8_strlen(extension, -1) == 0)
			goto free_mem;

		log_debug("DEBUG : extension == %s\n", extension);

		/* Get the basename without any extension */
		basename_no_extension = copy_and_remove_extension(basename);
		if(basename_no_extension == NULL || g_utf8_strlen(basename_no_extension, -1) == 0)
			goto free_mem;

		/* Identify the language and whether the file is a header or an implementation. */
		/* For each recognized language : */
		for(p_lang = languages_extensions ; p_lang != NULL ; p_lang = p_lang->next)
		{
			LanguageExtensions* p_lang_data = (LanguageExtensions*)(p_lang->data);

			/* Test the headers : */
			if(g_slist_find_custom(p_lang_data->head_extensions, extension, (GCompareFunc)(&g_strcmp0)) != NULL)
			{
				p_extensions_to_test = p_lang_data->impl_extensions;
				break;
			}

			/* Test the implementations : */
			else if(g_slist_find_custom(p_lang_data->impl_extensions, extension, (GCompareFunc)(&g_strcmp0)) != NULL)
			{
				p_extensions_to_test = p_lang_data->head_extensions;
				break;
			}
		}

		if(p_extensions_to_test == NULL)
			goto free_mem;

#ifdef CODE_NAVIGATION_DEBUG
		log_debug("DEBUG : extension known !\n");
		log_debug("DEBUG : p_extensions_to_test : ");
		g_slist_foreach(p_extensions_to_test, (GFunc)(&log_debug), NULL);
		log_debug("\n");
#endif

		/* Build a list of filenames to test : */
		filenames_to_test = NULL;
		for(p_ext = p_extensions_to_test ; p_ext != NULL ; p_ext = p_ext->next)
		{
			p_str = g_strdup_printf("%s.%s", basename_no_extension, (const gchar*)(p_ext->data));
			filenames_to_test = g_slist_prepend(filenames_to_test, p_str);
		}

		filenames_to_test = g_slist_reverse(filenames_to_test);

#ifdef CODE_NAVIGATION_DEBUG
		log_debug("DEBUG : filenames to test :\n");
		g_slist_foreach(filenames_to_test, (GFunc)(&log_debug), NULL);
		log_debug("\n");
#endif

		/* First : look for a corresponding file in the opened files.
		 * If found, open it. */
		for(i=0 ; i < nb_documents ; i++)
		{
			new_doc = document_index(i);

			for(p_filename = filenames_to_test ; p_filename != NULL ; p_filename = p_filename->next)
			{
				p_str = g_path_get_basename(new_doc->file_name);

				log_debug("DEBUG : comparing \"%s\" and \"%s\"\n", (const gchar*)(p_filename->data), p_str);
				if(g_strcmp0((const gchar*)(p_filename->data), p_str) == 0)
				{
					log_debug("DEBUG : FOUND !\n");
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
		for(p_ext = p_extensions_to_test ; p_ext != NULL ; p_ext = p_ext->next)
		{
			p_str = g_strdup_printf(	"%s" G_DIR_SEPARATOR_S "%s.%s",
										dirname, basename_no_extension, (const gchar*)(p_ext->data));

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
			p_str = g_strdup_printf("%s.%s", basename_no_extension, (const gchar*)(p_extensions_to_test->data));

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
		g_slist_foreach(filenames_to_test, (GFunc)(&g_free), NULL);
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
	LanguageExtensions* le = NULL;

	gchar* p_str = NULL;

	/* Get a pointer to the "Edit" menu */
	GtkWidget* edit_menu = ui_lookup_widget(geany->main_widgets->window, "edit1_menu");

	log_debug("DEBUG : plugin_init\n");

	/* Add items to the Edit menu : */

	/* - add a separator */
	file_menu_item_separator = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(edit_menu), file_menu_item_separator);
	gtk_widget_show(file_menu_item_separator);

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
	keybindings_set_item(	plugin_key_group,
							PLUGIN_KEYS_SWITCH,
							(GeanyKeyCallback)(&switch_menu_item_activate),
 							GDK_s, GDK_MOD1_MASK | GDK_SHIFT_MASK,
 							_("switch_header_impl"),
 							_("Switch header/implementation"),
 							switch_menu_item);

 	keybindings_set_item(	plugin_key_group,
 							PLUGIN_KEYS_GOTO_FILE,
 							(GeanyKeyCallback)(&goto_file_menu_item_activate),
 							GDK_g, GDK_MOD1_MASK | GDK_SHIFT_MASK,
 							_("goto_file"),
 							_("Goto file..."),
 							goto_file_menu_item);

	/* Initialize the extensions list.
	 * TODO : we should let the user configure this. */
	languages_extensions = NULL;
	le = NULL;

#define HEAD_PREPEND(str_ext) { p_str = g_strdup(str_ext); le->head_extensions = g_slist_prepend(le->head_extensions, p_str); }
#define IMPL_PREPEND(str_ext) { p_str = g_strdup(str_ext); le->impl_extensions = g_slist_prepend(le->impl_extensions, p_str); }

	/* C/C++ */
	le = g_malloc0(sizeof(LanguageExtensions));
	le->language_name = "c_cpp";

	HEAD_PREPEND("h");
	HEAD_PREPEND("hpp");
	HEAD_PREPEND("hxx");
	HEAD_PREPEND("h++");
	HEAD_PREPEND("hh");
	le->head_extensions = g_slist_reverse(le->head_extensions);

	IMPL_PREPEND("cpp");
	IMPL_PREPEND("cxx");
	IMPL_PREPEND("c++");
	IMPL_PREPEND("cc");
	IMPL_PREPEND("C");
	IMPL_PREPEND("c");
	le->impl_extensions = g_slist_reverse(le->impl_extensions);

	languages_extensions = g_slist_prepend(languages_extensions, le);

	/* GLSL */

	le = g_malloc0(sizeof(LanguageExtensions));
	le->language_name = "glsl";

	HEAD_PREPEND("vert");
	le->head_extensions = g_slist_reverse(le->head_extensions);

	IMPL_PREPEND("frag");
	le->impl_extensions = g_slist_reverse(le->impl_extensions);

	languages_extensions = g_slist_prepend(languages_extensions, le);

	/* Ada */

	le = g_malloc0(sizeof(LanguageExtensions));
	le->language_name = "ada";

	HEAD_PREPEND("ads");
	le->head_extensions = g_slist_reverse(le->head_extensions);

	IMPL_PREPEND("adb");
	le->impl_extensions = g_slist_reverse(le->impl_extensions);

	languages_extensions = g_slist_prepend(languages_extensions, le);

	/* Done : */
	languages_extensions = g_slist_reverse(languages_extensions);

#undef HEAD_PREPEND
#undef IMPL_PREPEND
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

		const gchar** lang_names = NULL;
		GSList* lang_iterator = NULL;
		guint i=0;
		const guint nb_languages = g_slist_length(languages_extensions);

		/* Open the GKeyFile */
		GKeyFile* key_file = g_key_file_new();
		gchar* config_dir = g_strconcat(geany->app->configdir, G_DIR_SEPARATOR_S "plugins" G_DIR_SEPARATOR_S
			"codenav" G_DIR_SEPARATOR_S, NULL);
		gchar* config_filename = g_strconcat(config_dir, "codenav.conf", NULL);

		g_key_file_load_from_file(key_file, config_filename, G_KEY_FILE_NONE, NULL);

		/* Build an array of language names */
		lang_names = g_malloc(nb_languages * sizeof(gchar*));
		for(lang_iterator = languages_extensions, i=0 ;
			lang_iterator != NULL ;
			lang_iterator = lang_iterator->next, i++)
		{
			lang_names[i] = ((const LanguageExtensions*)(lang_iterator->data))->language_name;
		}

		g_key_file_set_string_list(key_file, "switching", "languages", lang_names, nb_languages);

		g_free(lang_names);

		/* Finally write to the config file */
		if (!g_file_test(config_dir, G_FILE_TEST_IS_DIR)
		    && utils_mkdir(config_dir, TRUE) != 0)
		{
			dialogs_show_msgbox(GTK_MESSAGE_ERROR, _("Plugin configuration directory could not be created."));
		}
		else
		{
			gchar* data = g_key_file_to_data(key_file, NULL, NULL);
			utils_write_file(config_filename, data);
			g_free(data);
		}

		g_free(config_dir);
		g_free(config_filename);
		g_key_file_free(key_file);
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
	GSList* p_le = NULL;
	gchar* p_str = NULL;
	gint i=0;

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
	for(p_le = languages_extensions ; p_le != NULL ; p_le = p_le->next)
	{
		LanguageExtensions* le = (LanguageExtensions*)(p_le->data);
		GSList* p_extensions = NULL;
		GSList* p_ext = NULL;
		Column col;

		if(le->head_extensions == NULL || le->impl_extensions == NULL)
			continue;

		/* Fill the GtkListStore with comma-separated strings */
		/* loop : "headers", then "implementations" */
		col = COLUMN_HEAD;
		p_extensions = le->head_extensions;
		for(i=0 ; i<2 ; i++)
		{
			/* Copy extensions to str_array and then join the strings, separated with commas. */
			p_str = NULL;
			for(p_ext = p_extensions ; p_ext != NULL ; p_ext = p_ext->next)
			{
				gchar* temp = p_str;
				p_str = g_strjoin(",", (const gchar*)(p_ext->data), p_str, NULL);
				g_free(temp);
			}
			log_debug("DEBUG : str == \"%s\"", p_str);

			if(i == 0)
				gtk_list_store_append(list_store, &iter);
			gtk_list_store_set(list_store, &iter, col, p_str, -1);

			g_free(p_str);

			/* Next iteration : "implementations" */
			col = COLUMN_IMPL;
			p_extensions = le->impl_extensions;
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
	GSList* p_le = NULL;

	log_debug("DEBUG : plugin_cleanup\n");

	for(p_le = languages_extensions ; p_le != NULL ; p_le = p_le->next)
	{
		LanguageExtensions* le = (LanguageExtensions*)(p_le->data);

		g_slist_foreach(le->head_extensions, (GFunc)(&g_free), NULL);
		g_slist_free(le->head_extensions);

		g_slist_foreach(le->impl_extensions, (GFunc)(&g_free), NULL);
		g_slist_free(le->impl_extensions);
	}

	g_slist_free(languages_extensions);

	/* remove the menu item added in plugin_init() */
	gtk_widget_destroy(switch_menu_item);
	gtk_widget_destroy(goto_file_menu_item);
	gtk_widget_destroy(file_menu_item_separator);
}
