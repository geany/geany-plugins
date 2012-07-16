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
  MarkdownConfig *config;
} MarkdownPlugin;
MarkdownPlugin markdown_plugin = { { 0, 0 }, NULL, NULL, NULL };

/* Forward declarations */
static gboolean on_idle_handler(MarkdownPlugin *plugin);
static void handle_update_later(MarkdownPlugin *plugin);
static gboolean on_editor_notify(GObject *obj, GeanyEditor *editor, SCNotification *notif, MarkdownPlugin *plugin);
static void on_document_signal(GObject *obj, GeanyDocument *doc, MarkdownPlugin *plugin);
static void on_document_filetype_set(GObject *obj, GeanyDocument *doc, GeanyFiletype *ft_old, MarkdownPlugin *plugin);

static void 
on_conf_prop_notify(GObject *obj, GParamSpec *pspec, MarkdownPlugin *plugin)
{
  handle_update_later(plugin);
}

/* Main plugin entry point on plugin load. */
void plugin_init(GeanyData *data)
{
  gchar *conf_fn;
  MarkdownConfigViewPos view_pos;

  conf_fn = g_build_filename(geany->app->configdir, "plugins", "markdown", "markdown.conf", NULL);
  markdown_plugin.config = markdown_config_new(conf_fn);
  g_free(conf_fn);
  
  g_signal_connect(markdown_plugin.config, "notify",
    G_CALLBACK(on_conf_prop_notify), &markdown_plugin);

  g_object_get(markdown_plugin.config, "view-pos", &view_pos, NULL);

  switch (view_pos) {
    case MARKDOWN_CONFIG_VIEW_POS_MSGWIN:
      markdown_plugin.viewer = markdown_viewer_new(
        GTK_NOTEBOOK(geany->main_widgets->message_window_notebook));
      break;
    case MARKDOWN_CONFIG_VIEW_POS_SIDEBAR:
    default:
      markdown_plugin.viewer = markdown_viewer_new(
        GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook));
      break;
  }

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
}

/* Cleanup resources on plugin unload. */
void plugin_cleanup(void)
{
  g_object_unref(markdown_plugin.config);
  markdown_viewer_free(markdown_plugin.viewer);
  return;
}

GtkWidget *plugin_configure(GtkDialog *dialog)
{
  return markdown_config_gui(markdown_plugin.config, dialog);
}

/* Update markdown preview when idle. */
static gboolean on_idle_handler(MarkdownPlugin *plugin)
{
  gchar *md_text;
  GeanyDocument *doc = document_get_current();
  MarkdownConfigViewPos view_pos;

  g_object_get(plugin->config, "view-pos", &view_pos, NULL);
  
  switch (view_pos) {
    case MARKDOWN_CONFIG_VIEW_POS_MSGWIN:
      markdown_viewer_set_notebook(plugin->viewer,
        GTK_NOTEBOOK(geany->main_widgets->message_window_notebook));
      break;
    case MARKDOWN_CONFIG_VIEW_POS_SIDEBAR:
    default:
      markdown_viewer_set_notebook(plugin->viewer,
        GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook));
      break;
  }

  /* Only handle valid Markdown documents */
  if (!DOC_VALID(doc) || g_strcmp0(doc->file_type->name, "Markdown") != 0) {
    markdown_viewer_load_markdown_string(plugin->viewer,
      _("The current document does not have a Markdown filetype."), "UTF-8",
      plugin->config);
    plugin->handler_ids.update_view = 0;
    return FALSE;
  }

  md_text = (gchar*) scintilla_send_message(doc->editor->sci,
    SCI_GETCHARACTERPOINTER, 0, 0);
  markdown_viewer_load_markdown_string(plugin->viewer, md_text,
    doc->encoding, plugin->config);

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
