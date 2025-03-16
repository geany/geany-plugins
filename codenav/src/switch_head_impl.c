/*
 *      switch_head_impl.c - this file is part of "codenavigation", which is
 *      part of the "geany-plugins" project.
 *
 *      Copyright 2009 Lionel Fuentes <funto66(at)gmail(dot)com>
 * 		Copyright 2014 Federico Reghenzani <federico(dot)dev(at)reghe(dot)net>
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

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif
#include <geanyplugin.h>

#include "switch_head_impl.h"
#include "utils.h"


/****************************** Useful macro **************************/
#define HEAD_PREPEND(str_ext) \
	{ lang->head_extensions = g_slist_prepend(lang->head_extensions, g_strdup(str_ext)); }
#define IMPL_PREPEND(str_ext) \
	{ lang->impl_extensions = g_slist_prepend(lang->impl_extensions, g_strdup(str_ext)); }


/******************* Global variables for the feature *****************/

static GtkWidget* menu_item = NULL;
static GSList* languages = NULL;	/* handled languages */


/**************************** Prototypes ******************************/
void 
languages_clean(void);

static void
menu_item_activate(guint key_id);

/********************** Functions for the feature *********************/

/**
 * @brief Initialization
 * @param void 
 * @return void
 * 
 */
void
switch_head_impl_init(void)
{
	GtkWidget* edit_menu;
	log_func();

	edit_menu = ui_lookup_widget(geany->main_widgets->window, "edit1_menu");

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

}


/**
 * @brief Default plugin cleanup
 * @param void 
 * @return void
 * 
 */
void
switch_head_impl_cleanup(void)
{
	log_func();

	gtk_widget_destroy(menu_item);
	languages_clean();
}

/**
 * @brief	Free and cleanup all item in languages array, and set languages
 * 			to null
 * @param void 
 * @return void
 * 
 */
void languages_clean(void)
{
	GSList* iter = NULL;

	for(iter = languages ; iter != NULL ; iter = iter->next)
	{
		Language* lang = (Language*)(iter->data);

		g_slist_foreach(lang->head_extensions, (GFunc)(&g_free), NULL);
		g_slist_free(lang->head_extensions);

		g_slist_foreach(lang->impl_extensions, (GFunc)(&g_free), NULL);
		g_slist_free(lang->impl_extensions);
	}

	g_slist_free(languages);

	languages = NULL;
}

/**
 * @brief	Fill the languages variable with passed arguments.
 * @param	impl_list	list of implementation extensions 
 * @param	head_list	list of header extensions
 * @return	void
 * 
 */
void
fill_languages_list(const gchar** impl_list, const gchar** head_list, gsize n)
{
	gchar **splitted_list;
	Language* lang = NULL;
	gsize i;
	guint j;
	
	languages_clean();

	for ( i=0; i<n; i++ ) {
		lang = g_malloc0(sizeof(Language));
		if(!lang)
			return;  // TODO clean resources

		/* check if current item has no head or impl */
		if ( strlen(impl_list[i])==0 || strlen(head_list[i])==0 )
			continue;
		
		/* Set language implementation extensions */
		splitted_list = g_strsplit(impl_list[i], ",", 0);
		if(!splitted_list)
			return;  // TODO clean resources
		for ( j=0; splitted_list[j] != NULL; j++ )
			IMPL_PREPEND(splitted_list[j]);
		g_strfreev(splitted_list);
		
		/* Set language header extensions */
		splitted_list = g_strsplit(head_list[i], ",", 0);
		if(!splitted_list)
			return;  // TODO clean resources
		for ( j=0; splitted_list[j] != NULL; j++ )
			HEAD_PREPEND(splitted_list[j]);
		g_strfreev(splitted_list);
		
		/* add current language to main list */
		languages = g_slist_prepend(languages, lang);
	}
	
	/* reverse the list to match correct order */
	languages = g_slist_reverse(languages);

}

/**
 * @brief	Initialize the "languages" list to the default known languages
 * @param	void
 * @return	void
 * 
 */
void
fill_default_languages_list(void)
{
	Language* lang = NULL;

	languages_clean();

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

}

/**
 * @brief	Callback when the menu item is clicked.
 * @param	key_id	not used
 * @return	void
 * 
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
	guint i=0;

	gchar* dirname = NULL;
	gchar* basename = NULL;
	gchar* basename_no_extension = NULL;

	gchar* p_str = NULL;	/* Local variables, used as temporary buffers */
	gchar* p_str2 = NULL;

	if(current_doc != NULL && current_doc->file_name != NULL && current_doc->file_name[0] != '\0')
	{
		log_func();
		log_debug("current_doc->file_name == %s", current_doc->file_name);
		log_debug("geany->documents_array->len == %d", geany->documents_array->len);

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
			GtkWidget* dialog;

			p_str = g_strdup_printf("%s.%s", basename_no_extension, (const gchar*)(p_extensions_to_test->data));

			dialog = gtk_message_dialog_new(	GTK_WINDOW(geany_data->main_widgets->window),
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

/**
 * @brief	Extern function to get languages.
 * @param	void
 * @return	GSList*	languages list
 * 
 */
GSList* switch_head_impl_get_languages(void)
{
	return languages;
}
