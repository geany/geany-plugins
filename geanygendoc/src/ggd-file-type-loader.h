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

#ifndef H_GGD_CONF_LOADER
#define H_GGD_CONF_LOADER

#include <glib.h>

#include "ggd-file-type.h"
#include "ggd-macros.h"

G_BEGIN_DECLS
GGD_BEGIN_PLUGIN_API


/**
 * GgdConfError:
 * @GGD_FILE_TYPE_LOAD_ERROR_READ: A read failed
 * @GGD_FILE_TYPE_LOAD_ERROR_PARSE: Parsing of a part of the input failed
 * @GGD_FILE_TYPE_LOAD_ERROR_FAILED: Something failed
 * 
 * Error codes of the %GGD_FILE_TYPE_LOAD_ERROR domain.
 */
typedef enum _GgdConfError
{
  GGD_FILE_TYPE_LOAD_ERROR_READ,
  GGD_FILE_TYPE_LOAD_ERROR_PARSE,
  GGD_FILE_TYPE_LOAD_ERROR_FAILED
} GgdConfError;

/*< standard >*/
#define GGD_FILE_TYPE_LOAD_ERROR (ggd_file_type_load_error_quark ())

GQuark          ggd_file_type_load_error_quark    (void) G_GNUC_CONST;
gboolean        ggd_file_type_load                (GgdFileType *filetype,
                                                   const gchar *file,
                                                   GError     **error);


GGD_END_PLUGIN_API
G_END_DECLS

#endif /* guard */
