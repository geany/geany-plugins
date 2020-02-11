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
	KB_COUNT
};


static GtkWidget *s_fif_item, *s_ff_item, *s_ft_item, *s_shs_item, *s_sep_item, *s_context_osf_item, *s_context_sep_item;

/* GTK compatibility functions/macros */

#if ! GTK_CHECK_VERSION (2, 18, 0)
# define gtk_widget_get_visible(w) \
  (GTK_WIDGET_VISIBLE (w))
# define gtk_widget_set_can_focus(w, v)               \
  G_STMT_START {                                      \
    GtkWidget *widget = (w);                          \
    if (v) {                                          \
      GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_FOCUS);   \
    } else {                                          \
      GTK_WIDGET_UNSET_FLAGS (widget, GTK_CAN_FOCUS); \
    }                                                 \
  } G_STMT_END
#endif

#if ! GTK_CHECK_VERSION (2, 21, 8)
# define GDK_KEY_Down       GDK_Down
# define GDK_KEY_Escape     GDK_Escape
# define GDK_KEY_ISO_Enter  GDK_ISO_Enter
# define GDK_KEY_KP_Enter   GDK_KP_Enter
# define GDK_KEY_Page_Down  GDK_Page_Down
# define GDK_KEY_Page_Up    GDK_Page_Up
# define GDK_KEY_Return     GDK_Return
# define GDK_KEY_Tab        GDK_Tab
# define GDK_KEY_Up         GDK_Up
#endif

struct {
  GtkWidget *scroll;
  GtkWidget    *panel;
  GtkWidget    *entry;
  GtkWidget    *view;
  GtkListStore *store;
  GtkTreeModel *filter;
} kb_find_file = {
  NULL, NULL,
  NULL, NULL,
  NULL, NULL
};
static gboolean try_swap_header_source(gchar *utf8_file_name, gboolean is_header, GSList *file_list, GSList *header_patterns, GSList *source_patterns)
{
	gchar *name_pattern;
	GSList *elem = NULL;
	GPatternSpec *pattern;
	gboolean found = FALSE;

	name_pattern = g_path_get_basename(utf8_file_name);
	SETPTR(name_pattern, utils_remove_ext_from_filename(name_pattern));
	SETPTR(name_pattern, g_strconcat(name_pattern, ".*", NULL));
	pattern = g_pattern_spec_new(name_pattern);
	g_free(name_pattern);

	foreach_slist (elem, file_list)
	{
		gchar *full_name = elem->data;
		gchar *base_name = g_path_get_basename(full_name);

		if (g_pattern_match_string(pattern, base_name) &&
		    prjorg_project_is_in_project(full_name))
		{
			if ((is_header && patterns_match(source_patterns, base_name)) ||
				(!is_header && patterns_match(header_patterns, base_name)))
			{
				open_file(full_name);
				found = TRUE;
				g_free(base_name);
				break;
			}
		}
		g_free(base_name);
	}

	g_pattern_spec_free(pattern);
	return found;
}

static void on_swap_header_source(G_GNUC_UNUSED GtkMenuItem * menuitem, G_GNUC_UNUSED gpointer user_data)
{
	GSList *header_patterns, *source_patterns;
	GeanyDocument *doc;
	gboolean known_type = TRUE;
	gboolean is_header;
	gchar *doc_basename;
	doc = document_get_current();

	if (!prj_org || !geany_data->app->project || !doc || !doc->file_name)
		return;

	header_patterns = get_precompiled_patterns(prj_org->header_patterns);
	source_patterns = get_precompiled_patterns(prj_org->source_patterns);

	doc_basename = g_path_get_basename(doc->file_name);

	if (patterns_match(header_patterns, doc_basename))
		is_header = TRUE;
	else if (patterns_match(source_patterns, doc_basename))
		is_header = FALSE;
	else
		known_type = FALSE;

	if (known_type)
	{
		gboolean swapped;
		GSList *elem = NULL, *list = NULL;
		guint i = 0;

		foreach_document(i)
		{
			gchar *filename;

			filename = document_index(i)->file_name;
			if (prjorg_project_is_in_project(filename))
				list = g_slist_prepend(list, filename);
		}
		swapped = try_swap_header_source(doc->file_name, is_header, list, header_patterns, source_patterns);
		g_slist_free(list);
		list = NULL;

		if (!swapped)
		{
			gchar *utf8_doc_dir;
			gchar *locale_doc_dir;

			utf8_doc_dir = g_path_get_dirname(doc->file_name);
			locale_doc_dir = utils_get_locale_from_utf8(utf8_doc_dir);

			list = utils_get_file_list(locale_doc_dir, NULL, NULL);
			foreach_list (elem, list)
			{
				gchar *full_name;

				full_name = g_build_filename(locale_doc_dir, elem->data, NULL);
				SETPTR(full_name, utils_get_utf8_from_locale(full_name));
				SETPTR(elem->data, full_name);
			}
			swapped = try_swap_header_source(doc->file_name, is_header, list, header_patterns, source_patterns);
			g_slist_foreach(list, (GFunc) g_free, NULL);
			g_slist_free(list);
			g_free(utf8_doc_dir);
			g_free(locale_doc_dir);
			list = NULL;
		}

		if (!swapped)
		{
			foreach_slist(elem, prj_org->roots)
			{
				GHashTableIter iter;
				gpointer key, value;
				PrjOrgRoot *root = elem->data;

				list = NULL;
				g_hash_table_iter_init(&iter, root->file_table);
				while (g_hash_table_iter_next(&iter, &key, &value))
					list = g_slist_prepend(list, key);
				swapped = try_swap_header_source(doc->file_name, is_header, list, header_patterns, source_patterns);
				g_slist_free(list);
				if (swapped)
					break;
			}
		}
	}

	g_free(doc_basename);

	g_slist_foreach(header_patterns, (GFunc) g_pattern_spec_free, NULL);
	g_slist_free(header_patterns);
	g_slist_foreach(source_patterns, (GFunc) g_pattern_spec_free, NULL);
	g_slist_free(source_patterns);
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
static void
on_view_row_activated (GtkTreeView       *view,
                       GtkTreePath       *path,
                       GtkTreeViewColumn *column,
                       gpointer           dummy)
{
  GtkTreeModel *model = gtk_tree_view_get_model (view);
  GtkTreeIter   iter;
  gchar *utf8_path;
  
  if (gtk_tree_model_get_iter (model, &iter, path)) {
    gtk_tree_model_get (model, &iter, COL_FILE_NAME, &utf8_path, -1);
    open_file(utf8_path);
    gtk_widget_hide (kb_find_file.panel);
  }
}
static void
tree_view_set_cursor_from_iter (GtkTreeView *view,
                                GtkTreeIter *iter)
{
  GtkTreePath *path;
  
  path = gtk_tree_model_get_path (gtk_tree_view_get_model (view), iter);
  gtk_tree_view_set_cursor (view, path, NULL, FALSE);
  gtk_tree_path_free (path);
}

static void
tree_view_move_focus (GtkTreeView    *view,
                      GtkMovementStep step,
                      gint            amount)
{
  GtkTreeIter   iter;
  GtkTreePath  *path;
  GtkTreeModel *model = gtk_tree_view_get_model (view);
  gboolean      valid = FALSE;
  
  gtk_tree_view_get_cursor (view, &path, NULL);
  if (! path) {
    valid = gtk_tree_model_get_iter_first (model, &iter);
  } else {
    switch (step) {
      case GTK_MOVEMENT_BUFFER_ENDS:
        valid = gtk_tree_model_get_iter_first (model, &iter);
        if (valid && amount > 0) {
          GtkTreeIter prev;
          
          do {
            prev = iter;
          } while (gtk_tree_model_iter_next (model, &iter));
          iter = prev;
        }
        break;
      
        /* FIXME: move by page */
      case GTK_MOVEMENT_DISPLAY_LINES:
        gtk_tree_model_get_iter (model, &iter, path);
        if (amount > 0) {
          while ((valid = gtk_tree_model_iter_next (model, &iter)) &&
                 --amount > 0)
            ;
        } else if (amount < 0) {
          while ((valid = gtk_tree_path_prev (path)) && --amount > 0)
            ;
          
          if (valid) {
            gtk_tree_model_get_iter (model, &iter, path);
          }
        }
        break;
      
      default:
        g_assert_not_reached ();
    }
    gtk_tree_path_free (path);
  }
  
  if (valid) {
    tree_view_set_cursor_from_iter (view, &iter);
  } else {
    gtk_widget_error_bell (GTK_WIDGET (view));
  }
}

static void
tree_view_activate_focused_row (GtkTreeView *view)
{
  GtkTreePath        *path;
  GtkTreeViewColumn  *column;
  
  gtk_tree_view_get_cursor (view, &path, &column);
  if (path) {
    gtk_tree_view_row_activated (view, path, column);
    gtk_tree_path_free (path);
  }
}
static gboolean
on_panel_key_press_event (GtkWidget    *widget,
                          GdkEventKey  *event,
                          gpointer      dummy)
{
  switch (event->keyval) {
    case GDK_KEY_Escape:
      gtk_widget_hide (widget);
      return TRUE;
    
    case GDK_KEY_Tab:
      /* avoid leaving the entry */
      return TRUE;
    
    case GDK_KEY_Return:
    case GDK_KEY_KP_Enter:
    case GDK_KEY_ISO_Enter:
      tree_view_activate_focused_row (GTK_TREE_VIEW (kb_find_file.view));
      return TRUE;
    
    case GDK_KEY_Up:
    case GDK_KEY_Down: {
      tree_view_move_focus (GTK_TREE_VIEW (kb_find_file.view),
                            GTK_MOVEMENT_DISPLAY_LINES,
                            event->keyval == GDK_KEY_Up ? -1 : 1);
      return TRUE;
    }
  }
  
  return FALSE;
}
static void
on_panel_show (GtkWidget *widget,
               gpointer   dummy)
{
  gtk_widget_grab_focus (GTK_ENTRY(kb_find_file.entry));
}
static void
on_panel_hide (GtkWidget *widget,
               gpointer   dummy)
{

  GtkTreeView  *view = GTK_TREE_VIEW (kb_find_file.view);
  gtk_list_store_clear (kb_find_file.store);
}
static gboolean
visible_func (GtkTreeModel *model,
              GtkTreeIter  *iter,
              gpointer      data)
{
  gchar *paths;
  gtk_tree_model_get (model, iter, COL_FILE_NAME, &paths, -1);
  gchar  *haystack  = g_utf8_casefold (paths, -1);
  gchar  *needle   = g_utf8_casefold (data, -1);
  gboolean score = TRUE;

  if (data == NULL || haystack == NULL ||
        *needle == '\0' || *haystack == '\0') {
        return score;
    }
  score = (strstr(haystack, needle) == NULL)?FALSE:TRUE;
  g_free (needle);
  g_free (haystack);
  g_free(paths);
  return score;
}
static void
on_entry_activate (GtkEntry  *entry,
                   gpointer   dummy)
{
  tree_view_activate_focused_row (GTK_TREE_VIEW (kb_find_file.view));
}
static void
on_entry_text_notify (GObject    *object,
                      GParamSpec *pspec,
                      gpointer    dummy)
{
   guint16 key_length;
  const gchar  *key   = gtk_entry_get_text (GTK_ENTRY (kb_find_file.entry));
  key_length = gtk_entry_get_text_length(GTK_ENTRY (kb_find_file.entry));
  if (key_length > 2)
  {
    GtkTreeIter   iter;
    GtkTreeView  *view  = GTK_TREE_VIEW (kb_find_file.view);
    GtkTreeModel *model = gtk_tree_view_get_model (view);
    gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(model));
    gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(model), visible_func, key, NULL);
    if (gtk_tree_model_get_iter_first (model, &iter)) {
      tree_view_set_cursor_from_iter (view, &iter);
    }
  }
  else
  {
    if (key_length == 0){
      on_panel_hide(NULL, NULL);
      // gtk_window_resize(GTK_WINDOW(kb_find_file.panel), 100, 30);
      // gtk_widget_hide(kb_find_file.scroll);
    }
    else if(key_length > 1){
      
    GtkTreeViewColumn  *col;
    GtkCellRenderer    *cell;
    gchar *pattern_str;
    GPatternSpec *pattern;
    pattern_str = g_strconcat("*", key, "*", NULL);
    SETPTR(pattern_str, g_utf8_strdown(pattern_str, -1));
    gtk_window_resize(GTK_WINDOW(kb_find_file.panel), 100, 150);
    pattern = g_pattern_spec_new(pattern_str);
    prjorg_kb_find_file_in_active(pattern, kb_find_file.store);
    kb_find_file.filter = gtk_tree_model_filter_new(GTK_TREE_MODEL (kb_find_file.store), NULL);
    kb_find_file.view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (kb_find_file.filter));
    gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(kb_find_file.filter), visible_func, key, NULL);
    g_signal_connect (kb_find_file.view, "row-activated",
                G_CALLBACK (on_view_row_activated), NULL);
    gtk_widget_set_can_focus (kb_find_file.view, FALSE);
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (kb_find_file.view), FALSE);
    cell = gtk_cell_renderer_text_new ();
    g_object_set (cell, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    col = gtk_tree_view_column_new_with_attributes (NULL, cell,
                                              "markup", COL_FILE_LABEL,
                                              NULL);
    gtk_tree_view_append_column (kb_find_file.view, col);
    gtk_container_add (GTK_CONTAINER (kb_find_file.scroll), kb_find_file.view);
    gtk_widget_show_all(kb_find_file.scroll);
    g_pattern_spec_free(pattern); 
    g_free(pattern_str);
    }
  }
}
static void
fill_store (GtkListStore *store)
{
	//gchar *root_path = get_project_base_path();
	GSList *elem = NULL;
	foreach_slist (elem, prj_org->roots)
	{
		GHashTableIter iter;
		gpointer key, value;
		PrjOrgRoot *root = elem->data;
		g_hash_table_iter_init(&iter, root->file_table);
		while (g_hash_table_iter_next(&iter, &key, &value))
		{
			gchar *name = g_path_get_basename(key);
			gchar *label = g_markup_printf_escaped ("<big>%s</big>\n"
                                            "<small><i>%s</i></small>",
                                            name,
                                           key);

    
	gtk_list_store_insert_with_values (store, NULL, -1,
                                       COL_FILE_LABEL, label,
                                       COL_FILE_NAME, key,
                                       -1);
		g_free (label);
		g_free (name);
		}

}
}

static void
create_panel (void)
{
  GtkWidget          *frame;
  GtkWidget          *box;
  kb_find_file.panel = g_object_new (GTK_TYPE_WINDOW,
                                    "decorated", FALSE,
                                    "default-width", 100,
                                    "default-height", 30,
                                    "transient-for", geany_data->main_widgets->window,
                                    "window-position", GTK_WIN_POS_CENTER_ON_PARENT,
                                    "type-hint", GDK_WINDOW_TYPE_HINT_DIALOG,
                                    "skip-taskbar-hint", TRUE,
                                    "skip-pager-hint", TRUE,
                                    NULL);
  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  g_signal_connect (kb_find_file.panel, "focus-out-event",
                    G_CALLBACK (gtk_widget_hide), NULL);
  g_signal_connect (kb_find_file.panel, "show",
                    G_CALLBACK (on_panel_show), NULL);
  g_signal_connect (kb_find_file.panel, "hide",
                    G_CALLBACK (on_panel_hide), NULL);
  g_signal_connect (kb_find_file.panel, "key-press-event",
                    G_CALLBACK (on_panel_key_press_event), NULL);

  gtk_container_add (GTK_CONTAINER (kb_find_file.panel), frame);
  kb_find_file.store = gtk_list_store_new (COL_FILE_COUNT,
                                          G_TYPE_STRING,
                                          G_TYPE_STRING);
  fill_store(kb_find_file.store);
  box = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frame), box);
  kb_find_file.entry = gtk_entry_new();
  gtk_box_pack_start (box, kb_find_file.entry, FALSE, TRUE, 0);
  gtk_entry_set_width_chars(GTK_ENTRY(kb_find_file.entry), 40);
  ui_entry_add_clear_icon(GTK_ENTRY(kb_find_file.entry));
  gtk_entry_set_activates_default(GTK_ENTRY(kb_find_file.entry), TRUE);

  g_signal_connect (kb_find_file.entry, "notify::text",
                    G_CALLBACK (on_entry_text_notify), NULL);
  // g_signal_connect (kb_find_file.entry, "preedit-changed",
                    // G_CALLBACK (on_entry_preedit_notify), NULL);
    g_signal_connect (kb_find_file.entry, "activate",
                    G_CALLBACK (on_entry_activate), NULL);
   kb_find_file.scroll = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                         "hscrollbar-policy", GTK_POLICY_AUTOMATIC,
                         "vscrollbar-policy", GTK_POLICY_AUTOMATIC,
                         NULL);
   gtk_box_pack_start ( GTK_BOX(box), kb_find_file.scroll, TRUE, TRUE, 0);  
        gtk_widget_show_all (frame);
        gtk_widget_hide(kb_find_file.scroll);
        
}
static void on_find_file(G_GNUC_UNUSED GtkMenuItem * menuitem, G_GNUC_UNUSED gpointer user_data)
{
	if (geany_data->app->project){
                create_panel();
                gtk_widget_show(kb_find_file.panel);
    
  }
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

	s_sep_item = gtk_separator_menu_item_new();
	gtk_widget_show(s_sep_item);
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->project_menu), s_sep_item);

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

    keybindings_set_item(key_group, KB_FOCUS_SIDEBAR, (GeanyKeyCallback)prjorg_sidebar_focus_project_tab,
		0, 0, "focus_project_sidebar", _("Focus Project Sidebar"), NULL);

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
	gtk_widget_set_sensitive(s_shs_item, activate);
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
	gtk_widget_destroy(s_sep_item);

	gtk_widget_destroy(s_context_osf_item);
	gtk_widget_destroy(s_context_sep_item);
}
