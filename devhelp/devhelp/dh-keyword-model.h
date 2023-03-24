/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002 CodeFactory AB
 * Copyright (C) 2002 Mikael Hallendal <micke@imendio.com>
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

#ifndef __DH_KEYWORD_MODEL_H__
#define __DH_KEYWORD_MODEL_H__

#include <gtk/gtk.h>
#include "dh-link.h"
#include "dh-book-manager.h"

G_BEGIN_DECLS

#define DH_TYPE_KEYWORD_MODEL            (dh_keyword_model_get_type ())
#define DH_KEYWORD_MODEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), DH_TYPE_KEYWORD_MODEL, DhKeywordModel))
#define DH_KEYWORD_MODEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), DH_TYPE_KEYWORD_MODEL, DhKeywordModelClass))
#define DH_IS_KEYWORD_MODEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), DH_TYPE_KEYWORD_MODEL))
#define DH_IS_KEYWORD_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), DH_TYPE_KEYWORD_MODEL))
#define DH_KEYWORD_MODEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), DH_TYPE_KEYWORD_MODEL, DhKeywordModelClass))

typedef struct _DhKeywordModel      DhKeywordModel;
typedef struct _DhKeywordModelClass DhKeywordModelClass;
typedef struct _DhKeywordModelPriv  DhKeywordModelPriv;

struct _DhKeywordModel
{
        GObject             parent_instance;
        DhKeywordModelPriv *priv;
};

struct _DhKeywordModelClass
{
        GObjectClass parent_class;
};

enum {
        DH_KEYWORD_MODEL_COL_NAME,
        DH_KEYWORD_MODEL_COL_LINK,
        DH_KEYWORD_MODEL_NUM_COLS
};

GType           dh_keyword_model_get_type  (void);
DhKeywordModel *dh_keyword_model_new       (void);
void            dh_keyword_model_set_words (DhKeywordModel *model,
                                            DhBookManager  *book_manager);
DhLink *        dh_keyword_model_filter    (DhKeywordModel *model,
                                            const gchar    *string,
                                            const gchar    *book_id);

G_END_DECLS

#endif /* __DH_KEYWORD_MODEL_H__ */
