/*
 *  
 *  GeanyGenDoc, a Geany plugin to ease generation of source code documentation
 *  Copyright (C) 2009-2011  Colomban Wendling <ban@herbesfolles.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h" /* for the gettext domain */
#endif

#include "ggd.h"

#include <string.h>
#include <ctype.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <ctpl/ctpl.h>
#include <geanyplugin.h>

#include "ggd-file-type.h"
#include "ggd-file-type-manager.h"
#include "ggd-utils.h"
#include "ggd-plugin.h"


/* sci_get_line_indentation() is not in the plugin API for now (API v183), then
 * implement it -- borrowed from Geany's sciwrappers.c */
#ifndef sci_get_line_indentation
# define sci_get_line_indentation(sci, line) \
  (scintilla_send_message (sci, SCI_GETLINEINDENTATION, line, 0))
#endif /* sci_get_line_indentation */


/* The value that replace the "cursor" variable in templates, used to find it
 * and therefore the cursor position. This should be a value that the user never
 * want to output; otherwise it would behave strangely from the user point of
 * view, since it is removed from the output. */
#define GGD_CURSOR_IDENTIFIER     "{cursor}"
#define GGD_CURSOR_IDENTIFIER_LEN 8

/* wrapper for ctpl_parser_parse() that returns a string. Free with g_free() */
static gchar *
parser_parse_to_string (const CtplToken *tree,
                        CtplEnviron     *env,
                        GError         **error)
{
  GOutputStream    *gostream;
  CtplOutputStream *ostream;
  gchar            *output = NULL;
  
  gostream = g_memory_output_stream_new (NULL, 0, g_try_realloc, NULL);
  ostream = ctpl_output_stream_new (gostream);
  if (ctpl_parser_parse (tree, env, ostream, error)) {
    GMemoryOutputStream  *memostream = G_MEMORY_OUTPUT_STREAM (gostream);
    gsize                 size;
    gsize                 data_size;
    
    output = g_memory_output_stream_get_data (memostream);
    size = g_memory_output_stream_get_size (memostream);
    data_size = g_memory_output_stream_get_data_size (memostream);
    if (size <= data_size) {
      gpointer newptr;
      
      newptr = g_try_realloc (output, size + 1);
      if (newptr) {
        output = newptr;
        size ++;
      } else {
        /* not enough memory, simulate a memory output stream error */
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_NO_SPACE,
                     _("Failed to resize memory output stream"));
        g_free (output);
        output = NULL;
      }
    }
    if (size > data_size) {
      output[data_size] = 0;
    }
  }
  ctpl_output_stream_unref (ostream);
  g_object_unref (gostream);
  
  return output;
}

/* extracts arguments from @args according to the given filetype */
static CtplValue *
get_arg_list_from_string (GgdFileType  *ft,
                          const gchar  *args)
{
  CtplValue *arg_list = NULL;
  
  g_return_val_if_fail (args != NULL, NULL);
  
  if (ft->match_function_arguments) {
    GMatchInfo *match_info;
    
    /*g_debug ("Trying to match against \"%s\"", args);*/
    if (! g_regex_match (ft->match_function_arguments, args, 0, &match_info)) {
      msgwin_status_add (_("Argument parsing regular expression did not match "
                           "(argument list was: \"%s\")"), args);
    } else {
      arg_list = ctpl_value_new_array (CTPL_VTYPE_STRING, 0, NULL);
      
      while (g_match_info_matches (match_info)) {
        gchar  *word = g_match_info_fetch (match_info, 1);
        
        /*g_debug ("Found arg: '%s'", word);*/
        if (word) {
          ctpl_value_array_append_string (arg_list, word);
        }
        g_free (word);
        g_match_info_next (match_info, NULL);
      }
    }
    g_match_info_free (match_info);
  }
  
  return arg_list;
}

/* pushes @value into @env as @{symbol}_list */
static void
hash_table_env_push_list_cb (gpointer symbol,
                             gpointer value,
                             gpointer env)
{
  gchar *symbol_name;
  
  symbol_name = g_strconcat (symbol, "_list", NULL);
  ctpl_environ_push (env, symbol_name, value);
  g_free (symbol_name);
}

/* gets the environment for a particular tag */
static CtplEnviron *
get_env_for_tag (GgdFileType   *ft,
                 GgdDocSetting *setting,
                 GeanyDocument *doc,
                 const TMTag   *tag)
{
  CtplEnviron  *env;
  GList        *children = NULL;
  GPtrArray    *tag_array = doc->tm_file->tags_array;
  gboolean      returns;
  
  env = ctpl_environ_new ();
  ctpl_environ_push_string (env, "cursor", GGD_CURSOR_IDENTIFIER);
  ctpl_environ_push_string (env, "symbol", tag->name);
  /* get argument list it it exists */
  if (tag->arglist) {
    CtplValue  *v;
    
    v = get_arg_list_from_string (ft, tag->arglist);
    if (v) {
      ctpl_environ_push (env, "argument_list", v);
      ctpl_value_free (v);
    }
  }
  /* get return type -- no matter if the return concept is pointless for that
   * particular tag, it's up to the rule to use it when it makes sense */
  returns = ! (tag->var_type != NULL &&
               /* C-style none return type hack */
               strcmp ("void", tag->var_type) == 0);
  ctpl_environ_push_int (env, "returns", returns);
  /* get direct children tags */
  children = ggd_tag_find_children (tag_array, tag,
                                    FILETYPE_ID (doc->file_type));
  if (setting->merge_children) {
    CtplValue *v;
    
    v = ctpl_value_new_array (CTPL_VTYPE_STRING, 0, NULL);
    while (children) {
      TMTag  *el = children->data;
      GList  *tmp = children;
      
      if (el->type & setting->matches) {
        ctpl_value_array_append_string (v, el->name);
      }
      
      children = g_list_next (children);
      g_list_free_1 (tmp);
    }
    ctpl_environ_push (env, "children", v);
    ctpl_value_free (v);
  } else {
    GHashTable  *vars;
    
    vars = g_hash_table_new_full (g_str_hash, g_str_equal,
                                  NULL, (GDestroyNotify)ctpl_value_free);
    while (children) {
      TMTag        *el        = children->data;
      const gchar  *type_name = ggd_tag_get_type_name (el);
      CtplValue    *v;
      GList        *tmp = children;
      
      if (type_name && el->type & setting->matches) {
        v = g_hash_table_lookup (vars, type_name);
        if (! v) {
          v = ctpl_value_new_array (CTPL_VTYPE_STRING, 0, NULL);
          g_hash_table_insert (vars, (gpointer)type_name, v);
        }
        ctpl_value_array_append_string (v, el->name);
      }
      
      children = g_list_next (children);
      g_list_free_1 (tmp);
    }
    /* insert children into the environment */
    g_hash_table_foreach (vars, hash_table_env_push_list_cb, env);
    g_hash_table_destroy (vars);
  }
  
  return env;
}

/* parses the template @tpl with the environment of @tag */
static gchar *
get_comment (GgdFileType   *ft,
             GgdDocSetting *setting,
             GeanyDocument *doc,
             const TMTag   *tag,
             gint          *cursor_offset)
{
  gchar *comment = NULL;
  
  if (setting->template) {
    GError      *err = NULL;
    CtplEnviron *env;
    
    env = get_env_for_tag (ft, setting, doc, tag);
    ctpl_environ_merge (env, ft->user_env, FALSE);
    if (! ctpl_environ_add_from_string (env, GGD_OPT_environ, &err)) {
      msgwin_status_add (_("Failed to add global environment, skipping: %s"),
                         err->message);
      g_clear_error (&err);
    }
    comment = parser_parse_to_string (setting->template, env, &err);
    if (! comment) {
      msgwin_status_add (_("Failed to build comment: %s"), err->message);
      g_error_free (err);
    } else {
      gchar *cursor_str;
      
      cursor_str = strstr (comment, GGD_CURSOR_IDENTIFIER);
      if (cursor_str && cursor_offset) {
        *cursor_offset = cursor_str - comment;
      }
      /* remove the cursor identifier(s) from the final comment */
      while (cursor_str) {
        memmove (cursor_str, cursor_str + GGD_CURSOR_IDENTIFIER_LEN,
                 strlen (cursor_str) - GGD_CURSOR_IDENTIFIER_LEN + 1);
        cursor_str = strstr (cursor_str, GGD_CURSOR_IDENTIFIER);
      }
    }
  }
  
  return comment;
}

/* Adjusts line where insert a function documentation comment.
 * This function adjusts start line of a (C) function start to be sure it is
 * before the return type, even if style places return type in the line that
 * precede function name.
 */
static gint
adjust_function_start_line (ScintillaObject *sci,
                            const gchar     *func_name,
                            gint             line)
{
  gchar  *str;
  gsize   i;
  
  str = sci_get_line (sci, line);
  for (i = 0; isspace (str[i]); i++);
  if (strncmp (&str[i], func_name, strlen (func_name)) == 0) {
    /* function return type is not in the same line as function name */
    #if 0
    /* this solution is more excat but perhaps too much complex for the gain */
    gint pos;
    gchar c;
    
    pos = sci_get_position_from_line (sci, line) - 1;
    for (; (c = sci_get_char_at (sci, pos)); pos --) {
      if (c == '\n' || c == '\r')
        line --;
      else if (! isspace (c))
        break;
    }
    #else
    line --;
    #endif
  }
  g_free (str);
  
  return line;
}

/* adjusts the start line of a tag */
static gint
adjust_start_line (ScintillaObject *sci,
                   GPtrArray       *tags,
                   const TMTag     *tag,
                   gint             line)
{
  if (tag->type & (tm_tag_function_t | tm_tag_prototype_t |
                   tm_tag_macro_with_arg_t)) {
    line = adjust_function_start_line (sci, tag->name, line);
  }
  
  return line;
}

/* inserts the comment for @tag in @sci according to @setting */
static gboolean
do_insert_comment (GeanyDocument   *doc,
                   const TMTag     *tag,
                   GgdFileType     *ft,
                   GgdDocSetting   *setting)
{
  gboolean          success = FALSE;
  gchar            *comment;
  gint              cursor_offset = 0;
  ScintillaObject  *sci = doc->editor->sci;
  GPtrArray        *tag_array = doc->tm_file->tags_array;
  
  comment = get_comment (ft, setting, doc, tag, &cursor_offset);
  if (comment) {
    gint pos = 0;
    
    switch (setting->position) {
      case GGD_POS_AFTER:
        pos = sci_get_line_end_position (sci, tag->line - 1);
        break;
      
      case GGD_POS_BEFORE: {
        gint line;
        
        line = tag->line - 1;
        line = adjust_start_line (sci, tag_array, tag, line);
        pos = sci_get_position_from_line (sci, line);
        if (GGD_OPT_indent) {
          while (isspace (sci_get_char_at (sci, pos))) {
            pos++;
          }
        }
        break;
      }
      
      case GGD_POS_CURSOR:
        pos = sci_get_current_position (sci);
        break;
    }
    editor_insert_text_block (doc->editor, comment, pos, cursor_offset,
                              -1, TRUE);
    success = TRUE;
  }
  g_free (comment);
  
  return success;
}

/* Gets the #GgdDocSetting that applies for a given tag.
 * Since a policy may forward documenting to a parent, tag that actually applies
 * is returned in @real_tag. */
static GgdDocSetting *
get_setting_from_tag (GgdDocType     *doctype,
                      GeanyDocument  *doc,
                      const TMTag    *tag,
                      const TMTag   **real_tag)
{
  GgdDocSetting  *setting = NULL;
  gchar          *hierarchy;
  GPtrArray      *tag_array = doc->tm_file->tags_array;
  GeanyFiletypeID geany_ft = FILETYPE_ID (doc->file_type);
  
  hierarchy = ggd_tag_resolve_type_hierarchy (tag_array, geany_ft, tag);
  /*g_debug ("type hierarchy for tag %s is: %s", tag->name, hierarchy);*/
  *real_tag = tag;
  if (hierarchy) {
    gint  nth_child;
    
    setting = ggd_doc_type_resolve_setting (doctype, hierarchy, &nth_child);
    if (setting) {
      for (; nth_child > 0; nth_child--) {
        *real_tag = ggd_tag_find_parent (tag_array, geany_ft, *real_tag);
      }
    }
    g_free (hierarchy);
  }
  
  return setting;
}

/*
 * get_config:
 * @doc: A #GeanyDocument for which get the configuration
 * @doc_type: A documentation type identifier
 * @file_type_: Return location for the corresponding #GgdFileType, or %NULL
 * @doc_type_: Return location for the corresponding #GgdDocType, or %NULL
 * 
 * Gets the configuration for a document and a documentation type.
 * 
 * Returns: %TRUE on success, %FALSE otherwise.
 */
static gboolean
get_config (GeanyDocument  *doc,
            const gchar    *doc_type,
            GgdFileType   **file_type_,
            GgdDocType    **doc_type_)
{
  gboolean      success = FALSE;
  GgdFileType  *ft;
  
  ft = ggd_file_type_manager_get_file_type (doc->file_type->id);
  if (ft) {
    GgdDocType *doctype;
    
    doctype = ggd_file_type_get_doc (ft, doc_type);
    if (! doctype) {
      msgwin_status_add (_("Documentation type \"%s\" does not exist for "
                           "language \"%s\"."),
                         doc_type, doc->file_type->name);
    } else {
      if (file_type_) *file_type_ = ft;
      if (doc_type_) *doc_type_ = doctype;
      success = TRUE;
    }
  }
  
  return success;
}

/*
 * insert_multiple_comments:
 * @doc: A #GeanyDocument in which insert comments
 * @filetype: The #GgdFileType to use
 * @doctype: The #GgdDocType to use
 * @sorted_tag_list: A list of tag to document. This list must be sorted by
 *                   tag's line.
 * 
 * Tries to insert the documentation for all tags listed in @sorted_tag_list,
 * taking care of settings and duplications.
 * 
 * Returns: %TRUE on success, %FALSE otherwise.
 */
static gboolean
insert_multiple_comments (GeanyDocument *doc,
                          GgdFileType   *filetype,
                          GgdDocType    *doctype,
                          GList         *sorted_tag_list)
{
  gboolean          success = FALSE;
  GList            *node;
  ScintillaObject  *sci = doc->editor->sci;
  GHashTable       *tag_done_table; /* keeps the list of documented tags.
                                     * Useful since documenting a tag might
                                     * actually document another one */
  
  success = TRUE;
  tag_done_table = g_hash_table_new (NULL, NULL);
  sci_start_undo_action (sci);
  for (node = sorted_tag_list; node; node = node->next) {
    GgdDocSetting  *setting;
    const TMTag    *tag = node->data;
    
    setting = get_setting_from_tag (doctype, doc, tag, &tag);
    if (setting && ! g_hash_table_lookup (tag_done_table, tag)) {
      if (! do_insert_comment (doc, tag, filetype, setting)) {
        success = FALSE;
        break;
      } else {
        g_hash_table_insert (tag_done_table, (gpointer)tag, (gpointer)tag);
      }
    } else if (! setting) {
      msgwin_status_add (_("No setting applies to symbol \"%s\" of type \"%s\" "
                           "at line %lu."),
                         tag->name, ggd_tag_get_type_name (tag),
                         tag->line);
    }
  }
  sci_end_undo_action (sci);
  g_hash_table_destroy (tag_done_table);
  
  return success;
}

/**
 * ggd_insert_comment:
 * @doc: The document in which insert the comment
 * @line: SCI's line for which generate a comment. Usually the current line.
 * @doc_type: Documentation type identifier
 * 
 * Tries to insert a comment in a #GeanyDocument.
 * 
 * <warning>
 *   if tag list is not up-to-date, the result can be surprising
 * </warning>
 * 
 * Returns: %TRUE is a comment was added, %FALSE otherwise.
 */
gboolean
ggd_insert_comment (GeanyDocument  *doc,
                    gint            line,
                    const gchar    *doc_type)
{
  gboolean          success = FALSE;
  const TMTag      *tag = NULL;
  GPtrArray        *tag_array = NULL;
  GgdFileType      *filetype = NULL;
  GgdDocType       *doctype = NULL;
  
  g_return_val_if_fail (DOC_VALID (doc), FALSE);
  
 again:
  
  if (doc->tm_file) {
    tag_array = doc->tm_file->tags_array;
    tag = ggd_tag_find_from_line (tag_array, line + 1 /* it is a SCI line */);
  }
  if (! tag) {
    msgwin_status_add (_("No valid tag at line %d."), line);
  } else {
    if (get_config (doc, doc_type, &filetype, &doctype)) {
      GgdDocSetting  *setting;
      GList          *tag_list = NULL;
      
      setting = get_setting_from_tag (doctype, doc, tag, &tag);
      if (setting && setting->policy == GGD_POLICY_PASS) {
        /* We want to completely skip this tag, so try previous line instead
         * FIXME: this implementation is kinda ugly... */
        line--;
        goto again;
      }
      if (setting && setting->autodoc_children) {
        tag_list = ggd_tag_find_children_filtered (tag_array, tag,
                                                   FILETYPE_ID (doc->file_type),
                                                   setting->matches);
      }
      /* we assume that a parent always comes before any children, then simply add
       * it at the end */
      tag_list = g_list_append (tag_list, (gpointer)tag);
      success = insert_multiple_comments (doc, filetype, doctype, tag_list);
      g_list_free (tag_list);
    }
  }
  
  return success;
}

/**
 * ggd_insert_all_comments:
 * @doc: A #GeanyDocument for which insert the comments
 * @doc_type: Documentation type identifier
 * 
 * Tries to insert a comment for each symbol of a document.
 * 
 * Returns: %TRUE on full success, %FALSE otherwise.
 */
gboolean
ggd_insert_all_comments (GeanyDocument *doc,
                         const gchar   *doc_type)
{
  gboolean      success = FALSE;
  GgdFileType  *filetype = NULL;
  GgdDocType   *doctype = NULL;
  
  g_return_val_if_fail (DOC_VALID (doc), FALSE);
  
  if (! doc->tm_file) {
    msgwin_status_add (_("No tags in the document"));
  } else if (get_config (doc, doc_type, &filetype, &doctype)) {
    GList    *tag_list;
    
    /* get a sorted list of tags to be sure to insert by the end of the
     * document, then we don't modify the element's position of tags we'll work
     * on */
    tag_list = ggd_tag_sort_by_line_to_list (doc->tm_file->tags_array,
                                             GGD_SORT_DESC);
    success = insert_multiple_comments (doc, filetype, doctype, tag_list);
    g_list_free (tag_list);
  }
  
  return success;
}
