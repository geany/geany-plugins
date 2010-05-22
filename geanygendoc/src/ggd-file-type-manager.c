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


#include "ggd-file-type-manager.h"

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <geanyplugin.h>

#include "ggd-plugin.h" /* to be able to use Geany data */
#include "ggd-file-type.h"
#include "ggd-file-type-loader.h"
#include "ggd-utils.h"


/**
 * SECTION: ggd-file-type-manager
 * @include ggd-file-type-manager.h
 * 
 * The file type manager. It manages a set of file type, caching them and
 * loading them when needed.
 * 
 * Use ggd_file_type_manager_init() to initialize the file type manager, and
 * ggd_file_type_manager_uninit() to de-initialize it, releasing the resources
 * allocated for it. A re-initialization can be done be de-initializing and the
 * re-initializing the file type manager. This can be useful to e.g. flush the
 * caches and the for reloading of file types.
 * 
 * To get a file type from the file type manager, simply use
 * ggd_file_type_manager_get_file_type(). If the requested file type must be
 * loaded first, it will be done transparently.
 */


/* Hash table holding the loaded #GgdFileType<!-- -->s */
static GHashTable *GGD_ft_table = NULL;

/* checks whether the file type manager is initialized */
#define ggd_file_type_manager_is_initialized() (GGD_ft_table != NULL)

/**
 * ggd_file_type_manager_init:
 * 
 * Initializes the file type manager. This must be called before any other file
 * type manager functions.
 */
void
ggd_file_type_manager_init (void)
{
  g_return_if_fail (! ggd_file_type_manager_is_initialized ());
  
  GGD_ft_table = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                        NULL,
                                        (GDestroyNotify)ggd_file_type_unref);
}

/**
 * ggd_file_type_manager_uninit:
 * 
 * De-initializes the file type manager, releasing memory allocated by it.
 * It may also be used to force reloading of file type descriptions: simply call
 * it and then re-initialize it with ggd_file_type_manager_init().
 */
void
ggd_file_type_manager_uninit (void)
{
  g_return_if_fail (ggd_file_type_manager_is_initialized ());
  
  g_hash_table_destroy (GGD_ft_table);
  GGD_ft_table = NULL;
}

/**
 * ggd_file_type_manager_add_file_type:
 * @filetype: A #GgdFileType
 * 
 * Adds a file type to the file type manager.
 */
void
ggd_file_type_manager_add_file_type (GgdFileType *filetype)
{
  g_return_if_fail (ggd_file_type_manager_is_initialized ());
  g_return_if_fail (filetype != NULL);
  
  g_hash_table_insert (GGD_ft_table,
                       GINT_TO_POINTER (filetype->geany_ft),
                       ggd_file_type_ref (filetype));
}

/* Same as ggd_file_type_manager_get_conf_path() but uses a #GeanyFiletype and
 * doesn't do come safety checks. */
static gchar *
ggd_file_type_manager_get_conf_path_intern (GeanyFiletype  *geany_ft,
                                            GgdPerms        prems_req,
                                            GError        **error)
{
  gchar  *ft_name_down;
  gchar  *ft_name_conf;
  gchar  *filename;
  
  ft_name_down = g_ascii_strdown (geany_ft->name, -1);
  ft_name_conf = g_strconcat (ft_name_down, ".conf", NULL);
  g_free (ft_name_down);
  filename = ggd_get_config_file (ft_name_conf, "filetypes", prems_req, error);
  g_free (ft_name_conf);
  
  return filename;
}

/**
 * ggd_file_type_manager_get_conf_path:
 * @id: The ID of a #GeanyFiletype
 * @prems_req: Required permissions on the file
 * @error: Return location for errors, or %NULL to ignore them
 * 
 * Gets the path to the configuration file of a file type from a given
 * #GeanyFiletype ID.
 * 
 * Returns: A newly allocated path to the requested configuration file that
 *          should be freed with g_free(), or %NULL on error.
 */
gchar *
ggd_file_type_manager_get_conf_path (filetype_id  id,
                                     GgdPerms     perms_req,
                                     GError     **error)
{
  g_return_val_if_fail (id >= 0 && id < geany->filetypes_array->len, NULL);
  
  return ggd_file_type_manager_get_conf_path_intern (filetypes[id], perms_req,
                                                     error);
}

/**
 * ggd_file_type_manager_load_file_type:
 * @id: A #GeanyFiletype ID
 * 
 * Loads the #GgdFileType corresponding to a Geany's file type.
 * 
 * Returns: The loaded #GgdFileType, that should not be unref'd, or %NULL on
 *          failure.
 */
GgdFileType *
ggd_file_type_manager_load_file_type (filetype_id id)
{
  GeanyFiletype  *geany_ft;
  GgdFileType    *ft = NULL;
  gchar          *filename;
  GError         *err = NULL;
  
  g_return_val_if_fail (ggd_file_type_manager_is_initialized (), NULL);
  g_return_val_if_fail (id >= 0 && id < geany->filetypes_array->len, NULL);
  
  geany_ft = filetypes[id];
  filename = ggd_file_type_manager_get_conf_path_intern (geany_ft, GGD_PERM_R,
                                                         &err);
  if (! filename) {
    msgwin_status_add (_("File type configuration file for language \"%s\" not "
                         "found: %s"),
                       geany_ft->name, err->message);
    g_error_free (err);
  } else {
    ft = ggd_file_type_new (id);
    if (! ggd_file_type_load (ft, filename, &err)) {
      msgwin_status_add (_("Failed to load file type \"%s\" from file \"%s\": "
                           "%s"),
                         geany_ft->name, filename, err->message);
      g_error_free (err);
      ggd_file_type_unref (ft), ft = NULL;
    } else {
      ggd_file_type_manager_add_file_type (ft);
      ggd_file_type_unref (ft);
    }
    g_free (filename);
  }
  
  return ft;
}

/**
 * ggd_file_type_manager_get_file_type:
 * @id: A #GeanyFiletype ID
 * 
 * Gets a #GgdFileType from the file type manager by its Geany's file type ID.
 * If the file type isn't already known, it will be transparently loaded.
 * 
 * Returns: A #GgdFileType that shouldn't be unref'd, or %NULL if the requested
 *          file type neither can be found on the loaded file type nor can be
 *          loaded.
 */
GgdFileType *
ggd_file_type_manager_get_file_type (filetype_id id)
{
  GgdFileType *ft;
  
  g_return_val_if_fail (ggd_file_type_manager_is_initialized (), NULL);
  
  ft = g_hash_table_lookup (GGD_ft_table, GINT_TO_POINTER (id));
  if (! ft) {
    /* if the filetype isn't loaded, try to load it
     * FIXME: it might be useful to have a way to know if a filetype loading
     *        had already failed not to retry again and again on each query? */
    ft = ggd_file_type_manager_load_file_type (id);
  }
  
  return ft;
}

/**
 * ggd_file_type_manager_get_doc_type:
 * @ft: A #GeanyFiletype ID
 * @docname: The documentation type identifier
 * 
 * Gets the #GgdDocType for a file type and a documentation type identifier.
 * 
 * Returns: The #GgdDocType corresponding to the file and documentation type,
 *          or %NULL on failure.
 */
GgdDocType *
ggd_file_type_manager_get_doc_type (filetype_id  ft,
                                    const gchar *docname)
{
  GgdDocType   *doctype = NULL;
  GgdFileType  *filetype;
  
  filetype = ggd_file_type_manager_get_file_type (ft);
  if (filetype) {
    doctype = ggd_file_type_get_doc (filetype, docname);
  }
  
  return doctype;
}
