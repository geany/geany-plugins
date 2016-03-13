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

#include "config.h"
#include <geanyplugin.h>
#include "viewer.h"
#include "conf.h"

GeanyData      *geany_data;
GeanyPlugin    *geany_plugin;

PLUGIN_VERSION_CHECK(224)

PLUGIN_SET_TRANSLATABLE_INFO(LOCALEDIR, GETTEXT_PACKAGE,
                             "Markdown",
                             _("Real-time Markdown preview"),
                             "0.01",
                             "Matthew Brush <mbrush@codebrainz.ca>")

/* Should be defined by build system, this is just a fallback */
#ifndef MARKDOWN_DOC_DIR
#  define MARKDOWN_DOC_DIR "/usr/local/share/doc/geany-plugins/markdown"
#endif
#ifndef MARKDOWN_HELP_FILE
#  define MARKDOWN_HELP_FILE MARKDOWN_DOC_DIR "/html/help.html"
#endif

#define MARKDOWN_PREVIEW_LABEL _("Markdown Preview")

/* Global data */
static MarkdownViewer *g_viewer = NULL;
static GtkWidget *g_scrolled_win = NULL;

/* Forward declarations */
static void update_markdown_viewer(MarkdownViewer *viewer);
static gboolean on_editor_notify(GObject *obj, GeanyEditor *editor, SCNotification *notif, MarkdownViewer *viewer);
static void on_document_signal(GObject *obj, GeanyDocument *doc, MarkdownViewer *viewer);
static void on_document_filetype_set(GObject *obj, GeanyDocument *doc, GeanyFiletype *ft_old, MarkdownViewer *viewer);
static void on_view_pos_notify(GObject *obj, GParamSpec *pspec, MarkdownViewer *viewer);

/* Main plugin entry point on plugin load. */
void plugin_init(GeanyData *data)
{
  gint page_num;
  gchar *conf_fn;
  MarkdownConfig *conf;
  MarkdownConfigViewPos view_pos;
  GtkWidget *viewer;
  GtkNotebook *nb;

  /* Setup the config object which is needed by the view. */
  conf_fn = g_build_filename(geany->app->configdir, "plugins", "markdown",
    "markdown.conf", NULL);
  conf = markdown_config_new(conf_fn);
  g_free(conf_fn);

  viewer = markdown_viewer_new(conf);
  /* store as global for plugin_cleanup() */
  g_viewer = MARKDOWN_VIEWER(viewer);
  view_pos = markdown_config_get_view_pos(conf);

  g_scrolled_win = gtk_scrolled_window_new(NULL, NULL);
  gtk_container_add(GTK_CONTAINER(g_scrolled_win), viewer);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(g_scrolled_win),
    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  if (view_pos == MARKDOWN_CONFIG_VIEW_POS_MSGWIN) {
    nb = GTK_NOTEBOOK(geany->main_widgets->message_window_notebook);
    page_num = gtk_notebook_append_page(nb,
      g_scrolled_win, gtk_label_new(MARKDOWN_PREVIEW_LABEL));
  } else {
    nb = GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook);
    page_num = gtk_notebook_append_page(nb,
      g_scrolled_win, gtk_label_new(MARKDOWN_PREVIEW_LABEL));
  }

  gtk_widget_show_all(g_scrolled_win);
  gtk_notebook_set_current_page(nb, page_num);

  g_signal_connect(conf, "notify::view-pos", G_CALLBACK(on_view_pos_notify), viewer);

#define MD_PSC(sig, cb) \
  plugin_signal_connect(geany_plugin, NULL, (sig), TRUE, G_CALLBACK(cb), viewer)
  /* Geany takes care of disconnecting these for us when the plugin is unloaded,
   * the macro is just to make the code smaller/clearer. */
  MD_PSC("editor-notify", on_editor_notify);
  MD_PSC("document-activate", on_document_signal);
  MD_PSC("document-filetype-set", on_document_filetype_set);
  MD_PSC("document-new", on_document_signal);
  MD_PSC("document-open", on_document_signal);
  MD_PSC("document-reload", on_document_signal);
#undef MD_PSC

  /* Prevent segfault in plugin when it registers GTypes and gets unloaded
   * and when reloaded tries to re-register the GTypes. */
  plugin_module_make_resident(geany_plugin);

  update_markdown_viewer(MARKDOWN_VIEWER(viewer));
}

/* Cleanup resources on plugin unload. */
void plugin_cleanup(void)
{
  gtk_widget_destroy(g_scrolled_win);
}

/* Called to show the preferences GUI. */
GtkWidget *plugin_configure(GtkDialog *dialog)
{
  MarkdownConfig *conf = NULL;
  g_object_get(g_viewer, "config", &conf, NULL);
  return markdown_config_gui(conf, dialog);
}

/* Called to show the plugin's help */
void plugin_help(void)
{
#ifdef G_OS_WIN32
  gchar *prefix = g_win32_get_package_installation_directory_of_module(NULL);
#else
  gchar *prefix = NULL;
#endif
  gchar *uri = g_strconcat("file://", prefix ? prefix : "", MARKDOWN_HELP_FILE, NULL);

  utils_open_browser(uri);

  g_free(uri);
  g_free(prefix);
}

/* All of the various signal handlers call this function to update the
 * MarkdownViewer on specific events. This causes a bunch of memory
 * allocations, re-compiles the Markdown to HTML, reformats the HTML
 * template, copies the HTML into the webview and causes it to (eventually)
 * be redrawn. Only call it when really needed, like when the scintilla
 * editor's text contents change and not on other editor events.
 */
static void
update_markdown_viewer(MarkdownViewer *viewer)
{
  GeanyDocument *doc = document_get_current();

  if (DOC_VALID(doc) && g_strcmp0(doc->file_type->name, "Markdown") == 0) {
    gchar *text;
    text = (gchar*) scintilla_send_message(doc->editor->sci, SCI_GETCHARACTERPOINTER, 0, 0);
    markdown_viewer_set_markdown(viewer, text, doc->encoding);
  } else {
    markdown_viewer_set_markdown(viewer,
      _("The current document does not have a Markdown filetype."), "UTF-8");
  }

  markdown_viewer_queue_update(viewer);
}

/* Return TRUE if event is a buffer modification that inserts or deletes
 * text and which caused a text changed length greater than 0. */
#define IS_MOD_NOTIF(nt) (nt->nmhdr.code == SCN_MODIFIED && \
                          nt->length > 0 && ( \
                          (nt->modificationType & SC_MOD_INSERTTEXT) || \
                          (nt->modificationType & SC_MOD_DELETETEXT)))

/* Queue update of the markdown preview on editor text change. */
static gboolean on_editor_notify(GObject *obj, GeanyEditor *editor,
  SCNotification *notif, MarkdownViewer *viewer)
{
  if (IS_MOD_NOTIF(notif)) {
    update_markdown_viewer(viewer);
  }
  return FALSE; /* Allow others to handle this event too */
}

/* Queue update of the markdown preview on document signals (new, open,
 * activate, etc.) */
static void on_document_signal(GObject *obj, GeanyDocument *doc, MarkdownViewer *viewer)
{
  update_markdown_viewer(viewer);
}

/* Queue update of the markdown preview when a document's filetype is set */
static void on_document_filetype_set(GObject *obj, GeanyDocument *doc, GeanyFiletype *ft_old,
  MarkdownViewer *viewer)
{
  update_markdown_viewer(viewer);
}

/* Move the MarkdownViewer to the correct notebook when the view position
 * is changed. */
static void
on_view_pos_notify(GObject *obj, GParamSpec *pspec, MarkdownViewer *viewer)
{
  gint page_num;
  GtkNotebook *newnb;
  GtkNotebook *snb = GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook);
  GtkNotebook *mnb = GTK_NOTEBOOK(geany->main_widgets->message_window_notebook);
  MarkdownConfigViewPos view_pos;

  g_object_ref(g_scrolled_win); /* Prevent it from being destroyed */

  /* Remove the tab from whichever notebook its in (sidebar or msgwin) */
  page_num = gtk_notebook_page_num(snb, g_scrolled_win);
  if (page_num >= 0) {
    gtk_notebook_remove_page(snb, page_num);
  } else {
    page_num = gtk_notebook_page_num(mnb, g_scrolled_win);
    if (page_num >= 0) {
      gtk_notebook_remove_page(mnb, page_num);
    } else {
      g_warning("Unable to relocate the Markdown preview tab: not found");
    }
  }

  /* Check the user preference to get the new notebook */
  view_pos = markdown_config_get_view_pos(MARKDOWN_CONFIG(obj));
  newnb = (view_pos == MARKDOWN_CONFIG_VIEW_POS_MSGWIN) ? mnb : snb;

  page_num = gtk_notebook_append_page(newnb, g_scrolled_win,
    gtk_label_new(MARKDOWN_PREVIEW_LABEL));

  gtk_notebook_set_current_page(newnb, page_num);

  g_object_unref(g_scrolled_win); /* The new notebook owns it now */

  update_markdown_viewer(viewer);
}
