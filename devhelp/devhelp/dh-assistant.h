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

#ifndef __DH_ASSISTANT_H__
#define __DH_ASSISTANT_H__

#include <gtk/gtk.h>
#include "dh-base.h"

G_BEGIN_DECLS

#define DH_TYPE_ASSISTANT         (dh_assistant_get_type ())
#define DH_ASSISTANT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), DH_TYPE_ASSISTANT, DhAssistant))
#define DH_ASSISTANT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), DH_TYPE_ASSISTANT, DhAssistantClass))
#define DH_IS_ASSISTANT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), DH_TYPE_ASSISTANT))
#define DH_IS_ASSISTANT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), DH_TYPE_ASSISTANT))
#define DH_ASSISTANT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), DH_TYPE_ASSISTANT, DhAssistantClass))

typedef struct _DhAssistant      DhAssistant;
typedef struct _DhAssistantClass DhAssistantClass;

struct _DhAssistant {
        GtkWindow parent_instance;
};

struct _DhAssistantClass {
        GtkWindowClass parent_class;
};

GType      dh_assistant_get_type  (void) G_GNUC_CONST;
GtkWidget *dh_assistant_new       (DhBase      *base);
gboolean   dh_assistant_search    (DhAssistant *assistant,
                                   const gchar *str);

G_END_DECLS

#endif /* __DH_ASSISTANT_H__ */
