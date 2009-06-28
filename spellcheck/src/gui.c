/*
 *      gui.c - this file is part of Spellcheck, a Geany plugin
 *
 *      Copyright 2008-2009 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2008-2009 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 *
 * $Id$
 */


#include "geany.h"
#include "support.h"

#include <ctype.h>
#include <string.h>

#include "plugindata.h"

#include "document.h"
#include "editor.h"
#include "msgwindow.h"
#include "utils.h"
#include "ui_utils.h"

#include "geanyfunctions.h"

#include "gui.h"
#include "scplugin.h"
#include "speller.h"



typedef struct
{
	gint pos;
	GeanyDocument *doc;
	/* static storage for the misspelled word under the cursor when using the editing menu */
	gchar *word;
} SpellClickInfo;
static SpellClickInfo clickinfo;

/* Flag to indicate that a callback function will be triggered by generating the appropriate event
 * but the callback should be ignored. */
static gboolean sc_ignore_callback = FALSE;



static void print_typing_changed_message(void)
{
	if (sc_info->check_while_typing)
		ui_set_statusbar(FALSE, _("Spell checking while typing is now enabled"));
	else
		ui_set_statusbar(FALSE, _("Spell checking while typing is now disabled"));
}


static void toolbar_item_toggled_cb(GtkToggleToolButton *button, gpointer user_data)
{
	if (sc_ignore_callback)
		return;

	sc_info->check_while_typing = gtk_toggle_tool_button_get_active(button);

	print_typing_changed_message();

}


void sc_gui_update_tooltip(void)
{
}


void sc_gui_update_toolbar(void)
{
	/* toolbar item is not requested, so remove the item if it exists */
	if (! sc_info->show_toolbar_item)
	{
		if (sc_info->toolbar_button != NULL)
		{
			gtk_widget_hide(GTK_WIDGET(sc_info->toolbar_button));
		}
	}
	else
	{
		if (sc_info->toolbar_button == NULL)
		{
			sc_info->toolbar_button = gtk_toggle_tool_button_new_from_stock(GTK_STOCK_SPELL_CHECK);
			sc_gui_update_tooltip();

			plugin_add_toolbar_item(geany_plugin, sc_info->toolbar_button);
			ui_add_document_sensitive(GTK_WIDGET(sc_info->toolbar_button));

			g_signal_connect(sc_info->toolbar_button, "toggled",
				G_CALLBACK(toolbar_item_toggled_cb), NULL);
		}
		gtk_widget_show(GTK_WIDGET(sc_info->toolbar_button));

		sc_ignore_callback = TRUE;
		gtk_toggle_tool_button_set_active(
			GTK_TOGGLE_TOOL_BUTTON(sc_info->toolbar_button), sc_info->check_while_typing);
		sc_ignore_callback = FALSE;
	}
}


static void menu_suggestion_item_activate_cb(GtkMenuItem *menuitem, gpointer gdata)
{
	const gchar *sugg;
	gint startword, endword;
	ScintillaObject *sci = clickinfo.doc->editor->sci;

	g_return_if_fail(clickinfo.doc != NULL && clickinfo.pos != -1);

	startword = scintilla_send_message(sci, SCI_WORDSTARTPOSITION, clickinfo.pos, 0);
	endword = scintilla_send_message(sci, SCI_WORDENDPOSITION, clickinfo.pos, 0);

	if (startword != endword)
	{
		gchar *word;

		sci_set_selection_start(sci, startword);
		sci_set_selection_end(sci, endword);

		/* retrieve the old text */
		word = g_malloc(sci_get_selected_text_length(sci) + 1);
		sci_get_selected_text(sci, word);

		/* retrieve the new text */
		sugg = gtk_label_get_text(GTK_LABEL(gtk_bin_get_child(GTK_BIN(menuitem))));

		/* replace the misspelled word with the chosen suggestion */
		sci_replace_sel(sci, sugg);

		/* store the replacement for future checks */
		sc_speller_store_replacement(word, sugg);

		/* remove indicator */
		sci_indicator_clear(sci, startword, endword - startword);

		g_free(word);
	}
}


static void menu_addword_item_activate_cd(GtkMenuItem *menuitem, gpointer gdata)
{
	gint startword, endword, i, doc_len;
	ScintillaObject *sci;
	GString *str;
	gboolean ignore = GPOINTER_TO_INT(gdata);

	if (clickinfo.doc == NULL || clickinfo.word == NULL || clickinfo.pos == -1)
		return;

	/* if we ignore the word, we add it to the current session, to ignore it
	 * also for further checks*/
	if (ignore)
		sc_speller_add_word_to_session(clickinfo.word);
	/* if we do not ignore the word, we add the word to the personal dictionary */
	else
		sc_speller_add_word(clickinfo.word);

	/* Remove all indicators on the added/ignored word */
	sci = clickinfo.doc->editor->sci;
	str = g_string_sized_new(256);
	doc_len = sci_get_length(sci);
	for (i = 0; i < doc_len; i++)
	{
		startword = scintilla_send_message(sci, SCI_INDICATORSTART, 0, i);
		if (startword >= 0)
		{
			endword = scintilla_send_message(sci, SCI_INDICATOREND, 0, startword);
			if (startword == endword)
				continue;

			if (str->len < (guint)(endword - startword + 1))
				str = g_string_set_size(str, endword - startword + 1);
			sci_get_text_range(sci, startword, endword, str->str);

			if (strcmp(str->str, clickinfo.word) == 0)
				sci_indicator_clear(sci, startword, endword - startword);

			i = endword;
		}
	}
	g_string_free(str, TRUE);
}


/* Create a @c GtkImageMenuItem with a stock image and a custom label.
 * @param stock_id Stock image ID, e.g. @c GTK_STOCK_OPEN.
 * @param label Menu item label.
 * @return The new @c GtkImageMenuItem. */
static GtkWidget *image_menu_item_new(const gchar *stock_id, const gchar *label)
{
	GtkWidget *item = gtk_image_menu_item_new_with_label(label);
	GtkWidget *image = gtk_image_new_from_stock(stock_id, GTK_ICON_SIZE_MENU);

	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	gtk_widget_show(image);
	return item;
}


void sc_gui_update_editor_menu_cb(GObject *obj, const gchar *word, gint pos,
								  GeanyDocument *doc, gpointer user_data)
{
	gchar *search_word;

	g_return_if_fail(doc != NULL && doc->is_valid);

	/* hide the submenu in any case, we will reshow it again if we actually found something */
	gtk_widget_hide(sc_info->edit_menu);
	gtk_widget_hide(sc_info->edit_menu_sep);

	/* if we have a selection, prefer it over the current word */
	if (sci_has_selection(doc->editor->sci))
	{
		gint len = sci_get_selected_text_length(doc->editor->sci);
		search_word = g_malloc(len + 1);
		sci_get_selected_text(doc->editor->sci, search_word);
	}
	else
		search_word = g_strdup(word);

	/* ignore numbers or words starting with digits and non-text */
	if (! NZV(search_word) || isdigit(*search_word) || ! sc_speller_is_text(doc, pos))
	{
		g_free(search_word);
		return;
	}

	if (sc_speller_dict_check(search_word) != 0)
	{
		GtkWidget *menu_item, *menu;
		gchar *label;
		gsize n_suggs, i;
		gchar **suggs;

		suggs = sc_speller_dict_suggest(search_word, &n_suggs);

		clickinfo.pos = pos;
		clickinfo.doc = doc;
		setptr(clickinfo.word, search_word);

		if (sc_info->edit_menu_sub != NULL && GTK_IS_WIDGET(sc_info->edit_menu_sub))
			gtk_widget_destroy(sc_info->edit_menu_sub);

		sc_info->edit_menu_sub = menu = gtk_menu_new();
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(sc_info->edit_menu), sc_info->edit_menu_sub);

		for (i = 0; i < n_suggs; i++)
		{
			if (i > 0 && i % 10 == 0)
			{
				menu_item = gtk_menu_item_new();
				gtk_widget_show(menu_item);
				gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);

				menu_item = gtk_menu_item_new_with_label(_("More..."));
				gtk_widget_show(menu_item);
				gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);

				menu = gtk_menu_new();
				gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), menu);
			}
			menu_item = gtk_menu_item_new_with_label(suggs[i]);
			gtk_container_add(GTK_CONTAINER(menu), menu_item);
			g_signal_connect(menu_item, "activate",
				G_CALLBACK(menu_suggestion_item_activate_cb), NULL);
		}
		if (suggs == NULL)
		{
			menu_item = gtk_menu_item_new_with_label(_("(No Suggestions)"));
			gtk_widget_set_sensitive(menu_item, FALSE);
			gtk_container_add(GTK_CONTAINER(sc_info->edit_menu_sub), menu_item);
		}
		menu_item = gtk_separator_menu_item_new();
		gtk_container_add(GTK_CONTAINER(sc_info->edit_menu_sub), menu_item);

		label = g_strdup_printf(_("Add \"%s\" to Dictionary"), search_word);
		menu_item = image_menu_item_new(GTK_STOCK_ADD, label);
		gtk_container_add(GTK_CONTAINER(sc_info->edit_menu_sub), menu_item);
		g_signal_connect(menu_item, "activate",
			G_CALLBACK(menu_addword_item_activate_cd), GINT_TO_POINTER(0));

		menu_item = image_menu_item_new(GTK_STOCK_REMOVE, _("Ignore All"));
		gtk_container_add(GTK_CONTAINER(sc_info->edit_menu_sub), menu_item);
		g_signal_connect(menu_item, "activate",
			G_CALLBACK(menu_addword_item_activate_cd), GINT_TO_POINTER(1));

		gtk_widget_show(sc_info->edit_menu);
		gtk_widget_show(sc_info->edit_menu_sep);
		gtk_widget_show_all(sc_info->edit_menu_sub);

		if (suggs != NULL)
			sc_speller_dict_free_string_list(suggs);

		g_free(label);
	}
	else
	{
		g_free(search_word);
	}
}


static void indicator_clear_on_line(GeanyDocument *doc, gint line_number)
{
	gint start_pos, length;

	g_return_if_fail(doc != NULL);

	start_pos = sci_get_position_from_line(doc->editor->sci, line_number);
	length = sci_get_line_length(doc->editor->sci, line_number);

	sci_indicator_clear(doc->editor->sci, start_pos, length);
}


static gboolean need_delay(void)
{
	static gint64 time_prev = 0; /* time in microseconds */
	gint64 time_now;
	GTimeVal t;

	g_get_current_time(&t);

	time_now = ((gint64) t.tv_sec * G_USEC_PER_SEC) + t.tv_usec;

	/* delay keypresses for 0.5 seconds */
	if (time_now < (time_prev + 500000))
		return TRUE;

	/* set current time for the next key press */
	time_prev = time_now;

	return FALSE;
}


/* Checks only the last word before the current cursor position -> check as you type. */
gboolean sc_gui_key_release_cb(GtkWidget *widget, GdkEventKey *ev, gpointer data)
{
	gint line_number;
	gchar *line;
	GeanyDocument *doc;
	GtkWidget *focusw;

	if (! sc_info->check_while_typing)
		return FALSE;

	/* check only once in a while */
	if (need_delay())
		return FALSE;

	doc = document_get_current();

	if (ev->keyval == '\r' &&
		scintilla_send_message(doc->editor->sci, SCI_GETEOLMODE, 0, 0) == SC_EOL_CRLF)
	{	/* prevent double line checking */
		return FALSE;
	}

	/* bail out if we don't have a document or if we are not in the editor widget */
	focusw = gtk_window_get_focus(GTK_WINDOW(geany->main_widgets->window));
	if (doc == NULL || focusw != GTK_WIDGET(doc->editor->sci))
		return FALSE;

	line_number = sci_get_current_line(doc->editor->sci);
	if (ev->keyval == '\n' || ev->keyval == '\r')
		line_number--; /* check previous line if we start a new one */
	line = sci_get_line(doc->editor->sci, line_number);

	indicator_clear_on_line(doc, line_number);
	if (sc_speller_process_line(doc, line_number, line) != 0)
	{
		if (sc_info->use_msgwin)
			msgwin_switch_tab(MSG_MESSAGE, FALSE);
	}

	g_free(line);

	return FALSE;
}


#if ! GTK_CHECK_VERSION(2, 16, 0)
static void gtk_menu_item_set_label(GtkMenuItem *menu_item, const gchar *label)
{
	if (GTK_BIN(menu_item)->child != NULL)
	{
		GtkWidget *child = GTK_BIN(menu_item)->child;

		if (GTK_IS_LABEL(child))
			gtk_label_set_text(GTK_LABEL(child), label);
	}
}
#endif


static void update_labels(void)
{
	gchar *label;

	label = g_strdup_printf(_("Default (%s)"),
		(sc_info->default_language != NULL) ? sc_info->default_language : _("unknown"));

	gtk_menu_item_set_label(GTK_MENU_ITEM(sc_info->submenu_item_default), label);

	g_free(label);

#if GTK_CHECK_VERSION(2, 12, 0)
	if (sc_info->toolbar_button != NULL)
	{
		gchar *text = g_strdup_printf(
			_("Toggle spell check while typing (current language: %s)"),
			(sc_info->default_language != NULL) ? sc_info->default_language : _("unknown"));
		gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(sc_info->toolbar_button), text);
		g_free(text);
	}
#endif
}


static void menu_item_toggled_cb(GtkCheckMenuItem *menuitem, gpointer gdata)
{
	GeanyDocument *doc;

	if (sc_ignore_callback)
		return;

	if (menuitem != NULL &&
		GTK_IS_CHECK_MENU_ITEM(menuitem) &&
		! gtk_check_menu_item_get_active(menuitem))
	{
		return;
	}
	doc = document_get_current();

	/* Another language was chosen from the menu item, so make it default for this session. */
    if (gdata != NULL)
	{
		setptr(sc_info->default_language, g_strdup(gdata));
		sc_speller_reinit_enchant_dict();
		update_labels();
	}

	editor_indicator_clear(doc->editor, GEANY_INDICATOR_ERROR);
	if (sc_info->use_msgwin)
	{
		msgwin_clear_tab(MSG_MESSAGE);
		msgwin_switch_tab(MSG_MESSAGE, FALSE);
	}

	sc_speller_check_document(doc);
}


void sc_gui_kb_run_activate_cb(guint key_id)
{
	menu_item_toggled_cb(NULL, NULL);
}


void sc_gui_kb_toggle_typing_activate_cb(guint key_id)
{
	sc_info->check_while_typing = ! sc_info->check_while_typing;

	print_typing_changed_message();

	sc_gui_update_toolbar();
}


void sc_gui_create_edit_menu(void)
{
	sc_info->edit_menu = ui_image_menu_item_new(GTK_STOCK_SPELL_CHECK, _("Spelling Suggestions"));
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->editor_menu), sc_info->edit_menu);
	gtk_menu_reorder_child(GTK_MENU(geany->main_widgets->editor_menu), sc_info->edit_menu, 0);

	sc_info->edit_menu_sep = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->editor_menu), sc_info->edit_menu_sep);
	gtk_menu_reorder_child(GTK_MENU(geany->main_widgets->editor_menu), sc_info->edit_menu_sep, 1);

	gtk_widget_show_all(sc_info->edit_menu);
}


void sc_gui_update_menu(void)
{
	GtkWidget *child, *menu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(sc_info->menu_item));
	GList *l, *children = gtk_container_get_children(GTK_CONTAINER(menu));

	sc_ignore_callback = TRUE;
	for (l = children; l != NULL; l = g_list_next(l))
	{
		if ((child = GTK_BIN(l->data)->child) != NULL)
		{
			if (GTK_IS_LABEL(child))
			{
				if (utils_str_equal(sc_info->default_language, gtk_label_get_text(GTK_LABEL(child))))
					gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(l->data), TRUE);
			}
		}
	}
	sc_ignore_callback = FALSE;

	update_labels();
}


void sc_gui_create_menu(GtkWidget *sp_item)
{
	GtkWidget *menu, *menu_item;
	guint i;
	GSList *group = NULL;
	gchar *label;

	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), sp_item);

	menu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(sp_item), menu);

	sc_info->submenu_item_default = gtk_menu_item_new_with_label(NULL);
	gtk_container_add(GTK_CONTAINER(menu), sc_info->submenu_item_default);
	g_signal_connect(sc_info->submenu_item_default, "activate",
		G_CALLBACK(menu_item_toggled_cb), NULL);

	update_labels();

	menu_item = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(menu), menu_item);

	sc_ignore_callback = TRUE;
	for (i = 0; i < sc_info->dicts->len; i++)
	{
		label = g_ptr_array_index(sc_info->dicts, i);
		menu_item = gtk_radio_menu_item_new_with_label(group, label);
		group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(menu_item));
		if (utils_str_equal(sc_info->default_language, label))
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item), TRUE);
		gtk_container_add(GTK_CONTAINER(menu), menu_item);
		g_signal_connect(menu_item, "toggled", G_CALLBACK(menu_item_toggled_cb), label);
	}
	sc_ignore_callback = FALSE;
}


void sc_gui_init(void)
{
	clickinfo.word = NULL;
}


void sc_gui_free(void)
{
	g_free(clickinfo.word);
}
