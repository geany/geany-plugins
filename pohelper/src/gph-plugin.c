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


PLUGIN_VERSION_CHECK (224)

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
  GPH_KB_SHOW_STATS,
  GPH_KB_COUNT
};


static struct Plugin {
  gboolean update_headers;
  /* stats dialog colors */
  GdkColor color_translated;
  GdkColor color_fuzzy;
  GdkColor color_untranslated;
  
  GeanyKeyGroup *key_group;
  GtkWidget *menu_item;
} plugin = {
  TRUE,
  { 0, 0x7373, 0xd2d2, 0x1616 }, /* tango mid green */
  { 0, 0xeded, 0xd4d4, 0x0000 }, /* tango mid yellow */
  { 0, 0xcccc, 0x0000, 0x0000 }, /* tango mid red */
  NULL,
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

/* like find_style(), but searches for the first style change from @start to
 * @end.  Returns the first position in the search direction with a style
 * different from the one at @start, or -1 */
static gint
find_style_boundary (ScintillaObject *sci,
                     gint             start,
                     gint             end)
{
  gint style = sci_get_style_at (sci, start);
  gint pos;
  
  if (start > end) {  /* search backwards */
    for (pos = start; pos >= end; pos--) {
      if (sci_get_style_at (sci, pos) != style)
        break;
    }
    if (pos < end)
      return -1;
  } else {
    for (pos = start; pos < end; pos++) {
      if (sci_get_style_at (sci, pos) != style)
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
    
    /* if searching backwards and already in a msgstr style, search previous
     * again not to go to current's start */
    if (pos >= 0 && start > end) {
      gint style = sci_get_style_at (sci, start);
      
      /* don't take default style into account, so find previous non-default */
      if (style == SCE_PO_DEFAULT) {
        gint style_pos = find_style_boundary (sci, start, end);
        if (style_pos >= 0) {
          style = sci_get_style_at (sci, style_pos);
        }
      }
      
      if (style == SCE_PO_MSGSTR ||
          style == SCE_PO_MSGSTR_TEXT ||
          style == SCE_PO_MSGSTR_TEXT_EOL) {
        pos = find_style_boundary (sci, pos, end);
        if (pos >= 0) {
          pos = find_style (sci, SCE_PO_MSGSTR, pos, end);
        }
      }
    }
    
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
    
    while (start >= 0) {
      gint pos;
      
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
    gint pos = find_message (doc, sci_get_current_position (doc->editor->sci),
                             0);
    
    if (pos >= 0) {
      editor_goto_pos (doc->editor, pos, FALSE);
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
update_menu_items_sensitivity (GeanyDocument *doc)
{
  gboolean sensitive = doc_is_po (doc);
  guint i;
  
  /* since all the document-sensitive items have keybindings and all
   * keybinginds that have a widget are document-sensitive, just walk
   * the keybindings list to fetch the widgets */
  for (i = 0; i < GPH_KB_COUNT; i++) {
    GeanyKeyBinding *kb = keybindings_get_item (plugin.key_group, i);
    
    if (kb->menu_item) {
      gtk_widget_set_sensitive (kb->menu_item, sensitive);
    }
  }
}

static void
on_document_activate (GObject        *obj,
                      GeanyDocument  *doc,
                      gpointer        user_data)
{
  update_menu_items_sensitivity (doc);
}

static void
on_document_filetype_set (GObject        *obj,
                          GeanyDocument  *doc,
                          GeanyFiletype  *old_ft,
                          gpointer        user_data)
{
  update_menu_items_sensitivity (doc);
}

static void
on_document_close (GObject       *obj,
                   GeanyDocument *doc,
                   gpointer       user_data)
{
  GtkNotebook *nb = GTK_NOTEBOOK (geany_data->main_widgets->notebook);
  
  /* the :document-close signal is emitted before a document gets closed,
   * so there always still is the current document open (hence the < 2) */
  if (gtk_notebook_get_n_pages (nb) < 2) {
    update_menu_items_sensitivity (NULL);
  }
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
    gint length = sci_get_length (sci);
    
    while (sci_get_style_at (sci, pos) == SCE_PO_MSGSTR_TEXT) {
      pos++; /* skip opening quote */
      while (sci_get_style_at (sci, pos + 1) == SCE_PO_MSGSTR_TEXT) {
        g_string_append_c (msgstr, sci_get_char_at (sci, pos));
        pos++;
      }
      pos++; /* skip closing quote */
      
      /* skip until next non-default style */
      while (pos < length && sci_get_style_at (sci, pos) == SCE_PO_DEFAULT) {
        pos++;
      }
    }
    
    return msgstr;
  }
  
  return NULL;
}

/* finds the start of the msgid text at @pos.  the returned position is the
 * start of the msgid text style, so it's on the first opening quote.  Returns
 * -1 if none found */
static gint
find_msgid_start_at (GeanyDocument  *doc,
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
    if (style == SCE_PO_MSGID_TEXT ||
        style == SCE_PO_MSGSTR ||
        style == SCE_PO_MSGSTR_TEXT) {
      pos = find_style (sci, SCE_PO_MSGID, pos, 0);
      if (pos >= 0)
        style = SCE_PO_MSGID;
    }
    
    if (style == SCE_PO_MSGID) {
      return find_style (sci, SCE_PO_MSGID_TEXT, pos, sci_get_length (sci));
    }
  }
  
  return -1;
}

static GString *
get_msgid_text_at (GeanyDocument *doc,
                   gint           pos)
{
  pos = find_msgid_start_at (doc, pos);
  
  if (pos >= 0) {
    ScintillaObject *sci = doc->editor->sci;
    GString *msgid = g_string_new (NULL);
    gint length = sci_get_length (sci);
    
    while (sci_get_style_at (sci, pos) == SCE_PO_MSGID_TEXT) {
      pos++; /* skip opening quote */
      while (sci_get_style_at (sci, pos + 1) == SCE_PO_MSGID_TEXT) {
        g_string_append_c (msgid, sci_get_char_at (sci, pos));
        pos++;
      }
      pos++; /* skip closing quote */
      
      /* skip until next non-default style */
      while (pos < length && sci_get_style_at (sci, pos) == SCE_PO_DEFAULT) {
        pos++;
      }
    }
    
    return msgid;
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

static gint
find_msgid_line_at (GeanyDocument  *doc,
                    gint            pos)
{
  ScintillaObject *sci = doc->editor->sci;
  gint line = sci_get_line_from_position (sci, pos);
  gint style = find_first_non_default_style_on_line (sci, line);
  
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
  
  return (style == SCE_PO_MSGID) ? line : -1;
}

static gint
find_flags_line_at (GeanyDocument  *doc,
                    gint            pos)
{
  gint line = find_msgid_line_at (doc, pos);
  
  if (line > 0) {
    gint style;
    
    do {
      line--;
      style = find_first_non_default_style_on_line (doc->editor->sci, line);
    } while (line > 0 &&
             (style == SCE_PO_COMMENT ||
              style == SCE_PO_PROGRAMMER_COMMENT ||
              style == SCE_PO_REFERENCE));
    
    if (style != SCE_PO_FLAGS && style != SCE_PO_FUZZY) {
      line = -1;
    }
  }
  
  return line;
}

static GPtrArray *
get_flags_at (GeanyDocument  *doc,
              gint            pos)
{
  GPtrArray *flags = NULL;
  gint line = find_flags_line_at (doc, pos);
  
  if (line >= 0) {
    flags = g_ptr_array_new_with_free_func (g_free);
    parse_flags_line (doc->editor->sci, line, flags);
  }
  
  return flags;
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
    gint msgid_line = find_msgid_line_at (doc, pos);
    gint flags_line = find_flags_line_at (doc, pos);
    
    if (flags_line >= 0 || msgid_line >= 0) {
      GPtrArray *flags = g_ptr_array_new_with_free_func (g_free);
      
      sci_start_undo_action (sci);
      
      if (flags_line >= 0) {
        parse_flags_line (sci, flags_line, flags);
        delete_line (sci, flags_line);
      } else {
        flags_line = msgid_line;
      }
      
      toggle_flag (flags, "fuzzy");
      write_flags (sci, sci_get_position_from_line (sci, flags_line), flags);
      
      sci_end_undo_action (sci);
      
      g_ptr_array_free (flags, TRUE);
    }
  }
}

typedef struct {
  gdouble translated;
  gdouble fuzzy;
  gdouble untranslated;
} StatsGraphData;

/*
 * rounded_rectangle:
 * @cr: a Cairo context
 * @x: X coordinate of the top-left corner of the rectangle
 * @y: Y coordinate of the top-left corner of the rectangle
 * @width: width of the rectangle
 * @height: height of the rectangle
 * @r1: radius of the top-left corner
 * @r2: radius of the top-right corner
 * @r3: radius of the bottom-right corner
 * @r4: radius of the bottom-left corner
 * 
 * Creates a rectangle path with rounded corners.
 * 
 * Warning: The rectangle should be big enough to include the corners,
 *          otherwise the result will be weird.  For example, if all corners
 *          radius are set to 5, the rectangle should be at least 10x10.
 */
static void
rounded_rectangle (cairo_t *cr,
                   gdouble  x,
                   gdouble  y,
                   gdouble  width,
                   gdouble  height,
                   gdouble  r1,
                   gdouble  r2,
                   gdouble  r3,
                   gdouble  r4)
{
  cairo_move_to (cr, x + r1, y);
  cairo_arc (cr, x + width - r2, y + r2, r2, -G_PI/2.0, 0);
  cairo_arc (cr, x + width - r3, y + height - r3, r3, 0, G_PI/2.0);
  cairo_arc (cr, x + r4, y + height - r4, r4, G_PI/2.0, -G_PI);
  cairo_arc (cr, x + r1, y + r1, r1, -G_PI, -G_PI/2.0);
  cairo_close_path (cr);
}

#if ! GTK_CHECK_VERSION (3, 0, 0) && ! defined (gtk_widget_get_allocated_width)
# define gtk_widget_get_allocated_width(w) (GTK_WIDGET (w)->allocation.width)
#endif
#if ! GTK_CHECK_VERSION (3, 0, 0) && ! defined (gtk_widget_get_allocated_height)
# define gtk_widget_get_allocated_height(w) (GTK_WIDGET (w)->allocation.height)
#endif

static gboolean
stats_graph_draw (GtkWidget  *widget,
                  cairo_t    *cr,
                  gpointer    user_data)
{
  const StatsGraphData *data         = user_data;
  const gint            width        = gtk_widget_get_allocated_width (widget);
  const gint            height       = gtk_widget_get_allocated_height (widget);
  const gdouble         translated   = width * data->translated;
  const gdouble         fuzzy        = width * data->fuzzy;
  const gdouble         untranslated = width * data->untranslated;
  const gdouble         r            = MIN (width / 4, height / 4);
  cairo_pattern_t      *pat;
  
  rounded_rectangle (cr, 0, 0, width, height, r, r, r, r);
  cairo_clip (cr);
  
  gdk_cairo_set_source_color (cr, &plugin.color_translated);
  cairo_rectangle (cr, 0, 0, translated, height);
  cairo_fill (cr);
  
  gdk_cairo_set_source_color (cr, &plugin.color_fuzzy);
  cairo_rectangle (cr, translated, 0, fuzzy, height);
  cairo_fill (cr);
  
  gdk_cairo_set_source_color (cr, &plugin.color_untranslated);
  cairo_rectangle (cr, translated + fuzzy, 0, untranslated, height);
  cairo_fill (cr);
  
  /* draw a nice thin border */
  cairo_set_line_width (cr, 1.0);
  cairo_set_source_rgba (cr, 0, 0, 0, 0.2);
  rounded_rectangle (cr, 0.5, 0.5, width - 1, height - 1, r, r, r, r);
  cairo_stroke (cr);
  
  /* draw a gradient to give the graph a little depth */
  pat = cairo_pattern_create_linear (0, 0, 0, height);
  cairo_pattern_add_color_stop_rgba (pat, 0,      1, 1, 1, 0.2);
  cairo_pattern_add_color_stop_rgba (pat, height, 0, 0, 0, 0.2);
  cairo_set_source (cr, pat);
  cairo_pattern_destroy (pat);
  cairo_rectangle (cr, 0, 0, width, height);
  cairo_paint (cr);
  
  return TRUE;
}

static gboolean
stats_graph_query_tooltip (GtkWidget   *widget,
                           gint         x,
                           gint         y,
                           gboolean     keyboard_mode,
                           GtkTooltip  *tooltip,
                           gpointer     user_data)
{
  const StatsGraphData *data    = user_data;
  gchar                *markup  = NULL;
  
  if (keyboard_mode) {
    gchar *translated_str   = g_strdup_printf (_("<b>Translated:</b> %.3g%%"),
                                               data->translated * 100);
    gchar *fuzzy_str        = g_strdup_printf (_("<b>Fuzzy:</b> %.3g%%"),
                                               data->fuzzy * 100);
    gchar *untranslated_str = g_strdup_printf (_("<b>Untranslated:</b> %.3g%%"),
                                               data->untranslated * 100);
    
    markup = g_strconcat (translated_str,   "\n",
                          fuzzy_str,        "\n",
                          untranslated_str, NULL);
    g_free (translated_str);
    g_free (fuzzy_str);
    g_free (untranslated_str);
  } else {
    const gint width = gtk_widget_get_allocated_width (widget);
    
    if (x <= width * data->translated) {
      markup = g_strdup_printf (_("<b>Translated:</b> %.3g%%"),
                                data->translated * 100);
    } else if (x <= width * (data->translated + data->fuzzy)) {
      markup = g_strdup_printf (_("<b>Fuzzy:</b> %.3g%%"), data->fuzzy * 100);
    } else {
      markup = g_strdup_printf (_("<b>Untranslated:</b> %.3g%%"),
                                data->untranslated * 100);
    }
  }
  
  gtk_tooltip_set_markup (tooltip, markup);
  g_free (markup);
  
  return TRUE;
}

#if ! GTK_CHECK_VERSION (3, 0, 0)
static gboolean
on_stats_graph_expose_event (GtkWidget *widget,
                             GdkEvent  *event,
                             gpointer   data)
{
  cairo_t  *cr  = gdk_cairo_create (GDK_DRAWABLE (widget->window));
  gboolean  ret = stats_graph_draw (widget, cr, data);
  
  cairo_destroy (cr);
  
  return ret;
}
#endif

static void
on_color_button_color_notify (GtkWidget  *widget,
                              GParamSpec *pspec,
                              gpointer    user_data)
{
  gtk_color_button_get_color (GTK_COLOR_BUTTON (widget), user_data);
}

static gchar *
get_data_dir_path (const gchar *filename)
{
#ifdef G_OS_WIN32
  gchar *prefix = g_win32_get_package_installation_directory_of_module (NULL);
#else
  gchar *prefix = NULL;
#endif
  gchar *path   = g_build_filename (prefix ? prefix : "", PLUGINDATADIR,
                                    filename, NULL);
  
  g_free (prefix);
  
  return path;
}

static void
show_stats_dialog (guint  all,
                   guint  translated,
                   guint  fuzzy,
                   guint  untranslated)
{
  GError     *error = NULL;
  gchar      *ui_filename = get_data_dir_path ("stats.ui");;
  GtkBuilder *builder = gtk_builder_new ();
  
  gtk_builder_set_translation_domain (builder, GETTEXT_PACKAGE);
  if (! gtk_builder_add_from_file (builder, ui_filename, &error)) {
    g_critical (_("Failed to load UI definition, please check your "
                  "installation. The error was: %s"), error->message);
    g_error_free (error);
  } else {
    StatsGraphData  data;
    GObject        *dialog;
    GObject        *drawing_area;
    
    data.translated   = all ? (translated   * 1.0 / all) : 0;
    data.fuzzy        = all ? (fuzzy        * 1.0 / all) : 0;
    data.untranslated = all ? (untranslated * 1.0 / all) : 0;
    
    drawing_area = gtk_builder_get_object (builder, "drawing_area");
#if ! GTK_CHECK_VERSION (3, 0, 0)
    g_signal_connect (drawing_area,
                      "expose-event", G_CALLBACK (on_stats_graph_expose_event),
                      &data);
#else
    g_signal_connect (drawing_area,
                      "draw", G_CALLBACK (stats_graph_draw),
                      &data);
#endif
    g_signal_connect (drawing_area,
                      "query-tooltip", G_CALLBACK (stats_graph_query_tooltip),
                      &data);
    gtk_widget_set_has_tooltip (GTK_WIDGET (drawing_area), TRUE);
    
    #define SET_LABEL_N(id, value)                                             \
      do {                                                                     \
        GObject *obj__ = gtk_builder_get_object (builder, (id));               \
                                                                               \
        if (! obj__) {                                                         \
          g_warning ("Object \"%s\" is missing from the UI definition", (id)); \
        } else {                                                               \
          gchar *text__ = g_strdup_printf (_("%u (%.3g%%)"),                   \
                                           (value),                            \
                                           all ? ((value) * 100.0 / all) : 0); \
                                                                               \
          gtk_label_set_text (GTK_LABEL (obj__), text__);                      \
          g_free (text__);                                                     \
        }                                                                      \
      } while (0)
    
    SET_LABEL_N ("n_translated",    translated);
    SET_LABEL_N ("n_fuzzy",         fuzzy);
    SET_LABEL_N ("n_untranslated",  untranslated);
    
    #undef SET_LABEL_N
    
    #define BIND_COLOR_BTN(id, color)                                          \
      do {                                                                     \
        GObject *obj__ = gtk_builder_get_object (builder, (id));               \
                                                                               \
        if (! obj__) {                                                         \
          g_warning ("Object \"%s\" is missing from the UI definition", (id)); \
        } else {                                                               \
          gtk_color_button_set_color (GTK_COLOR_BUTTON (obj__), (color));      \
          g_signal_connect (obj__, "notify::color",                            \
                            G_CALLBACK (on_color_button_color_notify),         \
                            (color));                                          \
          /* queue a redraw on the drawing area so it uses the new color */    \
          g_signal_connect_swapped (obj__, "notify::color",                    \
                                    G_CALLBACK (gtk_widget_queue_draw),        \
                                    drawing_area);                             \
        }                                                                      \
      } while (0)
    
    BIND_COLOR_BTN ("color_translated",   &plugin.color_translated);
    BIND_COLOR_BTN ("color_fuzzy",        &plugin.color_fuzzy);
    BIND_COLOR_BTN ("color_untranslated", &plugin.color_untranslated);
    
    #undef BIND_COLOR_BTN
    
    dialog = gtk_builder_get_object (builder, "dialog");
    gtk_window_set_transient_for (GTK_WINDOW (dialog),
                                  GTK_WINDOW (geany_data->main_widgets->window));
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (GTK_WIDGET (dialog));
  }
  g_free (ui_filename);
  g_object_unref (builder);
}

static void
on_kb_show_stats (guint key_id)
{
  GeanyDocument *doc = document_get_current ();
  
  if (doc_is_po (doc)) {
    ScintillaObject  *sci           = doc->editor->sci;
    const gint        len           = sci_get_length (sci);
    gint              pos           = 0;
    guint             all           = 0;
    guint             untranslated  = 0;
    guint             fuzzy         = 0;
    
    /* don't use find_message() because we want only match one block, not each
     * msgstr as there might be plural forms */
    while ((pos = find_style (sci, SCE_PO_MSGID, pos, len)) >= 0 &&
           (pos = find_style (sci, SCE_PO_MSGSTR, pos, len)) >= 0) {
      GString *msgid = get_msgid_text_at (doc, pos);
      GString *msgstr = get_msgstr_text_at (doc, pos);
      
      if (msgid->len > 0) {
        all++;
        if (msgstr->len < 1) {
          untranslated++;
        } else {
          GPtrArray *flags = get_flags_at (doc, pos);
          
          if (flags) {
            fuzzy += ! toggle_flag (flags, "fuzzy");
            
            g_ptr_array_free (flags, TRUE);
          }
        }
      }
      g_string_free (msgstr, TRUE);
      g_string_free (msgid, TRUE);
    }
    
    show_stats_dialog (all, all - untranslated - fuzzy, fuzzy, untranslated);
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
    N_("Toggle current translation fuzziness"), "toggle_fuzziness" },
  { GPH_KB_SHOW_STATS, "show-stats",
    on_kb_show_stats,
    N_("Show statistics of the current document"), "show_stats" }
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

/*
 * get_setting_color:
 * @kf: a #GKeyFile from which load the color
 * @group: the key file group
 * @key: the key file key
 * @color: (out): the color to fill with the read value.  If the key is not
 *                found, the color isn't updated
 * 
 * Loads a color from a key file entry.
 * 
 * Returns: %TRUE if the color was loaded, %FALSE otherwise.
 */
static gboolean
get_setting_color (GKeyFile    *kf,
                   const gchar *group,
                   const gchar *key,
                   GdkColor    *color)
{
  gboolean  success = FALSE;
  gchar    *value   = g_key_file_get_value (kf, group, key, NULL);
  
  if (value) {
    success = gdk_color_parse (value, color);
    g_free (value);
  }
  
  return success;
}

static void
set_setting_color (GKeyFile        *kf,
                   const gchar     *group,
                   const gchar     *key,
                   const GdkColor  *color)
{
  gchar *value = gdk_color_to_string (color);
  
  g_key_file_set_value (kf, group, key, value);
  g_free (value);
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
    get_setting_color (kf, "colors", "translated", &plugin.color_translated);
    get_setting_color (kf, "colors", "fuzzy", &plugin.color_fuzzy);
    get_setting_color (kf, "colors", "untranslated", &plugin.color_untranslated);
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
  set_setting_color (kf, "colors", "translated", &plugin.color_translated);
  set_setting_color (kf, "colors", "fuzzy", &plugin.color_fuzzy);
  set_setting_color (kf, "colors", "untranslated", &plugin.color_untranslated);
  write_keyfile (kf, filename);
  
  g_key_file_free (kf);
  g_free (filename);
}

void
plugin_init (GeanyData *data)
{
  GtkBuilder *builder;
  GError *error = NULL;
  gchar *ui_filename;
  guint i;
  
  load_config ();
  
  ui_filename = get_data_dir_path ("menus.ui");
  builder = gtk_builder_new ();
  gtk_builder_set_translation_domain (builder, GETTEXT_PACKAGE);
  if (! gtk_builder_add_from_file (builder, ui_filename, &error)) {
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
  g_free (ui_filename);
  
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
  plugin.key_group = plugin_set_key_group (geany_plugin, "pohelper",
                                           GPH_KB_COUNT, NULL);
  
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
    
    keybindings_set_item (plugin.key_group, G_actions[i].id,
                          G_actions[i].callback, 0, 0, G_actions[i].name,
                          _(G_actions[i].label), widget);
  }
  /* initial items sensitivity update */
  update_menu_items_sensitivity (document_get_current ());
  
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
