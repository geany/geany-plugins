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

#include <gtk/gtk.h>
#include <webkit/webkitwebview.h>
#include "markdown.h"
#include "viewer.h"
#include "conf.h"

#define MD_ENC_MAX 256

enum
{
  PROP_0,
  PROP_CONFIG,
  PROP_TEXT,
  PROP_ENCODING,
  N_PROPERTIES
};

struct _MarkdownViewerPrivate
{
  MarkdownConfig *conf;
  gulong load_handle;
  guint update_handle;
  gulong prop_handle;
  GString *text;
  gchar enc[MD_ENC_MAX];
  gdouble vscroll_pos;
  gdouble hscroll_pos;
};

static void markdown_viewer_finalize (GObject *object);

static GParamSpec *viewer_props[N_PROPERTIES] = { NULL };

G_DEFINE_TYPE (MarkdownViewer, markdown_viewer, WEBKIT_TYPE_WEB_VIEW)

static GString *
update_internal_text(MarkdownViewer *self, const gchar *val)
{
  if (!self->priv->text) {
    self->priv->text = g_string_new(val);
  } else {
    gsize len = strlen(val);
    g_string_overwrite_len(self->priv->text, 0, val, len);
    g_string_truncate(self->priv->text, len);
  }
  /* TODO: queue re-draw */
  return self->priv->text;
}

static void
markdown_viewer_set_property(GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  MarkdownViewer *self = MARKDOWN_VIEWER(obj);

  switch (prop_id) {
    case PROP_CONFIG:
      if (self->priv->conf) {
        g_object_unref(self->priv->conf);
      }
      self->priv->conf = MARKDOWN_CONFIG(g_value_get_object(value));
      break;
    case PROP_TEXT:
      update_internal_text(self, g_value_get_string(value));
      break;
    case PROP_ENCODING:
      strncpy(self->priv->enc, g_value_get_string(value), MD_ENC_MAX);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
      break;
  }
}

static void
markdown_viewer_get_property(GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec)
{
  MarkdownViewer *self = MARKDOWN_VIEWER(obj);

  switch (prop_id) {
    case PROP_CONFIG:
      g_value_set_object(value, self->priv->conf);
      break;
    case PROP_TEXT:
      g_value_set_string(value, self->priv->text->str);
      break;
    case PROP_ENCODING:
      g_value_set_string(value, self->priv->enc);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
      break;
  }
}

static void
markdown_viewer_class_init(MarkdownViewerClass *klass)
{
  GObjectClass *g_object_class;

  g_object_class = G_OBJECT_CLASS(klass);
  g_object_class->set_property = markdown_viewer_set_property;
  g_object_class->get_property = markdown_viewer_get_property;
  g_object_class->finalize = markdown_viewer_finalize;
  g_type_class_add_private((gpointer)klass, sizeof(MarkdownViewerPrivate));

  viewer_props[PROP_CONFIG] = g_param_spec_object("config", "Config",
    "MarkdownConfig object", MARKDOWN_TYPE_CONFIG,
    G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
  viewer_props[PROP_TEXT] = g_param_spec_string("text", "MarkdownText",
    "The Markdown text to render", "", G_PARAM_READWRITE);
  viewer_props[PROP_ENCODING] = g_param_spec_string("encoding", "TextEncoding",
    "The encoding of the Markdown text", "UTF-8", G_PARAM_READWRITE);

  g_object_class_install_properties(g_object_class, N_PROPERTIES, viewer_props);
}

static void
markdown_viewer_finalize(GObject *object)
{
  MarkdownViewer *self;
  g_return_if_fail(MARKDOWN_IS_VIEWER(object));
  self = MARKDOWN_VIEWER(object);
  if (self->priv->conf) {
    g_signal_handler_disconnect(self->priv->conf, self->priv->prop_handle);
    g_object_unref(self->priv->conf);
  }
  if (self->priv->text) {
    g_string_free(self->priv->text, TRUE);
  }
  G_OBJECT_CLASS(markdown_viewer_parent_class)->finalize(object);
}

static void
markdown_viewer_init(MarkdownViewer *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self, MARKDOWN_TYPE_VIEWER, MarkdownViewerPrivate);
}


GtkWidget *
markdown_viewer_new(MarkdownConfig *conf)
{
  MarkdownViewer *self;

  self = g_object_new(MARKDOWN_TYPE_VIEWER, "config", conf, NULL);

  /* Cause the view to be updated whenever the config changes. */
  self->priv->prop_handle = g_signal_connect_swapped(self->priv->conf, "notify",
      G_CALLBACK(markdown_viewer_queue_update), self);

  return GTK_WIDGET(self);
}

static void
replace_all(MarkdownViewer *self,
            GString *haystack,
            const gchar *needle,
            const gchar *replacement)
{
  gchar *ptr;
  gsize needle_len = strlen(needle);

  /* For each occurrence of needle in haystack */
  while ((ptr = strstr(haystack->str, needle)) != NULL) {
    goffset offset = ptr - haystack->str;
    g_string_erase(haystack, offset, needle_len);
    g_string_insert(haystack, offset, replacement);
  }
}

static gchar *
template_replace(MarkdownViewer *self, const gchar *html_text)
{
  MarkdownConfigViewPos view_pos;
  guint font_point_size = 0, code_font_point_size = 0;
  gchar *font_name = NULL, *code_font_name = NULL;
  gchar *bg_color = NULL, *fg_color = NULL;
  gchar font_pt_size[10] = { 0 };
  gchar code_font_pt_size[10] = { 0 };
  GString *tmpl;

  { /* Read all the configuration settings into strings */
    g_object_get(self->priv->conf,
                 "view-pos", &view_pos,
                 "font-name", &font_name,
                 "code-font-name", &code_font_name,
                 "font-point-size", &font_point_size,
                 "code-font-point-size", &code_font_point_size,
                 "bg-color", &bg_color,
                 "fg-color", &fg_color,
                 NULL);
    g_snprintf(font_pt_size, 10, "%d", font_point_size);
    g_snprintf(code_font_pt_size, 10, "%d", code_font_point_size);
  }

  /* Load the template into a GString to be modified in place */
  tmpl = g_string_new(markdown_config_get_template_text(self->priv->conf));

  replace_all(self, tmpl, "@@font_name@@", font_name);
  replace_all(self, tmpl, "@@code_font_name@@", code_font_name);
  replace_all(self, tmpl, "@@font_point_size@@", font_pt_size);
  replace_all(self, tmpl, "@@code_font_point_size@@", code_font_pt_size);
  replace_all(self, tmpl, "@@bg_color@@", bg_color);
  replace_all(self, tmpl, "@@fg_color@@", fg_color);
  replace_all(self, tmpl, "@@markdown@@", html_text);

  g_free(font_name);
  g_free(code_font_name);
  g_free(bg_color);
  g_free(fg_color);

  return g_string_free(tmpl, FALSE);
}

static gboolean
push_scroll_pos(MarkdownViewer *self)
{
  GtkWidget *parent;
  gboolean pushed = FALSE;

  parent = gtk_widget_get_parent(GTK_WIDGET(self));
  if (GTK_IS_SCROLLED_WINDOW(parent)) {
    GtkAdjustment *adj;
    adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(parent));
    self->priv->vscroll_pos = gtk_adjustment_get_value(adj);
    adj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(parent));
    self->priv->hscroll_pos = gtk_adjustment_get_value(adj);
    pushed = TRUE;
  }

  return pushed;
}

static gboolean
pop_scroll_pos(MarkdownViewer *self)
{
  GtkWidget *parent;
  gboolean popped = FALSE;

  /* first process any pending events, like drawing of the webview */
  while (gtk_events_pending()) {
    gtk_main_iteration();
  }

  parent = gtk_widget_get_parent(GTK_WIDGET(self));
  if (GTK_IS_SCROLLED_WINDOW(parent)) {
    GtkAdjustment *adj;
    adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(parent));
    gtk_adjustment_set_value(adj, self->priv->vscroll_pos);
    adj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(parent));
    gtk_adjustment_set_value(adj, self->priv->hscroll_pos);
    /* process any new events, like making sure the new scroll position
     * takes effect. */
    while (gtk_events_pending()) {
      gtk_main_iteration();
    }
    popped = TRUE;
  }

  return popped;
}

static void
on_webview_load_status_notify(WebKitWebView *view, GParamSpec *pspec,
  MarkdownViewer *self)
{
  WebKitLoadStatus load_status;

  g_object_get(view, "load-status", &load_status, NULL);

  /* When the webkit is done loading, reset the scroll position. */
  if (load_status == WEBKIT_LOAD_FINISHED) {
    pop_scroll_pos(self);
  }
}

static gchar *
markdown_viewer_get_html(MarkdownViewer *self)
{
  gchar *md_as_html, *html = NULL;

  /* Ensure the internal buffer is created */
  if (!self->priv->text) {
    update_internal_text(self, "");
  }

  md_as_html = mkd_compile_document(self->priv->text->str, 0);
  if (md_as_html) {
    html = template_replace(self, md_as_html);
    g_free(md_as_html);
  }

  return html;
}

static gboolean
markdown_viewer_update_view(MarkdownViewer *self)
{
  gchar *html = markdown_viewer_get_html(self);

  push_scroll_pos(self);

  if (html) {
    static const gchar *base_uri = "file://.";

    /* Connect a signal handler (only needed once) to restore the scroll
     * position once the webview is reloaded. */
    if (self->priv->load_handle == 0) {
      self->priv->load_handle =
        g_signal_connect_swapped(WEBKIT_WEB_VIEW(self), "notify::load-status",
          G_CALLBACK(on_webview_load_status_notify), self);
    }

    webkit_web_view_load_string(WEBKIT_WEB_VIEW(self), html, "text/html",
      self->priv->enc, base_uri);

    g_free(html);
  }

  if (self->priv->update_handle != 0) {
    g_source_remove(self->priv->update_handle);
  }
  self->priv->update_handle = 0;

  return FALSE; /* When used as an idle handler, says to remove the source */
}

void
markdown_viewer_queue_update(MarkdownViewer *self)
{
  g_return_if_fail(MARKDOWN_IS_VIEWER(self));
  if (self->priv->update_handle == 0) {
    self->priv->update_handle = g_idle_add(
      (GSourceFunc) markdown_viewer_update_view, self);
  }
}

void
markdown_viewer_set_markdown(MarkdownViewer *self, const gchar *text, const gchar *encoding)
{
  g_return_if_fail(MARKDOWN_IS_VIEWER(self));
  g_object_set(self, "text", text, "encoding", encoding, NULL);
  markdown_viewer_queue_update(self);
}
