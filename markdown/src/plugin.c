/*
 * plugin.c - Part of the Geany Markdown plugin
 *
 * Copyright 2012 Matthew Brush <mbrush@codebrainz.ca>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include <geanyplugin.h>
#include "viewer.h"
#include "tmplmgr.h"
#include "conf.h"

GeanyData      *geany_data;
GeanyPlugin    *geany_plugin;
GeanyFunctions *geany_functions;

PLUGIN_VERSION_CHECK(211)

PLUGIN_SET_INFO("Markdown",
                _("Real-time Markdown preview"),
                "0.01",
                "Matthew Brush <mbrush@codebrainz.ca>")

/* Should be defined by build system, this is just a fallback */
#ifndef MARKDOWN_DATA_DIR
#  define MARKDOWN_DATA_DIR "/usr/local/share/geany-plugins/markdown"
#endif
#ifndef MARKDOWN_DOC_DIR
#  define MARKDOWN_DOC_DIR "/usr/local/share/doc/geany-plugins/markdown"
#endif

/* Global data */
typedef struct MarkdownPlugin {
  struct {
    guint update_view;
    guint save_conf;
  } handler_ids;
  GtkWidget *menu_item;
  MarkdownViewer *viewer;
  MarkdownConf *conf;
} MarkdownPlugin;
MarkdownPlugin markdown_plugin = { { 0, 0 }, NULL, NULL, NULL };

/* Forward declarations */
static gboolean on_conf_idle_handler(MarkdownPlugin *plugin);
static void handle_conf_save_later(MarkdownPlugin *plugin);
static gboolean on_idle_handler(MarkdownPlugin *plugin);
static void handle_update_later(MarkdownPlugin *plugin);
static gboolean on_editor_notify(GObject *obj, GeanyEditor *editor, SCNotification *notif, MarkdownPlugin *plugin);
static void on_document_signal(GObject *obj, GeanyDocument *doc, MarkdownPlugin *plugin);
static void on_document_filetype_set(GObject *obj, GeanyDocument *doc, GeanyFiletype *ft_old, MarkdownPlugin *plugin);
static void on_template_menu_item_activate(GtkCheckMenuItem *item, MarkdownPlugin *plugin);
static void on_view_position_menu_item_activate(GtkCheckMenuItem *item, MarkdownPlugin *plugin);
static void on_view_readme_activate(GtkMenuItem *item, MarkdownPlugin *plugin);
static GSList *get_all_templates(void);

/* Main plugin entry point on plugin load. */
void plugin_init(GeanyData *data)
{
  GtkMenu *menu, *tmpl_menu, *pos_menu;
  GtkWidget *itm, *tmpl_item, *pos_item;
  GSList *iter, *templates;
  GSList *radio_group = NULL;
  gchar *conf_fn, *conf_tmpl_name, *readme_fn;
  MarkdownViewPosition view_pos;

  conf_fn = g_build_filename(geany->app->configdir, "plugins", "markdown",
    "markdown.conf", NULL);
  markdown_plugin.conf = markdown_conf_new(conf_fn);
  g_free(conf_fn);
  conf_tmpl_name = markdown_conf_get_template_name(markdown_plugin.conf);
  view_pos = markdown_conf_get_view_position(markdown_plugin.conf);

  switch (view_pos) {
    case MARKDOWN_VIEW_POS_MSGWIN:
      markdown_plugin.viewer = markdown_viewer_new(
        GTK_NOTEBOOK(geany->main_widgets->message_window_notebook));
      break;
    case MARKDOWN_VIEW_POS_SIDEBAR:
    default:
      markdown_plugin.viewer = markdown_viewer_new(
        GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook));
      break;
  }
  markdown_conf_set_view_position(markdown_plugin.conf, view_pos);
  handle_conf_save_later(&markdown_plugin);

  markdown_plugin.menu_item = gtk_menu_item_new_with_label(_("Markdown"));
  menu = GTK_MENU(gtk_menu_new());
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(markdown_plugin.menu_item), GTK_WIDGET(menu));
  gtk_menu_shell_append(GTK_MENU_SHELL(geany->main_widgets->tools_menu), markdown_plugin.menu_item);

  pos_item = gtk_menu_item_new_with_label(_("Position"));
  pos_menu = GTK_MENU(gtk_menu_new());
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(pos_item), GTK_WIDGET(pos_menu));
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), pos_item);

  itm = gtk_radio_menu_item_new_with_label(radio_group, _("Sidebar"));
  radio_group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(itm));
  gtk_menu_shell_append(GTK_MENU_SHELL(pos_menu), itm);
  g_object_set_data(G_OBJECT(itm), "pos",
    GINT_TO_POINTER((gint) MARKDOWN_VIEW_POS_SIDEBAR));
  g_signal_connect(G_OBJECT(itm), "activate",
    G_CALLBACK(on_view_position_menu_item_activate), &markdown_plugin);
  if (view_pos == MARKDOWN_VIEW_POS_SIDEBAR)
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(itm), TRUE);

  itm = gtk_radio_menu_item_new_with_label(radio_group, _("Message Window"));
  radio_group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(itm));
  gtk_menu_shell_append(GTK_MENU_SHELL(pos_menu), itm);
  g_object_set_data(G_OBJECT(itm), "pos",
    GINT_TO_POINTER((gint) MARKDOWN_VIEW_POS_MSGWIN));
  g_signal_connect(G_OBJECT(itm), "activate",
    G_CALLBACK(on_view_position_menu_item_activate), &markdown_plugin);
  if (view_pos == MARKDOWN_VIEW_POS_MSGWIN)
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(itm), TRUE);
  /* ... */
  radio_group = NULL; /* for tmpl items */

  tmpl_item = gtk_menu_item_new_with_label(_("Template"));
  tmpl_menu = GTK_MENU(gtk_menu_new());
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(tmpl_item), GTK_WIDGET(tmpl_menu));
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), tmpl_item);

  templates = get_all_templates();
  for (iter = templates; iter != NULL; iter = g_slist_next(iter)) {
    MarkdownTemplate *t = (MarkdownTemplate*) iter->data;
    itm = gtk_radio_menu_item_new_with_label(radio_group,
      markdown_template_get_name(t));
    radio_group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(itm));
    /* Menu items take ownership of the templates */
    g_object_set_data_full(G_OBJECT(itm), "tmpl", t, (GDestroyNotify) markdown_template_free);
    gtk_menu_shell_append(GTK_MENU_SHELL(tmpl_menu), itm);
    g_signal_connect(G_OBJECT(itm), "activate", G_CALLBACK(on_template_menu_item_activate),
      &markdown_plugin);
    if (g_strcmp0(markdown_template_get_name(t), conf_tmpl_name) == 0) {
      gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(itm), TRUE);
      markdown_viewer_set_template(markdown_plugin.viewer, t);
      ui_set_statusbar(TRUE, _("Activated Markdown template: %s"),
        markdown_template_get_name(t));
    }
  }
  g_slist_free(templates);
  g_free(conf_tmpl_name);

  itm = gtk_menu_item_new_with_label("README.md");
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), itm);
  g_signal_connect(G_OBJECT(itm), "activate", G_CALLBACK(on_view_readme_activate),
    &markdown_plugin);

  gtk_widget_show_all(markdown_plugin.menu_item);

  plugin_signal_connect(geany_plugin, NULL, "editor-notify", TRUE,
    G_CALLBACK(on_editor_notify), &markdown_plugin);
  plugin_signal_connect(geany_plugin, NULL, "document-activate", TRUE,
    G_CALLBACK(on_document_signal), &markdown_plugin);
  plugin_signal_connect(geany_plugin, NULL, "document-filetype-set", TRUE,
    G_CALLBACK(on_document_filetype_set), &markdown_plugin);
  plugin_signal_connect(geany_plugin, NULL, "document-new", TRUE,
    G_CALLBACK(on_document_signal), &markdown_plugin);
  plugin_signal_connect(geany_plugin, NULL, "document-open", TRUE,
    G_CALLBACK(on_document_signal), &markdown_plugin);
  plugin_signal_connect(geany_plugin, NULL, "document-reload", TRUE,
    G_CALLBACK(on_document_signal), &markdown_plugin);

  handle_update_later(&markdown_plugin);

  /* Prevent segmentation fault when plugin is reloaded. */
  plugin_module_make_resident(geany_plugin);

  /* On the first load show the README.md file */
  if (!markdown_conf_get_readme_shown(markdown_plugin.conf)) {
    readme_fn = g_build_filename(MARKDOWN_DOC_DIR, "README.md", NULL);
    document_open_file(readme_fn, TRUE, NULL, NULL);
    g_free(readme_fn);
    markdown_conf_set_readme_shown(markdown_plugin.conf, TRUE);
    handle_conf_save_later(&markdown_plugin);
  }
}

/* Cleanup resources on plugin unload. */
void plugin_cleanup(void)
{
  markdown_conf_save(markdown_plugin.conf);
  markdown_conf_free(markdown_plugin.conf);
  gtk_widget_destroy(markdown_plugin.menu_item);
  markdown_viewer_free(markdown_plugin.viewer);
  return;
}

/* Save configuration file when idle. */
static gboolean on_conf_idle_handler(MarkdownPlugin *plugin)
{
  markdown_conf_save(plugin->conf);
  plugin->handler_ids.save_conf = 0;
  return FALSE;
}

/* Queue a configuration file save later if one isn't queued. */
static void handle_conf_save_later(MarkdownPlugin *plugin)
{
  if (plugin->handler_ids.save_conf == 0) {
    plugin->handler_ids.save_conf = plugin_idle_add(geany_plugin,
      (GSourceFunc) on_conf_idle_handler, plugin);
  }
}

/* Update markdown preview when idle. */
static gboolean on_idle_handler(MarkdownPlugin *plugin)
{
  gchar *md_text;
  GeanyDocument *doc = document_get_current();

  /* Only handle valid Markdown documents */
  if (!DOC_VALID(doc) || g_strcmp0(doc->file_type->name, "Markdown") != 0) {
    markdown_viewer_update_content(plugin->viewer,
      _("The current document does not have a Markdown filetype."), "UTF-8");
    plugin->handler_ids.update_view = 0;
    return FALSE;
  }

  md_text = (gchar*) scintilla_send_message(doc->editor->sci,
    SCI_GETCHARACTERPOINTER, 0, 0);
  markdown_viewer_update_content(plugin->viewer, md_text, doc->encoding);

  plugin->handler_ids.update_view = 0;

  return FALSE;
}

/* Queue update of the markdown view later if an update isn't queued. */
static void handle_update_later(MarkdownPlugin *plugin)
{
  if (plugin->handler_ids.update_view == 0) {
    plugin->handler_ids.update_view = plugin_idle_add(geany_plugin,
      (GSourceFunc) on_idle_handler, plugin);
  }
}

/* Queue update of the markdown preview on editor text change. */
static gboolean on_editor_notify(GObject *obj, GeanyEditor *editor,
  SCNotification *notif, MarkdownPlugin *plugin)
{
  GeanyDocument *doc = document_get_current();

  if (DOC_VALID(doc) && g_strcmp0(doc->file_type->name, "Markdown") == 0 &&
      notif->nmhdr.code == SCN_MODIFIED &&
      ((notif->modificationType & SC_MOD_INSERTTEXT) || (notif->modificationType & SC_MOD_DELETETEXT)) &&
      notif->length > 0)
  {
    handle_update_later(plugin);
  }

  return FALSE;
}

/* Queue update of the markdown preview on document signals (new, open,
 * activate, etc.) */
static void on_document_signal(GObject *obj, GeanyDocument *doc, MarkdownPlugin *plugin)
{
  handle_update_later(plugin);
}

/* Queue update of the markdown preview when a document's filetype is set */
static void on_document_filetype_set(GObject *obj, GeanyDocument *doc, GeanyFiletype *ft_old,
  MarkdownPlugin *plugin)
{
  handle_update_later(plugin);
}

/* Change the active markdown preview template when the menu item is clicked. */
static void on_template_menu_item_activate(GtkCheckMenuItem *item, MarkdownPlugin *plugin)
{
  MarkdownTemplate *tmpl;

  { /* FIXME: this is stupid */
    static gboolean inhibit = FALSE;
    if (inhibit) { return; }
    inhibit = TRUE;
    gtk_check_menu_item_set_active(item, !gtk_check_menu_item_get_active(item));
    inhibit = FALSE;
  }

  tmpl = (MarkdownTemplate *) g_object_get_data(G_OBJECT(item), "tmpl");
  markdown_viewer_set_template(plugin->viewer, tmpl);

  handle_update_later(plugin);

  ui_set_statusbar(TRUE, _("Activated Markdown template: %s"),
    markdown_template_get_name(tmpl));

  markdown_conf_set_template_name(plugin->conf, markdown_template_get_name(tmpl));
  handle_conf_save_later(plugin);
}

/* Change the position/location of the view (ex. sidebar, msgwin, etc.) */
static void on_view_position_menu_item_activate(GtkCheckMenuItem *item, MarkdownPlugin *plugin)
{
  gpointer pos_as_ptr = g_object_get_data(G_OBJECT(item), "pos");
  gint pos = GPOINTER_TO_INT(pos_as_ptr);
  MarkdownViewPosition old_pos = markdown_conf_get_view_position(plugin->conf);

  /* It hasn't changed positions */
  if ((gint) old_pos == pos)
    return;

  switch (pos) {
    case MARKDOWN_VIEW_POS_MSGWIN:
      markdown_viewer_set_notebook(plugin->viewer,
        GTK_NOTEBOOK(geany->main_widgets->message_window_notebook));
      break;
    case MARKDOWN_VIEW_POS_SIDEBAR:
    default:
      markdown_viewer_set_notebook(plugin->viewer,
        GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook));
      break;
  }

  markdown_conf_set_view_position(plugin->conf, (MarkdownViewPosition) pos);
  handle_conf_save_later(plugin);
}

static void on_view_readme_activate(GtkMenuItem *item, MarkdownPlugin *plugin)
{
  gchar *readme_fn = g_build_filename(MARKDOWN_DOC_DIR, "README.md", NULL);
  document_open_file(readme_fn, TRUE, NULL, NULL);
  g_free(readme_fn);
}

/* Retrieve a list of all templates in user config dir and sys config dir */
static GSList *get_all_templates(void)
{
  GSList *list;
  gchar *usr_dir, *sys_dir;
  MarkdownTemplateManager *mgr;

  usr_dir = g_build_filename(geany->app->configdir, "plugins", "markdown", "templates", NULL);
  sys_dir = g_build_filename(MARKDOWN_DATA_DIR, "templates", NULL);

  mgr = markdown_template_manager_new(usr_dir, sys_dir);
  list = markdown_template_manager_list_templates(mgr);
  markdown_template_manager_free(mgr);

  g_free(usr_dir);
  g_free(sys_dir);

  return list;
}
