/*
 * overviewscintilla.h - This file is part of the Geany Overview plugin
 *
 * Copyright (c) 2015 Matthew Brush <mbrush@codebrainz.ca>
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
 */

#ifndef OVERVIEWSCINTILLA_H_
#define OVERVIEWSCINTILLA_H_ 1

#include "overviewplugin.h"
#include "overviewcolor.h"

G_BEGIN_DECLS

#define OVERVIEW_TYPE_SCINTILLA            (overview_scintilla_get_type ())
#define OVERVIEW_SCINTILLA(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), OVERVIEW_TYPE_SCINTILLA, OverviewScintilla))
#define OVERVIEW_SCINTILLA_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), OVERVIEW_TYPE_SCINTILLA, OverviewScintillaClass))
#define OVERVIEW_IS_SCINTILLA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OVERVIEW_TYPE_SCINTILLA))
#define OVERVIEW_IS_SCINTILLA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), OVERVIEW_TYPE_SCINTILLA))
#define OVERVIEW_SCINTILLA_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), OVERVIEW_TYPE_SCINTILLA, OverviewScintillaClass))

typedef struct OverviewScintilla_        OverviewScintilla;
typedef struct OverviewScintillaClass_   OverviewScintillaClass;

GType         overview_scintilla_get_type                  (void);
GtkWidget    *overview_scintilla_new                       (ScintillaObject     *src_sci);
void          overview_scintilla_sync                      (OverviewScintilla   *sci);
GdkCursorType overview_scintilla_get_cursor                (OverviewScintilla   *sci);
void          overview_scintilla_set_cursor                (OverviewScintilla   *sci,
                                                            GdkCursorType        cursor_type);
void          overview_scintilla_get_visible_rect          (OverviewScintilla   *sci,
                                                            GdkRectangle        *rect);
void          overview_scintilla_set_visible_rect          (OverviewScintilla   *sci,
                                                            const GdkRectangle  *rect);
guint         overview_scintilla_get_width                 (OverviewScintilla   *sci);
void          overview_scintilla_set_width                 (OverviewScintilla   *sci,
                                                            guint                width);
gint          overview_scintilla_get_zoom                  (OverviewScintilla   *sci);
void          overview_scintilla_set_zoom                  (OverviewScintilla   *sci,
                                                            gint                 zoom);
gboolean      overview_scintilla_get_show_tooltip          (OverviewScintilla   *sci);
void          overview_scintilla_set_show_tooltip          (OverviewScintilla   *sci,
                                                            gboolean             show);
gboolean      overview_scintilla_get_overlay_enabled       (OverviewScintilla   *sci);
void          overview_scintilla_set_overlay_enabled       (OverviewScintilla   *sci,
                                                            gboolean             enabled);
void          overview_scintilla_get_overlay_color         (OverviewScintilla   *sci,
                                                            OverviewColor       *color);
void          overview_scintilla_set_overlay_color         (OverviewScintilla   *sci,
                                                            const OverviewColor *color);
void          overview_scintilla_get_overlay_outline_color (OverviewScintilla   *sci,
                                                            OverviewColor       *color);
void          overview_scintilla_set_overlay_outline_color (OverviewScintilla   *sci,
                                                            const OverviewColor *color);
gboolean      overview_scintilla_get_overlay_inverted      (OverviewScintilla   *sci);
void          overview_scintilla_set_overlay_inverted      (OverviewScintilla   *sci,
                                                            gboolean             inverted);
gboolean      overview_scintilla_get_double_buffered       (OverviewScintilla   *sci);
void          overview_scintilla_set_double_buffered       (OverviewScintilla   *sci,
                                                            gboolean             enabled);
gint          overview_scintilla_get_scroll_lines          (OverviewScintilla   *sci);
void          overview_scintilla_set_scroll_lines          (OverviewScintilla   *sci,
                                                            gint                 lines);
gboolean      overview_scintilla_get_show_scrollbar        (OverviewScintilla   *sci);
void          overview_scintilla_set_show_scrollbar        (OverviewScintilla   *sci,
                                                            gboolean             show);

G_END_DECLS

#endif /* OVERVIEWSCINTILLA_H_ */
