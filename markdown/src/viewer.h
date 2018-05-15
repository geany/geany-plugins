/*
 * viewer.h - Part of the Geany Markdown plugin
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

#ifndef MARKDOWN_VIEWER_H
#define MARKDOWN_VIEWER_H 1

#include <gtk/gtk.h>
#ifdef MARKDOWN_WEBKIT2
# include <webkit2/webkit2.h>
#else
# include <webkit/webkitwebview.h>
#endif

G_BEGIN_DECLS

#include "conf.h"

#define MARKDOWN_TYPE_VIEWER             (markdown_viewer_get_type ())
#define MARKDOWN_VIEWER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), MARKDOWN_TYPE_VIEWER, MarkdownViewer))
#define MARKDOWN_VIEWER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), MARKDOWN_TYPE_VIEWER, MarkdownViewerClass))
#define MARKDOWN_IS_VIEWER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MARKDOWN_TYPE_VIEWER))
#define MARKDOWN_IS_VIEWER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), MARKDOWN_TYPE_VIEWER))
#define MARKDOWN_VIEWER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), MARKDOWN_TYPE_VIEWER, MarkdownViewerClass))

typedef struct _MarkdownViewer         MarkdownViewer;
typedef struct _MarkdownViewerClass    MarkdownViewerClass;
typedef struct _MarkdownViewerPrivate  MarkdownViewerPrivate;

struct _MarkdownViewer
{
  WebKitWebView parent;
  MarkdownViewerPrivate *priv;
};

struct _MarkdownViewerClass
{
  WebKitWebViewClass parent_class;
};

GType markdown_viewer_get_type(void);
GtkWidget *markdown_viewer_new(MarkdownConfig *conf);
void markdown_viewer_set_markdown(MarkdownViewer *self, const gchar *text,
  const gchar *encoding);
void markdown_viewer_queue_update(MarkdownViewer *self);
gchar *markdown_viewer_get_html(MarkdownViewer *self);

G_END_DECLS

#endif /* __MARKDOWNVIEWER_H__ */
