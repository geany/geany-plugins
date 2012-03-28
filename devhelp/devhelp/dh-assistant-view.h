/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Sven Herzberg
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifndef __DH_ASSISTANT_VIEW_H__
#define __DH_ASSISTANT_VIEW_H__

#include <webkit/webkit.h>
#include "dh-base.h"
#include "dh-link.h"

G_BEGIN_DECLS

#define DH_TYPE_ASSISTANT_VIEW         (dh_assistant_view_get_type ())
#define DH_ASSISTANT_VIEW(i)           (G_TYPE_CHECK_INSTANCE_CAST ((i), DH_TYPE_ASSISTANT_VIEW, DhAssistantView))
#define DH_ASSISTANT_VIEW_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), DH_TYPE_ASSISTANT_VIEW, DhAssistantViewClass))
#define DH_IS_ASSISTANT_VIEW(i)        (G_TYPE_CHECK_INSTANCE_TYPE ((i), DH_TYPE_ASSISTANT_VIEW))
#define DH_IS_ASSISTANT_VIEW_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), DH_ASSISTANT_VIEW))
#define DH_ASSISTANT_VIEW_GET_CLASS(i) (G_TYPE_INSTANCE_GET_CLASS ((i), DH_TYPE_ASSISTANT_VIEW, DhAssistantView))

typedef struct _DhAssistantView      DhAssistantView;
typedef struct _DhAssistantViewClass DhAssistantViewClass;

struct _DhAssistantView {
        WebKitWebView parent_instance;
};

struct _DhAssistantViewClass {
        WebKitWebViewClass parent_class;
};

GType      dh_assistant_view_get_type (void) G_GNUC_CONST;
GtkWidget* dh_assistant_view_new      (void);
gboolean   dh_assistant_view_search   (DhAssistantView *view,
                                       const gchar     *str);
DhBase*    dh_assistant_view_get_base (DhAssistantView *view);
void       dh_assistant_view_set_base (DhAssistantView *view,
                                       DhBase          *base);
gboolean   dh_assistant_view_set_link (DhAssistantView *view,
                                       DhLink          *link);
G_END_DECLS

#endif /* __DH_ASSISTANT_VIEW_H__ */
