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

#ifndef H_GGD_UTILS
#define H_GGD_UTILS

#include <glib.h>
#include <geanyplugin.h> /* Geany's utils for some wrappers */

#include "ggd-macros.h"

G_BEGIN_DECLS
GGD_BEGIN_PLUGIN_API


/**
 * GgdPerms:
 * @GGD_PERM_R: Read permission
 * @GGD_PERM_W: Write permission
 * @GGD_PERM_RW: Both read and write permissions
 * @GGD_PERM_NOCREAT: Don't create new files
 * 
 * Flags representing permissions.
 */
enum _GgdPerms {
  GGD_PERM_R        = 1 << 0,
  GGD_PERM_W        = 1 << 1,
  GGD_PERM_RW       = GGD_PERM_R | GGD_PERM_W,
  /* a bit ugly, it isn't a permission */
  GGD_PERM_NOCREAT  = 1 << 2
};

typedef enum _GgdPerms GgdPerms;

gchar          *ggd_get_config_file             (const gchar *name,
                                                 const gchar *section,
                                                 GgdPerms     perms_req,
                                                 GError     **error);

/**
 * GGD_PTR_ARRAY_FOR:
 * @ptr_array: A #GPtrArray
 * @idx: A guint variable to use as the index of the current element in the
 *       array
 * @item: A pointer to set to the array's current element
 * 
 * <code>for</code> header to iterate over a #GPtrArray.
 */
#define GGD_PTR_ARRAY_FOR(ptr_array, idx, item) \
  foreach_ptr_array ((item), (idx), (ptr_array))


GGD_END_PLUGIN_API
G_END_DECLS

#endif /* guard */
