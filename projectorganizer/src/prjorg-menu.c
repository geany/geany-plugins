/*
 * Copyright 2010 Jiri Techet <techet@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <sys/time.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gstdio.h>

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif
#include <geanyplugin.h>

#include "prjorg-menu.h"
#include "prjorg-project.h"
#include "prjorg-utils.h"
#include "prjorg-sidebar.h"
#include "prjorg-goto-anywhere.h"

#include <string.h>

extern GeanyPlugin *geany_plugin;
extern GeanyData *geany_data;


enum
{
	KB_SWAP_HEADER_SOURCE,
	KB_FIND_IN_PROJECT,
	KB_FIND_FILE,
	KB_FIND_TAG,
	KB_FOCUS_SIDEBAR,
	KB_OPEN_FILE_MANAGER,
	KB_OPEN_TERMINAL,
	KB_GOTO_ANYWHERE,
	KB_GOTO_DOC_SYMBOL,
	KB_GOTO_WORKSPACE_SYMBOL,
	KB_GOTO_LINE,
	KB_COUNT
};


static GtkWidget *s_fif_item, *s_ff_item, *s_ft_item, *s_shs_item, *s_sep_item1,
	*s_context_osf_item, *s_context_sep_item,
	*s_sep_item2, *s_goto_any_item, *s_goto_doc_item, *s_goto_wks_item, *s_goto_line_item;


static void on_swap_header_source(G_GNUC_UNUSED GtkMenuItem * menuitem, G_GNUC_UNUSED gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	gchar *full_name = find_header_source(doc);
	open_file(full_name);
	g_free(full_name);
}


static void on_find_in_project(G_GNUC_UNUSED GtkMenuItem * menuitem, G_GNUC_UNUSED gpointer user_data)
{
	if (geany_data->app->project)
	{
		gchar *utf8_base_path = get_project_base_path();

		search_show_find_in_files_dialog(utf8_base_path);
		g_free(utf8_base_path);
	}
}


static void on_find_file(G_GNUC_UNUSED GtkMenuItem * menuitem, G_GNUC_UNUSED gpointer user_data)
{
	if (geany_data->app->project)
		prjorg_sidebar_find_file_in_active();
}


static void on_find_tag(G_GNUC_UNUSED GtkMenuItem * menuitem, G_GNUC_UNUSED gpointer user_data)
{
	if (geany_data->app->project)
		prjorg_sidebar_find_tag_in_active();
}


static gboolean kb_callback(guint key_id)
{
	switch (key_id)
	{
		case KB_SWAP_HEADER_SOURCE:
			on_swap_header_source(NULL, NULL);
			return TRUE;
		case KB_FIND_IN_PROJECT:
			on_find_in_project(NULL, NULL);
			return TRUE;
		case KB_FIND_FILE:
			on_find_file(NULL, NULL);
			return TRUE;
		case KB_FIND_TAG:
			on_find_tag(NULL, NULL);
			return TRUE;
	}
	return FALSE;
}


static void on_open_selected_file(GtkMenuItem *menuitem, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	gchar *utf8_sel, *locale_sel;
	gchar *filename = NULL;  /* locale */

	g_return_if_fail(doc != NULL);

	utf8_sel = get_selection();

	if (!utf8_sel)
		return;

	locale_sel = utils_get_locale_from_utf8(utf8_sel);

	if (g_path_is_absolute(locale_sel))
	{
		filename = g_strdup(locale_sel);
		if (!g_file_test(filename, G_FILE_TEST_EXISTS))
		{
			g_free(filename);
			filename = NULL;
		}
	}

	if (!filename)
	{
		gchar *locale_path = NULL;

		if (doc->file_name)
		{
			locale_path = g_path_get_dirname(doc->file_name);
			SETPTR(locale_path, utils_get_locale_from_utf8(locale_path));
		}

		if (!locale_path)
			locale_path = g_get_current_dir();

		filename = g_build_path(G_DIR_SEPARATOR_S, locale_path, locale_sel, NULL);
		if (!g_file_test(filename, G_FILE_TEST_EXISTS))
		{
			g_free(filename);
			filename = NULL;
		}

		g_free(locale_path);
	}

	if (!filename && geany_data->app->project != NULL)
	{
		gchar *utf8_path;
		gchar **pathv;
		gint i;

		utf8_path = g_strdup("");
		pathv = g_strsplit_set(utf8_sel, "/\\", -1);
		for (i = g_strv_length(pathv) - 1; i >= 0; i--)
		{
			if (g_strcmp0(pathv[i], "..") == 0)
				break;
			SETPTR(utf8_path, g_build_filename(G_DIR_SEPARATOR_S, pathv[i], utf8_path, NULL));
		}
		g_strfreev(pathv);

		if (g_strcmp0(utf8_path, "") != 0)
		{
			GSList *elem = NULL;
			const gchar *found_path = NULL;

			foreach_slist (elem, prj_org->roots)
			{
				PrjOrgRoot *root = elem->data;
				gpointer key, value;
				GHashTableIter iter;

				g_hash_table_iter_init(&iter, root->file_table);
				while (g_hash_table_iter_next(&iter, &key, &value))
				{
					gchar *file_name = key;
					gchar *pos = g_strrstr(file_name, utf8_path);

					if (pos && (pos - file_name + strlen(utf8_path) == strlen(file_name)))
					{
						found_path = file_name;
						break;
					}
				}

				if (found_path)
					break;
			}

			if (found_path)
			{
				filename = utils_get_locale_from_utf8(found_path);
				if (!g_file_test(filename, G_FILE_TEST_EXISTS))
				{
					g_free(filename);
					filename = NULL;
				}
			}
		}
		g_free(utf8_path);
	}

#ifdef G_OS_UNIX
	if (!filename)
	{
		filename = g_build_path(G_DIR_SEPARATOR_S, "/usr/local/include", locale_sel, NULL);
		if (!g_file_test(filename, G_FILE_TEST_EXISTS))
		{
			g_free(filename);
			filename = NULL;
		}
	}

	if (!filename)
	{
		filename = g_build_path(G_DIR_SEPARATOR_S, "/usr/include", locale_sel, NULL);
		if (!g_file_test(filename, G_FILE_TEST_EXISTS))
		{
			g_free(filename);
			filename = NULL;
		}
	}
#endif

	if (filename)
	{
		gchar *utf8_filename = utils_get_utf8_from_locale(filename);

		open_file(utf8_filename);
		g_free(utf8_filename);
	}

	g_free(filename);
	g_free(utf8_sel);
	g_free(locale_sel);
}


void prjorg_menu_init(void)
{
	GeanyKeyGroup *key_group = plugin_set_key_group(geany_plugin, "ProjectOrganizer", KB_COUNT, kb_callback);

	s_sep_item1 = gtk_separator_menu_item_new();
	gtk_widget_show(s_sep_item1);
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->project_menu), s_sep_item1);

	s_fif_item = menu_item_new("edit-find", _("Find in Project Files..."));
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->project_menu), s_fif_item);
	g_signal_connect((gpointer) s_fif_item, "activate", G_CALLBACK(on_find_in_project), NULL);
	keybindings_set_item(key_group, KB_FIND_IN_PROJECT, NULL,
		0, 0, "find_in_project", _("Find in project files"), s_fif_item);

	s_ff_item = menu_item_new("edit-find", _("Find Project File..."));
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->project_menu), s_ff_item);
	g_signal_connect((gpointer) s_ff_item, "activate", G_CALLBACK(on_find_file), NULL);
	keybindings_set_item(key_group, KB_FIND_FILE, NULL,
		0, 0, "find_file", _("Find project file"), s_ff_item);

	s_ft_item = menu_item_new("edit-find", _("Find Project Symbol..."));
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->project_menu), s_ft_item);
	g_signal_connect((gpointer) s_ft_item, "activate", G_CALLBACK(on_find_tag), NULL);
	keybindings_set_item(key_group, KB_FIND_TAG, NULL,
		0, 0, "find_tag", _("Find project symbol"), s_ft_item);

	s_shs_item = gtk_menu_item_new_with_mnemonic(_("Swap Header/Source"));
	gtk_widget_show(s_shs_item);
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->project_menu), s_shs_item);
	g_signal_connect((gpointer) s_shs_item, "activate", G_CALLBACK(on_swap_header_source), NULL);
	keybindings_set_item(key_group, KB_SWAP_HEADER_SOURCE, NULL,
		0, 0, "swap_header_source", _("Swap header/source"), s_shs_item);

	s_sep_item2 = gtk_separator_menu_item_new();
	gtk_widget_show(s_sep_item2);
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->project_menu), s_sep_item2);

	s_goto_any_item = gtk_menu_item_new_with_mnemonic(_("Go to _Anywhere..."));
	gtk_widget_show(s_goto_any_item);
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->project_menu), s_goto_any_item);
	g_signal_connect(s_goto_any_item, "activate", G_CALLBACK(prjorg_goto_anywhere_for_file), NULL);
	keybindings_set_item(key_group, KB_GOTO_ANYWHERE, NULL, 0, 0, "goto_anywhere",
		_("Go to anywhere"), s_goto_any_item);

	s_goto_doc_item = gtk_menu_item_new_with_mnemonic(_("Go to _Document Symbol..."));
	gtk_widget_show(s_goto_doc_item);
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->project_menu), s_goto_doc_item);
	g_signal_connect(s_goto_doc_item, "activate", G_CALLBACK(prjorg_goto_anywhere_for_doc), NULL);
	keybindings_set_item(key_group, KB_GOTO_DOC_SYMBOL, NULL, 0, 0, "goto_doc_symbol",
		_("Go to document symbol"), s_goto_doc_item);

	s_goto_wks_item = gtk_menu_item_new_with_mnemonic(_("Go to _Workspace Symbol..."));
	gtk_widget_show(s_goto_wks_item);
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->project_menu), s_goto_wks_item);
	g_signal_connect(s_goto_wks_item, "activate", G_CALLBACK(prjorg_goto_anywhere_for_workspace), NULL);
	keybindings_set_item(key_group, KB_GOTO_WORKSPACE_SYMBOL, NULL, 0, 0, "goto_workspace_symbol",
		_("Go to workspace symbol"), s_goto_wks_item);

	s_goto_line_item = gtk_menu_item_new_with_mnemonic(_("Go to _Line..."));
	gtk_widget_show(s_goto_line_item);
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->project_menu), s_goto_line_item);
	g_signal_connect(s_goto_line_item, "activate", G_CALLBACK(prjorg_goto_anywhere_for_line), NULL);
	keybindings_set_item(key_group, KB_GOTO_LINE, NULL, 0, 0, "goto_line",
		_("Go to line"), s_goto_line_item);

    keybindings_set_item(key_group, KB_FOCUS_SIDEBAR, (GeanyKeyCallback)prjorg_sidebar_focus_project_tab,
		0, 0, "focus_project_sidebar", _("Focus Project Sidebar"), NULL);

    keybindings_set_item(key_group, KB_OPEN_FILE_MANAGER, (GeanyKeyCallback)on_open_file_manager,
		0, 0, "open_file_manager", _("Open File Manager at file location"), NULL);

    keybindings_set_item(key_group, KB_OPEN_TERMINAL, (GeanyKeyCallback)on_open_terminal,
		0, 0, "open_terminal", _("Open Terminal at file location"), NULL);

	s_context_sep_item = gtk_separator_menu_item_new();
	gtk_widget_show(s_context_sep_item);
	gtk_menu_shell_prepend(GTK_MENU_SHELL(geany->main_widgets->editor_menu), s_context_sep_item);

	s_context_osf_item = gtk_menu_item_new_with_mnemonic(_("Open Selected File (Project Organizer)"));
	gtk_widget_show(s_context_osf_item);
	gtk_menu_shell_prepend(GTK_MENU_SHELL(geany->main_widgets->editor_menu), s_context_osf_item);
	g_signal_connect((gpointer) s_context_osf_item, "activate", G_CALLBACK(on_open_selected_file), NULL);

	prjorg_menu_activate_menu_items(FALSE);
}


void prjorg_menu_activate_menu_items(gboolean activate)
{
	gtk_widget_set_sensitive(s_context_osf_item, activate);
	gtk_widget_set_sensitive(s_ff_item, activate);
	gtk_widget_set_sensitive(s_ft_item, activate);
	gtk_widget_set_sensitive(s_fif_item, activate);
}


void prjorg_menu_cleanup(void)
{
	gtk_widget_destroy(s_fif_item);
	gtk_widget_destroy(s_ff_item);
	gtk_widget_destroy(s_ft_item);
	gtk_widget_destroy(s_shs_item);
	gtk_widget_destroy(s_sep_item1);

	gtk_widget_destroy(s_goto_any_item);
	gtk_widget_destroy(s_goto_doc_item);
	gtk_widget_destroy(s_goto_wks_item);
	gtk_widget_destroy(s_goto_line_item);
	gtk_widget_destroy(s_sep_item2);

	gtk_widget_destroy(s_context_osf_item);
	gtk_widget_destroy(s_context_sep_item);
}
