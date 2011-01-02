/*
 *  
 *  GeanyGenDoc, a Geany plugin to ease generation of source code documentation
 *  Copyright (C) 2010-2011  Colomban Wendling <ban@herbesfolles.org>
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

#ifndef H_GGD_DOC_TYPE
#define H_GGD_DOC_TYPE

#include <stdio.h>
#include <glib.h>

#include "ggd-doc-setting.h"
#include "ggd-macros.h"

G_BEGIN_DECLS
GGD_BEGIN_PLUGIN_API


typedef struct  _GgdDocType GgdDocType;

struct _GgdDocType
{
  gint    ref_count;
  gchar  *name;
  GList  *settings;
};


GgdDocType     *ggd_doc_type_new              (const gchar *name);
GgdDocType     *ggd_doc_type_ref              (GgdDocType  *doctype);
void            ggd_doc_type_unref            (GgdDocType  *doctype);
void            ggd_doc_type_dump             (const GgdDocType  *doctype,
                                               FILE              *stream);
GgdDocSetting  *ggd_doc_type_get_setting      (const GgdDocType  *doctype,
                                               const gchar       *match);
GgdDocSetting  *ggd_doc_type_resolve_setting  (const GgdDocType  *doctype,
                                               const gchar       *match,
                                               gint              *nth_child);


GGD_END_PLUGIN_API
G_END_DECLS

#endif /* guard */
