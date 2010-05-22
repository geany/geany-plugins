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

#ifndef H_GGD_OPTIONS
#define H_GGD_OPTIONS

#include <glib.h>
#include <gtk/gtk.h>

#include "ggd-macros.h"

G_BEGIN_DECLS
GGD_BEGIN_PLUGIN_API


/**
 * GgdOptGroup:
 * 
 * Opaque object representing a group of options.
 */
typedef struct _GgdOptGroup GgdOptGroup;


GgdOptGroup  *ggd_opt_group_new                       (const gchar   *section);
void          ggd_opt_group_free                      (GgdOptGroup *group,
                                                       gboolean     free_opts);
void          ggd_opt_group_add_boolean               (GgdOptGroup   *group,
                                                       gboolean      *optvar,
                                                       const gchar   *key);
void          ggd_opt_group_add_string                (GgdOptGroup   *group,
                                                       gchar        **optvar,
                                                       const gchar   *key);
void          ggd_opt_group_sync_to_proxies           (GgdOptGroup *group);
void          ggd_opt_group_sync_from_proxies         (GgdOptGroup *group);
gboolean      ggd_opt_group_set_proxy_full            (GgdOptGroup  *group,
                                                       gpointer      optvar,
                                                       gboolean      check_type,
                                                       GType         type_check,
                                                       GObject      *proxy,
                                                       const gchar  *prop);
gboolean      ggd_opt_group_set_proxy_gtkobject_full  (GgdOptGroup  *group,
                                                       gpointer      optvar,
                                                       gboolean      check_type,
                                                       GType         type_check,
                                                       GtkObject    *proxy,
                                                       const gchar  *prop);
void          ggd_opt_group_remove_proxy              (GgdOptGroup *group,
                                                       gpointer     optvar);
void          ggd_opt_group_load_from_key_file        (GgdOptGroup  *group,
                                                       GKeyFile     *key_file);
gboolean      ggd_opt_group_load_from_file            (GgdOptGroup  *group,
                                                       const gchar  *filename,
                                                       GError      **error);
void          ggd_opt_group_write_to_key_file         (GgdOptGroup *group,
                                                       GKeyFile    *key_file);
gboolean      ggd_opt_group_write_to_file             (GgdOptGroup *group,
                                                       const gchar *filename,
                                                       GError     **error);

#define ggd_opt_group_set_proxy(group, optvar, proxy, prop)                    \
  (ggd_opt_group_set_proxy_full (group, optvar, FALSE, 0, proxy, prop))

#define ggd_opt_group_set_proxy_gtkentry(group, optvar, proxy)                 \
  (ggd_opt_group_set_proxy_gtkobject_full (group, optvar, TRUE, G_TYPE_STRING, \
                                           GTK_OBJECT (proxy), "text"))
#define ggd_opt_group_set_proxy_gtktogglebutton(group, optvar, proxy)          \
  (ggd_opt_group_set_proxy_gtkobject_full (group, optvar, TRUE, G_TYPE_BOOLEAN,\
                                           GTK_OBJECT (proxy), "active"))


GGD_END_PLUGIN_API
G_END_DECLS

#endif /* guard */
