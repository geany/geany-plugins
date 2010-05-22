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

#ifndef H_GGD_FILE_TYPE_MANAGER
#define H_GGD_FILE_TYPE_MANAGER

#include <glib.h>
#include <geanyplugin.h>

#include "ggd-file-type.h"
#include "ggd-utils.h"
#include "ggd-macros.h"

G_BEGIN_DECLS
GGD_BEGIN_PLUGIN_API


void              ggd_file_type_manager_init            (void);
void              ggd_file_type_manager_uninit          (void);
void              ggd_file_type_manager_add_file_type   (GgdFileType *filetype);
gchar            *ggd_file_type_manager_get_conf_path   (filetype_id  id,
                                                         GgdPerms     perms_req,
                                                         GError     **error);
GgdFileType      *ggd_file_type_manager_load_file_type  (filetype_id id);
GgdFileType      *ggd_file_type_manager_get_file_type   (filetype_id ft);
GgdDocType       *ggd_file_type_manager_get_doc_type    (filetype_id  ft,
                                                         const gchar *docname);


GGD_END_PLUGIN_API
G_END_DECLS

#endif /* guard */
