/*
 * overviewprefs.h - This file is part of the Geany Overview plugin
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

#ifndef OVERVIEWPREFS_H_
#define OVERVIEWPREFS_H_ 1

#include <glib-object.h>

G_BEGIN_DECLS

#define OVERVIEW_TYPE_PREFS            (overview_prefs_get_type ())
#define OVERVIEW_PREFS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), OVERVIEW_TYPE_PREFS, OverviewPrefs))
#define OVERVIEW_PREFS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), OVERVIEW_TYPE_PREFS, OverviewPrefsClass))
#define OVERVIEW_IS_PREFS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OVERVIEW_TYPE_PREFS))
#define OVERVIEW_IS_PREFS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), OVERVIEW_TYPE_PREFS))
#define OVERVIEW_PREFS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), OVERVIEW_TYPE_PREFS, OverviewPrefsClass))

typedef struct OverviewPrefs_      OverviewPrefs;
typedef struct OverviewPrefsClass_ OverviewPrefsClass;

GType          overview_prefs_get_type       (void);
OverviewPrefs *overview_prefs_new            (void);
gboolean       overview_prefs_load           (OverviewPrefs *prefs,
                                              const gchar   *filename,
                                              GError       **error);
gboolean       overview_prefs_save           (OverviewPrefs *prefs,
                                              const gchar   *filename,
                                              GError       **error);
gboolean       overview_prefs_from_data      (OverviewPrefs *prefs,
                                              const gchar   *contents,
                                              gssize         size,
                                              GError       **error);
gchar*         overview_prefs_to_data        (OverviewPrefs *prefs,
                                              gsize         *size,
                                              GError       **error);
void           overview_prefs_bind_scintilla (OverviewPrefs *prefs,
                                              GObject       *sci);

#define OVERVIEW_PREFS_DEFAULT_CONFIG   \
    "[overview]\n"                      \
    "width = 120\n"                     \
    "zoom = -10\n"                      \
    "show-tooltip = true\n"             \
    "double-buffered = true\n"          \
    "scroll-lines = 4\n"                \
    "show-scrollbar = true\n"           \
    "overlay-enabled = true\n"          \
    "overlay-color = #000000\n"         \
    "overlay-alpha = 0.10\n"            \
    "overlay-outline-color = #000000\n" \
    "overlay-outline-alpha = 0.10\n"    \
    "overlay-inverted = true\n"         \
    "position = right\n"                \
    "visible = true\n"                  \
    "\n"

G_END_DECLS

#endif /* OVERVIEWPREFS_H_ */
