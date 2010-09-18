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


#include "ggd-doc-setting.h"

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <ctpl/ctpl.h>
#include <geanyplugin.h>


/**
 * ggd_doc_setting_new:
 * @match: A setting match pattern
 * 
 * Creates a new #GgdDocSetting.
 * 
 * Returns: The newly created #GgdDocSetting that should be unref'd with
 *          ggd_doc_setting_unref() when no longer needed.
 */
GgdDocSetting *
ggd_doc_setting_new (const gchar *match)
{
  GgdDocSetting *setting;
  
  setting = g_slice_alloc (sizeof *setting);
  setting->ref_count = 1;
  setting->match = g_strdup (match);
  setting->template = NULL;
  setting->position = GGD_POS_BEFORE;
  setting->policy = GGD_POLICY_KEEP;
  setting->merge_children = FALSE;
  setting->matches = tm_tag_max_t;
  setting->autodoc_children = FALSE;
  
  /*g_debug ("New setting matching '%s'", match);*/
  
  return setting;
}

/**
 * ggd_doc_setting_ref:
 * @setting: A #GgdDocSetting
 * 
 * Adds a reference to a #GgdDocSetting.
 * 
 * Returns: The setting.
 */
GgdDocSetting *
ggd_doc_setting_ref (GgdDocSetting *setting)
{
  g_return_val_if_fail (setting != NULL, NULL);
  
  g_atomic_int_inc (&setting->ref_count);
  
  return setting;
}

/**
 * ggd_doc_setting_unref:
 * @setting: A #GgdDocSetting
 * 
 * Drops a reference from a #GgdDocSetting. When its reference count drops to 0,
 * the setting is destroyed.
 */
void
ggd_doc_setting_unref (GgdDocSetting *setting)
{
  g_return_if_fail (setting != NULL);
  
  if (g_atomic_int_dec_and_test (&setting->ref_count)) {
    g_free (setting->match);
    ctpl_lexer_free_tree (setting->template);
    g_slice_free1 (sizeof *setting, setting);
  }
}

/**
 * ggd_doc_setting_dump:
 * @setting: A #GgdDocSetting
 * @stream: A file stream to which write the dump
 * 
 * Dumps a #GgdDocSetting in a human-readable form. This is mostly meant for
 * debugging purposes.
 */
void
ggd_doc_setting_dump (const GgdDocSetting *setting,
                      FILE                *stream)
{
  g_return_if_fail (setting != NULL);
  
  fprintf (stream, "  %s [%p]:\n"
                   "          template: %p\n"
                   "          position: %u\n"
                   "            policy: %u\n"
                   " auto-doc-children: %s\n",
                   setting->match, (gpointer)setting,
                   (gpointer)setting->template,
                   setting->position,
                   setting->policy,
                   setting->autodoc_children ? "True" : "False");
}

/**
 * ggd_doc_setting_matches:
 * @setting: A #GgdDocSetting
 * @match: A match pattern
 * @match_len: Length of @match or -1 if zero-terminated.
 * 
 * Checks whether a #GgdDocSetting matches a given pattern.
 * 
 * Returns: %TRUE if @setting matches the given pattern, %FALSE otherwise
 */
gboolean
ggd_doc_setting_matches (const GgdDocSetting *setting,
                         const gchar         *match,
                         gssize               match_len)
{
  gssize    intern_i = strlen (setting->match);
  gssize    extern_i = match_len >= 0 ? match_len : (gssize)strlen (match);
  gboolean  matches = TRUE;
  
  g_return_val_if_fail (setting != NULL, FALSE);
  
  for (; intern_i >= 0 && extern_i >= 0 && matches; intern_i--, extern_i--) {
    matches = setting->match[intern_i] == match[extern_i];
  }
  
  return matches && intern_i <= 0;
}



/**
 * ggd_position_from_string:
 * @string: A string representing a #GgdPosition
 * 
 * Translates a string to a #GgdPosition.
 * 
 * Returns: The corresponding #GgdPosition, or -1 on error.
 */
GgdPosition
ggd_position_from_string (const gchar *string)
{
  static const struct
  {
    const gchar  *name;
    GgdPosition   pos;
  } positions[] = {
    { "BEFORE", GGD_POS_BEFORE },
    { "AFTER",  GGD_POS_AFTER },
    { "CURSOR", GGD_POS_CURSOR }
  };
  guint i;
  
  g_return_val_if_fail (string != NULL, -1);
  
  for (i = 0; i < G_N_ELEMENTS (positions); i++) {
    if (strcmp (string, positions[i].name) == 0) {
      /*g_debug ("matching position name: %s", positions[i].name);*/
      return positions[i].pos;
    }
  }
  
  return -1;
}

/**
 * ggd_policy_from_string:
 * @string: A string representing a #GgdPolicy
 * 
 * Translates a string to a #GgdPolicy.
 * 
 * Returns: The corresponding #GgdPolicy, or -1 on error.
 */
GgdPolicy
ggd_policy_from_string (const gchar *string)
{
  static const struct
  {
    const gchar  *name;
    GgdPolicy     policy;
  } policies[] = {
    { "KEEP",       GGD_POLICY_KEEP },
    { "FORWARD",    GGD_POLICY_FORWARD },
    { "PASS",       GGD_POLICY_PASS }/*,
    { "DUPLICATE",  GGD_POLICY_DUPLICATE }*/
  };
  guint i;
  
  g_return_val_if_fail (string != NULL, -1);
  
  for (i = 0; i < G_N_ELEMENTS (policies); i++) {
    if (strcmp (string, policies[i].name) == 0) {
      /*g_debug ("matching policy name: %s", policies[i].name);*/
      return policies[i].policy;
    }
  }
  
  return -1;
}

/**
 * ggd_merge_policy_from_string:
 * @string: A string representing a merge policy
 * 
 * Translates a string to a merge policy.
 * 
 * Returns: The merge policy, or -1 on error.
 */
gboolean
ggd_merge_policy_from_string (const gchar *string)
{
  static const struct
  {
    const gchar  *name;
    gboolean      merge;
  } policies[] = {
    { "MERGE",  TRUE },
    { "SPLIT",  FALSE }
  };
  guint i;
  
  g_return_val_if_fail (string != NULL, -1);
  
  for (i = 0; i < G_N_ELEMENTS (policies); i++) {
    if (strcmp (string, policies[i].name) == 0) {
      /*g_debug ("matching merge policy name: %s", policies[i].name);*/
      return policies[i].merge;
    }
  }
  
  return -1;
}

/**
 * ggd_match_from_string:
 * @name: A string representing a match
 * 
 * Translates a string to a match.
 * 
 * Returns: The match, or 0 on error.
 */
/* documents a define found in the header */
