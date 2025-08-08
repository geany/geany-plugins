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

#include <string.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <geanyplugin.h>


/* uncomment to display each row score (for debugging sort) */
/*#define DISPLAY_SCORE 1*/


GeanyPlugin      *geany_plugin;
GeanyData        *geany_data;

PLUGIN_VERSION_CHECK(226)

PLUGIN_SET_TRANSLATABLE_INFO (
  LOCALEDIR, GETTEXT_PACKAGE,
  _("Commander"),
  _("Provides a command panel for quick access to actions, files and more"),
  VERSION,
  "Colomban Wendling <ban@herbesfolles.org>"
)


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


/* Plugin */

enum {
  KB_SHOW_PANEL,
  KB_SHOW_PANEL_COMMANDS,
  KB_SHOW_PANEL_FILES,
  KB_COUNT
};

static struct {
  GtkWidget    *panel;
  GtkWidget    *entry;
  GtkWidget    *view;
  GtkListStore *store;
  GtkTreeModel *sort;
  
  GtkTreePath  *last_path;

  gchar        *config_file;

  gint          panel_width;
  gint          panel_height;
} plugin_data;

typedef enum {
  COL_TYPE_MENU_ITEM  = 1 << 0,
  COL_TYPE_FILE       = 1 << 1,
  COL_TYPE_ANY        = 0xffff
} ColType;

enum {
  COL_LABEL,
  COL_PATH,
  COL_TYPE,
  COL_WIDGET,
  COL_DOCUMENT,
  COL_COUNT
};


#define PATH_SEPARATOR " \342\206\222 " /* right arrow */

#define SEPARATORS        " -_./\\\"'"
#define IS_SEPARATOR(c)   (strchr (SEPARATORS, (c)) != NULL)
#define next_separator(p) (strpbrk (p, SEPARATORS))

/* TODO: be more tolerant regarding unmatched character in the needle.
 * Right now, we implicitly accept unmatched characters at the end of the
 * needle but absolutely not at the start.  e.g. "xpy" won't match "python" at
 * all, though "pyx" will. */
static inline gint
get_score (const gchar *needle,
           const gchar *haystack)
{
  if (! needle || ! haystack) {
    return needle == NULL;
  } else if (! *needle || ! *haystack) {
    return *needle == 0;
  }
  
  if (IS_SEPARATOR (*haystack)) {
    return get_score (needle + IS_SEPARATOR (*needle), haystack + 1);
  }

  if (IS_SEPARATOR (*needle)) {
    return get_score (needle + 1, next_separator (haystack));
  }

  if (*needle == *haystack) {
    gint a = get_score (needle + 1, haystack + 1) + 1 + IS_SEPARATOR (haystack[1]);
    gint b = get_score (needle, next_separator (haystack));
    
    return MAX (a, b);
  } else {
    return get_score (needle, next_separator (haystack));
  }
}

static const gchar *
path_basename (const gchar *path)
{
  const gchar *p1 = strrchr (path, '/');
  const gchar *p2 = g_strrstr (path, PATH_SEPARATOR);

  if (! p1 && ! p2) {
    return path;
  } else if (p1 > p2) {
    return p1;
  } else {
    return p2;
  }
}

static gint
key_score (const gchar *key_,
           const gchar *text_)
{  
  gchar  *text  = g_utf8_casefold (text_, -1);
  gchar  *key   = g_utf8_casefold (key_, -1);
  gint    score;

  /* full compliance should have high priority */
  if (strcmp (key, text) == 0)
    return 0xf000;

  score = get_score (key, text) + get_score (key, path_basename (text)) / 2;
  
  g_free (text);
  g_free (key);
  
  return score;
}

static const gchar *
get_key (gint *type_)
{
  gint          type  = COL_TYPE_ANY;
  const gchar  *key   = gtk_entry_get_text (GTK_ENTRY (plugin_data.entry));
  
  if (g_str_has_prefix (key, "f:")) {
    key += 2;
    type = COL_TYPE_FILE;
  } else if (g_str_has_prefix (key, "c:")) {
    key += 2;
    type = COL_TYPE_MENU_ITEM;
  }
  
  if (type_) {
    *type_ = type;
  }
  
  return key;
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
      
      case GTK_MOVEMENT_PAGES:
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
      
      /* GtkStock is deprectaed since GTK 3.10, but we have to use it in order
       * to get the actual label of the menu item */
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      if (GTK_IS_IMAGE_MENU_ITEM (node->data) &&
          gtk_image_menu_item_get_use_stock (node->data) &&
          gtk_stock_lookup (gtk_menu_item_get_label (node->data), &item)) {
        item_label = g_strdup (item.label);
        use_underline = TRUE;
      } else {
        item_label = g_strdup (gtk_menu_item_get_label (node->data));
        use_underline = gtk_menu_item_get_use_underline (node->data);
      }
      G_GNUC_END_IGNORE_DEPRECATIONS
      
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
        path = g_strconcat (parent_path, PATH_SEPARATOR, item_label, NULL);
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
    } else if (GTK_IS_CONTAINER (node->data)) {
      menubar = find_menubar (node->data);
    }
  }
  g_list_free (children);
  
  return menubar;
}

static void
fill_store (GtkListStore *store)
{
  GtkWidget  *menubar;
  guint       i = 0;
  
  /* menu items */
  menubar = find_menubar (GTK_CONTAINER (geany_data->main_widgets->window));
  store_populate_menu_items (store, GTK_MENU_SHELL (menubar), NULL);
  
  /* open files */
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

static gint
sort_func (GtkTreeModel            *model,
           GtkTreeIter             *a,
           GtkTreeIter             *b,
           G_GNUC_UNUSED gpointer   dummy)
{
  gint          scorea;
  gint          scoreb;
  gchar        *patha;
  gchar        *pathb;
  gint          typea;
  gint          typeb;
  gint          type;
  const gchar  *key = get_key (&type);
  
  gtk_tree_model_get (model, a, COL_PATH, &patha, COL_TYPE, &typea, -1);
  gtk_tree_model_get (model, b, COL_PATH, &pathb, COL_TYPE, &typeb, -1);
  
  scorea = key_score (key, patha);
  scoreb = key_score (key, pathb);
  
  if (! (typea & type)) {
    scorea -= 0xf000;
  }
  if (! (typeb & type)) {
    scoreb -= 0xf000;
  }
  
  g_free (patha);
  g_free (pathb);
  
  return scoreb - scorea;
}

static gboolean
on_panel_key_press_event (GtkWidget               *widget,
                          GdkEventKey             *event,
                          G_GNUC_UNUSED gpointer   dummy)
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
      tree_view_activate_focused_row (GTK_TREE_VIEW (plugin_data.view));
      return TRUE;
    
    case GDK_KEY_Page_Up:
    case GDK_KEY_Page_Down:
      tree_view_move_focus (GTK_TREE_VIEW (plugin_data.view),
                            GTK_MOVEMENT_PAGES,
                            event->keyval == GDK_KEY_Page_Up ? -1 : 1);
      return TRUE;
    
    case GDK_KEY_Up:
    case GDK_KEY_Down: {
      tree_view_move_focus (GTK_TREE_VIEW (plugin_data.view),
                            GTK_MOVEMENT_DISPLAY_LINES,
                            event->keyval == GDK_KEY_Up ? -1 : 1);
      return TRUE;
    }
  }
  
  return FALSE;
}

static void
on_entry_text_notify (G_GNUC_UNUSED GObject    *object,
                      G_GNUC_UNUSED GParamSpec *pspec,
                      G_GNUC_UNUSED gpointer    dummy)
{
  GtkTreeIter   iter;
  GtkTreeView  *view  = GTK_TREE_VIEW (plugin_data.view);
  GtkTreeModel *model = gtk_tree_view_get_model (view);
  
  /* we force re-sorting the whole model from how it was before, and the
   * back to the new filter.  this is somewhat hackish but since we don't
   * know the original sorting order, and GtkTreeSortable don't have a
   * resort() API anyway. */
  gtk_tree_model_sort_reset_default_sort_func (GTK_TREE_MODEL_SORT (model));
  gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (model),
                                           sort_func, NULL, NULL);
  
  if (gtk_tree_model_get_iter_first (model, &iter)) {
    tree_view_set_cursor_from_iter (view, &iter);
  }
}

static void
on_entry_activate (G_GNUC_UNUSED GtkEntry  *entry,
                   G_GNUC_UNUSED gpointer   dummy)
{
  tree_view_activate_focused_row (GTK_TREE_VIEW (plugin_data.view));
}

static void
on_panel_hide (G_GNUC_UNUSED GtkWidget *widget,
               G_GNUC_UNUSED gpointer   dummy)
{
  GtkTreeView  *view = GTK_TREE_VIEW (plugin_data.view);
  
  if (plugin_data.last_path) {
    gtk_tree_path_free (plugin_data.last_path);
    plugin_data.last_path = NULL;
  }
  gtk_tree_view_get_cursor (view, &plugin_data.last_path, NULL);
  
  gtk_list_store_clear (plugin_data.store);
}

static void
on_panel_show (G_GNUC_UNUSED GtkWidget *widget,
               G_GNUC_UNUSED gpointer   dummy)
{
  GtkTreePath *path;
  GtkTreeView *view = GTK_TREE_VIEW (plugin_data.view);
  
  fill_store (plugin_data.store);
  
  gtk_widget_grab_focus (plugin_data.entry);
  
  if (plugin_data.last_path) {
    gtk_tree_view_set_cursor (view, plugin_data.last_path, NULL, FALSE);
    gtk_tree_view_scroll_to_cell (view, plugin_data.last_path, NULL,
                                  TRUE, 0.5, 0.5);
  }
  /* make sure the cursor is set (e.g. if plugin_data.last_path wasn't valid) */
  gtk_tree_view_get_cursor (view, &path, NULL);
  if (path) {
    gtk_tree_path_free (path);
  } else {
    GtkTreeIter iter;
    
    if (gtk_tree_model_get_iter_first (gtk_tree_view_get_model (view), &iter)) {
      tree_view_set_cursor_from_iter (GTK_TREE_VIEW (plugin_data.view), &iter);
    }
  }
}

static void
on_view_row_activated (GtkTreeView                      *view,
                       GtkTreePath                      *path,
                       G_GNUC_UNUSED GtkTreeViewColumn  *column,
                       G_GNUC_UNUSED gpointer            dummy)
{
  GtkTreeModel *model = gtk_tree_view_get_model (view);
  GtkTreeIter   iter;
  
  if (gtk_tree_model_get_iter (model, &iter, path)) {
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
    gtk_widget_hide (plugin_data.panel);
  }
}

#ifdef DISPLAY_SCORE
static void
score_cell_data (GtkTreeViewColumn *column,
                 GtkCellRenderer   *cell,
                 GtkTreeModel      *model,
                 GtkTreeIter       *iter,
                 gpointer           col)
{
  gint          score;
  gchar        *text;
  gchar        *path;
  gint          pathtype;
  gint          type;
  gint          width, old_width;
  const gchar  *key = get_key (&type);
  
  gtk_tree_model_get (model, iter, COL_PATH, &path, COL_TYPE, &pathtype, -1);
  
  score = key_score (key, path);
  if (! (pathtype & type)) {
    score -= 0xf000;
  }
  
  text = g_strdup_printf ("%d", score);
  g_object_set (cell, "text", text, NULL);
  
  /* automatic column sizing is buggy, so just make an acceptable wild guess */
  width = 8 + strlen (text) * 10;
  old_width = gtk_tree_view_column_get_fixed_width (col);
  if (old_width < width) {
    gtk_tree_view_column_set_fixed_width (col, width);
  }
  
  g_free (text);
  g_free (path);
}
#endif

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
                                    "default-width", plugin_data.panel_width,
                                    "default-height", plugin_data.panel_height,
                                    "transient-for", geany_data->main_widgets->window,
                                    "window-position", GTK_WIN_POS_CENTER_ON_PARENT,
                                    "type-hint", GDK_WINDOW_TYPE_HINT_DIALOG,
                                    "skip-taskbar-hint", TRUE,
                                    "skip-pager-hint", TRUE,
                                    NULL);
  g_signal_connect (plugin_data.panel, "focus-out-event",
                    G_CALLBACK (gtk_widget_hide), NULL);
  g_signal_connect (plugin_data.panel, "show",
                    G_CALLBACK (on_panel_show), NULL);
  g_signal_connect (plugin_data.panel, "hide",
                    G_CALLBACK (on_panel_hide), NULL);
  g_signal_connect (plugin_data.panel, "key-press-event",
                    G_CALLBACK (on_panel_key_press_event), NULL);
  
  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  gtk_container_add (GTK_CONTAINER (plugin_data.panel), frame);
  
  box = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frame), box);
  
  plugin_data.entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (box), plugin_data.entry, FALSE, TRUE, 0);
  
  plugin_data.store = gtk_list_store_new (COL_COUNT,
                                          G_TYPE_STRING,
                                          G_TYPE_STRING,
                                          G_TYPE_INT,
                                          GTK_TYPE_WIDGET,
                                          G_TYPE_POINTER);
  
  plugin_data.sort = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (plugin_data.store));
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (plugin_data.sort),
                                        GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
                                        GTK_SORT_ASCENDING);
  
  scroll = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                         "hscrollbar-policy", GTK_POLICY_AUTOMATIC,
                         "vscrollbar-policy", GTK_POLICY_AUTOMATIC,
                         NULL);
  gtk_box_pack_start (GTK_BOX (box), scroll, TRUE, TRUE, 0);
  
  plugin_data.view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (plugin_data.sort));
  gtk_widget_set_can_focus (plugin_data.view, FALSE);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (plugin_data.view), FALSE);

  /* eliminates graphic artifacts */
  gtk_tree_view_set_fixed_height_mode (GTK_TREE_VIEW (plugin_data.view), TRUE);
#ifdef DISPLAY_SCORE
  cell = gtk_cell_renderer_text_new ();
  col = gtk_tree_view_column_new_with_attributes (NULL, cell, NULL);
  gtk_tree_view_column_set_sizing (col, GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_column_set_cell_data_func (col, cell, score_cell_data, col, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (plugin_data.view), col);
#endif
  cell = gtk_cell_renderer_text_new ();
  g_object_set (cell, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
  col = gtk_tree_view_column_new_with_attributes (NULL, cell,
                                                  "markup", COL_LABEL,
                                                  NULL);
  gtk_tree_view_column_set_sizing (col, GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_append_column (GTK_TREE_VIEW (plugin_data.view), col);
  g_signal_connect (plugin_data.view, "row-activated",
                    G_CALLBACK (on_view_row_activated), NULL);
  gtk_container_add (GTK_CONTAINER (scroll), plugin_data.view);
  
  /* connect entry signals after the view is created as they use it */
  g_signal_connect (plugin_data.entry, "notify::text",
                    G_CALLBACK (on_entry_text_notify), NULL);
  g_signal_connect (plugin_data.entry, "activate",
                    G_CALLBACK (on_entry_activate), NULL);
  
  gtk_widget_show_all (frame);
}

static gboolean
on_kb_show_panel (G_GNUC_UNUSED GeanyKeyBinding  *kb,
                  G_GNUC_UNUSED guint             key_id,
                  gpointer                        data)
{
  const gchar *prefix = data;
  
  gtk_widget_show (plugin_data.panel);
  
  if (prefix) {
    const gchar *key = gtk_entry_get_text (GTK_ENTRY (plugin_data.entry));
    
    if (! g_str_has_prefix (key, prefix)) {
      gtk_entry_set_text (GTK_ENTRY (plugin_data.entry), prefix);
    }
    /* select the non-prefix part */
    gtk_editable_select_region (GTK_EDITABLE (plugin_data.entry),
                                g_utf8_strlen (prefix, -1), -1);
  }
  
  return TRUE;
}

static gboolean
on_plugin_idle_init (G_GNUC_UNUSED gpointer dummy)
{
  create_panel ();
  
  return FALSE;
}

void
plugin_init (G_GNUC_UNUSED GeanyData *data)
{
  GeanyKeyGroup *group;
  GKeyFile      *config;

  /* keybindings */
  
  group = plugin_set_key_group (geany_plugin, "commander", KB_COUNT, NULL);
  keybindings_set_item_full (group, KB_SHOW_PANEL, 0, 0, "show_panel",
                             _("Show Command Panel"), NULL,
                             on_kb_show_panel, NULL, NULL);
  keybindings_set_item_full (group, KB_SHOW_PANEL_COMMANDS, 0, 0,
                             "show_panel_commands",
                             _("Show Command Panel (Commands Only)"), NULL,
                             on_kb_show_panel, (gpointer) "c:", NULL);
  keybindings_set_item_full (group, KB_SHOW_PANEL_FILES, 0, 0,
                             "show_panel_files",
                             _("Show Command Panel (Files Only)"), NULL,
                             on_kb_show_panel, (gpointer) "f:", NULL);

  /* config */
  
  config = g_key_file_new ();
  plugin_data.config_file = g_strconcat (geany->app->configdir, G_DIR_SEPARATOR_S,
		                                     "plugins", G_DIR_SEPARATOR_S, 
                                         "commander", G_DIR_SEPARATOR_S, 
                                         "commander.conf", NULL);

  plugin_data.panel_width = utils_get_setting_integer (config,
                                                       "commander", 
                                                       "panel_width", 
                                                       500);

  plugin_data.panel_height = utils_get_setting_integer (config,
                                                        "commander", 
                                                        "panel_height", 
                                                        300);
  
  g_key_file_free (config);
  
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
  if (plugin_data.last_path) {
    gtk_tree_path_free (plugin_data.last_path);
  }
  g_free (plugin_data.config_file);
}

void
plugin_help (void)
{
  utils_open_browser (DOCDIR "/" PLUGIN "/README");
}

static gboolean
ui_cfg_read_check_button (GtkDialog *dialog, GKeyFile *config,
                          const gchar *widget_code,
                          const gchar *config_code)
{
  GtkWidget    *check_button;
  gboolean      active; 

  check_button = g_object_get_data (G_OBJECT (dialog), widget_code);
  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check_button));
  g_key_file_set_boolean (config, "commander", config_code, active);
  return active;
}

static glong
ui_cfg_read_spin_button (GtkDialog *dialog, GKeyFile *config,
                         const gchar *widget_code,
                         const gchar *config_code)
{
  GtkWidget    *spin_button;
  gdouble       value;

  spin_button = g_object_get_data (G_OBJECT (dialog), widget_code);
  value = gtk_spin_button_get_value (GTK_SPIN_BUTTON (spin_button));
  g_key_file_set_integer (config, "commander", config_code, value);
  return value;
}

static void 
plugin_configure_response_cb (GtkDialog *dialog, gint response, G_GNUC_UNUSED gpointer user_data)
{
  GKeyFile     *config;
  gchar        *data;
  gchar        *config_dir;

  if (response != GTK_RESPONSE_OK && response != GTK_RESPONSE_APPLY)
    return;
  
  /* mkdir config_dir */
  config_dir = g_path_get_dirname (plugin_data.config_file);
  if (!g_file_test (config_dir, G_FILE_TEST_IS_DIR) && utils_mkdir (config_dir, TRUE) != 0) {
    dialogs_show_msgbox (GTK_MESSAGE_ERROR, _("Plugin configuration directory could not be created."));
    g_free (config_dir);
    return;
  }
  g_free (config_dir);
  
  /* load config */
  config = g_key_file_new ();
  g_key_file_load_from_file (config, plugin_data.config_file, G_KEY_FILE_NONE, NULL);

  /* palette */
  plugin_data.panel_width
    = ui_cfg_read_spin_button (dialog, config,
                               "panel_width_spin_button",
                               "panel_width");
  plugin_data.panel_height
    = ui_cfg_read_spin_button (dialog, config,
                               "panel_height_spin_button",
                               "panel_height");

  gtk_window_resize (GTK_WINDOW (plugin_data.panel), plugin_data.panel_width, plugin_data.panel_height);          

  /* write config */
  data = g_key_file_to_data (config, NULL, NULL);
  utils_write_file (plugin_data.config_file, data);
  g_free (data);
  g_key_file_free (config);
}


static GtkWidget *
ui_cfg_frame_new (GtkWidget *vbox)
{
  GtkWidget    *frame;
  GtkWidget    *frame_vbox;

  frame = gtk_frame_new (NULL);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 3);

  frame_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (frame), frame_vbox);

  return frame_vbox;
}

static void
ui_cfg_check_button_new (GtkDialog *dialog, GtkWidget *vbox,
                         const gchar *code, const gchar *label, gboolean active)
{
  GtkWidget    *hbox;
  GtkWidget    *check_button;

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 3);

  check_button = gtk_check_button_new_with_label (label);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button), active);
  gtk_box_pack_start (GTK_BOX (hbox), check_button, FALSE, FALSE, 3);
  g_object_set_data (G_OBJECT (dialog), code, check_button);
}

static void
ui_cfg_spin_button_new (GtkDialog *dialog, GtkWidget *vbox,
                        const gchar *code, const gchar *label_text,
                        gdouble start, gdouble end, gdouble step,
                        gdouble value)
{
  GtkWidget    *hbox;
  GtkWidget    *label;
  GtkWidget    *spin_button;

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 3);

  label = gtk_label_new_with_mnemonic (label_text);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 3);

  spin_button = gtk_spin_button_new_with_range (start, end, step);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin_button), value);
  gtk_box_pack_start (GTK_BOX (hbox), spin_button, FALSE, FALSE, 3);
  g_object_set_data (G_OBJECT (dialog), code, spin_button);
}                           

GtkWidget *
plugin_configure (GtkDialog *dialog)
{
  GtkWidget    *vbox;
  GtkWidget    *palette_container;
   
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);

  /* palette */
  palette_container = ui_cfg_frame_new (vbox);

  ui_cfg_spin_button_new (dialog, palette_container,
                          "panel_width_spin_button",
                          _("Command panel window width"),
                          100, 4096, 1,
                          plugin_data.panel_width);

  ui_cfg_spin_button_new (dialog, palette_container,
                          "panel_height_spin_button",
                          _("Command panel window height"),
                          100, 4096, 1,
                          plugin_data.panel_height);
  
  gtk_widget_show_all (vbox);
  
  g_signal_connect (dialog, "response", G_CALLBACK (plugin_configure_response_cb), NULL);
  
  return vbox;
}
