/*
 *  
 *  Copyright (C) 2013  Colomban Wendling <ban@herbesfolles.org>
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
 * TODO:
 * * add keybinding to toggle fuzzyness
 * * allow to configure whether to update the metadata upon save
 * * add a menu in the UI
 */

#include "config.h"

#include <string.h>
#include <glib.h>
#include <glib/gi18n-lib.h>

#include <geanyplugin.h>
#include <geany.h>
#include <document.h>
#include <SciLexer.h>


GeanyPlugin      *geany_plugin;
GeanyData        *geany_data;
GeanyFunctions   *geany_functions;


PLUGIN_VERSION_CHECK (211) /* FIXME: what's the version really required? */

PLUGIN_SET_TRANSLATABLE_INFO (
  LOCALEDIR, GETTEXT_PACKAGE,
  _("Translation helper"),
  _("Improves support for GetText translation files."),
  "0.1",
  "Colomban Wendling <ban@herbesfolles.org>"
)


enum {
  GPH_KB_GOTO_PREV_UNTRANSLATED,
  GPH_KB_GOTO_NEXT_UNTRANSLATED,
  GPH_KB_GOTO_PREV_FUZZY,
  GPH_KB_GOTO_NEXT_FUZZY,
  GPH_KB_GOTO_PREV_UNTRANSLATED_OR_FUZZY,
  GPH_KB_GOTO_NEXT_UNTRANSLATED_OR_FUZZY,
  GPH_KB_PASTE_UNTRANSLATED,
  GPH_KB_REFLOW,
  GPH_KB_COUNT
};


#define doc_is_po(doc) (DOC_VALID (doc) && \
                        (doc)->file_type && \
                        (doc)->file_type->id == GEANY_FILETYPES_PO)


/*
 * find_style:
 * @sci: a #ScintillaObject
 * @style: a style ID to search for
 * @start: start of the search range
 * @end: end of the search range
 * 
 * Search for a style in a #ScintillaObject.  Backward search is possible if
 * start is > end.  Note that this find the first occurrence of @style in the
 * search direction, which means that the start of the style will be found when
 * searching onwards but the end when searching backwards.  Also, if the start
 * position is already on the style to search for this position is returned
 * rather than one bound.
 * 
 * Returns: The first found position with style @style, or -1 if not found in
 *          the given range.
 */
static gint
find_style (ScintillaObject  *sci,
            gint              style,
            gint              start,
            gint              end)
{
  gint pos;
  
  if (start > end) {  /* search backwards */
    for (pos = start; pos >= end; pos--) {
      if (sci_get_style_at (sci, pos) == style)
        break;
    }
    if (pos < end)
      return -1;
  } else {
    for (pos = start; pos < end; pos++) {
      if (sci_get_style_at (sci, pos) == style)
        break;
    }
    if (pos >= end)
      return -1;
  }
  
  return pos;
}

/*
 * find_message:
 * @doc: A #GeanyDocument
 * @start: start of the search range
 * @end: end of the search range
 * 
 * Finds the start position of the next msgstr in the given range.  If start is
 * > end, searches backwards.
 * 
 * Returns: The start position of the next msgstr in the given range, or -1 if
 *          not found.
 */
static gint
find_message (GeanyDocument  *doc,
              gint            start,
              gint            end)
{
  if (doc_is_po (doc)) {
    ScintillaObject *sci = doc->editor->sci;
    gint pos = find_style (sci, SCE_PO_MSGSTR, start, end);
    
    if (pos >= 0) {
      pos = find_style (sci, SCE_PO_MSGSTR_TEXT, pos, sci_get_length (sci));
      if (pos >= 0) {
        return pos + 1;
      } 
    }
  }
  
  return -1;
}

/*
 * find_untranslated:
 * @doc: A #GeanyDocument
 * @start: start of the search range
 * @end: end of the search range
 * 
 * Searches for the next untranslated message in the given range.  If start is
 * > end, searches backwards.
 * 
 * Returns: The start position of the next untranslated message in the given
 *          range, or -1 if not found.
 */
static gint
find_untranslated (GeanyDocument *doc,
                   gint           start,
                   gint           end)
{
  if (doc_is_po (doc)) {
    ScintillaObject *sci = doc->editor->sci;
    gboolean backwards = start > end;
    
    while (start >= 0) {
      gint pos;
      
      if (backwards) {
        start = find_style (sci, SCE_PO_MSGID, start, end);
      }
      
      pos = find_message (doc, start, end);
      if (pos < 0) {
        return -1;
      } else {
        gint i = pos;
        
        for (i = pos; i < sci_get_length (sci); i++) {
          gint style = sci_get_style_at (sci, i);
          
          if (style == SCE_PO_MSGSTR_TEXT) {
            if (sci_get_char_at (sci, i) != '"') {
              /* if any character in the text is not a delimiter, there's a
               * translation */
              i = -1;
              break;
            }
          } else if (style != SCE_PO_DEFAULT) {
            /* if we reached something else than the text and the background,
             * we're done searching */
            break;
          }
        }
        if (i >= 0)
          return pos;
      }
      
      start = pos;
    }
  }
  
  return -1;
}

static gint
find_prev_untranslated (GeanyDocument  *doc)
{
  return find_untranslated (doc, sci_get_current_position (doc->editor->sci),
                            0);
}

static gint
find_next_untranslated (GeanyDocument  *doc)
{
  return find_untranslated (doc, sci_get_current_position (doc->editor->sci),
                            sci_get_length (doc->editor->sci));
}

static gint
find_fuzzy (GeanyDocument  *doc,
            gint            start,
            gint            end)
{
  if (doc_is_po (doc)) {
    ScintillaObject *sci = doc->editor->sci;
    
    if (start > end) {
      /* if searching backwards, first go to the previous msgstr not to find
       * the current one */
      gint style = sci_get_style_at (sci, start);
      
      if (style == SCE_PO_MSGSTR || style == SCE_PO_MSGSTR_TEXT) {
        start = find_style (sci, SCE_PO_MSGID, start, end);
        if (start >= 0) {
          start = find_style (sci, SCE_PO_MSGSTR, start, end);
        }
      }
    }
    
    if (start >= 0) {
      struct Sci_TextToFind ttf;
      
      ttf.chrg.cpMin = start;
      ttf.chrg.cpMax = end;
      ttf.lpstrText = (gchar *)"fuzzy";
      
      while (sci_find_text (sci, SCFIND_WHOLEWORD | SCFIND_MATCHCASE,
                            &ttf) >= 0) {
        gint style = sci_get_style_at (sci, (gint) ttf.chrgText.cpMin);
        
        if (style == SCE_PO_FUZZY || style == SCE_PO_FLAGS) {
          /* OK, now find the start of the translation */
          return find_message (doc, (gint) ttf.chrgText.cpMax,
                               start > end ? sci_get_length (sci) : end);
        }
        
        ttf.chrg.cpMin = start > end ? ttf.chrgText.cpMin : ttf.chrgText.cpMax;
      }
    }
  }
  
  return -1;
}

static gint
find_prev_fuzzy (GeanyDocument *doc)
{
  return find_fuzzy (doc, sci_get_current_position (doc->editor->sci), 0);
}

static gint
find_next_fuzzy (GeanyDocument *doc)
{
  return find_fuzzy (doc, sci_get_current_position (doc->editor->sci),
                     sci_get_length (doc->editor->sci));
}

/* goto */

static void
goto_prev_untranslated (GeanyDocument *doc)
{
  if (doc_is_po (doc)) {
    gint pos = find_prev_untranslated (doc);
    
    if (pos >= 0) {
      editor_goto_pos (doc->editor, pos, FALSE);
    }
  }
}

static void
goto_next_untranslated (GeanyDocument *doc)
{
  if (doc_is_po (doc)) {
    gint pos = find_next_untranslated (doc);
    
    if (pos >= 0) {
      editor_goto_pos (doc->editor, pos, FALSE);
    }
  }
}

static void
goto_prev_fuzzy (GeanyDocument *doc)
{
  if (doc_is_po (doc)) {
    gint pos = find_prev_fuzzy (doc);
    
    if (pos >= 0) {
      editor_goto_pos (doc->editor, pos, FALSE);
    }
  }
}

static void
goto_next_fuzzy (GeanyDocument *doc)
{
  if (doc_is_po (doc)) {
    gint pos = find_next_fuzzy (doc);
    
    if (pos >= 0) {
      editor_goto_pos (doc->editor, pos, FALSE);
    }
  }
}

static void
goto_prev_untranslated_or_fuzzy (GeanyDocument *doc)
{
  if (doc_is_po (doc)) {
    gint pos1 = find_prev_untranslated (doc);
    gint pos2 = find_prev_fuzzy (doc);
    
    if (pos1 >= 0 || pos2 >= 0) {
      editor_goto_pos (doc->editor, MAX (pos1, pos2), FALSE);
    }
  }
}

static void
goto_next_untranslated_or_fuzzy (GeanyDocument *doc)
{
  if (doc_is_po (doc)) {
    gint pos1 = find_next_untranslated (doc);
    gint pos2 = find_next_fuzzy (doc);
    
    if (pos1 >= 0 || pos2 >= 0) {
      editor_goto_pos (doc->editor, MIN (pos1, pos2), FALSE);
    }
  }
}

/* basic regex search/replace without captures or back references */
static gboolean
regex_replace (ScintillaObject *sci,
               const gchar     *scire,
               const gchar     *repl)
{
  struct Sci_TextToFind ttf;
  
  ttf.chrg.cpMin = 0;
  ttf.chrg.cpMax = sci_get_length (sci);
  ttf.lpstrText = (gchar *) scire;
  
  if (sci_find_text (sci, SCFIND_REGEXP, &ttf)) {
    sci_set_target_start (sci, (gint) ttf.chrgText.cpMin);
    sci_set_target_end (sci, (gint) ttf.chrgText.cpMax);
    sci_replace_target (sci, repl, FALSE);
    
    return TRUE;
  }
  
  return FALSE;
}

static void
on_document_save (GObject        *obj,
                  GeanyDocument  *doc,
                  gpointer        user_data)
{
  if (doc_is_po (doc)) {
    gboolean update_header = TRUE; /* FIXME: make this configurable */
    
    if (update_header) {
      gchar *date;
      gchar *translator;
      
      date = utils_get_date_time ("\"PO-Revision-Date: %Y-%m-%d %H:%M%z\\n\"",
                                  NULL);
      translator = g_strdup_printf ("\"Last-Translator: %s <%s>\\n\"",
                                    geany_data->template_prefs->developer,
                                    geany_data->template_prefs->mail);
      
      sci_start_undo_action (doc->editor->sci);
      regex_replace (doc->editor->sci, "^\"PO-Revision-Date: .*\"$", date);
      regex_replace (doc->editor->sci, "^\"Last-Translator: .*\"$", translator);
      sci_end_undo_action (doc->editor->sci);
      
      g_free (date);
      g_free (translator);
    }
  }
}

static void
on_kb_goto_prev_untranslated (guint key_id)
{
  goto_prev_untranslated (document_get_current ());
}

static void
on_kb_goto_next_untranslated (guint key_id)
{
  goto_next_untranslated (document_get_current ());
}

static void
on_kb_goto_prev_fuzzy (guint key_id)
{
  goto_prev_fuzzy (document_get_current ());
}

static void
on_kb_goto_next_fuzzy (guint key_id)
{
  goto_next_fuzzy (document_get_current ());
}

static void
on_kb_goto_prev_untranslated_or_fuzzy (guint key_id)
{
  goto_prev_untranslated_or_fuzzy (document_get_current ());
}

static void
on_kb_goto_next_untranslated_or_fuzzy (guint key_id)
{
  goto_next_untranslated_or_fuzzy (document_get_current ());
}

/*
 * on_kb_paste_untranslated:
 * @key_id: unused
 * 
 * Replaces the msgstr at the current position with it corresponding msgid.
 */
static void
on_kb_paste_untranslated (guint key_id)
{
  GeanyDocument *doc = document_get_current ();
  
  if (doc_is_po (doc)) {
    ScintillaObject *sci = doc->editor->sci;
    gint pos = sci_get_current_position (sci);
    gint style = sci_get_style_at (sci, pos);
    
    while (pos > 0 && style == SCE_PO_DEFAULT) {
      style = sci_get_style_at (sci, --pos);
    }
    
    if (style == SCE_PO_MSGID_TEXT ||
        style == SCE_PO_MSGSTR ||
        style == SCE_PO_MSGSTR_TEXT) {
      pos = find_style (sci, SCE_PO_MSGID, pos, 0);
      if (pos >= 0)
        style = SCE_PO_MSGID;
    }
    
    if (style == SCE_PO_MSGID) {
      gint start = find_style (sci, SCE_PO_MSGID_TEXT,
                               pos, sci_get_length (sci));
      
      if (start >= 0) {
        gchar *msgid;
        gint end = start;
        
        /* find msgid range and copy it */
        for (pos = start + 1; pos < sci_get_length (sci); pos++) {
          style = sci_get_style_at (sci, pos);
          if (style == SCE_PO_MSGID_TEXT)
            end = pos;
          else if (style != SCE_PO_DEFAULT)
            break;
        }
        
        if (end - start <= 2 /* 2 is because we include the quotes */) {
          /* don't allow replacing the header (empty) msgid */
        } else {
          msgid = sci_get_contents_range (sci, start, end);
          
          start = find_style (sci, SCE_PO_MSGSTR_TEXT, end,
                              sci_get_length (sci));
          if (start >= 0) {
            /* find msgstr range and replace it */
            end = start;
            sci_set_target_start (sci, start);
            for (pos = start; pos < sci_get_length (sci); pos++) {
              style = sci_get_style_at (sci, pos);
              if (style == SCE_PO_MSGSTR_TEXT)
                end = pos;
              else if (style != SCE_PO_DEFAULT)
                break;
            }
            sci_set_target_end (sci, end);
            sci_replace_target (sci, msgid, FALSE);
            scintilla_send_message (sci, SCI_GOTOPOS, (uptr_t) start + 1, 0);
          }
          g_free (msgid);
        }
      }
    }
  }
}

/* finds the start of the msgstr text at @pos.  the returned position is the
 * start of the msgstr text style, so it's on the first opening quote.  Returns
 * -1 if none found */
static gint
find_msgstr_start_at (GeanyDocument  *doc,
                      gint            pos)
{
  if (doc_is_po (doc)) {
    ScintillaObject *sci = doc->editor->sci;
    gint style = sci_get_style_at (sci, pos);
    
    /* find the previous non-default style */
    while (pos > 0 && style == SCE_PO_DEFAULT) {
      style = sci_get_style_at (sci, --pos);
    }
    
    /* if a msgid or msgstr, go to the msgstr keyword */
    if (style == SCE_PO_MSGID ||
        style == SCE_PO_MSGID_TEXT ||
        style == SCE_PO_MSGSTR_TEXT) {
      pos = find_style (sci, SCE_PO_MSGSTR, pos,
                        style == SCE_PO_MSGSTR_TEXT ? 0 : sci_get_length (sci));
      if (pos >= 0)
        style = SCE_PO_MSGSTR;
    }
    
    if (style == SCE_PO_MSGSTR) {
      return find_style (sci, SCE_PO_MSGSTR_TEXT, pos, sci_get_length (sci));
    }
  }
  
  return -1;
}

/* like find_msgstr_start_at() but finds the end rather than the start */
static gint
find_msgstr_end_at (GeanyDocument  *doc,
                    gint            pos)
{
  pos = find_msgstr_start_at (doc, pos);
  if (pos >= 0) {
    ScintillaObject *sci = doc->editor->sci;
    gint end = pos;
    
    for (; pos < sci_get_length (sci); pos++) {
      gint style = sci_get_style_at (sci, pos);
      
      if (style == SCE_PO_MSGSTR_TEXT)
        end = pos;
      else if (style != SCE_PO_DEFAULT)
        break;
    }
    
    return end;
  }
  
  return -1;
}

static GString *
get_msgstr_text_at (GeanyDocument  *doc,
                    gint            pos)
{
  pos = find_msgstr_start_at (doc, pos);
  
  if (pos >= 0) {
    ScintillaObject *sci = doc->editor->sci;
    GString *msgstr = g_string_new (NULL);
    
    while (sci_get_style_at (sci, pos) == SCE_PO_MSGSTR_TEXT) {
      pos++; /* skip opening quote */
      while (sci_get_style_at (sci, pos + 1) == SCE_PO_MSGSTR_TEXT) {
        g_string_append_c (msgstr, sci_get_char_at (sci, pos));
        pos++;
      }
      pos++; /* skip closing quote */
      
      /* skip until next non-default style */
      while (sci_get_style_at (sci, pos) == SCE_PO_DEFAULT) {
        pos++;
      }
    }
    
    return msgstr;
  }
  
  return NULL;
}

/* cuts @str in human-readable chunks for max @len.
 * cuts first at \n, then at spaces and punctuation */
/* FIXME: support utf-8 */
static gchar **
split_msg (const gchar *str,
           gint         len)
{
  GPtrArray *chunks = g_ptr_array_new ();
  
  g_return_val_if_fail (len >= 0, NULL);
  
  while (*str) {
    GString *chunk = g_string_sized_new ((gsize) len);
    
    while (*str) {
      const gchar *nl = strstr (str, "\\n");
      const gchar *p = strpbrk (str, " \t\v\r\n?!,.;:");
      
      if (nl)
        nl += 2;
      
      if (p)
        p++;
      else /* if there is no separator, use the end of the string */
        p = strchr (str, 0);
      
      if (nl && (chunk->len + nl - str <= len ||
                 (nl < p && chunk->len == 0))) {
        g_string_append_len (chunk, str, nl - str);
        str = nl;
        break;
      } else if (chunk->len + p - str <= len ||
                 chunk->len == 0) {
        g_string_append_len (chunk, str, p - str);
        str = p;
      } else {
        /* give up and leave to next chunk */
        break;
      }
    }
    g_ptr_array_add (chunks, g_string_free (chunk, FALSE));
  }
  
  g_ptr_array_add (chunks, NULL);
  
  return (gchar **) g_ptr_array_free (chunks, FALSE);
}

static void
on_kb_reflow (guint key_id)
{
  GeanyDocument *doc = document_get_current ();
  
  if (doc_is_po (doc)) {
    ScintillaObject *sci = doc->editor->sci;
    gint pos = sci_get_current_position (sci);
    GString *msgstr = get_msgstr_text_at (doc, pos);
  
    if (msgstr) {
      gint start = find_msgstr_start_at (doc, pos);
      gint end = find_msgstr_end_at (doc, pos);
      glong len = g_utf8_strlen (msgstr->str, msgstr->len);
      /* FIXME: line_break_column isn't supposedly public */
      gint line_len = geany_data->editor_prefs->line_break_column;
      gint msgstr_kw_len;
      
      sci_start_undo_action (sci);
      scintilla_send_message (sci, SCI_DELETERANGE,
                              (uptr_t) start, end + 1 - start);
      
      msgstr_kw_len = start - sci_get_position_from_line (sci, sci_get_line_from_position (sci, start));
      if (msgstr_kw_len + len + 2 <= line_len) {
        /* if all can go in the msgstr line, put it here */
        gchar *text = g_strconcat ("\"", msgstr->str, "\"", NULL);
        sci_insert_text (sci, start, text);
        g_free (text);
      } else {
        /* otherwise, put nothing on the msgstr line and split it up through
         * next ones */
        gchar **chunks = split_msg (msgstr->str, line_len - 2);
        guint i;
        
        sci_insert_text (sci, start, "\"\""); /* nothing on the msgstr line */
        start += 2;
        for (i = 0; chunks[i]; i++) {
          SETPTR (chunks[i], g_strconcat ("\n\"", chunks[i], "\"", NULL));
          sci_insert_text (sci, start, chunks[i]);
          start += (gint) strlen (chunks[i]);
        }
        
        g_strfreev (chunks);
      }
      
      scintilla_send_message (sci, SCI_GOTOPOS, (uptr_t) (start + 1), 0);
      sci_end_undo_action (sci);
      
      g_string_free (msgstr, TRUE);
    }
  }
}

void
plugin_init (GeanyData *data)
{
  GeanyKeyGroup *group;
  
  plugin_signal_connect (geany_plugin, NULL, "document-before-save", TRUE,
                         G_CALLBACK (on_document_save), NULL);
  
  /* add keybindings */
  group = plugin_set_key_group (geany_plugin, "pohelper", GPH_KB_COUNT, NULL);
  
  keybindings_set_item (group, GPH_KB_GOTO_PREV_UNTRANSLATED,
                        on_kb_goto_prev_untranslated, 0, 0,
                        "goto-prev-untranslated",
                        _("Go to previous untranslated string"),
                        NULL);
  keybindings_set_item (group, GPH_KB_GOTO_NEXT_UNTRANSLATED,
                        on_kb_goto_next_untranslated, 0, 0,
                        "goto-next-untranslated",
                        _("Go to next untranslated string"),
                        NULL);
  keybindings_set_item (group, GPH_KB_GOTO_PREV_FUZZY,
                        on_kb_goto_prev_fuzzy, 0, 0,
                        "goto-prev-fuzzy",
                        _("Go to previous fuzzily translated string"),
                        NULL);
  keybindings_set_item (group, GPH_KB_GOTO_NEXT_FUZZY,
                        on_kb_goto_next_fuzzy, 0, 0,
                        "goto-next-fuzzy",
                        _("Go to next fuzzily translated string"),
                        NULL);
  keybindings_set_item (group, GPH_KB_GOTO_PREV_UNTRANSLATED_OR_FUZZY,
                        on_kb_goto_prev_untranslated_or_fuzzy, 0, 0,
                        "goto-prev-untranslated-or-fuzzy",
                        _("Go to previous untranslated or fuzzy string"),
                        NULL);
  keybindings_set_item (group, GPH_KB_GOTO_NEXT_UNTRANSLATED_OR_FUZZY,
                        on_kb_goto_next_untranslated_or_fuzzy, 0, 0,
                        "goto-next-untranslated-or-fuzzy",
                        _("Go to next untranslated or fuzzy string"),
                        NULL);
  keybindings_set_item (group, GPH_KB_PASTE_UNTRANSLATED,
                        on_kb_paste_untranslated, 0, 0,
                        "paste-untranslated",
                        _("Paste original untranslated string to translation"),
                        NULL);
  keybindings_set_item (group, GPH_KB_REFLOW,
                        on_kb_reflow, 0, 0,
                        "reflow",
                        _("Reflow the translated string"),
                        NULL);
}

void
plugin_cleanup (void)
{
}
