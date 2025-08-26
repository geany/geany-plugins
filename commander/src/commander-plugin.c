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
#include <filelist.h>


/* uncomment to display each row score (for debugging sort) */
/*#define DISPLAY_SCORE 1*/


GeanyPlugin      *geany_plugin;
GeanyData        *geany_data;

PLUGIN_VERSION_CHECK(226)

PLUGIN_SET_TRANSLATABLE_INFO (
  LOCALEDIR, GETTEXT_PACKAGE,
  _("Commander"),
  _("Provides a command panel for quick access to actions, files, symbols and more"),
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
  KB_SHOW_PANEL_SYMBOLS,
  KB_COUNT
};

static struct {
  GtkWidget           *panel;
  GtkWidget           *entry;
  GtkWidget           *view;
  GtkListStore        *store;
  GtkTreeModel        *sort;
  GtkWidget           *spinner;
  
  GtkTreePath         *last_path;

  gchar               *config_file;

  GCancellable        *show_project_file_task_cancellable;
  gboolean             show_project_file;
  guint                show_project_file_limit;
  gboolean             show_project_file_skip_hidden_file;
  gboolean             show_project_file_skip_hidden_dir;
  
  gboolean             show_symbol;

  gint                 panel_width;
  gint                 panel_height;
} plugin_data;

typedef enum {
  COL_TYPE_MENU_ITEM    = 1 << 0,
  COL_TYPE_FILE         = 1 << 1,
  COL_TYPE_SYMBOL       = 1 << 2,
  COL_TYPE_ANY          = 0xffff
} ColType;

enum {
  COL_LABEL,       /* display text */
  
  COL_VALUE,       /* text that is checked against the input */

  COL_TYPE,        /* enum ColType */
  

  COL_WIDGET,
  COL_DOCUMENT,
  COL_FILE,
  COL_LINE,
  COL_ICON,
  COL_COUNT
};

typedef struct {
  gint   score;
  gchar *path;
} FileQueueItem;

typedef struct {
  GtkListStore *store;
  gchar        *dir_path;
  GSList       *patterns;
  gchar        *key;
} FileSearchTaskInput;

static gulong VISIBLE_TAGS =  tm_tag_class_t | tm_tag_enum_t | tm_tag_function_t |
                              tm_tag_interface_t | tm_tag_method_t | tm_tag_struct_t |
                              tm_tag_union_t | tm_tag_variable_t | tm_tag_macro_t |
                              tm_tag_macro_with_arg_t;

#define PATH_SEPARATOR " \342\206\222 " /* right arrow */

#define SEPARATORS        " -_./\\\"':"
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
  } else if (g_str_has_prefix (key, "s:")) {
    key += 2;
    type = COL_TYPE_SYMBOL;
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
                                           COL_VALUE, path,
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
store_add_file (GtkListStore *store, GeanyDocument *doc, const gchar *path)
{
  gchar      *basename;
  gchar      *label;

  basename = g_path_get_basename (path);

  label = g_markup_printf_escaped ("<big>%s</big>\n"
                                   "<small><i>%s</i></small>",
                                   basename,
                                   path);

  gtk_list_store_insert_with_values (store, NULL, -1,
                                     COL_LABEL, label,
                                     COL_VALUE, path,
                                     COL_FILE, path,
                                     COL_TYPE, COL_TYPE_FILE,
                                     COL_DOCUMENT, doc,
                                     -1);

  g_free (basename);
  g_free (label);
}

static gboolean
file_has_allowed_ext (const gchar *filename, GSList *allow_ext)
{
  if (allow_ext == NULL)
    return TRUE;
  
  return filelist_patterns_match (allow_ext, filename);
}

static FileQueueItem *
file_queue_item_new (gint score, const gchar *filepath)
{
  FileQueueItem *self;

  self = g_new (FileQueueItem, 1);
  self->score = score;
  self->path = g_strdup (filepath);
  return self;
}

static void
file_queue_item_free (FileQueueItem *self)
{
  g_free (self->path);
  g_free (self);
}

static gint
file_queue_item_cmp (const FileQueueItem *a, const FileQueueItem *b, G_GNUC_UNUSED gpointer udata)
{
  return (a->score > b->score) - (a->score < b->score);
}

static void
file_queue_add (GQueue *queue, guint limit, const gchar *key, const gchar *filepath)
{
  FileQueueItem *qitem;
  FileQueueItem *head;
  gint score;

  score = key_score (key, filepath);

  if (g_queue_get_length (queue) > limit) {
    head = g_queue_peek_head (queue);
    if (score < head->score) {
      return;
    }

    file_queue_item_free (g_queue_pop_head (queue));
  } 

  qitem = file_queue_item_new (score, filepath);
  g_queue_insert_sorted (queue, qitem, (GCompareDataFunc) file_queue_item_cmp, NULL);
}

static void file_search_task_input_free (FileSearchTaskInput *self)
{
  g_free (self->dir_path);
  g_slist_free_full (self->patterns, (GDestroyNotify) g_pattern_spec_free);
  g_free (self->key);
}

static void tree_view_cursor_to_top (void)
{
  GtkTreeIter   iter;
  GtkTreeView  *view  = GTK_TREE_VIEW (plugin_data.view);
  GtkTreeModel *model = gtk_tree_view_get_model (view);

  if (gtk_tree_model_get_iter_first (model, &iter)) {
    tree_view_set_cursor_from_iter (view, &iter);
  }
}

/* recursively adding project files to the queue */
static void
store_add_dir (GQueue       *queue,
               const gchar  *dir_path,
               GSList       *allow_ext,
               const gchar  *key,
               GCancellable *cancellable)
{
  GDir         *dir;
  const gchar  *filename;
  gchar        *filepath;
  gboolean      add_file;
  gboolean      is_hidden;

  if (g_cancellable_is_cancelled (cancellable))
    return;

  if ((dir = g_dir_open (dir_path, 0, NULL)) == NULL)
    return;

  while ((filename = g_dir_read_name (dir)) != NULL) {
    if (strcmp (filename, ".") == 0 || strcmp (filename, "..") == 0)
      continue;

    is_hidden = filename[0] == '.';

    filepath = g_build_path (G_DIR_SEPARATOR_S, dir_path, filename, NULL);

    if (g_file_test (filepath, G_FILE_TEST_IS_REGULAR)) {
      add_file = file_has_allowed_ext (filename, allow_ext)
              && document_find_by_real_path (filepath) == NULL
              && (!is_hidden || !plugin_data.show_project_file_skip_hidden_file);
      
      if (add_file) {
        file_queue_add (queue, plugin_data.show_project_file_limit, key, filepath);
      }
    } else if (g_file_test (filepath, G_FILE_TEST_IS_DIR)) {
      if (!is_hidden || !plugin_data.show_project_file_skip_hidden_dir) {
        store_add_dir (queue, filepath, allow_ext, key, cancellable);
      }
    }

    g_free (filepath);
  }

  g_dir_close (dir);
}

/* when entering a search query, the old set of files is deleted and
 * a new one is added, this function deletes the old parameters */
static void
clear_store_project_files (GtkListStore *store)
{
  GtkTreeIter  iter;

  gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter);

  while (gtk_list_store_iter_is_valid (GTK_LIST_STORE (store), &iter)) {
    GValue value_type = G_VALUE_INIT;
    gtk_tree_model_get_value (GTK_TREE_MODEL (store), &iter, COL_TYPE, &value_type);
    if (g_value_get_int (&value_type) != COL_TYPE_FILE)
      goto next_iter;

    GValue value_doc = G_VALUE_INIT;
    gtk_tree_model_get_value (GTK_TREE_MODEL (store), &iter, COL_DOCUMENT, &value_doc);
    if (g_value_get_pointer (&value_doc) == NULL) {
      gtk_list_store_remove (GTK_LIST_STORE (store), &iter);
      continue;
    }

next_iter:
    gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter);
  }
}

static void
store_add_project_file (FileQueueItem *item, GtkListStore *store)
{
  store_add_file (store, NULL, item->path);
}

static void
do_show_project_file_task (GTask                      *task,
                           G_GNUC_UNUSED gpointer      source_object,
                           gpointer                    task_data,
                           GCancellable               *cancellable)
{
  FileSearchTaskInput *file_search_task_input;
  GQueue              *file_queue;

  file_search_task_input = task_data;

  /* To prevent GTK from slowing down, you need to reduce the number of options and
   * leave the best ones. GQueue is well suited for such selection. */
  file_queue = g_queue_new ();

  store_add_dir (file_queue,
                 file_search_task_input->dir_path,
                 file_search_task_input->patterns,
                 file_search_task_input->key,
                 cancellable);

  if (g_task_return_error_if_cancelled (task))
    return;

  g_task_return_pointer (task, file_queue, NULL);
}

static void
on_show_project_file_task_ready (G_GNUC_UNUSED GObject       *source_object,
                                 GAsyncResult                *result,
                                 GError                     **error)
{
  FileSearchTaskInput *task_input;
  GQueue              *file_queue;

  task_input = g_task_get_task_data (G_TASK(result));
  file_queue = g_task_propagate_pointer (G_TASK (result), error);

  if (file_queue == NULL)
    return;

  /* Adding found files to store */
  clear_store_project_files (task_input->store);
  g_queue_foreach (file_queue, (GFunc) store_add_project_file, plugin_data.store);
  g_queue_free_full (file_queue, (GDestroyNotify) file_queue_item_free);

  tree_view_cursor_to_top ();
  gtk_widget_hide (plugin_data.spinner);
}

static void
cancel_fill_store_project_files_task (void)
{
  if (plugin_data.show_project_file_task_cancellable != NULL) {
    g_cancellable_cancel (plugin_data.show_project_file_task_cancellable);
    g_object_unref (plugin_data.show_project_file_task_cancellable);
  }

  plugin_data.show_project_file_task_cancellable = g_cancellable_new ();
}

static void
add_fill_store_project_files_task (GtkListStore *store)
{
  GeanyProject        *project;
  GSList              *patterns;
  const gchar         *key;
  gint                 key_type;
  FileSearchTaskInput *file_search_task_input;
  GTask               *task;

  if (!plugin_data.show_project_file)
    return;

  if ((project = geany->app->project) == NULL)
    return;

  if (project->base_path == NULL)
    return;

  key = get_key (&key_type);

  patterns = filelist_get_precompiled_patterns (project->file_patterns);

  file_search_task_input = g_new0 (FileSearchTaskInput, 1);
  file_search_task_input->store = store;
  file_search_task_input->dir_path = g_strdup(project->base_path);
  file_search_task_input->patterns = patterns;
  file_search_task_input->key = g_strdup(key);

  cancel_fill_store_project_files_task ();

  /* creating a new task */
  task = g_task_new (NULL,
                     plugin_data.show_project_file_task_cancellable,
                     (GAsyncReadyCallback) on_show_project_file_task_ready,
                     NULL);

  g_task_set_task_data (task,
                        file_search_task_input,
                        (GDestroyNotify) file_search_task_input_free);

  g_task_run_in_thread (task, do_show_project_file_task);
  g_object_unref (task);

  gtk_widget_show (plugin_data.spinner);
}

static const gchar *
get_symbol_icon (const TMTag *tag)
{
  if (tag->type & tm_tag_struct_t)
    return "classviewer-struct";
  
  if (tag->type & (tm_tag_class_t | tm_tag_interface_t))
    return "classviewer-class";

  if (tag->type & (tm_tag_function_t | tm_tag_function_t))
    return "classviewer-method";
  
  if (tag->type & (tm_tag_macro_t | tm_tag_macro_with_arg_t))
    return "classviewer-macro";

  if (tag->type & tm_tag_variable_t)
    return "classviewer-var";

  return "classviewer-other";
}

static void
fill_store_symbols (GtkListStore *store)
{
  const TMWorkspace  *ws;
  guint               i;
  GPtrArray          *tags;
  TMTag              *tag;
  gchar              *name;
  gchar              *label;
  
  if (!plugin_data.show_symbol)
    return;

  if ((ws = geany->app->tm_workspace) == NULL)
    return;

  if (ws->tags_array == NULL)
    return;

  tags = ws->tags_array;
  for (i = 0; i < tags->len; ++i) {
    tag = TM_TAG (tags->pdata[i]);

    if (tag->file == NULL)
      continue;
    
    if ((tag->type & VISIBLE_TAGS) == 0)
      continue;

    if (tag->scope == NULL) {
      name = g_strdup (tag->name);
    } else {
      name = g_strdup_printf ("%s::%s", tag->scope, tag->name);
    }

    label = g_markup_printf_escaped ("<big>%s</big>\n"
                                     "<small><i>%s:%lu</i></small>",
                                     name,
                                     tag->file->file_name,
                                     tag->line);

    gtk_list_store_insert_with_values (store, NULL, -1,
                                       COL_LABEL, label,
                                       COL_VALUE, name,
                                       COL_TYPE, COL_TYPE_SYMBOL,
                                       COL_FILE, tag->file->file_name,
                                       COL_LINE, tag->line,
                                       COL_ICON, get_symbol_icon (tag),
                                       -1);

    g_free (name);
    g_free (label);
  }
}

static void
fill_store (GtkListStore *store)
{
  GtkWidget    *menubar;
  guint         i = 0;

  /* menu items */
  menubar = find_menubar (GTK_CONTAINER (geany_data->main_widgets->window));
  store_populate_menu_items (store, GTK_MENU_SHELL (menubar), NULL);
  
  /* open files */
  foreach_document (i) {
    store_add_file (store, documents[i], DOC_FILENAME (documents[i]));
  }
  
  /* project files */
  add_fill_store_project_files_task (store);

  /* symbols */
  fill_store_symbols (store);
}

/* when you enter a search query, some options are updated */
static void
refresh_store (GtkListStore *store)
{
  /* project files */
  add_fill_store_project_files_task (store);
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
  
  gtk_tree_model_get (model, a, COL_VALUE, &patha, COL_TYPE, &typea, -1);
  gtk_tree_model_get (model, b, COL_VALUE, &pathb, COL_TYPE, &typeb, -1);
  
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
  GtkTreeView  *view  = GTK_TREE_VIEW (plugin_data.view);
  GtkTreeModel *model = gtk_tree_view_get_model (view);

  /* input related update */
  refresh_store (plugin_data.store);
  
  /* we force re-sorting the whole model from how it was before, and the
   * back to the new filter.  this is somewhat hackish but since we don't
   * know the original sorting order, and GtkTreeSortable don't have a
   * resort() API anyway. */
  gtk_tree_model_sort_reset_default_sort_func (GTK_TREE_MODEL_SORT (model));
  gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (model),
                                           sort_func, NULL, NULL);
  
  tree_view_cursor_to_top ();
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
        const gchar    *filepath;
        
        gtk_tree_model_get (model, &iter, COL_DOCUMENT, &doc, -1);
        
        if (doc == NULL) {
          /* file may not be open yet */
          
          gtk_tree_model_get (model, &iter, COL_FILE, &filepath, -1);
          if (filepath == NULL)
            break;
          
          if ((doc = document_open_file (filepath, FALSE, NULL, NULL)) == NULL)
            break;
        }
        
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

      case COL_TYPE_SYMBOL: {
        GeanyDocument *doc;
        const gchar   *filepath;
        gulong         line;

        gtk_tree_model_get (model, &iter, COL_FILE, &filepath, -1);
        gtk_tree_model_get (model, &iter, COL_LINE, &line, -1);

        if ((doc = document_open_file (filepath, FALSE, NULL, NULL)) == NULL)
          break;

        navqueue_goto_line (document_get_current (), doc, line);
  
        break;
      }
    }
    gtk_widget_hide (plugin_data.panel);
  }
}

static void
cell_icon_data (G_GNUC_UNUSED GtkTreeViewColumn *column,
                GtkCellRenderer                 *cell,
                GtkTreeModel                    *model,
                GtkTreeIter                     *iter,
                G_GNUC_UNUSED gpointer           udata)
{
  const gchar *icon_name;
  gint type;
  
  gtk_tree_model_get (model, iter, COL_TYPE, &type, -1);

  switch (type) {
    case COL_TYPE_FILE: {
      g_object_set (cell, "icon-name", "text-x-generic", NULL);
      break;
    }

    case COL_TYPE_MENU_ITEM: {
      g_object_set (cell, "icon-name", "geany", NULL);
      break;
    }

    case COL_TYPE_SYMBOL: {
      gtk_tree_model_get (model, iter, COL_ICON, &icon_name, -1);
      g_object_set (cell, "icon-name", icon_name, NULL);
      break;
    }
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
  
  gtk_tree_model_get (model, iter, COL_VALUE, &path, COL_TYPE, &pathtype, -1);
  
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
  GtkWidget             *frame;
  GtkWidget             *box;
  GtkWidget             *scroll;
  GtkTreeViewColumn     *col_icon;
  GtkTreeViewColumn     *col;
  GtkCellRenderer       *cell_icon;
  GtkCellRenderer       *cell;
  
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
                                          G_TYPE_POINTER,
                                          G_TYPE_STRING,
                                          G_TYPE_ULONG,
                                          G_TYPE_STRING);
  
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

  cell_icon = gtk_cell_renderer_pixbuf_new ();
  col_icon = gtk_tree_view_column_new_with_attributes (NULL, cell_icon, NULL);
  gtk_tree_view_column_set_cell_data_func (col_icon, cell_icon, cell_icon_data, NULL, NULL);
  gtk_tree_view_column_set_sizing (col_icon, GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_append_column (GTK_TREE_VIEW (plugin_data.view), col_icon);
  
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
  
  /* spinner */
  plugin_data.spinner = gtk_spinner_new ();
  gtk_spinner_start (GTK_SPINNER (plugin_data.spinner));
  gtk_box_pack_start (GTK_BOX (box), plugin_data.spinner, FALSE, TRUE, 5);
  gtk_widget_set_size_request (plugin_data.spinner, 24, 24);

  /* connect entry signals after the view is created as they use it */
  g_signal_connect (plugin_data.entry, "notify::text",
                    G_CALLBACK (on_entry_text_notify), NULL);
  g_signal_connect (plugin_data.entry, "activate",
                    G_CALLBACK (on_entry_activate), NULL);
  
  gtk_widget_show_all (frame);
  gtk_widget_hide (plugin_data.spinner);
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
  keybindings_set_item_full (group, KB_SHOW_PANEL_SYMBOLS, 0, 0,
                             "show_panel_symbols",
                             _("Show Command Panel (Symbols Only)"), NULL,
                             on_kb_show_panel, (gpointer) "s:", NULL);                             
  
  /* config */
  
  config = g_key_file_new ();
  plugin_data.config_file = g_strconcat (geany->app->configdir, G_DIR_SEPARATOR_S,
		                                     "plugins", G_DIR_SEPARATOR_S, 
                                         "commander", G_DIR_SEPARATOR_S, 
                                         "commander.conf", NULL);

  g_key_file_load_from_file (config, plugin_data.config_file, G_KEY_FILE_NONE, NULL);

  plugin_data.show_project_file_limit = utils_get_setting_integer (config,
		                                                               "commander", 
                                                                   "show_project_file_limit", 
                                                                   16);

  plugin_data.show_project_file = utils_get_setting_boolean (config,
                                                             "commander", 
                                                             "show_project_file", 
                                                             TRUE);

  plugin_data.show_project_file_skip_hidden_file =
    utils_get_setting_boolean (config,
                               "commander", 
                               "show_project_file_skip_hidden_file", 
                               FALSE);

  plugin_data.show_project_file_skip_hidden_dir =
    utils_get_setting_boolean (config,
                               "commander", 
                               "show_project_file_skip_hidden_dir", 
                               TRUE);

  plugin_data.show_symbol = utils_get_setting_boolean (config,
                                                       "commander", 
                                                       "show_symbol", 
                                                       TRUE);

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
  cancel_fill_store_project_files_task ();
  g_object_unref (plugin_data.show_project_file_task_cancellable);

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

  /* project file */
  plugin_data.show_project_file
    = ui_cfg_read_check_button (dialog, config,
                                "show_project_file_check_button",
                                "show_project_file");

  plugin_data.show_project_file_limit
    = ui_cfg_read_spin_button (dialog, config,
                               "show_project_file_limit_spin_button",
                               "show_project_file_limit");

  plugin_data.show_project_file_skip_hidden_file
    = ui_cfg_read_check_button (dialog, config,
                                "show_project_file_skip_hidden_file_check_button",
                                "show_project_file_skip_hidden_file");

  plugin_data.show_project_file_skip_hidden_dir
    = ui_cfg_read_check_button (dialog, config,
                                "show_project_file_skip_hidden_dir_check_button",
                                "show_project_file_skip_hidden_dir");

  /* symbols */
  plugin_data.show_symbol
    = ui_cfg_read_check_button (dialog, config,
                                "show_symbol_check_button",
                                "show_symbol");

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
  GtkWidget    *show_project_file_container;
  GtkWidget    *show_symbol_container;
  GtkWidget    *palette_container;
   
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);

  /* project files */
  show_project_file_container = ui_cfg_frame_new (vbox);

  ui_cfg_check_button_new (dialog, show_project_file_container,
                           "show_project_file_check_button",
                           _("Show project files"),
                           plugin_data.show_project_file);

  ui_cfg_spin_button_new (dialog, show_project_file_container,
                          "show_project_file_limit_spin_button",
                          _("Limit of displayed project files:"),
                          0, 1024, 1,
                          plugin_data.show_project_file_limit);

  ui_cfg_check_button_new (dialog, show_project_file_container,
                           "show_project_file_skip_hidden_file_check_button",
                           _("Don't show hidden project files"),
                           plugin_data.show_project_file_skip_hidden_file);

  ui_cfg_check_button_new (dialog, show_project_file_container,
                           "show_project_file_skip_hidden_dir_check_button",
                           _("Don't show hidden project directories"),
                           plugin_data.show_project_file_skip_hidden_dir);

  /* symbols */
  show_symbol_container = ui_cfg_frame_new (vbox);

  ui_cfg_check_button_new (dialog, show_symbol_container,
                           "show_symbol_check_button",
                           _("Show symbols"),
                           plugin_data.show_symbol);

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
