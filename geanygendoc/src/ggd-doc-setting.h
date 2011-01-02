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

#ifndef H_GGD_DOC_SETTING
#define H_GGD_DOC_SETTING

#include <stdio.h>
#include <glib.h>
#include <ctpl/ctpl.h>

#include "ggd-tag-utils.h"
#include "ggd-macros.h"

G_BEGIN_DECLS
GGD_BEGIN_PLUGIN_API


/**
 * _GgdPosition:
 * @GGD_POS_BEFORE: Place the documentation before what it documents
 * @GGD_POS_AFTER: Place the documentation after what it documents
 * @GGD_POS_CURSOR: Place the documentation at the cursor's position
 * 
 * Possible positions for documentation insertion.
 */
typedef enum _GgdPosition
{
  GGD_POS_BEFORE,
  GGD_POS_AFTER,
  GGD_POS_CURSOR
} GgdPosition;

/**
 * _GgdPolicy:
 * @GGD_POLICY_KEEP: Document the symbol
 * @GGD_POLICY_FORWARD: Forward the documentation request to the parent symbol
 * @GGD_POLICY_PASS: Ignore the symbol, do as if it didn't exist
 * 
 * Possible policies for documenting symbols.
 */
typedef enum _GgdPolicy
{
  GGD_POLICY_KEEP,
  GGD_POLICY_FORWARD,
  GGD_POLICY_PASS/*,
  GGD_POLICY_DUPLICATE*/
} GgdPolicy;

typedef struct _GgdDocSetting GgdDocSetting;

struct _GgdDocSetting
{
  gint        ref_count;
  gchar      *match;
  
  CtplToken  *template;
  GgdPosition position;
  GgdPolicy   policy;
  gboolean    merge_children;
  TMTagType   matches;
  gboolean    autodoc_children;
};


GgdPosition     ggd_position_from_string      (const gchar *string);
GgdPolicy       ggd_policy_from_string        (const gchar *string);
gboolean        ggd_merge_policy_from_string  (const gchar *string);
#define         ggd_match_from_string         ggd_tag_type_from_name

GgdDocSetting  *ggd_doc_setting_new           (const gchar *match);
GgdDocSetting  *ggd_doc_setting_ref           (GgdDocSetting *setting);
void            ggd_doc_setting_unref         (GgdDocSetting *setting);
void            ggd_doc_setting_dump          (const GgdDocSetting *setting,
                                               FILE                *stream);
gboolean        ggd_doc_setting_matches       (const GgdDocSetting *setting,
                                               const gchar         *match,
                                               gssize               match_len);


GGD_END_PLUGIN_API
G_END_DECLS

#endif /* guard */
