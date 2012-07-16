/*
 * markdownconfig.h
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
 * 
 * 
 */


#ifndef __MARKDOWNCONFIG_H__
#define __MARKDOWNCONFIG_H__

#include <gtk/gtk.h>
#include <glib-object.h>

G_BEGIN_DECLS


#define MARKDOWN_TYPE_CONFIG             (markdown_config_get_type ())
#define MARKDOWN_CONFIG(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), MARKDOWN_TYPE_CONFIG, MarkdownConfig))
#define MARKDOWN_CONFIG_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), MARKDOWN_TYPE_CONFIG, MarkdownConfigClass))
#define MARKDOWN_IS_CONFIG(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MARKDOWN_TYPE_CONFIG))
#define MARKDOWN_IS_CONFIG_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), MARKDOWN_TYPE_CONFIG))
#define MARKDOWN_CONFIG_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), MARKDOWN_TYPE_CONFIG, MarkdownConfigClass))

typedef struct _MarkdownConfig         MarkdownConfig;
typedef struct _MarkdownConfigClass    MarkdownConfigClass;
typedef struct _MarkdownConfigPrivate  MarkdownConfigPrivate;

typedef enum {
  MARKDOWN_CONFIG_VIEW_POS_SIDEBAR=0,
  MARKDOWN_CONFIG_VIEW_POS_MSGWIN=1,
  MARKDOWN_CONFIG_VIEW_POS_MAX
} MarkdownConfigViewPos;

struct _MarkdownConfig
{
  GObject parent;
  MarkdownConfigPrivate *priv;
};

struct _MarkdownConfigClass
{
  GObjectClass parent_class;
};


GType markdown_config_get_type(void);
MarkdownConfig *markdown_config_new(const gchar *filename);
gboolean markdown_config_save(MarkdownConfig *conf);
GtkWidget *markdown_config_gui(MarkdownConfig *conf, GtkDialog *dialog);

const gchar *markdown_config_get_template_text(MarkdownConfig *conf);

G_END_DECLS

#endif /* __MARKDOWNCONFIG_H__ */
