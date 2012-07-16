/*
 * viewer.c - Part of the Geany Markdown plugin
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
#include <gtk/gtk.h>
#include <webkit/webkitwebview.h>
#include "viewer.h"
#include "md.h"
#include "conf.h"

#define MARKDOWN_VIEWER_TAB_LABEL _("Markdown Preview")

struct MarkdownViewer {
  GtkScrolledWindow *scrolled_win; /* The GtkScrolledWindow containing the WebKitView */
  WebKitWebView     *webview;      /* The Webkit preview widget */
  GtkNotebook       *notebook;     /* Either the sidebar notebook or the msgwin notebook */
  gdouble old_pos; /* Position before reload, used to reset scroll pos. */
};

MarkdownViewer *markdown_viewer_new(GtkNotebook *notebook)
{
  MarkdownViewer *tmpl = g_slice_new0(MarkdownViewer);
  if (tmpl) {
    tmpl->scrolled_win = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(NULL, NULL));
    tmpl->webview = WEBKIT_WEB_VIEW(webkit_web_view_new());
    gtk_container_add(GTK_CONTAINER(tmpl->scrolled_win), GTK_WIDGET(tmpl->webview));
    gtk_scrolled_window_set_policy(tmpl->scrolled_win, GTK_POLICY_AUTOMATIC,
      GTK_POLICY_AUTOMATIC);
    tmpl->notebook = notebook;
    gtk_notebook_append_page(tmpl->notebook, GTK_WIDGET(tmpl->scrolled_win),
      gtk_label_new(MARKDOWN_VIEWER_TAB_LABEL));
    gtk_widget_show_all(GTK_WIDGET(notebook));
  }
  return tmpl;
}

void markdown_viewer_free(MarkdownViewer *viewer)
{
  if (viewer) {
    gtk_widget_destroy(GTK_WIDGET(viewer->scrolled_win));
    g_slice_free(MarkdownViewer, viewer);
  }
}

GtkNotebook *markdown_viewer_get_notebook(MarkdownViewer *viewer)
{
  g_return_val_if_fail(viewer, NULL);
  return viewer->notebook;
}

void markdown_viewer_set_notebook(MarkdownViewer *viewer, GtkNotebook *nb)
{
  gint page_num;

  g_return_if_fail(viewer);
  g_return_if_fail(GTK_IS_NOTEBOOK(nb));

  g_object_ref(G_OBJECT(viewer->scrolled_win));

  page_num = gtk_notebook_page_num(viewer->notebook, GTK_WIDGET(viewer->scrolled_win));
  gtk_notebook_remove_page(viewer->notebook, page_num);

  viewer->notebook = nb;

  page_num = gtk_notebook_append_page(viewer->notebook, GTK_WIDGET(viewer->scrolled_win),
    gtk_label_new(MARKDOWN_VIEWER_TAB_LABEL));

  gtk_notebook_set_current_page(viewer->notebook, page_num);

  g_object_unref(G_OBJECT(viewer->scrolled_win));
}

void on_viewer_load_status_notify(GObject *obj, GParamSpec *pspec, MarkdownViewer *viewer)
{
  GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(viewer->scrolled_win);
  gtk_adjustment_set_value(vadj, viewer->old_pos);
  while (gtk_events_pending()) {
    gtk_main_iteration();
  }
}



static gchar *
str_replace(const gchar *haystack, const gchar *needle, const gchar *repl)
{
  gchar *out_str, **parts;
  
  parts = g_strsplit(haystack, needle, 0);
  out_str = g_strjoinv(repl, parts);
  g_strfreev(parts);
  
  return out_str;
}

static gchar *
markdown_viewer_replace(const gchar *html_text, MarkdownConfig *config)
{
  MarkdownConfigViewPos view_pos;
  guint font_point_size = 0;
  gchar *out_str, *tmp;
  gchar *font_pt_size, *code_font_pt_size;
  gchar *font_name = NULL, *code_font_name = NULL;
  gchar *bg_color = NULL, *fg_color = NULL;
  guint code_font_point_size = 0;
  
  g_object_get(config,
               "view-pos", &view_pos,
               "font-name", &font_name,
               "code-font-name", &code_font_name,
               "font-point-size", &font_point_size,
               "code-font-point-size", &code_font_point_size,
               "bg-color", &bg_color,
               "fg-color", &fg_color,
               NULL);
  
  font_pt_size = g_strdup_printf("%d", font_point_size);
  code_font_pt_size = g_strdup_printf("%d", code_font_point_size);
  
  tmp = str_replace(markdown_config_get_template_text(config),
    "@@font_name@@", font_name);
  out_str = tmp;
  tmp = str_replace(out_str, "@@code_font_name@@", code_font_name);
  g_free(out_str); out_str = tmp;
  tmp = str_replace(out_str, "@@font_point_size@@", font_pt_size);
  g_free(out_str); out_str = tmp;
  tmp = str_replace(out_str, "@@code_font_point_size@@", code_font_pt_size);
  g_free(out_str); out_str = tmp;
  tmp = str_replace(out_str, "@@bg_color@@", bg_color);
  g_free(out_str); out_str = tmp;
  tmp = str_replace(out_str, "@@fg_color@@", fg_color);
  g_free(out_str); out_str = tmp;
  tmp = str_replace(out_str, "@@markdown@@", html_text);
  g_free(out_str); out_str = tmp;
  
  g_free(font_name);
  g_free(code_font_name);
  g_free(font_pt_size);
  g_free(code_font_pt_size);
  g_free(bg_color);
  g_free(fg_color);
  
  /*g_debug("Replaced:\n%s", out_str);*/
  
  return out_str;
}

void markdown_viewer_load_markdown_string(MarkdownViewer *viewer,
  const gchar *md_str, const gchar *encoding, MarkdownConfig *config)
{
  g_return_if_fail(viewer);
  gchar *html = markdown_to_html(md_str);
  gchar *new_text = markdown_viewer_replace(html, config);
  GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(viewer->scrolled_win);
  viewer->old_pos = gtk_adjustment_get_value(vadj);
  g_free(html);
  if (new_text) {
    gchar *base_uri = g_strdup_printf("file://.");
    g_signal_connect(viewer->webview, "notify::load-status",
      G_CALLBACK(on_viewer_load_status_notify), viewer);
    webkit_web_view_load_string(viewer->webview, new_text, "text/html",
      encoding, base_uri);
    g_free(new_text);
    g_free(base_uri);
  }
}

void markdown_viewer_show(MarkdownViewer *viewer)
{
  g_return_if_fail(viewer);
  gtk_widget_show(GTK_WIDGET(viewer->scrolled_win));
}

void markdown_viewer_hide(MarkdownViewer *viewer)
{
  g_return_if_fail(viewer);
  gtk_widget_hide(GTK_WIDGET(viewer->scrolled_win));
}
