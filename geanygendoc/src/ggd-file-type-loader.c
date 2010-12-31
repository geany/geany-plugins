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

/*
 * FIXME: This file is a little big and have some duplicated things. Try to
 *        merge and/or what can be.
 */


#include "ggd-file-type-loader.h"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <ctpl/ctpl.h>
#include <geanyplugin.h>

#include "ggd-doc-setting.h"
#include "ggd-doc-type.h"
#include "ggd-file-type.h"


/**
 * ggd-file-type-loader:
 * 
 * Loader for #GgdFileType, that provides ggd_file_type_load().
 * 
 * File type is of the form:
 * 
 * <code>
 * string               ::= ( """ .* """ | "'" .* "'" )
 * constant             ::= [_A-Z][_A-Z0-9]+
 * integer              ::= [0-9]+
 * boolean              ::= ( "True" | "False" )
 * setting_value        ::= ( string | constant | integer )
 * setting              ::= "setting-name" "=" setting_value ";"
 * setting_list         ::= ( "{" setting* "}" | setting )
 * setting_section      ::= "settings" "=" setting_list
 * 
 * position             ::= ( "BEFORE" | "AFTER" | "CURSOR" )
 * policy               ::= ( "KEEP" | "FORWARD" )
 * children             ::= ( "SPLIT" | "MERGE" )
 * type                 ::= ( "class" | "enum" | "enumval" | "field" |
 *                            "function" | "interface" | "member" | "method" |
 *                            "namespace" | "package" | "prototype" | "struct" |
 *                            "typedef" | "union" | "variable" | "extern" |
 *                            "define" | "macro" | "file" )
 * matches              ::= type ( "|" type )*
 * doctype_subsetting   ::= ( "template"          "=" string |
 *                            "position"          "=" position |
 *                            "policy"            "=" policy |
 *                            "children"          "=" children |
 *                            "matches"           "=" matches |
 *                            "auto_doc_children" "=" boolean ) ";"
 * match                ::= type ( "." type )*
 * doctype_setting      ::= ( match "=" "{" doctype_subsetting* "}" |
 *                            match "." doctype_subsetting )
 * doctype_setting_list ::= ( "{" doctype_setting* "}" | doctype_setting )
 * doctype              ::= "doctype-name" "=" doctype_setting_list
 * doctype_list         ::= ( "{" doctype* "}" | doctype )
 * doctype_section      ::= "doctypes" "=" doctype_list
 * 
 * document             ::= ( setting_section? doctype_section? |
 *                            doctype_section? setting_section? )
 * </code>
 */


/*< standard >*/
GQuark
ggd_file_type_load_error_quark (void)
{
  static GQuark q = 0;
  
  if (G_UNLIKELY (q == 0)) {
    q = g_quark_from_static_string ("ggd-file-type-loader-error");
  }
  
  return q;
}

/* reads a boolean (True or False) */
static gboolean
ggd_file_type_read_boolean (GScanner *scanner,
                            gboolean *value_)
{
  gboolean success = FALSE;
  
  if (g_scanner_get_next_token (scanner) != G_TOKEN_IDENTIFIER) {
    g_scanner_unexp_token (scanner, G_TOKEN_IDENTIFIER,
                           _("boolean value"), NULL, NULL, NULL, TRUE);
  } else {
    const gchar  *bool_str  = scanner->value.v_identifier;
    gboolean      value     = FALSE;
    
    success = TRUE;
    if (strcmp (bool_str, "TRUE") == 0 ||
        strcmp (bool_str, "True") == 0) {
      value = TRUE;
    } else if (strcmp (bool_str, "FALSE") == 0 ||
               strcmp (bool_str, "False") == 0) {
      value = FALSE;
    } else {
      g_scanner_error (scanner, _("invalid boolean value \"%s\""), bool_str);
      success = FALSE;
    }
    if (success && value_) {
      *value_ = value;
    }
  }
  
  return success;
}

/* reads #GgdDocSetting:template */
static gboolean
ggd_file_type_read_setting_template (GScanner       *scanner,
                                     GgdDocSetting  *setting)
{
  gboolean success = FALSE;
  
  if (g_scanner_get_next_token (scanner) != G_TOKEN_STRING) {
    g_scanner_unexp_token (scanner, G_TOKEN_STRING,
                           NULL, NULL, NULL, NULL, TRUE);
  } else {
    GError     *err = NULL;
    CtplToken  *tree;
    
    tree = ctpl_lexer_lex_string (scanner->value.v_string, &err);
    if (! tree) {
      g_scanner_error (scanner, _("invalid template: %s"), err->message);
      g_error_free (err);
    } else {
      ctpl_token_free (setting->template);
      setting->template = tree;
      success = TRUE;
    }
  }
  
  return success;
}

/* reads #GgdDocSetting:position */
static gboolean
ggd_file_type_read_setting_position (GScanner       *scanner,
                                     GgdDocSetting  *setting)
{
  gboolean success = FALSE;
  
  if (g_scanner_get_next_token (scanner) != G_TOKEN_IDENTIFIER) {
    g_scanner_unexp_token (scanner, G_TOKEN_IDENTIFIER,
                           _("position name"), NULL, NULL, NULL, TRUE);
  } else {
    const gchar  *pos_string = scanner->value.v_identifier;
    GgdPosition   pos;
    
    pos = ggd_position_from_string (pos_string);
    if ((gint)pos < 0) {
      g_scanner_error (scanner, _("invalid position \"%s\""), pos_string);
    } else {
      setting->position = pos;
      success = TRUE;
    }
  }
  
  return success;
}

/* reads #GgdDocSetting:policy */
static gboolean
ggd_file_type_read_setting_policy (GScanner       *scanner,
                                   GgdDocSetting  *setting)
{
  gboolean success = FALSE;
  
  if (g_scanner_get_next_token (scanner) != G_TOKEN_IDENTIFIER) {
    g_scanner_unexp_token (scanner, G_TOKEN_IDENTIFIER,
                           _("policy name"), NULL, NULL, NULL, TRUE);
  } else {
    const gchar  *policy_string = scanner->value.v_identifier;
    GgdPolicy     policy;
    
    policy = ggd_policy_from_string (policy_string);
    if ((gint)policy < 0) {
      g_scanner_error (scanner, _("invalid policy \"%s\""), policy_string);
    } else {
      setting->policy = policy;
      success = TRUE;
    }
  }
  
  return success;
}

/* reads #GgdDocSetting:merge_children */
static gboolean
ggd_file_type_read_setting_children (GScanner       *scanner,
                                     GgdDocSetting  *setting)
{
  gboolean success = FALSE;
  
  if (g_scanner_get_next_token (scanner) != G_TOKEN_IDENTIFIER) {
    g_scanner_unexp_token (scanner, G_TOKEN_IDENTIFIER,
                           _("merge policy"), NULL, NULL, NULL, TRUE);
  } else {
    const gchar  *merge_string = scanner->value.v_identifier;
    gboolean      merge;
    
    merge = ggd_merge_policy_from_string (merge_string);
    if ((gint)merge < 0) {
      g_scanner_error (scanner, _("invalid merge policy \"%s\""), merge_string);
    } else {
      setting->merge_children = merge;
      success = TRUE;
    }
  }
  
  return success;
}

/* reads #GgdDocSetting:matches */
static gboolean
ggd_file_type_read_setting_matches (GScanner       *scanner,
                                    GgdDocSetting  *setting)
{
  gboolean  success = FALSE;
  TMTagType match = 0;
  
  do {
    success = FALSE;
    if (g_scanner_get_next_token (scanner) != G_TOKEN_IDENTIFIER) {
      g_scanner_unexp_token (scanner, G_TOKEN_IDENTIFIER,
                             _("type"), NULL, NULL, NULL, TRUE);
    } else {
      TMTagType     type;
      const gchar  *match_string = scanner->value.v_identifier;
      
      type = ggd_match_from_string (match_string);
      if (! type) {
        g_scanner_error (scanner, _("invalid type \"%s\""), match_string);
      } else {
        if (g_scanner_peek_next_token (scanner) == '|') {
          g_scanner_get_next_token (scanner); /* eat it */
        }
        match |= type;
        success = TRUE;
      }
    }
  } while (success && g_scanner_peek_next_token (scanner) == G_TOKEN_IDENTIFIER);
  if (success) {
    setting->matches = match;
  }
  
  return success;
}

/* reads #GgdDocSetting:autodoc_children */
static gboolean
ggd_file_type_read_setting_auto_doc_children (GScanner       *scanner,
                                              GgdDocSetting  *setting)
{
  return ggd_file_type_read_boolean (scanner, &setting->autodoc_children);
}

/* dispatches read of value of doctype subsetting @name */
static gboolean
ggd_file_type_read_setting_value (GScanner      *scanner,
                                  const gchar   *name,
                                  GgdDocSetting *setting)
{
  static const struct
  {
    const gchar  *setting;
    gboolean    (*handler)  (GScanner      *scanner,
                             GgdDocSetting *setting);
  } settings_table[] = {
    { "template",           ggd_file_type_read_setting_template           },
    { "position",           ggd_file_type_read_setting_position           },
    { "policy",             ggd_file_type_read_setting_policy             },
    { "children",           ggd_file_type_read_setting_children           },
    { "matches",            ggd_file_type_read_setting_matches            },
    { "auto_doc_children",  ggd_file_type_read_setting_auto_doc_children  }
  };
  gboolean  success = FALSE;
  gboolean  found   = FALSE;
  guint     i;
  
  for (i = 0; ! found && i < G_N_ELEMENTS (settings_table); i++) {
    if (strcmp (settings_table[i].setting, name) == 0) {
      success = settings_table[i].handler (scanner, setting);
      if (success) {
        if (g_scanner_get_next_token (scanner) != ';') {
          g_scanner_unexp_token (scanner, ';',
                                 NULL, NULL, NULL, NULL, TRUE);
          success = FALSE;
        }
      }
      found = TRUE;
    }
  }
  if (! found) {
    g_scanner_error (scanner, _("invalid setting name \"%s\""), name);
    success = FALSE;
  }
  
  return success;
}

static gboolean
ggd_file_type_read_setting (GScanner      *scanner,
                            GgdDocSetting *setting)
{
  gboolean  success = TRUE;
  gboolean  multiple = FALSE;
  GTokenType ttype;
  
  do {
    ttype = g_scanner_get_next_token (scanner);
    switch (ttype) {
      case G_TOKEN_LEFT_CURLY:
        if (multiple) {
          g_scanner_unexp_token (scanner, G_TOKEN_IDENTIFIER,
                                 _("setting identifier"), NULL, NULL, NULL,
                                 TRUE);
          success = FALSE;
        } else {
          multiple = TRUE;
        }
        break;
      
      case G_TOKEN_RIGHT_CURLY:
        if (! multiple) {
          g_scanner_unexp_token (scanner, G_TOKEN_IDENTIFIER,
                                 _("setting identifier"), NULL, NULL, NULL,
                                 TRUE);
          success = FALSE;
        }
        break;
      
      case G_TOKEN_IDENTIFIER: {
        /* need to backup identifier as we call get_next_toekn() before using
         * it */
        gchar *identifier = g_strdup (scanner->value.v_identifier);
        
        if (g_scanner_get_next_token (scanner) != G_TOKEN_EQUAL_SIGN) {
          g_scanner_unexp_token (scanner, G_TOKEN_EQUAL_SIGN,
                                 NULL, NULL, NULL, NULL, TRUE);
          success = FALSE;
        } else {
          success = ggd_file_type_read_setting_value (scanner, identifier,
                                                      setting);
        }
        g_free (identifier);
        break;
      }
      
      default:
        g_scanner_unexp_token (scanner, G_TOKEN_IDENTIFIER,
                               _("setting identifier"), NULL, NULL, NULL, TRUE);
        success = FALSE;
    }
  } while (success && multiple && ttype != G_TOKEN_RIGHT_CURLY);
  
  return success;
}

static gchar *
ggd_file_type_read_match (GScanner  *scanner,
                          gchar    **setting)
{
  GString  *match_str;
  gchar    *match = NULL;
  gboolean  done = FALSE;
  gboolean  success = TRUE;
  
  match_str = g_string_new (NULL);
  do {
    switch (g_scanner_get_next_token (scanner)) {
      case G_TOKEN_IDENTIFIER: {
        gchar    *identifier;
        gboolean  is_setting_name = FALSE;
        
        identifier = g_strdup (scanner->value.v_identifier);
        /*g_debug ("identifier is %s", identifier);*/
        switch (g_scanner_get_next_token (scanner)) {
          case '.': /* skip it */ break;
          
          case G_TOKEN_EQUAL_SIGN:
            /* just stop here, we're done */
            done = TRUE;
            is_setting_name = g_scanner_peek_next_token (scanner) != G_TOKEN_LEFT_CURLY;
            break;
          
          default:
            g_scanner_unexp_token (scanner, G_TOKEN_EQUAL_SIGN,
                                   NULL, NULL, NULL, NULL, TRUE);
            success = FALSE;
        }
        if (success) {
          if (is_setting_name) {
            *setting = g_strdup (identifier);
          } else {
            if (! ggd_match_from_string (identifier)) {
              g_scanner_warn (scanner,
                              _("Unknown type \"%s\", is it a typo?"),
                              identifier);
            }
            if (match_str->len > 0) {
              g_string_append_c (match_str, '.');
            }
            g_string_append (match_str, identifier);
          }
        }
        g_free (identifier);
        break;
      }
      
      default:
        g_scanner_unexp_token (scanner, G_TOKEN_IDENTIFIER,
                               _("match identifier"), NULL, NULL, NULL, TRUE);
        success = FALSE;
    }
  } while (success && ! done);
  if (success && match_str->str && *match_str->str) {
    match = g_string_free (match_str, FALSE);
  } else {
    if (success) {
      g_scanner_error (scanner, _("match identifier is empty"));
    }
    g_string_free (match_str, TRUE);
    success = FALSE;
  }
  
  return match;
}

static GgdDocType *
ggd_file_type_read_doctype (GScanner *scanner)
{
  gboolean    success         = FALSE;
  GgdDocType *doctype         = NULL;
  gboolean    multiple        = FALSE;
  
  if (g_scanner_get_next_token (scanner) != G_TOKEN_IDENTIFIER) {
    g_scanner_unexp_token (scanner, G_TOKEN_IDENTIFIER,
                           _("documentation type identifier"), NULL, NULL, NULL,
                           TRUE);
  } else {
    doctype = ggd_doc_type_new (scanner->value.v_identifier);
    if (g_scanner_get_next_token (scanner) != G_TOKEN_EQUAL_SIGN) {
      g_scanner_unexp_token (scanner, G_TOKEN_EQUAL_SIGN,
                             NULL, NULL, NULL, NULL, TRUE);
    } else {
      GTokenType ttype;
      
      success = TRUE;
      do {
        ttype = g_scanner_peek_next_token (scanner);
        switch (ttype) {
          case G_TOKEN_LEFT_CURLY:
            g_scanner_get_next_token (scanner); /* eat it */
            if (multiple) {
              g_scanner_unexp_token (scanner, G_TOKEN_IDENTIFIER,
                                     _("match identifier"), NULL, NULL, NULL,
                                     TRUE);
              success = FALSE;
            } else {
              multiple = TRUE;
            }
            break;
          
          case G_TOKEN_RIGHT_CURLY:
            g_scanner_get_next_token (scanner); /* eat it */
            if (! multiple) {
              g_scanner_unexp_token (scanner, G_TOKEN_IDENTIFIER,
                                     _("match identifier"), NULL, NULL, NULL,
                                     TRUE);
              success = FALSE;
            }
            break;
          
          case G_TOKEN_IDENTIFIER: {
            gchar          *match;
            gchar          *setting_name = NULL;
            
            match = ggd_file_type_read_match (scanner, &setting_name);
            if (! match || ! *match) {
              success = FALSE;
            } else {
              GgdDocSetting  *setting;
              
              setting = ggd_doc_setting_new (match);
              if (setting_name) {
                success = ggd_file_type_read_setting_value (scanner,
                                                            setting_name,
                                                            setting);
              } else {
                success = ggd_file_type_read_setting (scanner, setting);
              }
              if (! success) {
                ggd_doc_setting_unref (setting);
              } else {
                doctype->settings = g_list_append (doctype->settings, setting);
              }
            }
            g_free (match);
            g_free (setting_name);
            break;
          }
          
          default:
            g_scanner_get_next_token (scanner); /* eat it */
            g_scanner_unexp_token (scanner, G_TOKEN_IDENTIFIER,
                                   _("match identifier"), NULL, NULL, NULL,
                                   TRUE);
            success = FALSE;
        }
      } while (success && multiple && ttype != G_TOKEN_RIGHT_CURLY);
    }
    if (! success) {
      ggd_doc_type_unref (doctype);
      doctype = NULL;
    }
  }
  
  return doctype;
}

static gboolean
ggd_file_type_read_doctypes (GScanner    *scanner,
                             GgdFileType *filetype)
{
  gboolean    success         = FALSE;
  gboolean    multiple        = FALSE;
  
  if (g_scanner_get_next_token (scanner) != G_TOKEN_EQUAL_SIGN) {
    g_scanner_unexp_token (scanner, G_TOKEN_EQUAL_SIGN,
                           NULL, NULL, NULL, NULL, TRUE);
  } else {
    GTokenType ttype;
    
    success = TRUE;
    do {
      ttype = g_scanner_peek_next_token (scanner);
      switch (ttype) {
        case G_TOKEN_LEFT_CURLY:
          g_scanner_get_next_token (scanner); /* eat it */
          if (multiple) {
            g_scanner_unexp_token (scanner, G_TOKEN_IDENTIFIER,
                                   _("documentation type"), NULL, NULL, NULL,
                                   TRUE);
            success = FALSE;
          } else {
            multiple = TRUE;
          }
          break;
        
        case G_TOKEN_RIGHT_CURLY:
          g_scanner_get_next_token (scanner); /* eat it */
          if (! multiple) {
            g_scanner_unexp_token (scanner, G_TOKEN_IDENTIFIER,
                                   _("documentation type"), NULL, NULL, NULL,
                                   TRUE);
            success = FALSE;
          }
          break;
        
        case G_TOKEN_IDENTIFIER: {
          GgdDocType *doctype;
          
          doctype = ggd_file_type_read_doctype (scanner);
          if (! doctype) {
            success = FALSE;
          } else {
            ggd_file_type_add_doc (filetype, doctype);
            ggd_doc_type_unref (doctype);
          }
          break;
        }
        
        default:
          g_scanner_get_next_token (scanner); /* eat it */
          g_scanner_unexp_token (scanner, G_TOKEN_IDENTIFIER,
                                 _("documentation type"), NULL, NULL, NULL,
                                 TRUE);
          success = FALSE;
      }
    } while (success && multiple && ttype != G_TOKEN_RIGHT_CURLY);
  }
  
  return success;
}

static GRegex *
ggd_file_type_read_setting_regex (GScanner    *scanner,
                                  const gchar *name)
{
  GRegex *regex = NULL;
  
  if (g_scanner_get_next_token (scanner) != G_TOKEN_IDENTIFIER ||
      strcmp (scanner->value.v_identifier, name) != 0) {
    g_scanner_unexp_token (scanner, G_TOKEN_IDENTIFIER,
                           name, NULL, NULL, NULL, TRUE);
  } else {
    if (g_scanner_get_next_token (scanner) != G_TOKEN_EQUAL_SIGN) {
      g_scanner_unexp_token (scanner, G_TOKEN_EQUAL_SIGN,
                             NULL, NULL, NULL, NULL, TRUE);
    } else {
      if (g_scanner_get_next_token (scanner) != G_TOKEN_STRING) {
        g_scanner_unexp_token (scanner, G_TOKEN_EQUAL_SIGN,
                               NULL, NULL, NULL, NULL, TRUE);
      } else {
        const gchar  *pattern = scanner->value.v_string;
        GError       *err = NULL;
        
        /*g_debug ("building RE \"%s\"", pattern);*/
        regex = g_regex_new (pattern, G_REGEX_OPTIMIZE, 0, &err);
        if (! regex) {
          g_scanner_error (scanner, _("invalid regular expression: %s"),
                                    err->message);
          g_error_free (err);
        } else {
          if (g_scanner_get_next_token (scanner) != ';') {
            g_scanner_unexp_token (scanner, ';', NULL, NULL, NULL, NULL, TRUE);
            g_regex_unref (regex);
            regex = NULL;
          }
        }
      }
    }
  }
  
  return regex;
}

static gboolean
ggd_file_type_read_setting_match_function_arguments (GScanner     *scanner,
                                                     const gchar  *setting_name,
                                                     GgdFileType  *filetype)
{
  gboolean  success = FALSE;
  GRegex   *regex;
  
  regex = ggd_file_type_read_setting_regex (scanner, setting_name);
  if (regex) {
    if (filetype->match_function_arguments) {
      g_regex_unref (filetype->match_function_arguments);
    }
    filetype->match_function_arguments = regex;
    success = TRUE;
  }
  
  return success;
}

static gboolean
ggd_file_type_read_setting_user_environ (GScanner     *scanner,
                                         const gchar  *setting_name,
                                         GgdFileType  *filetype)
{
  gboolean success = FALSE;
  
g_scanner_get_next_token (scanner); /* eat setting identifier */
  if (g_scanner_get_next_token (scanner) != G_TOKEN_EQUAL_SIGN) {
    g_scanner_unexp_token (scanner, G_TOKEN_EQUAL_SIGN,
                           NULL, NULL, NULL, NULL, TRUE);
  } else {
    if (g_scanner_get_next_token (scanner) != G_TOKEN_STRING) {
      g_scanner_unexp_token (scanner, G_TOKEN_STRING, 
                             NULL, NULL, NULL, NULL, TRUE);
    } else {
      GError *err = NULL;
      
      if (! ctpl_environ_add_from_string (filetype->user_env,
                                          scanner->value.v_string, &err)) {
        g_scanner_error (scanner, _("invalid environment description: %s"),
                         err->message);
        g_error_free (err);
      } else {
        if (g_scanner_get_next_token (scanner) != ';') {
          g_scanner_unexp_token (scanner, ';', NULL, NULL, NULL, NULL, TRUE);
        } else {
          success = TRUE;
        }
      }
    }
  }
  
  return success;
}

static gboolean
ggd_file_type_read_settings (GScanner    *scanner,
                             GgdFileType *filetype)
{
  gboolean    success         = FALSE;
  gboolean    multiple        = FALSE;
  
  if (g_scanner_get_next_token (scanner) != G_TOKEN_EQUAL_SIGN) {
    g_scanner_unexp_token (scanner, G_TOKEN_EQUAL_SIGN,
                           NULL, NULL, NULL, NULL, TRUE);
  } else {
    GTokenType ttype;
    
    success = TRUE;
    do {
      ttype = g_scanner_peek_next_token (scanner);
      switch (ttype) {
        case G_TOKEN_LEFT_CURLY:
          g_scanner_get_next_token (scanner); /* eat it */
          if (multiple) {
            g_scanner_unexp_token (scanner, G_TOKEN_IDENTIFIER,
                                   _("setting"), NULL, NULL, NULL,TRUE);
            success = FALSE;
          } else {
            multiple = TRUE;
          }
          break;
        
        case G_TOKEN_RIGHT_CURLY:
          g_scanner_get_next_token (scanner); /* eat it */
          if (! multiple) {
            g_scanner_unexp_token (scanner, G_TOKEN_IDENTIFIER,
                                   _("setting"), NULL, NULL, NULL,TRUE);
            success = FALSE;
          }
          break;
        
        case G_TOKEN_IDENTIFIER: {
          static const struct
          {
            const gchar *setting;
            gboolean    (*handler)  (GScanner    *scanner,
                                     const gchar *setting_name,
                                     GgdFileType *filetype);
          } settings_handlers[] = {
            { "match_function_arguments",
              ggd_file_type_read_setting_match_function_arguments },
            { "global_environment",
              ggd_file_type_read_setting_user_environ }
          };
          const gchar  *setting = scanner->next_value.v_identifier;
          gboolean      found = FALSE;
          guint         i;
          
          for (i = 0; ! found && i < G_N_ELEMENTS (settings_handlers); i++) {
            if (strcmp (settings_handlers[i].setting, setting) == 0) {
              success = settings_handlers[i].handler (scanner, setting, filetype);
              found = TRUE;
            }
          }
          if (! found) {
            g_scanner_error (scanner, _("invalid setting name \"%s\""),
                             setting);
            success = FALSE;
          }
          break;
        }
        
        default:
          g_scanner_get_next_token (scanner); /* eat it */
          g_scanner_unexp_token (scanner, G_TOKEN_IDENTIFIER,
                                 _("setting"), NULL, NULL, NULL, TRUE);
          success = FALSE;
      }
    } while (success && multiple && ttype != G_TOKEN_RIGHT_CURLY);
  }
  
  return success;
}

static gboolean
ggd_file_type_read (GScanner     *scanner,
                    GgdFileType  *filetype)
{
  static const struct
  {
    const gchar  *section;
    gboolean    (*handler)  (GScanner    *scanner,
                             GgdFileType *filetype);
  } section_handlers[] = {
    { "settings", ggd_file_type_read_settings },
    { "doctypes", ggd_file_type_read_doctypes }
  };
  gboolean  read_sections[G_N_ELEMENTS (section_handlers)] = { 0 };
  gboolean  success = TRUE;
  
  while (success && g_scanner_peek_next_token (scanner) != G_TOKEN_EOF) {
    if (g_scanner_get_next_token (scanner) != G_TOKEN_IDENTIFIER) {
      g_scanner_unexp_token (scanner, G_TOKEN_IDENTIFIER,
                             _("section name"), NULL, NULL, NULL, TRUE);
      success = FALSE;
    } else {
      const gchar  *section = scanner->value.v_identifier;
      gboolean      found = FALSE;
      guint         i;
      
      for (i = 0; ! found && i < G_N_ELEMENTS (section_handlers); i++) {
        if (strcmp (section, section_handlers[i].section) == 0) {
          found = TRUE;
          if (read_sections[i]) {
            g_scanner_error (scanner, _("duplicated section \"%s\""), section);
            success = FALSE;
          } else {
            success = section_handlers[i].handler (scanner, filetype);
            read_sections[i] = TRUE;
          }
        }
      }
      if (! found) {
        g_scanner_error (scanner, _("invalid section name \"%s\""), section);
        success = FALSE;
      }
    }
  }
  if (success) {
    gboolean  none_read = TRUE;
    guint     i;
    
    for (i = 0; none_read && i < G_N_ELEMENTS (read_sections); i++) {
      none_read = ! read_sections[i];
    }
    if (none_read) {
      g_scanner_warn (scanner, _("input is empty"));
    }
  }
  
  return success;
}


static void
scanner_msg_handler (GScanner  *scanner,
                     gchar     *message,
                     gboolean   error)
{
  if (! error) {
    g_warning (_("Parser warning: %s:%u:%u: %s"),
               scanner->input_name, scanner->line, scanner->position, message);
  } else {
    g_critical (_("Parser error: %s:%u:%u: %s"),
                scanner->input_name, scanner->line, scanner->position, message);
    g_set_error (scanner->user_data,
                 GGD_FILE_TYPE_LOAD_ERROR, GGD_FILE_TYPE_LOAD_ERROR_PARSE,
                 _("%s:%u:%u: %s"),
                 scanner->input_name, scanner->line, scanner->position, message);
  }
}

#if 0
static GList *
ggd_conf_read (GScanner *scanner)
{
  gboolean  success   = TRUE;
  GList    *doctypes  = NULL;
  
  while (success && g_scanner_peek_next_token (scanner) != G_TOKEN_EOF) {
    GgdDocType *doctype;
    
    doctype = ggd_conf_read_doctype (scanner);
    if (! doctype) {
      success = FALSE;
    } else {
      doctypes = g_list_prepend (doctypes, doctype);
    }
  }
  if (! success) {
    while (doctypes) {
      GList *tmp = doctypes;
      
      doctypes = g_list_next (doctypes);
      ggd_doc_type_unref (tmp->data);
      g_list_free_1 (tmp);
    }
  } else if (! doctypes) {
    g_scanner_warn (scanner, _("input is empty"));
  }
  
  return doctypes;
}

/**
 * ggd_conf_load:
 * @file: input file
 * @error: return location for errors, or %NULL to ignore errors
 * 
 * 
 * Returns: The list of loaded #GgdDocType<!-- -->s. Note that %NULL can be
 *          returned without failure if input is empty (or contains only
 *          comments). Thus, you should use @error to differentiate failure and
 *          empty input.
 */
GList *
ggd_conf_load (const gchar *file,
               GError     **error)
{
  GList  *list = NULL;
  gint    fd;
  
  fd = g_open (file, O_RDONLY, 0);
  if (fd < 0) {
    gint errnum = errno;
    
    g_set_error (error, GGD_FILE_TYPE_LOAD_ERROR, GGD_FILE_TYPE_LOAD_ERROR_READ,
                 "%s", g_strerror (errnum));
  } else {
    GScanner *scanner;
    gchar    *filename;
    
    filename = g_filename_display_name (file);
    scanner = g_scanner_new (NULL);
    scanner->config->scan_float = FALSE; /* disable float scanning since it
                                          * prevent dot to be usable alone */
    scanner->input_name   = filename;
    scanner->user_data    = error;
    scanner->msg_handler  = scanner_msg_handler;
    g_scanner_input_file (scanner, fd);
    list = ggd_conf_read (scanner);
    g_scanner_destroy (scanner);
    g_free (filename);
    close (fd);
  }
  
  return list;
}
#else

/**
 * ggd_file_type_load:
 * @filetype: A #GgdFileType to fill with read settings
 * @file: a file from which load the settings, in the GLib file names encoding
 * @error: Return location for errors, or %NULL to ignore them
 * 
 * Tries to load a file type configuration from a file.
 * 
 * <warning>
 *   Even if the loading failed, the filetype object may have been partially
 *   or completely filled with read settings.
 * </warning>
 * 
 * Returns: %TRUE on success, %FALSE otherwise in which case @error contains the
 *          error.
 */
gboolean
ggd_file_type_load (GgdFileType *filetype,
                    const gchar *file,
                    GError     **error)
{
  gboolean  success = FALSE;
  gint      fd;
  
  fd = g_open (file, O_RDONLY, 0);
  if (fd < 0) {
    gint errnum = errno;
    
    g_set_error (error, GGD_FILE_TYPE_LOAD_ERROR, GGD_FILE_TYPE_LOAD_ERROR_READ,
                 "%s", g_strerror (errnum));
  } else {
    GScanner *scanner;
    gchar    *filename;
    
    filename = g_filename_display_name (file);
    scanner = g_scanner_new (NULL);
    scanner->config->scan_float = FALSE; /* disable float scanning since it
                                          * prevent dot to be usable alone */
    scanner->input_name   = filename;
    scanner->user_data    = error;
    scanner->msg_handler  = scanner_msg_handler;
    g_scanner_input_file (scanner, fd);
    success = ggd_file_type_read (scanner, filetype);
    g_scanner_destroy (scanner);
    g_free (filename);
    close (fd);
  }
  
  return success;
}

#endif
