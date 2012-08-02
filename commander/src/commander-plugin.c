/*
 *  
 *  Copyright (C) 2012  Colomban Wendling <ban@herbesfolles.org>
 *  
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *  
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <geanyplugin.h>
#include <geany.h>
#include <document.h>


GeanyPlugin      *geany_plugin;
GeanyData        *geany_data;
GeanyFunctions   *geany_functions;

PLUGIN_VERSION_CHECK(195)

PLUGIN_SET_TRANSLATABLE_INFO (
  LOCALEDIR, GETTEXT_PACKAGE,
  _("Commander"),
  _("Provides a command panel for quick access to actions, files and more"),
  VERSION,
  "Colomban Wendling <ban@herbesfolles.org>"
)


enum {
  KB_SHOW_PANEL,
  KB_COUNT
};

struct {
  GtkWidget    *panel;
  GtkWidget    *entry;
  GtkWidget    *view;
  GtkListStore *store;
  GtkTreeModel *filter;
  GtkTreeModel *sort;
} plugin_data = {
  NULL, NULL, NULL,
  NULL, NULL, NULL
};

typedef enum {
  COL_TYPE_MENU_ITEM,
  COL_TYPE_FILE
} ColType;

enum {
  COL_LABEL,
  COL_PATH,
  COL_TYPE,
  COL_WIDGET,
  COL_DOCUMENT,
  COL_COUNT
};


static gboolean
key_matches (const gchar *key_,
             const gchar *text_)
{
  gchar    *text;
  gchar    *key;
  gchar    *text_p;
  gchar    *key_p;
  gboolean  result;
  
  text_p  = text  = g_utf8_casefold (text_, -1);
  key_p   = key   = g_utf8_casefold (key_, -1);
  
  while (*key_p && *text_p) {
    if (*text_p == *key_p) {
      text_p ++;
      key_p ++;
    } else {
      text_p ++;
    }
  }
  
  result = *key_p == 0;
  
  g_free (text);
  g_free (key);
  
  return result;
}

/* TODO: improve this algorithm for better matches.
 * what's needed is probably not to simply eat a letter when it matches, but
 * rather give it a chance to provide an heavier match... whatever, be smart. */
static gint
key_dist (const gchar *key_,
          const gchar *text_)
{
  gchar  *text  = g_utf8_casefold (text_, -1);
  gchar  *key   = g_utf8_casefold (key_, -1);
  gint    dist  = 0;
  
#if 0
  const gchar  *text_p = text;
  const gchar  *key_p  = key;
  gint          weight = 0;
  
  while (*key_p && *text_p) {
    if (*text_p == *key_p) {
      text_p ++;
      key_p ++;
      weight ++;
      dist -= weight * 2;
    } else {
      text_p ++;
      dist ++;
      weight = 0;
    }
#else /* rather search end to start because end of paths are more interesting */
  const gchar  *text_p = text + strlen (text) - 1;
  const gchar  *key_p  = key  + strlen (key)  - 1;
  gint          weight = 0;
  
  while (key_p >= key && text_p >= text) {
    if (*text_p == *key_p) {
      text_p --;
      key_p --;
      weight ++;
      dist -= weight * 2;
    } else {
      text_p --;
      dist ++;
      weight = 0;
    }
  }
#endif
  
  g_free (text);
  g_free (key);
  
  return dist;
}

static gint
sort_func (GtkTreeModel  *model,
           GtkTreeIter   *a,
           GtkTreeIter   *b,
           gpointer       dummy)
{
  gint          dista;
  gint          distb;
  gchar        *patha;
  gchar        *pathb;
  const gchar  *key = gtk_entry_get_text (GTK_ENTRY (plugin_data.entry));
  
  gtk_tree_model_get (model, a, COL_PATH, &patha, -1);
  gtk_tree_model_get (model, b, COL_PATH, &pathb, -1);
  
  dista = key_dist (key, patha);
  distb = key_dist (key, pathb);
  
  g_free (patha);
  g_free (pathb);
  
  return dista - distb;
}

static gboolean
filter_visible_func (GtkTreeModel  *model,
                     GtkTreeIter   *iter,
                     gpointer       dummy)
{
  gboolean      visible = TRUE;
  gchar        *text;
  gint          type;
  const gchar  *key = gtk_entry_get_text (GTK_ENTRY (plugin_data.entry));
  
  gtk_tree_model_get (model, iter, COL_PATH, &text, COL_TYPE, &type, -1);
  if (g_str_has_prefix (key, "f:")) {
    key += 2;
    visible = type == COL_TYPE_FILE;
  } else if (g_str_has_prefix (key, "c:")) {
    key += 2;
    visible = type == COL_TYPE_MENU_ITEM;
  }
  if (visible) {
    visible = key_matches (key, text);
  }
  g_free (text);
  
  return visible;
}

static void
tree_view_set_active_cell (GtkTreeView *view,
                           GtkTreeIter *iter)
{
  GtkTreePath *path;
  
  path = gtk_tree_model_get_path (gtk_tree_view_get_model (view), iter);
  gtk_tree_selection_select_iter (gtk_tree_view_get_selection (view), iter);
  gtk_tree_view_scroll_to_cell (view, path, NULL, FALSE, 0.0, 0.0);
  gtk_tree_path_free (path);
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
    
    case GDK_KEY_Return:
    case GDK_KEY_KP_Enter:
    case GDK_KEY_ISO_Enter: {
      GtkTreeIter       iter;
      GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (plugin_data.view));
      GtkTreeModel     *model;
      
      if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
        gint type;
        
        gtk_tree_model_get (model, &iter, COL_TYPE, &type, -1);
        
        switch (type) {
          case COL_TYPE_FILE: {
            GeanyDocument  *doc;
            gint            page;
            
            gtk_tree_model_get (model, &iter, COL_DOCUMENT, &doc, -1);
            page = document_get_notebook_page (doc);
            gtk_notebook_set_current_page (GTK_NOTEBOOK (geany_data->main_widgets->notebook),
                                           page);
            break;
          }
          
          case COL_TYPE_MENU_ITEM: {
            GtkMenuItem *item;
            
            gtk_tree_model_get (model, &iter, COL_WIDGET, &item, -1);
            gtk_menu_item_activate (item);
            g_object_unref (item);
            
            break;
          }
        }
        gtk_widget_hide (widget);
      }
      
      return TRUE;
    }
    
    case GDK_KEY_Up:
    case GDK_KEY_Down: {
      GtkTreeIter       iter;
      GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (plugin_data.view));
      GtkTreeModel     *model = gtk_tree_view_get_model (GTK_TREE_VIEW (plugin_data.view));
      gboolean          valid;
      
      if (gtk_tree_selection_get_selected (selection, NULL, &iter)) {
        if (event->keyval == GDK_KEY_Up) {
          GtkTreePath *path = gtk_tree_model_get_path (model, &iter);
          
          valid = gtk_tree_path_prev (path);
          if (valid) {
            gtk_tree_model_get_iter (model, &iter, path);
          }
          gtk_tree_path_free (path);
        } else {
          valid = gtk_tree_model_iter_next (model, &iter);
        }
      } else {
        valid = gtk_tree_model_get_iter_first (model, &iter);
      }
      
      if (valid) {
        tree_view_set_active_cell (GTK_TREE_VIEW (plugin_data.view), &iter);
      } else {
        gdk_beep ();
      }
      return TRUE;
    }
  }
  
  return FALSE;
}

static void
on_entry_text_notify (GObject    *object,
                      GParamSpec *pspec,
                      gpointer    dummy)
{
  GtkTreeIter iter;
  
  gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (plugin_data.filter));
  
  if (gtk_tree_model_get_iter_first (plugin_data.sort, &iter)) {
    tree_view_set_active_cell (GTK_TREE_VIEW (plugin_data.view), &iter);
  }
}

static void
store_populate_menu_items (GtkListStore  *store,
                           GtkMenuShell  *menu,
                           const gchar   *parent_path)
{
  GList  *children;
  GList  *node;
  
  children = gtk_container_get_children (GTK_CONTAINER (menu));
  for (node = children; node; node = node->next) {
    if (GTK_IS_SEPARATOR_MENU_ITEM (node->data) ||
        ! gtk_widget_get_visible (node->data)) {
      /* skip that */
    } else if (GTK_IS_MENU_ITEM (node->data)) {
      GtkWidget    *submenu;
      gchar        *path;
      gchar        *item_label;
      gboolean      use_underline;
      GtkStockItem  item;
      
      if (GTK_IS_IMAGE_MENU_ITEM (node->data) &&
          gtk_image_menu_item_get_use_stock (node->data) &&
          gtk_stock_lookup (gtk_menu_item_get_label (node->data), &item)) {
        item_label = g_strdup (item.label);
        use_underline = TRUE;
      } else {
        item_label = g_strdup (gtk_menu_item_get_label (node->data));
        use_underline = gtk_menu_item_get_use_underline (node->data);
      }
      
      /* remove underlines */
      if (use_underline) {
        gchar  *p   = item_label;
        gsize   len = strlen (p);
        
        while ((p = strchr (p, '_')) != NULL) {
          len -= (gsize) (p - item_label);
          
          memmove (p, p + 1, len);
        }
      }
      
      if (parent_path) {
        path = g_strconcat (parent_path, " \342\206\222 " /* right arrow */,
                            item_label, NULL);
      } else {
        path = g_strdup (item_label);
      }
      
      submenu = gtk_menu_item_get_submenu (node->data);
      if (submenu) {
        /* go deeper in the menus... */
        store_populate_menu_items (store, GTK_MENU_SHELL (submenu), path);
      } else {
        gchar *tmp;
        gchar *tooltip;
        gchar *label = g_markup_printf_escaped ("<big>%s</big>", item_label);
        
        tooltip = gtk_widget_get_tooltip_markup (node->data);
        if (tooltip) {
          SETPTR (label, g_strconcat (label, "\n<small>", tooltip, "</small>", NULL));
          g_free (tooltip);
        }
        
        tmp = g_markup_escape_text (path, -1);
        SETPTR (label, g_strconcat (label, "\n<small><i>", tmp, "</i></small>", NULL));
        g_free (tmp);
        
        gtk_list_store_insert_with_values (store, NULL, -1,
                                           COL_LABEL, label,
                                           COL_PATH, path,
                                           COL_TYPE, COL_TYPE_MENU_ITEM,
                                           COL_WIDGET, node->data,
                                           -1);
        
        g_free (label);
      }
      
      g_free (item_label);
      g_free (path);
    } else {
      g_warning ("Unknown widget type in the menu: %s",
                 G_OBJECT_TYPE_NAME (node->data));
    }
  }
  g_list_free (children);
}

static GtkWidget *
find_menubar (GtkContainer *container)
{
  GList      *children;
  GList      *node;
  GtkWidget  *menubar = NULL;
  
  children = gtk_container_get_children (container);
  for (node = children; ! menubar && node; node = node->next) {
    if (GTK_IS_MENU_BAR (node->data)) {
      menubar = node->data;
    } else if GTK_IS_CONTAINER (node->data) {
      menubar = find_menubar (node->data);
    }
  }
  g_list_free (children);
  
  return menubar;
}

static void
fill_store (GtkListStore *store)
{
  GtkWidget *menubar;
  
  menubar = find_menubar (GTK_CONTAINER (geany_data->main_widgets->window));
  store_populate_menu_items (store, GTK_MENU_SHELL (menubar), NULL);
}

/* we reset all file items not to have to monitor the open/close/renames/etc */
static void
reset_file_items (GtkListStore *store)
{
  GtkTreeIter iter;
  guint       i;
  
  /* remove old items */
  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter)) {
    gboolean has_next = TRUE;
    
    while (has_next) {
      gint type;
      
      gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, COL_TYPE, &type, -1);
      if (type == COL_TYPE_FILE) {
        has_next = gtk_list_store_remove (store, &iter);
      } else {
        has_next = gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter);
      }
    }
  }
  
  /* and add new ones */
  foreach_document (i) {
    gchar *basename = g_path_get_basename (DOC_FILENAME (documents[i]));
    gchar *label = g_markup_printf_escaped ("<big>%s</big>\n"
                                            "<small><i>%s</i></small>",
                                            basename,
                                            DOC_FILENAME (documents[i]));
    
    gtk_list_store_insert_with_values (store, NULL, -1,
                                       COL_LABEL, label,
                                       COL_PATH, DOC_FILENAME (documents[i]),
                                       COL_TYPE, COL_TYPE_FILE,
                                       COL_DOCUMENT, documents[i],
                                       -1);
    g_free (basename);
    g_free (label);
  }
}

static void
create_panel (void)
{
  GtkWidget          *frame;
  GtkWidget          *box;
  GtkWidget          *scroll;
  GtkTreeViewColumn  *col;
  GtkCellRenderer    *cell;
  
  plugin_data.panel = g_object_new (GTK_TYPE_WINDOW,
                                    "decorated", FALSE,
                                    "default-width", 500,
                                    "default-height", 200,
                                    "transient-for", geany_data->main_widgets->window,
                                    "window-position", GTK_WIN_POS_CENTER_ON_PARENT,
                                    "type-hint", GDK_WINDOW_TYPE_HINT_DIALOG,
                                    "skip-taskbar-hint", TRUE,
                                    "skip-pager-hint", TRUE,
                                    NULL);
  g_signal_connect (plugin_data.panel, "focus-out-event",
                    G_CALLBACK (gtk_widget_hide), NULL);
  g_signal_connect (plugin_data.panel, "key-press-event",
                    G_CALLBACK (on_panel_key_press_event), NULL);
  
  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  gtk_container_add (GTK_CONTAINER (plugin_data.panel), frame);
  
  box = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frame), box);
  
  plugin_data.entry = gtk_entry_new ();
  g_signal_connect (plugin_data.entry, "notify::text",
                    G_CALLBACK (on_entry_text_notify), NULL);
  gtk_box_pack_start (GTK_BOX (box), plugin_data.entry, FALSE, TRUE, 0);
  
  plugin_data.store = gtk_list_store_new (COL_COUNT,
                                          G_TYPE_STRING,
                                          G_TYPE_STRING,
                                          G_TYPE_INT,
                                          GTK_TYPE_WIDGET,
                                          G_TYPE_POINTER);
  fill_store (plugin_data.store);
  
  plugin_data.filter = gtk_tree_model_filter_new (GTK_TREE_MODEL (plugin_data.store),
                                                  NULL);
  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (plugin_data.filter),
                                          filter_visible_func, NULL, NULL);
  
  plugin_data.sort = gtk_tree_model_sort_new_with_model (plugin_data.filter);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (plugin_data.sort),
                                   COL_PATH, sort_func, NULL, NULL);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (plugin_data.sort),
                                        COL_PATH, GTK_SORT_ASCENDING);
  
  scroll = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                         "hscrollbar-policy", GTK_POLICY_AUTOMATIC,
                         "vscrollbar-policy", GTK_POLICY_AUTOMATIC,
                         NULL);
  gtk_box_pack_start (GTK_BOX (box), scroll, TRUE, TRUE, 0);
  
  plugin_data.view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (plugin_data.sort));
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (plugin_data.view), FALSE);
  cell = gtk_cell_renderer_text_new ();
  g_object_set (cell, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
  col = gtk_tree_view_column_new_with_attributes (NULL, cell,
                                                  "markup", COL_LABEL,
                                                  NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (plugin_data.view), col);
  gtk_container_add (GTK_CONTAINER (scroll), plugin_data.view);
  
  on_entry_text_notify (G_OBJECT (plugin_data.entry), NULL, NULL);
  
  gtk_widget_show_all (frame);
}

static void
on_kb_show_panel (guint key_id)
{
  GtkTreeView *view = GTK_TREE_VIEW (plugin_data.view);
  
  reset_file_items (plugin_data.store);
  
  gtk_widget_show (plugin_data.panel);
  gtk_widget_grab_focus (plugin_data.entry);
  
  if (! gtk_tree_selection_get_selected (gtk_tree_view_get_selection (view),
                                         NULL, NULL)) {
    GtkTreeIter iter;
    
    if (gtk_tree_model_get_iter_first (plugin_data.sort, &iter)) {
      tree_view_set_active_cell (GTK_TREE_VIEW (plugin_data.view), &iter);
    }
  }
}

static gboolean
on_plugin_idle_init (gpointer dummy)
{
  create_panel ();
  
  return FALSE;
}

void
plugin_init (GeanyData *data)
{
  GeanyKeyGroup *group;
  
  group = plugin_set_key_group (geany_plugin, "commander", KB_COUNT, NULL);
  keybindings_set_item (group, KB_SHOW_PANEL, on_kb_show_panel,
                        0, 0, "show_panel", _("Show Command Panel"), NULL);
  
  /* delay for other plugins to have a chance to load before, so we will
   * include their items */
  plugin_idle_add (geany_plugin, on_plugin_idle_init, NULL);
}

void
plugin_cleanup (void)
{
  if (plugin_data.panel) {
    gtk_widget_destroy (plugin_data.panel);
  }
}
