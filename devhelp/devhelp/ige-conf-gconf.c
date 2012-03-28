/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
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

#include "config.h"
#include <string.h>
#include <gconf/gconf-client.h>
#include "ige-conf-private.h"

typedef struct {
        GConfClient *gconf_client;
        GList       *defaults;
} IgeConfPriv;

typedef struct {
        IgeConf           *conf;
        IgeConfNotifyFunc  func;
        gpointer           user_data;
} IgeConfNotifyData;

G_DEFINE_TYPE (IgeConf, ige_conf, G_TYPE_OBJECT);

#define GET_PRIVATE(instance) G_TYPE_INSTANCE_GET_PRIVATE \
  (instance, IGE_TYPE_CONF, IgeConfPriv);

static IgeConf *global_conf = NULL;

static void
conf_finalize (GObject *object)
{
        IgeConfPriv *priv;

        priv = GET_PRIVATE (object);

        /* FIXME: Remove added dirs.
        gconf_client_remove_dir (priv->gconf_client,
                                 CONF_PATH,
                                 NULL);
        */

        g_object_unref (priv->gconf_client);

        _ige_conf_defaults_free_list (priv->defaults);

        G_OBJECT_CLASS (ige_conf_parent_class)->finalize (object);
}

static void
ige_conf_class_init (IgeConfClass *class)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (class);

        object_class->finalize = conf_finalize;

        g_type_class_add_private (object_class, sizeof (IgeConfPriv));
}

static void
ige_conf_init (IgeConf *conf)
{
        IgeConfPriv *priv;

        priv = GET_PRIVATE (conf);

        priv->gconf_client = gconf_client_get_default ();
}

IgeConf *
ige_conf_get (void)
{
        if (!global_conf) {
                global_conf = g_object_new (IGE_TYPE_CONF, NULL);
        }

        return global_conf;
}

void
ige_conf_add_defaults (IgeConf     *conf,
                       const gchar *path)
{
        IgeConfPriv *priv = GET_PRIVATE (conf);
        gchar       *root;

        priv->defaults = _ige_conf_defaults_read_file (path, NULL);
        root = _ige_conf_defaults_get_root (priv->defaults);

        gconf_client_add_dir (priv->gconf_client,
                              root,
                              GCONF_CLIENT_PRELOAD_ONELEVEL,
                              NULL);

        g_free (root);
}

static GConfEntry *
conf_get_entry (IgeConf     *conf,
                const gchar *key)
{
        IgeConfPriv *priv;

        priv = GET_PRIVATE (conf);

        return gconf_client_get_entry (priv->gconf_client, key,
                                       NULL, TRUE, NULL);
}

gboolean
ige_conf_set_int (IgeConf     *conf,
                  const gchar *key,
                  gint         value)
{
        IgeConfPriv *priv;

        g_return_val_if_fail (IGE_IS_CONF (conf), FALSE);

        priv = GET_PRIVATE (conf);

        return gconf_client_set_int (priv->gconf_client,
                                     key,
                                     value,
                                     NULL);
}

gboolean
ige_conf_get_int (IgeConf     *conf,
                  const gchar *key,
                  gint        *value)
{
        IgeConfPriv *priv;
        GConfEntry  *entry;
        gboolean     got_value = FALSE;

        *value = 0;

        g_return_val_if_fail (IGE_IS_CONF (conf), FALSE);
        g_return_val_if_fail (value != NULL, FALSE);

        priv = GET_PRIVATE (conf);

        entry = conf_get_entry (conf, key);
        if (entry) {
                GConfValue *v;

                v = gconf_entry_get_value (entry);
                if (v) {
                        *value = gconf_value_get_int (v);
                        got_value = TRUE;
                }
        }

        gconf_entry_free (entry);

        if (!got_value) {
                *value = _ige_conf_defaults_get_int (priv->defaults, key);
        }

        return TRUE;
}

gboolean
ige_conf_set_bool (IgeConf     *conf,
                   const gchar *key,
                   gboolean     value)
{
        IgeConfPriv *priv;

        g_return_val_if_fail (IGE_IS_CONF (conf), FALSE);

        priv = GET_PRIVATE (conf);

        return gconf_client_set_bool (priv->gconf_client,
                                      key,
                                      value,
                                      NULL);
}

gboolean
ige_conf_get_bool (IgeConf     *conf,
                   const gchar *key,
                   gboolean    *value)
{
        IgeConfPriv *priv;
        GConfEntry  *entry;
        gboolean     got_value = FALSE;

        *value = 0;

        g_return_val_if_fail (IGE_IS_CONF (conf), FALSE);
        g_return_val_if_fail (value != NULL, FALSE);

        priv = GET_PRIVATE (conf);

        entry = conf_get_entry (conf, key);
        if (entry) {
                GConfValue *v;

                v = gconf_entry_get_value (entry);
                if (v) {
                        *value = gconf_value_get_bool (v);
                        got_value = TRUE;
                }
        }

        gconf_entry_free (entry);

        if (!got_value) {
                *value = _ige_conf_defaults_get_bool (priv->defaults, key);
        }

        return TRUE;
}

gboolean
ige_conf_set_string (IgeConf     *conf,
                     const gchar *key,
                     const gchar *value)
{
        IgeConfPriv *priv;

        g_return_val_if_fail (IGE_IS_CONF (conf), FALSE);

        priv = GET_PRIVATE (conf);

        return gconf_client_set_string (priv->gconf_client,
                                        key,
                                        value,
                                        NULL);
}

gboolean
ige_conf_get_string (IgeConf      *conf,
                     const gchar  *key,
                     gchar       **value)
{
        IgeConfPriv *priv;
        GError      *error = NULL;

        *value = NULL;

        g_return_val_if_fail (IGE_IS_CONF (conf), FALSE);

        priv = GET_PRIVATE (conf);

        *value = gconf_client_get_string (priv->gconf_client,
                                          key,
                                          &error);

        if (error) {
                g_error_free (error);
                return FALSE;
        }

        if (*value == NULL) {
                *value = g_strdup (_ige_conf_defaults_get_string (priv->defaults, key));
       }

        return TRUE;
}

gboolean
ige_conf_set_string_list (IgeConf     *conf,
                          const gchar *key,
                          GSList      *value)
{
        IgeConfPriv *priv;

        g_return_val_if_fail (IGE_IS_CONF (conf), FALSE);

        priv = GET_PRIVATE (conf);

        return gconf_client_set_list (priv->gconf_client,
                                      key,
                                      GCONF_VALUE_STRING,
                                      value,
                                      NULL);
}

gboolean
ige_conf_get_string_list (IgeConf      *conf,
                          const gchar  *key,
                          GSList      **value)
{
        IgeConfPriv *priv;
        GError      *error = NULL;

        *value = NULL;

        g_return_val_if_fail (IGE_IS_CONF (conf), FALSE);

        priv = GET_PRIVATE (conf);

        *value = gconf_client_get_list (priv->gconf_client,
                                        key,
                                        GCONF_VALUE_STRING,
                                        &error);
        if (error) {
                g_error_free (error);
                return FALSE;
        }

        return TRUE;
}

static void
conf_notify_data_free (IgeConfNotifyData *data)
{
        g_object_unref (data->conf);
        g_slice_free (IgeConfNotifyData, data);
}

static void
conf_notify_func (GConfClient *client,
                  guint        id,
                  GConfEntry  *entry,
                  gpointer     user_data)
{
        IgeConfNotifyData *data;

        data = user_data;

        data->func (data->conf,
                    gconf_entry_get_key (entry),
                    data->user_data);
}

guint
ige_conf_notify_add (IgeConf           *conf,
                     const gchar       *key,
                     IgeConfNotifyFunc  func,
                     gpointer           user_data)
{
        IgeConfPriv       *priv;
        guint              id;
        IgeConfNotifyData *data;

        g_return_val_if_fail (IGE_IS_CONF (conf), 0);

        priv = GET_PRIVATE (conf);

        data = g_slice_new (IgeConfNotifyData);
        data->func = func;
        data->user_data = user_data;
        data->conf = g_object_ref (conf);

        id = gconf_client_notify_add (priv->gconf_client,
                                      key,
                                      conf_notify_func,
                                      data,
                                      (GFreeFunc) conf_notify_data_free,
                                      NULL);

        return id;
}

gboolean
ige_conf_notify_remove (IgeConf *conf,
                        guint    id)
{
        IgeConfPriv *priv;

        g_return_val_if_fail (IGE_IS_CONF (conf), FALSE);

        priv = GET_PRIVATE (conf);

        gconf_client_notify_remove (priv->gconf_client, id);

        return TRUE;
}
