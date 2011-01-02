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


#include "ggd-file-type.h"

#include <stdio.h>
#include <glib.h>
#include <geanyplugin.h>

#include "ggd-doc-type.h"


/**
 * SECTION: ggd-file-type
 * @include: ggd-file-type.h
 * 
 * A #GgdFileType represents the rules for a language type.
 * 
 * It uses GObject-style reference counting through ggd_file_type_ref() and
 * ggd_file_type_unref().
 */


/**
 * ggd_file_type_new:
 * @type: A #GeanyFiletype ID
 * 
 * Creates a new #GgdFileType for a Geany's file type.
 * 
 * Returns: A newly created #GgdFileType.
 */
GgdFileType *
ggd_file_type_new (filetype_id type)
{
  GgdFileType *ft;
  
  ft = g_slice_alloc (sizeof *ft);
  ft->ref_count = 1;
  ft->geany_ft = type;
  ft->match_function_arguments = NULL;
  ft->user_env = ctpl_environ_new ();
  ft->doctypes = g_hash_table_new_full (g_str_hash, g_str_equal,
                                        NULL, (GDestroyNotify)ggd_doc_type_unref);
  
  return ft;
}

/**
 * ggd_file_type_ref:
 * @filetype: A #GgdFileType
 * 
 * Adds a reference to a #GgdFileType.
 * 
 * Returns: The filetype
 */
GgdFileType *
ggd_file_type_ref (GgdFileType *filetype)
{
  g_return_val_if_fail (filetype != NULL, NULL);
  
  g_atomic_int_inc (&filetype->ref_count);
  
  return filetype;
}

/**
 * ggd_file_type_unref:
 * @filetype: A #GgdFileType
 * 
 * Drops a reference from a #GgdFileType. When a file type's reference count
 * drops to zero, the file type is destroyed.
 */
void
ggd_file_type_unref (GgdFileType *filetype)
{
  g_return_if_fail (filetype != NULL);
  
  if (g_atomic_int_dec_and_test (&filetype->ref_count)) {
    g_hash_table_destroy (filetype->doctypes);
    if (filetype->match_function_arguments) {
      g_regex_unref (filetype->match_function_arguments);
    }
    ctpl_environ_unref (filetype->user_env);
    g_slice_free1 (sizeof *filetype, filetype);
  }
}

/* function to call on each element of the doc type table in order to dump it */
static void
dump_doctypes_hfunc (gpointer key   G_GNUC_UNUSED,
                     gpointer value,
                     gpointer data)
{
  ggd_doc_type_dump (value, data);
}

/**
 * ggd_file_type_dump:
 * @filetype: A #GgdFileType
 * @stream: A file stream to which write the dump
 * 
 * A user-readable dump of a #GgdFileType, mostly for debugging purposes.
 */
void
ggd_file_type_dump (const GgdFileType *filetype,
                    FILE              *stream)
{
  g_return_if_fail (filetype != NULL);
  
  fprintf (stream, "FileType %d [%p]:\n",
                   filetype->geany_ft,
                   (gpointer)filetype);
  fprintf (stream, "Settings:\n"
                   "  function arguments RE: [%p]\n"
                   "           user environ: [%p]\n",
                   (gpointer)filetype->match_function_arguments,
                   (gpointer)filetype->user_env);
  g_hash_table_foreach (filetype->doctypes, dump_doctypes_hfunc, stream);
}

/**
 * ggd_file_type_add_doc:
 * @filetype: A #GgdFileType
 * @doctype: A #GgdDocType
 * 
 * Adds a #GgdDocType to a #GgdFileType.
 */
void
ggd_file_type_add_doc (GgdFileType *filetype,
                       GgdDocType  *doctype)
{
  g_return_if_fail (filetype != NULL);
  
  if (! ggd_file_type_get_doc (filetype, doctype->name)) {
    g_hash_table_insert (filetype->doctypes, doctype->name,
                         ggd_doc_type_ref (doctype));
  }
}

/**
 * ggd_file_type_get_doc:
 * @filetype: A #GgdFileType
 * @name: A documentation type identifier
 * 
 * Gets the #GgdDocType of a #GgdFileType that corresponds to the given
 * documentation type identifier.
 * 
 * Returns: The #GgdDocType corresponding to @name that shouldn't be unref'd, or
 *          %NULL if not found in @filetype.
 */
GgdDocType *
ggd_file_type_get_doc (const GgdFileType *filetype,
                       const gchar       *name)
{
  g_return_val_if_fail (filetype != NULL, NULL);
  g_return_val_if_fail (name != NULL, NULL);
  
  return g_hash_table_lookup (filetype->doctypes, name);
}
