/*  shiftcolumn.c - a Geany plugin
 *
 *  Copyright 2009 Andrew L Janke <a.janke@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
 */


#include "geany.h"
#include "support.h"

#ifdef HAVE_LOCALE_H
# include <locale.h>
#endif

#include "ui_utils.h"

#include "sciwrappers.h"
#include "document.h"
#include "keybindings.h"
#include "plugindata.h"
#include "geanyfunctions.h"

#include <glib.h>
#include <glib/gprintf.h>
#include <gdk/gdkkeysyms.h>

GeanyPlugin     *geany_plugin;
GeanyData       *geany_data;
GeanyFunctions  *geany_functions;

PLUGIN_VERSION_CHECK(130);
PLUGIN_SET_INFO(_("Shift Column"),
                _("Shift a selection left and right"),
                VERSION, "Andrew L Janke <a.janke@gmail.com>");


static GtkWidget *menu_item_shift_left = NULL;
static GtkWidget *menu_item_shift_right = NULL;

/* Keybinding(s) */
enum{
   KB_SHIFT_LEFT,
   KB_SHIFT_RIGHT,
   KB_COUNT
   };
PLUGIN_KEY_GROUP(shiftcolumn, KB_COUNT)


static void shift_left_cb(G_GNUC_UNUSED GtkMenuItem *menuitem,
                          G_GNUC_UNUSED gpointer gdata){
   gchar *txt;
   gchar *txt_i;
   gchar char_before;
   gint txt_len;

   gint startpos;
   gint endpos;

   gint startline;
   gint endline;
   gint line_iter;
   gint linepos;
   gint linelen;

   gint startcol;
   gint endcol;

   gint i;

   gint n_spaces;
   gchar *spaces;

   ScintillaObject *sci;

   /* get a pointer to the scintilla object */
   sci = document_get_current()->editor->sci;

   if (sci_has_selection(sci)){

      startpos = sci_get_selection_start(sci);
      endpos = sci_get_selection_end(sci);

      /* sanity check -- we dont care which way the block was selected */
      if(startpos > endpos){
         i = endpos;
         endpos = startpos;
         startpos = i;
         }

      startline = sci_get_line_from_position(sci, startpos);
      endline = sci_get_line_from_position(sci, endpos);

      /* normal mode */
      // if(sci_get_selection_mode(sci) == 1){
      if(startline == endline){

         /* get the text in question */
         txt_len = endpos - startpos;
         txt_i = g_malloc(txt_len + 1);
         txt = g_malloc(txt_len + 2);
         sci_get_selected_text(sci, txt_i);

         char_before = sci_get_char_at(sci, startpos - 1);

         /* set up new text buf */
         (void) g_sprintf(txt, "%s%c", txt_i, char_before);

         /* start undo */
         sci_start_undo_action(sci);

         /* put the new text in */
         sci_set_selection_start(sci, startpos - 1);
         sci_replace_sel(sci, txt);

         /* select the right bit again */
         sci_set_selection_start(sci, startpos - 1);
         sci_set_selection_end(sci, endpos - 1);

         /* end undo */
         sci_end_undo_action(sci);

         g_free(txt);
         g_free(txt_i);
         }

      /* rectangle mode (we hope!) */
      else{
         startcol = sci_get_col_from_position(sci, startpos);
         endcol = sci_get_col_from_position(sci, endpos);

         /* return early for the trivial case */
         if(startcol == 0 || startcol == endcol){
            return;
            }

         /* start undo */
         sci_start_undo_action(sci);

         for(line_iter = startline; line_iter <= endline; line_iter++){
            linepos = sci_get_position_from_line(sci, line_iter);
            linelen = sci_get_line_length(sci, line_iter);

            /* do we need to do something */
            if(linelen >= startcol - 1 ){

               /* if between the two columns */
               /* pad to the end first */
               if(linelen <= endcol){

                  /* bung in some spaces -- sorry, I dont like tabs */
                  n_spaces = endcol - linelen + 1;
                  spaces = g_malloc(sizeof(gchar) * (n_spaces + 1));
                  for(i = 0; i < n_spaces; i++){
                     spaces[i] = ' ';
                     }
                  spaces[i] = '\0';

                  sci_insert_text(sci, linepos + linelen - 1, spaces);
                  g_free(spaces);
                  }

               /* now move the text itself */
               sci_set_selection_mode(sci, 0);
               sci_set_selection_start(sci, linepos + startcol);
               sci_set_selection_end(sci, linepos + endcol);

               txt_len = sci_get_selected_text_length(sci);
               txt_i = g_malloc(txt_len + 1);
               txt = g_malloc(txt_len + 2);

               sci_get_selected_text(sci, txt_i);
               char_before = sci_get_char_at(sci, linepos + startcol - 1);

               /* set up new text buf */
               (void) g_sprintf(txt, "%s%c", txt_i, char_before);

               /* put the new text in */
               sci_set_selection_start(sci, linepos + startcol - 1);
               sci_replace_sel(sci, txt);

               g_free(txt);
               g_free(txt_i);
               }
            }

         /* put the selection box back */
         /* here we rely upon the last result of linepos */
         sci_set_selection_mode(sci, 1);
         sci_set_selection_start(sci, startpos - 1);
         sci_set_selection_end(sci, linepos + endcol - 1);

         /* end undo action */
         sci_end_undo_action(sci);
         }

      }
   }

static void shift_right_cb(G_GNUC_UNUSED GtkMenuItem *menuitem,
                           G_GNUC_UNUSED gpointer gdata){
   gchar *txt;
   gchar *txt_i;
   gchar char_after;
   gint txt_len;

   gint startpos;
   gint endpos;

   gint startline;
   gint endline;
   gint line_iter;
   gint linepos;
   gint linelen;

   gint startcol;
   gint endcol;

   gint i;

   ScintillaObject *sci;

   /* get a pointer to the scintilla object */
   sci = document_get_current()->editor->sci;

   if (sci_has_selection(sci)){

      startpos = sci_get_selection_start(sci);
      endpos = sci_get_selection_end(sci);

      /* sanity check -- we dont care which way the block was selected */
      if(startpos > endpos){
         i = endpos;
         endpos = startpos;
         startpos = i;
         }

      startline = sci_get_line_from_position(sci, startpos);
      endline = sci_get_line_from_position(sci, endpos);

      /* normal mode */
      if(startline == endline){

         /* get the text in question */
         txt_len = endpos - startpos;
         txt_i = g_malloc(txt_len + 1);
         txt = g_malloc(txt_len + 2);
         sci_get_selected_text(sci, txt_i);

         char_after = sci_get_char_at(sci, endpos);

         /* set up new text buf */
         (void) g_sprintf(txt, "%c%s", char_after, txt_i);

         /* start undo */
         sci_start_undo_action(sci);

         /* put the new text in */
         sci_set_selection_end(sci, endpos + 1);
         sci_replace_sel(sci, txt);

         /* select the right bit again */
         sci_set_selection_start(sci, startpos + 1);
         sci_set_selection_end(sci, endpos + 1);

         /* end undo */
         sci_end_undo_action(sci);

         g_free(txt);
         g_free(txt_i);
         }

      /* rectangle mode (we hope!) */
      else{
         startcol = sci_get_col_from_position(sci, startpos);
         endcol = sci_get_col_from_position(sci, endpos);

         /* start undo */
         sci_start_undo_action(sci);

         for(line_iter = startline; line_iter <= endline; line_iter++){
            linepos = sci_get_position_from_line(sci, line_iter);
            linelen = sci_get_line_length(sci, line_iter);

            /* do we need to do something */
            if(linelen >= startcol - 1 ){

               /* if between the two columns or at the end */
               /* add in a space */
               if(linelen <= endcol || linelen - 1 == endcol){
                  txt = g_malloc(sizeof(gchar) * 2);
                  sprintf(txt, " ");

                  sci_insert_text(sci, linepos + startcol, txt);
                  g_free(txt);
                  }

               else{
                  /* move the text itself */
                  sci_set_selection_mode(sci, 0);
                  sci_set_selection_start(sci, linepos + startcol);
                  sci_set_selection_end(sci, linepos + endcol);

                  txt_len = sci_get_selected_text_length(sci);
                  txt_i = g_malloc(txt_len + 1);
                  txt = g_malloc(txt_len + 2);

                  sci_get_selected_text(sci, txt_i);
                  char_after = sci_get_char_at(sci, linepos + endcol);

                  /* set up new text buf */
                  (void) g_sprintf(txt, "%c%s", char_after, txt_i);

                  /* put the new text in */
                  sci_set_selection_end(sci, linepos + endcol + 1);
                  sci_replace_sel(sci, txt);

                  g_free(txt);
                  g_free(txt_i);
                  }
               }
            }

         /* put the selection box back */
         /* here we rely upon the last result of linepos */
         sci_set_selection_mode(sci, 1);
         sci_set_selection_start(sci, startpos + 1);
         sci_set_selection_end(sci, linepos + endcol + 1);

         /* end undo action */
         sci_end_undo_action(sci);
         }
      }
   }


static void kb_shift_left(G_GNUC_UNUSED guint key_id){
   
   /* sanity check */
   if (document_get_current() == NULL){
       return;
       }
   
   shift_left_cb(NULL, NULL);
   }

static void kb_shift_right(G_GNUC_UNUSED guint key_id){
   
   /* sanity check */
   if (document_get_current() == NULL){
       return;
       }
   
   shift_right_cb(NULL, NULL);
   }

void plugin_init(G_GNUC_UNUSED GeanyData *data){

   /* init gettext and friends */
   main_locale_init(LOCALEDIR, GETTEXT_PACKAGE);

   menu_item_shift_left = gtk_menu_item_new_with_mnemonic(_("Shift Left"));
   gtk_widget_show(menu_item_shift_left);
   gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu),
       menu_item_shift_left);
   g_signal_connect(menu_item_shift_left, "activate",
       G_CALLBACK(shift_left_cb), NULL);

   menu_item_shift_right = gtk_menu_item_new_with_mnemonic(_("Shift Right"));
   gtk_widget_show(menu_item_shift_right);
   gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu),
       menu_item_shift_right);
   g_signal_connect(menu_item_shift_right, "activate",
       G_CALLBACK(shift_right_cb), NULL);
   
   /* make sure our menu items aren't called when there is no doc open */
   ui_add_document_sensitive(menu_item_shift_right);
   ui_add_document_sensitive(menu_item_shift_left);

   /* setup keybindings */
   keybindings_set_item(plugin_key_group, KB_SHIFT_LEFT, kb_shift_left,
      GDK_9, GDK_CONTROL_MASK, "shift_left", _("Shift Left"), menu_item_shift_left);
   keybindings_set_item(plugin_key_group, KB_SHIFT_RIGHT, kb_shift_right,
      GDK_0, GDK_CONTROL_MASK, "shift_right", _("Shift Right"), menu_item_shift_right);
   }

void plugin_cleanup(void){
   gtk_widget_destroy(menu_item_shift_left);
   gtk_widget_destroy(menu_item_shift_right);
   }
