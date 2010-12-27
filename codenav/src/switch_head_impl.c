/*
 *      switch_head_impl.c - this file is part of "codenavigation", which is
 *      part of the "geany-plugins" project.
 *
 *      Copyright 2009 Lionel Fuentes <funto66(at)gmail(dot)com>
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

#include "switch_head_impl.h"
#include "utils.h"

/********************* Data types for the feature *********************/

/* Structure representing a handled language */
typedef struct
{
	const gchar* name;
	GSList* head_extensions;	/* e.g. : "h", "hpp", ... */
	GSList* impl_extensions; /* e.g. : "cpp", "cxx", ... */
} Language;

/* Column for the configuration widget */
typedef enum
{
	COLUMN_HEAD,
	COLUMN_IMPL,
	NB_COLUMNS
} Column;

/******************* Global variables for the feature *****************/

static GtkWidget* menu_item = NULL;
static GSList* languages = NULL;	/* handled languages */

/********************** Functions for the feature *********************/

static void
fill_default_languages_list();

static void
menu_item_activate(guint key_id);

static void
on_configure_add_language(GtkWidget* widget, gpointer data);

static void
on_configure_remove_language(GtkWidget* widget, gpointer data);

static void
on_configure_cell_edited(GtkCellRendererText* text, gchar* arg1, gchar* arg2, gpointer data);

/* ---------------------------------------------------------------------
 *  Initialization
 * ---------------------------------------------------------------------
 */
void
switch_head_impl_init()
{
	log_func();

	GtkWidget* edit_menu = ui_lookup_widget(geany->main_widgets->window, "edit1_menu");

	/* Add the menu item and make it sensitive only when a document is opened */
	menu_item = gtk_menu_item_new_with_mnemonic(_("Switch header/implementation"));
	gtk_widget_show(menu_item);
	gtk_container_add(GTK_CONTAINER(edit_menu), menu_item);
	ui_add_document_sensitive(menu_item);

	/* Callback connection */
	g_signal_connect(G_OBJECT(menu_item), "activate", G_CALLBACK(menu_item_activate), NULL);

	/* Initialize the key binding : */
	keybindings_set_item(	plugin_key_group,
 							KEY_ID_SWITCH_HEAD_IMPL,
 							(GeanyKeyCallback)(&menu_item_activate),
 							GDK_s, GDK_MOD1_MASK | GDK_SHIFT_MASK,
 							"switch_head_impl",
 							_("Switch header/implementation"),	/* used in the Preferences dialog */
 							menu_item);

	/* TODO : we should use the languages specified by the user or the default list */
	fill_default_languages_list();
}

/* ---------------------------------------------------------------------
 *  Cleanup
 * ---------------------------------------------------------------------
 */
void
switch_head_impl_cleanup()
{
	GSList* iter = NULL;

	log_func();

	gtk_widget_destroy(menu_item);

	for(iter = languages ; iter != NULL ; iter = iter->next)
	{
		Language* lang = (Language*)(iter->data);

		g_slist_foreach(lang->head_extensions, (GFunc)(&g_free), NULL);	/* free the data */
		g_slist_free(lang->head_extensions);	/* free the list */

		g_slist_foreach(lang->impl_extensions, (GFunc)(&g_free), NULL);
		g_slist_free(lang->impl_extensions);
	}

	g_slist_free(languages);
}


/* ---------------------------------------------------------------------
 *  Initialize the "languages" list to the default known languages
 * ---------------------------------------------------------------------
 */
static void
fill_default_languages_list()
{
	Language* lang = g_malloc0(sizeof(Language));

	languages = NULL;

#define HEAD_PREPEND(str_ext) \
	{ lang->head_extensions = g_slist_prepend(lang->head_extensions, g_strdup(str_ext)); }
#define IMPL_PREPEND(str_ext) \
	{ lang->impl_extensions = g_slist_prepend(lang->impl_extensions, g_strdup(str_ext)); }

	/* C/C++ */
	lang = g_malloc0(sizeof(Language));
	lang->name = "c_cpp";

	HEAD_PREPEND("h");
	HEAD_PREPEND("hpp");
	HEAD_PREPEND("hxx");
	HEAD_PREPEND("h++");
	HEAD_PREPEND("hh");
	lang->head_extensions = g_slist_reverse(lang->head_extensions);

	IMPL_PREPEND("cpp");
	IMPL_PREPEND("cxx");
	IMPL_PREPEND("c++");
	IMPL_PREPEND("cc");
	IMPL_PREPEND("C");
	IMPL_PREPEND("c");
	lang->impl_extensions = g_slist_reverse(lang->impl_extensions);

	languages = g_slist_prepend(languages, lang);

	/* GLSL */
	lang = g_malloc0(sizeof(Language));
	lang->name = "glsl";

	HEAD_PREPEND("vert");
	lang->head_extensions = g_slist_reverse(lang->head_extensions);

	IMPL_PREPEND("frag");
	lang->impl_extensions = g_slist_reverse(lang->impl_extensions);

	languages = g_slist_prepend(languages, lang);

	/* Ada */
	lang = g_malloc0(sizeof(Language));
	lang->name = "ada";

	HEAD_PREPEND("ads");
	lang->head_extensions = g_slist_reverse(lang->head_extensions);

	IMPL_PREPEND("adb");
	lang->impl_extensions = g_slist_reverse(lang->impl_extensions);

	languages = g_slist_prepend(languages, lang);

	/* Done : */
	languages = g_slist_reverse(languages);

#undef HEAD_PREPEND
#undef IMPL_PREPEND
}

/* ---------------------------------------------------------------------
 *  Callback when the menu item is clicked.
 * ---------------------------------------------------------------------
 */
static void
menu_item_activate(guint key_id)
{
	GeanyDocument* current_doc = document_get_current();
	GeanyDocument* new_doc = NULL;
	guint nb_documents = geany->documents_array->len;

	gchar* extension = NULL;	/* e.g. : "hpp" */

	GSList* p_extensions_to_test = NULL;	/* e.g. : ["cpp", "cxx", ...] */

	GSList* filenames_to_test = NULL;	/* e.g. : ["f.cpp", "f.cxx", ...] */

	GSList* iter_lang = NULL;
	GSList* iter_ext = NULL;
	GSList* iter_filename = NULL;
	gint i=0;

	gchar* dirname = NULL;
	gchar* basename = NULL;
	gchar* basename_no_extension = NULL;

	gchar* p_str = NULL;	/* Local variables, used as temporary buffers */
	gchar* p_str2 = NULL;

	log_func();
	log_debug("current_doc->file_name == %s", current_doc->file_name);
	log_debug("geany->documents_array->len == %d", geany->documents_array->len);

	if(current_doc != NULL && current_doc->file_name != NULL && current_doc->file_name[0] != '\0')
	{
		/* Get the basename, e.g. : "/home/me/file.cpp" -> "file.cpp" */
		basename = g_path_get_basename(current_doc->file_name);

		if(g_utf8_strlen(basename, -1) < 2)
			goto free_mem;

		log_debug("basename == %s", basename);

		/* Get the extension , e.g. : "cpp" */
		extension = get_extension(basename);

		if(extension == NULL || g_utf8_strlen(extension, -1) == 0)
			goto free_mem;

		log_debug("extension == %s", extension);

		/* Get the basename without any extension */
		basename_no_extension = copy_and_remove_extension(basename);
		if(basename_no_extension == NULL || g_utf8_strlen(basename_no_extension, -1) == 0)
			goto free_mem;

		/* Identify the language and whether the file is a header or an implementation. */
		/* For each recognized language : */
		for(iter_lang = languages ; iter_lang != NULL ; iter_lang = iter_lang->next)
		{
			Language* lang = (Language*)(iter_lang->data);

			/* Test the headers : */
			if(g_slist_find_custom(lang->head_extensions, extension, (GCompareFunc)(&compare_strings)) != NULL)
			{
				p_extensions_to_test = lang->impl_extensions;
				break;
			}

			/* Test the implementations : */
			else if(g_slist_find_custom(lang->impl_extensions, extension, (GCompareFunc)(&compare_strings)) != NULL)
			{
				p_extensions_to_test = lang->head_extensions;
				break;
			}
		}

		if(p_extensions_to_test == NULL)
			goto free_mem;

#ifdef CODE_NAVIGATION_DEBUG
		log_debug("extension known !");
		log_debug("p_extensions_to_test : ");
		g_slist_foreach(p_extensions_to_test, (GFunc)(&log_debug), NULL);
#endif

		/* Build a list of filenames to test : */
		filenames_to_test = NULL;
		for(iter_ext = p_extensions_to_test ; iter_ext != NULL ; iter_ext = iter_ext->next)
		{
			p_str = g_strdup_printf("%s.%s", basename_no_extension, (const gchar*)(iter_ext->data));
			filenames_to_test = g_slist_prepend(filenames_to_test, p_str);
		}

		filenames_to_test = g_slist_reverse(filenames_to_test);

#ifdef CODE_NAVIGATION_DEBUG
		log_debug("filenames to test :");
		g_slist_foreach(filenames_to_test, (GFunc)(&log_debug), NULL);
#endif

		/* First : look for a corresponding file in the opened files.
		 * If found, open it. */
		for(i=0 ; i < nb_documents ; i++)
		{
			new_doc = document_index(i);

			for(iter_filename = filenames_to_test ; iter_filename != NULL ; iter_filename = iter_filename->next)
			{
				p_str = g_path_get_basename(new_doc->file_name);

				log_debug("comparing \"%s\" and \"%s\"", (const gchar*)(iter_filename->data), p_str);
				if(utils_str_equal((const gchar*)(iter_filename->data), p_str))
				{
					log_debug("FOUND !");
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

		log_debug("dirname == \"%s\"", dirname);

		/* -> try all the extensions we should test */
		for(iter_ext = p_extensions_to_test ; iter_ext != NULL ; iter_ext = iter_ext->next)
		{
			p_str = g_strdup_printf(	"%s" G_DIR_SEPARATOR_S "%s.%s",
										dirname, basename_no_extension, (const gchar*)(iter_ext->data));

			p_str2 = g_locale_from_utf8(p_str, -1, NULL, NULL, NULL);
			g_free(p_str);

			log_debug("trying to open the file \"%s\"\n", p_str2);

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
														_("%s not found, create it?"), p_str);
			gtk_window_set_title(GTK_WINDOW(dialog), "Geany");
			if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
			{
				p_str2 = g_strdup_printf(	"%s" G_DIR_SEPARATOR_S "%s", dirname, p_str);

				document_new_file(p_str2, current_doc->file_type, NULL);
				document_set_text_changed(document_get_current(), TRUE);

				g_free(p_str2);
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

/* ---------------------------------------------------------------------
 *  Configuration widget
 * ---------------------------------------------------------------------
 */

/* ----- Utility function to concatenate the extensions of a -----
 *       language, separated with a comma. */
static gchar*
concatenate_extensions(GSList* extensions)
{
	GSList* iter_ext;
	gchar* p_str = NULL;
	gchar* temp = NULL;

	for(iter_ext = extensions ; iter_ext != NULL ; iter_ext = iter_ext->next)
	{
		temp = p_str;
		p_str = g_strjoin(",", (const gchar*)(iter_ext->data), p_str, NULL);
		g_free(temp);
	}

	return p_str;
}

/* ----- Utility function to add a language to a GtkListStore ----- */
static void
add_language(GtkListStore* list_store, Language* lang)
{
	gchar* p_str = NULL;
	GtkTreeIter tree_iter;

	if(lang->head_extensions == NULL || lang->impl_extensions == NULL)
		return;

	/* Append an empty row */
	gtk_list_store_append(list_store, &tree_iter);

	/* Header extensions */
	p_str = concatenate_extensions(lang->head_extensions);
	gtk_list_store_set(list_store, &tree_iter, COLUMN_HEAD, p_str, -1);
	g_free(p_str);

	/* Implementation extensions */
	p_str = concatenate_extensions(lang->impl_extensions);
	gtk_list_store_set(list_store, &tree_iter, COLUMN_IMPL, p_str, -1);
	g_free(p_str);
}

/* ----- Finally, the configuration widget ----- */
GtkWidget*
switch_head_impl_config_widget()
{
	GtkWidget *frame, *vbox, *tree_view;
	GtkWidget *hbox_buttons, *add_button, *remove_button;
	GtkListStore *list_store;
	GtkTreeViewColumn *column;
	GtkCellRenderer *cell_renderer;

	GSList *iter_lang;

	log_func();

	/* Frame, which is the returned widget */
	frame = gtk_frame_new(_("Switch header/implementation"));

	/* Main VBox */
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame), vbox);

	/* ======= Extensions list ======= */

	/* Add a list containing the extensions for each language (headers / implementations) */
	/* - create the GtkListStore */
	list_store = gtk_list_store_new(NB_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);

	/* - fill the GtkListStore with the extensions of the languages */
	for(iter_lang = languages ; iter_lang != NULL ; iter_lang = iter_lang->next)
		add_language(list_store, (Language*)(iter_lang->data));

	/* - create the GtkTreeView */
	tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));

	/* - add the columns */
	/* -> headers : */
	cell_renderer = gtk_cell_renderer_text_new();
	g_object_set(G_OBJECT(cell_renderer), "editable", TRUE, NULL);
	g_signal_connect(G_OBJECT(cell_renderer), "edited", G_CALLBACK(on_configure_cell_edited), GINT_TO_POINTER(COLUMN_HEAD));
	column = gtk_tree_view_column_new_with_attributes(	_("Headers extensions"), cell_renderer,
														"text", COLUMN_HEAD,
														NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);

	/* -> implementations : */
	cell_renderer = gtk_cell_renderer_text_new();
	g_object_set(G_OBJECT(cell_renderer), "editable", TRUE, NULL);
	g_signal_connect(G_OBJECT(cell_renderer), "edited", G_CALLBACK(on_configure_cell_edited), GINT_TO_POINTER(COLUMN_IMPL));
	column = gtk_tree_view_column_new_with_attributes(	_("Implementations extensions"), cell_renderer,
														"text", COLUMN_IMPL,
														NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);


	/* - finally add the GtkTreeView to the frame's vbox */
	gtk_box_pack_start(GTK_BOX(vbox), tree_view, TRUE, TRUE, 6);


	/* ========= Buttons ======== */

	/* HBox */
	hbox_buttons = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox_buttons, FALSE, FALSE, 0);

	/* Add the "add" button to the frame's hbox */
	add_button = gtk_button_new_from_stock(GTK_STOCK_ADD);
	g_signal_connect(G_OBJECT(add_button), "clicked", G_CALLBACK(on_configure_add_language), tree_view);
	gtk_box_pack_start(GTK_BOX(hbox_buttons), add_button, FALSE, FALSE, 0);

	/* Add the "remove" button to the frame's hbox */
	remove_button = gtk_button_new_from_stock(GTK_STOCK_REMOVE);
	gtk_widget_set_sensitive(remove_button, FALSE);	/* TODO ! */
	g_signal_connect(G_OBJECT(remove_button), "clicked", G_CALLBACK(on_configure_remove_language), tree_view);
	gtk_box_pack_start(GTK_BOX(hbox_buttons), remove_button, FALSE, FALSE, 0);

	return frame;
}

/* ---------------------------------------------------------------------
 * Callback for adding a language in the configuration dialog
 * ---------------------------------------------------------------------
 */
static void
on_configure_add_language(GtkWidget* widget, gpointer data)
{
	GtkWidget* tree_view = (GtkWidget*)data;
	GtkListStore *list_store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(tree_view)));
	GtkTreeIter tree_iter;
	GtkTreePath *path;
	GtkTreeViewColumn* column = NULL;
	gint nb_lines;

	/* Add a line */
	gtk_list_store_append(list_store, &tree_iter);

	/* and give the focus to it */
	nb_lines = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(list_store), NULL);

	path = gtk_tree_path_new_from_indices(nb_lines-1, -1);

	column = gtk_tree_view_get_column(GTK_TREE_VIEW(tree_view), 0);

	/* TODO : why isn't the cell being edited, although we say "TRUE" as last parameter ?? */
	gtk_tree_view_set_cursor(GTK_TREE_VIEW(tree_view), path, column, TRUE);
	gtk_widget_grab_focus(tree_view);

	gtk_tree_path_free(path);
}

/* ---------------------------------------------------------------------
 * Callback for removing a language in the configuration dialog
 * ---------------------------------------------------------------------
 */
static void
on_configure_remove_language(GtkWidget* widget, gpointer data)
{
	/* TODO ! */
}

/* ---------------------------------------------------------------------
 * Callback called when a cell has been edited in the configuration dialog
 * ---------------------------------------------------------------------
 */
static void
on_configure_cell_edited(GtkCellRendererText* text, gchar* arg1, gchar* arg2, gpointer data)
{
	/* TODO !! */
	Column col = (Column)(GPOINTER_TO_INT(data));
	log_debug("arg1 == %s, arg2 == %s\n", arg1, arg2);
}

/* ---------------------------------------------------------------------
 * Write the configuration of the feature
 * ---------------------------------------------------------------------
 */
void
write_switch_head_impl_config(GKeyFile* key_file)
{
	/* TODO ! */
	/* This is old code which needs to be updated */

/*	lang_names = g_malloc(nb_languages * sizeof(gchar*));
	for(lang_iterator = languages_extensions, i=0 ;
		lang_iterator != NULL ;
		lang_iterator = lang_iterator->next, i++)
	{
		lang_names[i] = ((const LanguageExtensions*)(lang_iterator->data))->language_name;
	}

	g_key_file_set_string_list(key_file, "switching", "languages", lang_names, nb_languages);

	g_free(lang_names);
*/
}
