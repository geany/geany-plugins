/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Imendio AB
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __IGE_CONF_H__
#define __IGE_CONF_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define IGE_TYPE_CONF         (ige_conf_get_type ())
#define IGE_CONF(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), IGE_TYPE_CONF, IgeConf))
#define IGE_CONF_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), IGE_TYPE_CONF, IgeConfClass))
#define IGE_IS_CONF(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), IGE_TYPE_CONF))
#define IGE_IS_CONF_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), IGE_TYPE_CONF))
#define IGE_CONF_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), IGE_TYPE_CONF, IgeConfClass))

typedef struct _IgeConf      IgeConf;
typedef struct _IgeConfClass IgeConfClass;

struct _IgeConf  {
        GObject parent_instance;
};

struct _IgeConfClass {
        GObjectClass parent_class;
};

typedef void (*IgeConfNotifyFunc) (IgeConf     *conf,
                                   const gchar *key,
                                   gpointer     user_data);

GType       ige_conf_get_type        (void);
IgeConf    *ige_conf_get             (void);
void        ige_conf_add_defaults    (IgeConf            *conf,
				      const gchar        *path);
guint       ige_conf_notify_add      (IgeConf            *conf,
                                      const gchar        *key,
                                      IgeConfNotifyFunc   func,
                                      gpointer            data);
gboolean    ige_conf_notify_remove   (IgeConf            *conf,
                                      guint               id);
gboolean    ige_conf_set_int         (IgeConf            *conf,
                                      const gchar        *key,
                                      gint                value);
gboolean    ige_conf_get_int         (IgeConf            *conf,
                                      const gchar        *key,
                                      gint               *value);
gboolean    ige_conf_set_bool        (IgeConf            *conf,
                                      const gchar        *key,
                                      gboolean            value);
gboolean    ige_conf_get_bool        (IgeConf            *conf,
                                      const gchar        *key,
                                      gboolean           *value);
gboolean    ige_conf_set_string      (IgeConf            *conf,
                                      const gchar        *key,
                                      const gchar        *value);
gboolean    ige_conf_get_string      (IgeConf            *conf,
                                      const gchar        *key,
                                      gchar             **value);
gboolean    ige_conf_set_string_list (IgeConf            *conf,
                                      const gchar        *key,
                                      GSList             *value);
gboolean    ige_conf_get_string_list (IgeConf            *conf,
                                      const gchar        *key,
                                      GSList            **value);

G_END_DECLS

#endif /* __IGE_CONF_H__ */
