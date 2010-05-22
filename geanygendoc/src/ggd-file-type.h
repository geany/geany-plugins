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

#ifndef H_GGD_FILE_TYPE
#define H_GGD_FILE_TYPE

#include <stdio.h>
#include <glib.h>
#include <ctpl/ctpl.h>
#include <geanyplugin.h>

#include "ggd-doc-type.h"
#include "ggd-macros.h"

G_BEGIN_DECLS
GGD_BEGIN_PLUGIN_API


typedef struct _GgdFileType GgdFileType;

struct _GgdFileType
{
  gint          ref_count;
  filetype_id   geany_ft;
  
  /* TODO: add support for custom environment variables, which may be used
   *       for tuning the templates easily (e.g. Doxygen's prefix character,
   *       whether to add a since tag and so on) */
  GRegex       *match_function_arguments;
  /*GRegex       *match_function_start;*/
  CtplEnviron  *user_env;
  GHashTable   *doctypes;
};


GgdFileType        *ggd_file_type_new       (filetype_id        type);
GgdFileType        *ggd_file_type_ref       (GgdFileType       *filetype);
void                ggd_file_type_unref     (GgdFileType       *filetype);
void                ggd_file_type_dump      (const GgdFileType *filetype,
                                             FILE              *stream);
void                ggd_file_type_add_doc   (GgdFileType       *filetype,
                                             GgdDocType        *doctype);
GgdDocType         *ggd_file_type_get_doc   (const GgdFileType *filetype,
                                             const gchar       *name);


GGD_END_PLUGIN_API
G_END_DECLS

#endif /* guard */
