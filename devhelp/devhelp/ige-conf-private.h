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

#ifndef __IGE_CONF_PRIVATE_H__
#define __IGE_CONF_PRIVATE_H__

#include <glib.h>
#include "ige-conf.h"

G_BEGIN_DECLS

typedef enum {
        IGE_CONF_TYPE_INT,
        IGE_CONF_TYPE_BOOLEAN,
        IGE_CONF_TYPE_STRING
} IgeConfType;

typedef struct {
        IgeConfType  type;
        gchar       *key;
        gchar       *value;
} IgeConfDefaultItem;

GList *      _ige_conf_defaults_read_file  (const gchar  *path,
					    GError      **error);
void         _ige_conf_defaults_free_list  (GList        *defaults);
gchar *      _ige_conf_defaults_get_root   (GList        *defaults);
const gchar *_ige_conf_defaults_get_string (GList        *defaults,
					    const gchar  *key);
gint         _ige_conf_defaults_get_int    (GList        *defaults,
					    const gchar  *key);
gboolean     _ige_conf_defaults_get_bool   (GList        *defaults,
					    const gchar  *key);

G_END_DECLS

#endif /* __IGE_CONF_PRIVATE_H__ */
