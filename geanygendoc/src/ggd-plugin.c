/*
 *  
 *  GeanyGenDoc, a Geany plugin to ease generation of source code documentation
 *  Copyright (C) 2010  Colomban Wendling <ban@herbesfolles.org>
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

#include "ggd-plugin.h"

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gdk/gdkkeysyms.h> /* for the key bindings */
#include <ctpl/ctpl.h>
#include <geanyplugin.h>

#include "ggd.h"
#include "ggd-file-type.h"
#include "ggd-file-type-manager.h"
#include "ggd-tag-utils.h"
#include "ggd-options.h"



/*
 * Questions:
 *  * how to update tag list? (tm_source_file_buffer_update() is not found in
 *    symbols table)
 */

/* These items are set by Geany before plugin_init() is called. */
GeanyPlugin     *geany_plugin;
GeanyData       *geany_data;
GeanyFunctions  *geany_functions;

/* TODO check minimum requierment */
PLUGIN_VERSION_CHECK (188)

PLUGIN_SET_TRANSLATABLE_INFO (
  LOCALEDIR, GETTEXT_PACKAGE,
  _("Documentation Generator"),
  _("Generates documentation basis from source code"),
  GGD_PLUGIN_VERSION,
  "Colomban Wendling <ban@herbesfolles.org>"
)

enum
{
  KB_INSERT,
  NUM_KB
};

PLUGIN_KEY_GROUP (GGD_PLUGIN_ONAME, NUM_KB)

typedef struct _PluginData
{
  GgdOptGroup *config;
  
  gint        editor_menu_popup_line;
  
  GtkWidget  *separator_item;
  GtkWidget  *edit_menu_item;
  GtkWidget  *tools_menu_item;
  gulong      edit_menu_item_hid;
} PluginData;

#define plugin (&plugin_data)
static PluginData plugin_data = {
  NULL, 0, NULL, NULL, NULL, 0l
};

/* global plugin options
 * default values that needs to be set dynamically goes at the top of
 * load_configuration() */
gchar      *GGD_OPT_doctype[GEANY_MAX_BUILT_IN_FILETYPES] = { NULL };
gboolean    GGD_OPT_save_to_refresh                       = FALSE;
gboolean    GGD_OPT_indent                                = TRUE;
gchar      *GGD_OPT_environ                               = NULL;


/* Gets an element of GGD_OPT_doctype, falling back to GGD_OPT_doctype[0]
 * (default) if the requested element is not set */
const gchar *
ggd_plugin_get_doctype (filetype_id id)
{
  const gchar *doctype;
  
  g_return_val_if_fail (id >= 0 && id < GEANY_MAX_BUILT_IN_FILETYPES, NULL);
  
  doctype = GGD_OPT_doctype[id];
  if (! doctype || ! *doctype) {
    doctype = GGD_OPT_doctype[0];
  }
  
  return doctype;
}


/* FIXME: tm_source_file_buffer_update() is not found in symbols table */
/* (tries to) refresh the tag list to the file's current state */
static void
refresh_tag_list (TMWorkObject    *tm_wo,
                  ScintillaObject *sci,
                  GeanyDocument   *doc)
{
  /*
  gint    len;
  guchar *buf;
  
  len = sci_get_length (sci);
  buf = g_malloc (len + 1);
  sci_get_text (sci, len + 1, (gchar *)buf);
  tm_source_file_buffer_update (tm_wo, buf, len, TRUE);
  g_free (buf);
  //*/
  if (GGD_OPT_save_to_refresh) {
    document_save_file (doc, FALSE);
  }
}

/* tries to insert a comment in the current document
 * @line: The line for which insert comment, or -1 for the current line */
static void
insert_comment (gint line)
{
  GeanyDocument *doc;
  
  doc = document_get_current ();
  if (DOC_VALID (doc)) {
    /* try to ensure tags corresponds to the actual state of the file */
    refresh_tag_list (doc->tm_file, doc->editor->sci, doc);
    if (line < 0) {
      line = sci_get_current_line (doc->editor->sci);
    }
    ggd_insert_comment (doc, line, ggd_plugin_get_doctype (doc->file_type->id));
  }
}

/* tries to insert comments for all the document */
static void
insert_all_comments (void)
{
  GeanyDocument *doc;
  
  doc = document_get_current ();
  if (DOC_VALID (doc)) {
    /* try to ensure tags corresponds to the actual state of the file */
    refresh_tag_list (doc->tm_file, doc->editor->sci, doc);
    ggd_insert_all_comments (doc, ggd_plugin_get_doctype (doc->file_type->id));
  }
}

/* Escapes a string to use it safely as a GKeyFile key.
 * It currently replaces only "=" and "#" since GKeyFile supports UTF-8 keys. */
static gchar *
normalize_key (const gchar *key)
{
  GString *nkey;
  
  nkey = g_string_new (NULL);
  for (; *key; key++) {
    switch (*key) {
      case '=': g_string_append (nkey, "Equal"); break;
      case '#': g_string_append (nkey, "Sharp"); break;
      default:  g_string_append_c (nkey, *key); break;
    }
  }
  
  return g_string_free (nkey, FALSE);
}

static gboolean
load_configuration (void)
{
  gboolean  success = FALSE;
  gchar    *conffile;
  GError   *err = NULL;
  guint     i;
  
  /* default options that needs to be set dynamically */
  GGD_OPT_doctype[0] = g_strdup ("doxygen");
  
  plugin->config = ggd_opt_group_new ("General");
  ggd_opt_group_add_string (plugin->config, &GGD_OPT_doctype[0], "doctype");
  for (i = 1; i < GEANY_MAX_BUILT_IN_FILETYPES; i++) {
    gchar *name;
    gchar *normal_ftname;
    
    normal_ftname = normalize_key (filetypes[i]->name);
    name = g_strconcat ("doctype_", normal_ftname, NULL);
    ggd_opt_group_add_string (plugin->config, &GGD_OPT_doctype[i], name);
    g_free (name);
    g_free (normal_ftname);
  }
  ggd_opt_group_add_boolean (plugin->config, &GGD_OPT_save_to_refresh, "save_to_refresh");
  ggd_opt_group_add_boolean (plugin->config, &GGD_OPT_indent, "indent");
  ggd_opt_group_add_string (plugin->config, &GGD_OPT_environ, "environ");
  conffile = ggd_get_config_file ("ggd.conf", NULL, GGD_PERM_R, &err);
  if (conffile) {
    success = ggd_opt_group_load_from_file (plugin->config, conffile, &err);
  }
  if (err) {
    GLogLevelFlags level = G_LOG_LEVEL_WARNING;
    
    if (err->domain == G_FILE_ERROR && err->code == G_FILE_ERROR_NOENT) {
      level = G_LOG_LEVEL_INFO;
    }
    g_log (G_LOG_DOMAIN, level,
           _("Failed to load configuration: %s"), err->message);
    g_error_free (err);
  }
  g_free (conffile);
  /* init filetype manager */
  ggd_file_type_manager_init ();
  
  return success;
}

static void
unload_configuration (void)
{
  gchar    *conffile;
  GError   *err = NULL;
  
  conffile = ggd_get_config_file ("ggd.conf", NULL, GGD_PERM_RW, &err);
  if (conffile) {
    ggd_opt_group_write_to_file (plugin->config, conffile, &err);
  }
  if (err) {
    g_warning (_("Failed to save configuration: %s"), err->message);
    g_error_free (err);
  }
  g_free (conffile);
  ggd_opt_group_free (plugin->config, TRUE);
  plugin->config = NULL;
  /* uninit filetype manager */
  ggd_file_type_manager_uninit ();
}

/* forces reloading of configuration files */
static gboolean
reload_configuration (void)
{
  unload_configuration ();
  return load_configuration ();
}


/* --- Actual Geany interaction --- */

/* handler for the editor's popup menu entry the plugin adds */
static void
editor_menu_acivated_handler (GtkMenuItem *menu_item,
                              PluginData  *pdata)
{
  (void)menu_item;
  
  insert_comment (pdata->editor_menu_popup_line);
}

/* hanlder for the keybinding that inserts a comment */
static void
insert_comment_keybinding_handler (guint key_id)
{
  (void)key_id;
  
  insert_comment (-1);
}

/* handler for the GeanyDocument::update-editor-menu signal.
 * It is used to get the line at which menu popuped */
static void
update_editor_menu_handler (GObject        *obj,
                            const gchar    *word,
                            gint            pos,
                            GeanyDocument  *doc,
                            PluginData     *pdata)
{
  pdata->editor_menu_popup_line = sci_get_line_from_position (doc->editor->sci,
                                                              pos);
}

/* handler that opens the current filetype's configuration file */
static void
open_current_filetype_conf_handler (GtkWidget  *widget,
                                    gpointer    data)
{
  GeanyDocument *doc;
  
  (void)widget;
  (void)data;
  
  doc = document_get_current ();
  if (DOC_VALID (doc)) {
    gchar  *path;
    GError *err = NULL;
    
    path = ggd_file_type_manager_get_conf_path (doc->file_type->id,
                                                GGD_PERM_R | GGD_PERM_W, &err);
    if (! path) {
      msgwin_status_add (_("Failed to find configuration file "
                           "for file type \"%s\": %s"),
                         doc->file_type->name, err->message);
      g_error_free (err);
    } else {
      document_open_file (path, FALSE, NULL, NULL);
      g_free (path);
    }
  }
}

static void
open_manual_handler (GtkWidget  *widget,
                     gpointer    data)
{
  utils_open_browser (DOCDIR "/" GGD_PLUGIN_CNAME "/html/manual.html");
}

/* handler that reloads the configuration */
static void
reload_configuration_hanlder (GtkWidget  *widget,
                              gpointer    data)
{
  (void)widget;
  (void)data;
  
  reload_configuration ();
}

/* handler that documents current symbol */
static void
document_current_symbol_handler (GObject   *obj,
                                 gpointer   data)
{
  insert_comment (-1);
}

/* handler that documents all symbols */
static void
document_all_symbols_handler (GObject  *obj,
                              gpointer  data)
{
  insert_all_comments ();
}


/* FIXME: make menu item appear in the edit menu too */
/* adds the menu item in the editor's popup menu */
static void
add_edit_menu_item (PluginData *pdata)
{
  GtkWidget *parent_menu;
  
  parent_menu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (
    ui_lookup_widget (geany->main_widgets->editor_menu, "comments")));
  if (! parent_menu) {
    parent_menu = geany->main_widgets->editor_menu;
    pdata->separator_item = gtk_separator_menu_item_new ();
    gtk_menu_shell_append (GTK_MENU_SHELL (parent_menu), pdata->separator_item);
    gtk_widget_show (pdata->separator_item);
  }
  pdata->edit_menu_item = gtk_menu_item_new_with_label (_("Insert Documentation Comment"));
  pdata->edit_menu_item_hid = g_signal_connect (pdata->edit_menu_item, "activate",
                                                G_CALLBACK (editor_menu_acivated_handler),
                                                pdata);
  gtk_menu_shell_append (GTK_MENU_SHELL (parent_menu), pdata->edit_menu_item);
  gtk_widget_show (pdata->edit_menu_item);
  /* make item document-presence sensitive */
  ui_add_document_sensitive (pdata->edit_menu_item);
  /* and attach a keybinding */
  keybindings_set_item (plugin_key_group, KB_INSERT, insert_comment_keybinding_handler,
                        GDK_d, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                        "instert_doc", _("Insert Documentation Comment"),
                        pdata->edit_menu_item);
}

/* removes the menu item in the editor's popup menu */
static void
remove_edit_menu_item (PluginData *pdata)
{
  g_signal_handler_disconnect (pdata->edit_menu_item, pdata->edit_menu_item_hid);
  pdata->edit_menu_item_hid = 0l;
  if (pdata->separator_item) {
    gtk_widget_destroy (pdata->separator_item);
  }
  gtk_widget_destroy (pdata->edit_menu_item);
}

static GtkWidget *
menu_add_item (GtkMenuShell  *menu,
               const gchar   *mnemonic,
               const gchar   *tooltip,
               const gchar   *stock_image,
               GCallback      activate_handler,
               gpointer       activate_data)
{
  GtkWidget *item;
  
  if (! stock_image) {
    item = gtk_menu_item_new_with_mnemonic (mnemonic);
  } else {
    item = gtk_image_menu_item_new_with_mnemonic (mnemonic);
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
                                   gtk_image_new_from_stock (stock_image,
                                                             GTK_ICON_SIZE_MENU));
  }
  ui_widget_set_tooltip_text (item, tooltip);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  if (activate_handler) {
    g_signal_connect (item, "activate", activate_handler, activate_data);
  }
  
  return item;
}

/* creates plugin's tool's menu */
static GtkWidget *
create_tools_menu_item (void)
{
  GtkWidget  *menu;
  GtkWidget  *item;
  
  /* build submenu */
  menu = gtk_menu_new ();
  /* build "document current symbol" item */
  item = menu_add_item (GTK_MENU_SHELL (menu),
                        _("_Document Current Symbol"),
                        _("Generate documentation for the current symbol"),
                        NULL,
                        G_CALLBACK (document_current_symbol_handler), NULL);
  ui_add_document_sensitive (item);
  /* build "document all" item */
  item = menu_add_item (GTK_MENU_SHELL (menu),
                        _("Document _All Symbols"),
                        _("Generate documentation for all symbols in the "
                          "current document"),
                        NULL,
                        G_CALLBACK (document_all_symbols_handler), NULL);
  ui_add_document_sensitive (item);
  /* separator */
  item = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  /* build "reload" item */
  item = menu_add_item (GTK_MENU_SHELL (menu),
                        _("_Reload Configuration Files"),
                        _("Force reloading of the configuration files"),
                        GTK_STOCK_REFRESH,
                        G_CALLBACK (reload_configuration_hanlder), NULL);
  /* language filetypes opener */
  item = menu_add_item (GTK_MENU_SHELL (menu),
                        _("_Edit Current Language Configuration"),
                        _("Open the current language configuration file for "
                          "editing"),
                        GTK_STOCK_EDIT,
                        G_CALLBACK (open_current_filetype_conf_handler), NULL);
  ui_add_document_sensitive (item);
  /* separator */
  item = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  /* help/manual opening */
  item = menu_add_item (GTK_MENU_SHELL (menu),
                        _("Open _Manual"),
                        _("Open the manual in a browser"),
                        GTK_STOCK_HELP,
                        G_CALLBACK (open_manual_handler), NULL);
  /* build tools menu item */
  item = gtk_menu_item_new_with_mnemonic (_("_Documentation Generator"));
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu);
  gtk_widget_show_all (item);
  
  return item;
}

/* build plugin's menus */
static void
build_menus (PluginData *pdata)
{
  add_edit_menu_item (pdata);
  pdata->tools_menu_item = create_tools_menu_item ();
  gtk_menu_shell_append (GTK_MENU_SHELL (geany->main_widgets->tools_menu),
                         pdata->tools_menu_item);
}

/* destroys plugin's menus */
static void
destroy_menus (PluginData *pdata)
{
  gtk_widget_destroy (pdata->tools_menu_item);
  pdata->tools_menu_item = NULL;
  remove_edit_menu_item (pdata);
}

void
plugin_init (GeanyData *data G_GNUC_UNUSED)
{
  load_configuration ();
  build_menus (plugin);
  plugin_signal_connect (geany_plugin, NULL, "update-editor-menu", FALSE,
                         G_CALLBACK (update_editor_menu_handler), plugin);
}

void
plugin_cleanup (void)
{
  destroy_menus (plugin);
  unload_configuration ();
}

void
plugin_help (void)
{
  open_manual_handler (NULL, NULL);
}


/* --- Configuration dialog --- */

#include "ggd-widget-frame.h"
#include "ggd-widget-doctype-selector.h"

static GtkWidget *GGD_W_doctype_selector;

static void
conf_dialog_response_handler (GtkDialog  *dialog,
                              gint        response_id,
                              PluginData *pdata)
{
  switch (response_id) {
    case GTK_RESPONSE_ACCEPT:
    case GTK_RESPONSE_APPLY:
    case GTK_RESPONSE_OK:
    case GTK_RESPONSE_YES: {
      guint i;
      
      ggd_opt_group_sync_from_proxies (pdata->config);
      for (i = 0; i < GEANY_MAX_BUILT_IN_FILETYPES; i++) {
        g_free (GGD_OPT_doctype[i]);
        GGD_OPT_doctype[i] = ggd_doctype_selector_get_doctype (GGD_DOCTYPE_SELECTOR (GGD_W_doctype_selector),
                                                               i);
      }
      break;
    }
    
    default: break;
  }
}

GtkWidget *
plugin_configure (GtkDialog *dialog)
{
  GtkWidget  *box;
  GtkWidget  *box2;
  GtkWidget  *frame;
  GtkWidget  *widget;
  guint       i;
  
  g_signal_connect (dialog, "response",
                    G_CALLBACK (conf_dialog_response_handler), plugin);
  
  box = gtk_vbox_new (FALSE, 12);
  
  /* General */
  frame = ggd_frame_new (_("General"));
  gtk_box_pack_start (GTK_BOX (box), frame, FALSE, FALSE, 0);
  box2 = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frame), box2);
  /* auto-save */
  widget = gtk_check_button_new_with_mnemonic (_("_Save file before generating "
                                                 "documentation"));
  ui_widget_set_tooltip_text (widget,
    _("Whether the current document should be saved to disc before generating "
      "the documentation. This is a technical detail, but it is currently "
      "needed to have an up-to-date tag list. If you disable this option and "
      "ask for documentation generation on a modified document, the behavior "
      "may be surprising since the comment will be generated for the last "
      "saved state of this document and not the current one."));
  ggd_opt_group_set_proxy_gtktogglebutton (plugin->config, &GGD_OPT_save_to_refresh,
                                           widget);
  gtk_box_pack_start (GTK_BOX (box2), widget, FALSE, FALSE, 0);
  /* indent */
  widget = gtk_check_button_new_with_mnemonic (_("_Indent inserted documentation"));
  ui_widget_set_tooltip_text (widget,
    _("Whether the inserted documentation should be indented to fit the "
      "indentation at the insertion position."));
  ggd_opt_group_set_proxy_gtktogglebutton (plugin->config, &GGD_OPT_indent,
                                           widget);
  gtk_box_pack_start (GTK_BOX (box2), widget, FALSE, FALSE, 0);
  
  /* Documentation type */
  frame = ggd_frame_new (_("Documentation type"));
  gtk_box_pack_start (GTK_BOX (box), frame, TRUE, TRUE, 0);
  box2 = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frame), box2);
  GGD_W_doctype_selector = ggd_doctype_selector_new ();
  for (i = 0; i < GEANY_MAX_BUILT_IN_FILETYPES; i++) {
    ggd_doctype_selector_set_doctype (GGD_DOCTYPE_SELECTOR (GGD_W_doctype_selector),
                                      i, GGD_OPT_doctype[i]);
  }
  ui_widget_set_tooltip_text (GGD_W_doctype_selector,
    _("Choose the documentation type to use with each file type. The special "
      "language \"All\" on top of the list is used to choose the default "
      "documentation type, used for all languages that haven't one set."));
  gtk_box_pack_start (GTK_BOX (box2), GGD_W_doctype_selector, TRUE, TRUE, 0);
  
  /* Environ editor */
  widget = ggd_frame_new (_("Global environment"));
  ui_widget_set_tooltip_text (widget,
    _("Global environment overrides and additions. This environment will be "
      "merged with the file-type-specific ones."));
  {
    GtkWidget *scrolled;
    GtkWidget *view;
    GObject   *proxy;
    
    scrolled = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled),
                                         GTK_SHADOW_IN);
    gtk_container_add (GTK_CONTAINER (widget), scrolled);
    view = gtk_text_view_new ();
    proxy = G_OBJECT (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));
    ggd_opt_group_set_proxy (plugin->config, &GGD_OPT_environ, proxy, "text");
    gtk_container_add (GTK_CONTAINER (scrolled), view);
  }
  gtk_box_pack_start (GTK_BOX (box), widget, TRUE, TRUE, 0);
  
  
  gtk_widget_show_all (box);
  
  return box;
}
