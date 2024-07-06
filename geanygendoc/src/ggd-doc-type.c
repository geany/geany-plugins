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


#include "ggd-doc-type.h"

#include <stdio.h>
#include <string.h>
#include <glib.h>

#include "ggd-doc-setting.h"


/**
 * ggd_doc_type_new:
 * @name: Documentation type's name
 * 
 * Creates a new documentation type representation.
 * 
 * Returns: The newly created #GgdDocType. Free with ggd_doc_type_free().
 */
GgdDocType *
ggd_doc_type_new (const gchar *name)
{
  GgdDocType *doctype;
  
  g_return_val_if_fail (name != NULL, NULL);
  
  doctype = g_slice_alloc (sizeof *doctype);
  doctype->ref_count = 1;
  doctype->name = g_strdup (name);
  doctype->settings = NULL;
  
  /*g_debug ("New doc type '%s' [%p]", name, doctype);*/
  
  return doctype;
}

/**
 * ggd_doc_type_ref:
 * @doctype: A #GgdDocType
 * 
 * Adds a reference to @doctype
 * 
 * Returns: the given @doctype
 */
GgdDocType *
ggd_doc_type_ref (GgdDocType *doctype)
{
  g_return_val_if_fail (doctype != NULL, NULL);
  
  g_atomic_int_inc (&doctype->ref_count);
  
  return doctype;
}

/**
 * ggd_doc_type_unref:
 * @doctype: A #GgdDocType
 * 
 * Removes a reference from @doctype and destroy it if it reaches 0.
 */
void
ggd_doc_type_unref (GgdDocType *doctype)
{
  g_return_if_fail (doctype != NULL);
  
  if (g_atomic_int_dec_and_test (&doctype->ref_count)) {
    g_free (doctype->name);
    while (doctype->settings) {
      GList *tmp = doctype->settings;
      
      doctype->settings = g_list_next (doctype->settings);
      ggd_doc_setting_unref (tmp->data);
      g_list_free_1 (tmp);
    }
    g_slice_free1 (sizeof *doctype, doctype);
  }
}

/**
 * ggd_doc_type_dump:
 * @doctype: A #GgdDocType object
 * @fp: File stream where write doctype dump.
 * 
 * Dumps @doctype to @stream.
 */
void
ggd_doc_type_dump (const GgdDocType *doctype,
                   FILE             *stream)
{
  g_return_if_fail (doctype != NULL);
  
  fprintf (stream, "DocType %s [%p]:\n", doctype->name, (gpointer)doctype);
  g_list_foreach (doctype->settings, (GFunc)ggd_doc_setting_dump, stream);
}

/**
 * ggd_doc_type_get_setting:
 * @doctype: A #GgdDocType
 * @match: A match "pattern"
 * 
 * Gets the setting that matches @match (the one that should be used for the
 * given match).
 * 
 * Returns: the first matching #GgdDocSetting or %NULL if none matches.
 */
GgdDocSetting *
ggd_doc_type_get_setting (const GgdDocType *doctype,
                          const gchar      *match)
{
  GgdDocSetting  *setting = NULL;
  GList          *tmp;
  gssize          match_len;
  
  g_return_val_if_fail (doctype != NULL, NULL);
  g_return_val_if_fail (match != NULL, NULL);
  
  match_len = (gssize) strlen (match);
  
  for (tmp = doctype->settings; tmp && ! setting; tmp = g_list_next (tmp)) {
    if (ggd_doc_setting_matches (tmp->data, match, match_len)) {
      setting = tmp->data;
    }
  }
  
  return setting;
}

/* gets the match for the parent of the given one (e.g. foo.bar when given
 * foo.bar.baz) */
static gchar *
get_parent_match (const gchar *match)
{
  gchar *last_dot;
  
  last_dot = strrchr (match, '.');
  
  return last_dot ? g_strndup (match, (gsize)(last_dot - match)) : NULL;
}

/**
 * ggd_doc_type_resolve_setting:
 * @doctype: A #GgdDocType
 * @match: A match "pattern"
 * @nth_child: return location for the number of redirection to go to the final
 *             match
 * 
 * Resolve the setting that should be applied of match @match. This is similar
 * to ggd_doc_type_get_setting() but applies FORWARD policies too (e.g. if
 * @match matches a setting of which the policy is FORWARD, this function will
 * try to resolve the parent and return it, and so on).
 * 
 * Returns: the matching #GgdDocSetting or %NULL if none matches.
 */
 /* FIXME: this doesn't support DUPLICATE, but it is quite useless... */
GgdDocSetting *
ggd_doc_type_resolve_setting (const GgdDocType  *doctype,
                              const gchar       *match,
                              gint              *nth_child)
{
  GgdDocSetting *setting     = NULL;
  gchar         *child_match = NULL;
  
  g_return_val_if_fail (doctype != NULL, NULL);
  g_return_val_if_fail (match != NULL, NULL);
  
  /*g_debug ("Resolving match \"%s\"...", child_match);*/
  if (nth_child) (*nth_child) = 0;
  child_match = g_strdup (match);
  setting = ggd_doc_type_get_setting (doctype, child_match);
  while (setting && setting->policy == GGD_POLICY_FORWARD) {
    gchar *parent_match = get_parent_match (child_match);
    
    if (nth_child) (*nth_child)++;
    /*g_debug ("going to parent (%s)", parent_match);*/
    if (! parent_match) {
      setting = NULL;
    } else {
      setting = ggd_doc_type_get_setting (doctype, parent_match);
      g_free (child_match);
      child_match = parent_match;
    }
  }
  g_free (child_match);
  /*g_debug ("Result is: %s (%p)",
           setting ? setting->match : "(null)",
           (gpointer)setting);*/
  
  return setting;
}
