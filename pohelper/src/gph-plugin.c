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
  _("Translation Helper"),
  _("Improves support for GetText translation files."),
  "0.1",
  "Colomban Wendling <ban@herbesfolles.org>"
)


enum {
  GPH_KB_GOTO_PREV,
  GPH_KB_GOTO_NEXT,
  GPH_KB_GOTO_PREV_UNTRANSLATED,
  GPH_KB_GOTO_NEXT_UNTRANSLATED,
  GPH_KB_GOTO_PREV_FUZZY,
  GPH_KB_GOTO_NEXT_FUZZY,
  GPH_KB_GOTO_PREV_UNTRANSLATED_OR_FUZZY,
  GPH_KB_GOTO_NEXT_UNTRANSLATED_OR_FUZZY,
  GPH_KB_PASTE_UNTRANSLATED,
  GPH_KB_REFLOW,
  GPH_KB_TOGGLE_FUZZY,
  GPH_KB_COUNT
};


static struct Plugin {
  gboolean update_headers;
  
  GtkWidget *menu_item;
} plugin = {
  TRUE,
  NULL
};


#define doc_is_po(doc) (DOC_VALID (doc) && \
                        (doc)->file_type && \
                        (doc)->file_type->id == GEANY_FILETYPES_PO)


/* gets the smallest valid position between @a and @b */
#define MIN_POS(a, b) ((a) < 0 ? (b) : (b) < 0 ? (a) : MIN ((a), (b)))
/* gets the highest valid position between @a and @b */
#define MAX_POS(a, b) (MAX ((a), (b)))


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
goto_prev (GeanyDocument *doc)
{
  if (doc_is_po (doc)) {
    gint pos = sci_get_current_position (doc->editor->sci);
    
    pos = find_style (doc->editor->sci, SCE_PO_MSGID, pos, 0);
    if (pos >= 0) {
      pos = find_message (doc, pos, 0);
      if (pos >= 0) {
        editor_goto_pos (doc->editor, pos, FALSE);
      }
    }
  }
}

static void
goto_next (GeanyDocument *doc)
{
  if (doc_is_po (doc)) {
    gint pos = find_message (doc, sci_get_current_position (doc->editor->sci),
                             sci_get_length (doc->editor->sci));
    
    if (pos >= 0) {
      editor_goto_pos (doc->editor, pos, FALSE);
    }
  }
}

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
    gint pos = MAX_POS (pos1, pos2);
    
    if (pos >= 0) {
      editor_goto_pos (doc->editor, pos, FALSE);
    }
  }
}

static void
goto_next_untranslated_or_fuzzy (GeanyDocument *doc)
{
  if (doc_is_po (doc)) {
    gint pos1 = find_next_untranslated (doc);
    gint pos2 = find_next_fuzzy (doc);
    gint pos = MIN_POS (pos1, pos2);
    
    if (pos >= 0) {
      editor_goto_pos (doc->editor, pos, FALSE);
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

/* escapes @str so it is valid to put it inside a message
 * escapes '\b', '\f', '\n', '\r', '\t', '\v', '\' and '"'
 * unlike g_strescape(), it doesn't escape non-ASCII characters so keeps
 * all of UTF-8 */
static gchar *
escape_string (const gchar *str)
{
  gchar *new = g_malloc (strlen (str) * 2 + 1);
  gchar *p;
  
  for (p = new; *str; str++) {
    switch (*str) {
      case '\b': *p++ = '\\'; *p++ = 'b'; break;
      case '\f': *p++ = '\\'; *p++ = 'f'; break;
      case '\n': *p++ = '\\'; *p++ = 'n'; break;
      case '\r': *p++ = '\\'; *p++ = 'r'; break;
      case '\t': *p++ = '\\'; *p++ = 't'; break;
      case '\v': *p++ = '\\'; *p++ = 'v'; break;
      case '\\': *p++ = '\\'; *p++ = '\\'; break;
      case '"':  *p++ = '\\'; *p++ = '"'; break;
      default:
        *p++ = *str;
    }
  }
  *p = 0;
  
  return new;
}

static void
on_document_save (GObject        *obj,
                  GeanyDocument  *doc,
                  gpointer        user_data)
{
  if (doc_is_po (doc) && plugin.update_headers) {
    gchar *name = escape_string (geany_data->template_prefs->developer);
    gchar *mail = escape_string (geany_data->template_prefs->mail);
    gchar *date;
    gchar *translator;
    
    date = utils_get_date_time ("\"PO-Revision-Date: %Y-%m-%d %H:%M%z\\n\"",
                                NULL);
    translator = g_strdup_printf ("\"Last-Translator: %s <%s>\\n\"",
                                  name, mail);
    
    sci_start_undo_action (doc->editor->sci);
    regex_replace (doc->editor->sci, "^\"PO-Revision-Date: .*\"$", date);
    regex_replace (doc->editor->sci, "^\"Last-Translator: .*\"$", translator);
    sci_end_undo_action (doc->editor->sci);
    
    g_free (date);
    g_free (translator);
    g_free (name);
    g_free (mail);
  }
}

static void
update_menus (GeanyDocument *doc)
{
  if (plugin.menu_item) {
    gtk_widget_set_sensitive (plugin.menu_item, doc_is_po (doc));
  }
}

static void
on_document_activate (GObject        *obj,
                      GeanyDocument  *doc,
                      gpointer        user_data)
{
  update_menus (doc);
}

static void
on_document_filetype_set (GObject        *obj,
                          GeanyDocument  *doc,
                          GeanyFiletype  *old_ft,
                          gpointer        user_data)
{
  update_menus (doc);
}

static void
on_document_close (GObject       *obj,
                   GeanyDocument *doc,
                   gpointer       user_data)
{
  update_menus (NULL);
}

static void
on_kb_goto_prev (guint key_id)
{
  goto_prev (document_get_current ());
}

static void
on_kb_goto_next (guint key_id)
{
  goto_next (document_get_current ());
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
static gchar **
split_msg (const gchar *str,
           gsize        len)
{
  GPtrArray *chunks = g_ptr_array_new ();
  
  while (*str) {
    GString *chunk = g_string_sized_new (len);
    
    while (*str) {
      const gchar *nl = strstr (str, "\\n");
      const gchar *p = strpbrk (str, " \t\v\r\n?!,.;:");
      glong chunk_len = g_utf8_strlen (chunk->str, (gssize) chunk->len);
      
      if (nl)
        nl += 2;
      
      if (p)
        p++;
      else /* if there is no separator, use the end of the string */
        p = strchr (str, 0);
      
      if (nl && ((gsize)(chunk_len + g_utf8_strlen (str, nl - str)) <= len ||
                 (nl < p && chunk->len == 0))) {
        g_string_append_len (chunk, str, nl - str);
        str = nl;
        break;
      } else if ((gsize)(chunk_len + g_utf8_strlen (str, p - str)) <= len ||
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
      glong len = g_utf8_strlen (msgstr->str, (gssize) msgstr->len);
      /* FIXME: line_break_column isn't supposedly public */
      gint line_len = geany_data->editor_prefs->line_break_column;
      gint msgstr_kw_len;
      
      /* if line break column doesn't have a reasonable value, don't use it */
      if (line_len < 8) {
        line_len = 72;
      }
      
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
        gchar **chunks = split_msg (msgstr->str, (gsize)(line_len - 2));
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

/* returns the first non-default style on the line, or the default style if
 * there is no other on that line */
static gint
find_first_non_default_style_on_line (ScintillaObject  *sci,
                                      gint              line)
{
  gint pos = sci_get_position_from_line (sci, line);
  gint end = sci_get_line_end_position (sci, line);
  gint style;
  
  do {
    style = sci_get_style_at (sci, pos++);
  } while (style == SCE_PO_DEFAULT && pos < end);
  
  return style;
}

/* checks whether @line is a primary msgid line, e.g. not a plural form */
static gboolean
line_is_primary_msgid (ScintillaObject *sci,
                       gint             line)
{
  gint pos = (gint) scintilla_send_message (sci, SCI_GETLINEINDENTPOSITION,
                                            (uptr_t) line, 0);
  
  return (sci_get_char_at (sci, pos++) == 'm' &&
          sci_get_char_at (sci, pos++) == 's' &&
          sci_get_char_at (sci, pos++) == 'g' &&
          sci_get_char_at (sci, pos++) == 'i' &&
          sci_get_char_at (sci, pos++) == 'd' &&
          g_ascii_isspace (sci_get_char_at (sci, pos)));
}

/* parse flags line @line and puts the read flags in @flags
 * a flags line looks like:
 * #, flag-1, flag-2, flag-2, ... */
static void
parse_flags_line (ScintillaObject  *sci,
                  gint              line,
                  GPtrArray        *flags)
{
  gint start = sci_get_position_from_line (sci, line);
  gint end = sci_get_line_end_position (sci, line);
  gint pos;
  gint ws, we;
  gint ch;
  
  pos = start;
  /* skip leading space and markers */
  while (pos <= end && ((ch = sci_get_char_at (sci, pos)) == '#' ||
                        ch == ',' || g_ascii_isspace (ch))) {
    pos++;
  }
  /* and read the flags */
  for (ws = we = pos; pos <= end; pos++) {
    ch = sci_get_char_at (sci, pos);
    
    if (ch == ',' || g_ascii_isspace (ch) || pos >= end) {
      if (ws < we) {
        g_ptr_array_add (flags, sci_get_contents_range (sci, ws, we + 1));
      }
      ws = pos + 1;
    } else {
      we = pos;
    }
  }
}

/* adds or remove @flag from @flags.  returns whether the flag was added */
static gboolean
toggle_flag (GPtrArray   *flags,
             const gchar *flag)
{
  gboolean add = TRUE;
  guint i;
  
  /* search for the flag and remove it */
  for (i = 0; i < flags->len; i++) {
    if (strcmp (g_ptr_array_index (flags, i), flag) == 0) {
      g_ptr_array_remove_index (flags, i);
      add = FALSE;
      break;
    }
  }
  /* if it wasntt there, add it */
  if (add) {
    g_ptr_array_add (flags, g_strdup (flag));
  }
  
  return add;
}

/* writes a flags line at @pos containgin @flags */
static void
write_flags (ScintillaObject *sci,
             gint             pos,
             GPtrArray       *flags)
{
  if (flags->len > 0) {
    guint i;
    
    sci_start_undo_action (sci);
    sci_insert_text (sci, pos, "#");
    pos ++;
    for (i = 0; i < flags->len; i++) {
      const gchar *flag = g_ptr_array_index (flags, i);
      
      sci_insert_text (sci, pos, ", ");
      pos += 2;
      sci_insert_text (sci, pos, flag);
      pos += (gint) strlen (flag);
    }
    sci_insert_text (sci, pos, "\n");
    sci_end_undo_action (sci);
  }
}

static void
delete_line (ScintillaObject *sci,
             gint             line)
{
  gint pos = sci_get_position_from_line (sci, line);
  gint length = sci_get_line_length (sci, line);
  
  scintilla_send_message (sci, SCI_DELETERANGE, (uptr_t) pos, (sptr_t) length);
}

static void
on_kb_toggle_fuzziness (guint key_id)
{
  GeanyDocument *doc = document_get_current ();
  
  if (doc_is_po (doc)) {
    ScintillaObject *sci = doc->editor->sci;
    gint pos = sci_get_current_position (sci);
    gint line = sci_get_line_from_position (sci, pos);
    gint style = find_first_non_default_style_on_line (sci, line);
    
    /* find the msgid for the current line */
    while (line > 0 &&
           (style == SCE_PO_DEFAULT ||
            (style == SCE_PO_MSGID && ! line_is_primary_msgid (sci, line)) ||
            style == SCE_PO_MSGID_TEXT ||
            style == SCE_PO_MSGSTR ||
            style == SCE_PO_MSGSTR_TEXT)) {
      line--;
      style = find_first_non_default_style_on_line (sci, line);
    }
    while (line < sci_get_line_count (sci) &&
           (style == SCE_PO_COMMENT ||
            style == SCE_PO_PROGRAMMER_COMMENT ||
            style == SCE_PO_REFERENCE ||
            style == SCE_PO_FLAGS ||
            style == SCE_PO_FUZZY)) {
      line++;
      style = find_first_non_default_style_on_line (sci, line);
    }
    
    if (style == SCE_PO_MSGID) {
      gint msgid_line = line;
      GPtrArray *flags = g_ptr_array_new ();
      
      sci_start_undo_action (sci);
      
      if (line > 0) {
        /* search for an existing flags line */
        do {
          line--;
          style = find_first_non_default_style_on_line (sci, line);
        } while (line > 0 &&
                 (style == SCE_PO_COMMENT ||
                  style == SCE_PO_PROGRAMMER_COMMENT ||
                  style == SCE_PO_REFERENCE));
        
        if (style == SCE_PO_FLAGS || style == SCE_PO_FUZZY) {
          /* ok we got a line with flags, parse them and remove them */
          parse_flags_line (sci, line, flags);
          delete_line (sci, line);
        } else {
          /* no flags, add the line */
          line = msgid_line;
        }
      }
      
      toggle_flag (flags, "fuzzy");
      write_flags (sci, sci_get_position_from_line (sci, line), flags);
      
      sci_end_undo_action (sci);
      
      g_ptr_array_foreach (flags, (GFunc) g_free, NULL);
      g_ptr_array_free (flags, TRUE);
    }
  }
}

static const struct Action {
  guint             id;
  const gchar      *name;
  GeanyKeyCallback  callback;
  const gchar      *label;
  const gchar      *widget;
} G_actions[] = {
  { GPH_KB_GOTO_PREV, "goto-prev",
    on_kb_goto_prev,
    N_("Go to previous string"), "previous_string" },
  { GPH_KB_GOTO_NEXT, "goto-next",
    on_kb_goto_next,
    N_("Go to next string"), "next_string" },
  { GPH_KB_GOTO_PREV_UNTRANSLATED, "goto-prev-untranslated",
    on_kb_goto_prev_untranslated,
    N_("Go to previous untranslated string"), "previous_untranslated" },
  { GPH_KB_GOTO_NEXT_UNTRANSLATED, "goto-next-untranslated",
    on_kb_goto_next_untranslated,
    N_("Go to next untranslated string"), "next_untranslated" },
  { GPH_KB_GOTO_PREV_FUZZY, "goto-prev-fuzzy",
    on_kb_goto_prev_fuzzy,
    N_("Go to previous fuzzily translated string"), "previous_fuzzy" },
  { GPH_KB_GOTO_NEXT_FUZZY, "goto-next-fuzzy",
    on_kb_goto_next_fuzzy,
    N_("Go to next fuzzily translated string"), "next_fuzzy" },
  { GPH_KB_GOTO_PREV_UNTRANSLATED_OR_FUZZY, "goto-prev-untranslated-or-fuzzy",
    on_kb_goto_prev_untranslated_or_fuzzy,
    N_("Go to previous untranslated or fuzzy string"),
    "previous_untranslated_or_fuzzy" },
  { GPH_KB_GOTO_NEXT_UNTRANSLATED_OR_FUZZY, "goto-next-untranslated-or-fuzzy",
    on_kb_goto_next_untranslated_or_fuzzy,
    N_("Go to next untranslated or fuzzy string"),
    "next_untranslated_or_fuzzy" },
  { GPH_KB_PASTE_UNTRANSLATED, "paste-untranslated",
    on_kb_paste_untranslated,
    N_("Paste original untranslated string to translation"),
    "paste_message_as_translation" },
  { GPH_KB_REFLOW, "reflow",
    on_kb_reflow,
    N_("Reflow the current translation string"), "reflow_translation" },
  { GPH_KB_TOGGLE_FUZZY, "toggle-fuzziness",
    on_kb_toggle_fuzziness,
    N_("Toggle current translation fuzziness"), "toggle_fuzziness" }
};

static void
on_widget_kb_activate (GtkMenuItem   *widget,
                       struct Action *action)
{
  action->callback (action->id);
}

static void
on_update_headers_upon_save_toggled (GtkCheckMenuItem  *item,
                                     gpointer           data)
{
  plugin.update_headers = gtk_check_menu_item_get_active (item);
}

static gchar *
get_config_filename (void)
{
  return g_build_filename (geany_data->app->configdir, "plugins",
                           "pohelper", "pohelper.conf", NULL);
}

/* loads @filename in @kf and return %FALSE if failed, emitting a warning
 * unless the file was simply missing */
static gboolean
load_keyfile (GKeyFile     *kf,
              const gchar  *filename,
              GKeyFileFlags flags)
{
  GError *error = NULL;
  
  if (! g_key_file_load_from_file (kf, filename, flags, &error)) {
    if (error->domain != G_FILE_ERROR || error->code != G_FILE_ERROR_NOENT) {
      g_warning (_("Failed to load configuration file: %s"), error->message);
    }
    g_error_free (error);
    
    return FALSE;
  }
  
  return TRUE;
}

/* writes @kf in @filename, possibly creating directories to be able to write
 * in @filename */
static gboolean
write_keyfile (GKeyFile    *kf,
               const gchar *filename)
{
  gchar *dirname = g_path_get_dirname (filename);
  GError *error = NULL;
  gint err;
  gchar *data;
  gsize length;
  gboolean success = FALSE;
  
  data = g_key_file_to_data (kf, &length, NULL);
  if ((err = utils_mkdir (dirname, TRUE)) != 0) {
    g_critical (_("Failed to create configuration directory \"%s\": %s"),
                dirname, g_strerror (err));
  } else if (! g_file_set_contents (filename, data, (gssize) length, &error)) {
    g_critical (_("Failed to save configuration file: %s"), error->message);
    g_error_free (error);
  } else {
    success = TRUE;
  }
  g_free (data);
  g_free (dirname);
  
  return success;
}

static void
load_config (void)
{
  gchar *filename = get_config_filename ();
  GKeyFile *kf = g_key_file_new ();
  
  if (load_keyfile (kf, filename, G_KEY_FILE_NONE)) {
    plugin.update_headers = utils_get_setting_boolean (kf, "general",
                                                       "update-headers",
                                                       plugin.update_headers);
  }
  g_key_file_free (kf);
  g_free (filename);
}

static void
save_config (void)
{
  gchar *filename = get_config_filename ();
  GKeyFile *kf = g_key_file_new ();
  
  load_keyfile (kf, filename, G_KEY_FILE_KEEP_COMMENTS);
  g_key_file_set_boolean (kf, "general", "update-headers",
                          plugin.update_headers);
  write_keyfile (kf, filename);
  
  g_key_file_free (kf);
  g_free (filename);
}

void
plugin_init (GeanyData *data)
{
  GeanyKeyGroup *group;
  GtkBuilder *builder;
  GError *error = NULL;
  guint i;
  
  load_config ();
  
  builder = gtk_builder_new ();
  gtk_builder_set_translation_domain (builder, GETTEXT_PACKAGE);
  if (! gtk_builder_add_from_file (builder, PKGDATADIR"/pohelper/menus.ui",
                                   &error)) {
    g_critical (_("Failed to load UI definition, please check your "
                  "installation. The error was: %s"), error->message);
    g_error_free (error);
    g_object_unref (builder);
    builder = NULL;
    plugin.menu_item = NULL;
  } else {
    GObject *obj;
    
    plugin.menu_item = GTK_WIDGET (gtk_builder_get_object (builder, "root_item"));
    gtk_menu_shell_append (GTK_MENU_SHELL (geany->main_widgets->tools_menu),
                           plugin.menu_item);
    
    obj = gtk_builder_get_object (builder, "update_headers_upon_save");
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (obj),
                                    plugin.update_headers);
    g_signal_connect (obj, "toggled",
                      G_CALLBACK (on_update_headers_upon_save_toggled), NULL);
  }
  
  /* signal handlers */
  plugin_signal_connect (geany_plugin, NULL, "document-activate", TRUE,
                         G_CALLBACK (on_document_activate), NULL);
  plugin_signal_connect (geany_plugin, NULL, "document-filetype-set", TRUE,
                         G_CALLBACK (on_document_filetype_set), NULL);
  plugin_signal_connect (geany_plugin, NULL, "document-close", TRUE,
                         G_CALLBACK (on_document_close), NULL);
  plugin_signal_connect (geany_plugin, NULL, "document-before-save", TRUE,
                         G_CALLBACK (on_document_save), NULL);
  
  /* add keybindings */
  group = plugin_set_key_group (geany_plugin, "pohelper", GPH_KB_COUNT, NULL);
  
  for (i = 0; i < G_N_ELEMENTS (G_actions); i++) {
    GtkWidget *widget = NULL;
    
    if (builder && G_actions[i].widget) {
      GObject *obj = gtk_builder_get_object (builder, G_actions[i].widget);
      
      if (! obj || ! GTK_IS_MENU_ITEM (obj)) {
        g_critical (_("Cannot find widget \"%s\" in the UI definition, "
                      "please check your installation."), G_actions[i].widget);
      } else {
        widget = GTK_WIDGET (obj);
        g_signal_connect (widget, "activate",
                          G_CALLBACK (on_widget_kb_activate),
                          (gpointer) &G_actions[i]);
      }
    }
    
    keybindings_set_item (group, G_actions[i].id, G_actions[i].callback, 0, 0,
                          G_actions[i].name, _(G_actions[i].label), widget);
  }
  
  if (builder) {
    g_object_unref (builder);
  }
}

void
plugin_cleanup (void)
{
  if (plugin.menu_item) {
    gtk_widget_destroy (plugin.menu_item);
  }
  
  save_config ();
}
