/*
 *      gui.c - this file is part of Spellcheck, a Geany plugin
 *
 *      Copyright 2008-2011 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2008-2010 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <geanyplugin.h>

#include <ctype.h>
#include <string.h>


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

typedef struct
{
	GeanyDocument *doc;
	gint line_number;
	gint line_count;
	guint check_while_typing_idle_source_id;
} CheckLineData;
static CheckLineData check_line_data;

/* Flag to indicate that a callback function will be triggered by generating the appropriate event
 * but the callback should be ignored. */
static gboolean sc_ignore_callback = FALSE;


static void perform_check(GeanyDocument *doc);


static void clear_spellcheck_error_markers(GeanyDocument *doc)
{
	editor_indicator_clear(doc->editor, GEANY_INDICATOR_ERROR);
}


static void perform_spell_check_toggle(void)
{
	/* force a rescan of the document if 'check while typing' has been turned on and clean
	 * errors if it has been turned off */
	GeanyDocument *doc = document_get_current();
	if (sc_info->check_while_typing)
	{
		perform_check(doc);
	}
	else
	{
		clear_spellcheck_error_markers(doc);
	}

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

	perform_spell_check_toggle();
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
		word = sci_get_selection_contents(sci);

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


static void menu_addword_item_activate_cb(GtkMenuItem *menuitem, gpointer gdata)
{
	gint startword, endword, i, doc_len;
	ScintillaObject *sci;
	gboolean ignore = GPOINTER_TO_INT(gdata);
	gint click_word_len;

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
	click_word_len = (gint) strlen(clickinfo.word);
	doc_len = sci_get_length(sci);
	for (i = 0; i < doc_len; i++)
	{
		startword = scintilla_send_message(sci, SCI_INDICATORSTART, 0, i);
		if (startword >= 0)
		{
			endword = scintilla_send_message(sci, SCI_INDICATOREND, 0, startword);
			if (startword == endword)
				continue;

			if (click_word_len == endword - startword)
			{
				const gchar *ptr = (const gchar *) scintilla_send_message(sci,
					SCI_GETRANGEPOINTER, startword, endword - startword);

				if (strncmp(ptr, clickinfo.word, click_word_len) == 0)
					sci_indicator_clear(sci, startword, endword - startword);
			}

			i = endword;
		}
	}
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


static GtkWidget *init_editor_submenu(void)
{
	if (sc_info->show_editor_menu_item_sub_menu)
	{
		if (sc_info->edit_menu_sub != NULL && GTK_IS_WIDGET(sc_info->edit_menu_sub))
			gtk_widget_destroy(sc_info->edit_menu_sub);

		sc_info->edit_menu_sub = gtk_menu_new();
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(sc_info->edit_menu), sc_info->edit_menu_sub);

		gtk_widget_show(sc_info->edit_menu);
		gtk_widget_show(sc_info->edit_menu_sep);
		gtk_widget_show(sc_info->edit_menu_sub);

		return sc_info->edit_menu_sub;
	}
	else
	{
		return geany->main_widgets->editor_menu;
	}
}


static void perform_check(GeanyDocument *doc)
{
	clear_spellcheck_error_markers(doc);

	if (sc_info->use_msgwin)
	{
		msgwin_clear_tab(MSG_MESSAGE);
		msgwin_switch_tab(MSG_MESSAGE, FALSE);
	}

	sc_speller_check_document(doc);
}


static void perform_spell_check_cb(GtkWidget *menu_item, GeanyDocument *doc)
{
	perform_check(doc);
}


static gboolean perform_check_delayed_cb(gpointer doc)
{
	perform_check((GeanyDocument*)doc);
	return FALSE;
}


void sc_gui_document_open_cb(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
	if (sc_info->check_on_document_open && main_is_realized())
		g_idle_add(perform_check_delayed_cb, doc);
}


static void menu_item_ref(GtkWidget *menu_item)
{
	if (! sc_info->show_editor_menu_item_sub_menu)
		sc_info->edit_menu_items = g_slist_append(sc_info->edit_menu_items, menu_item);
}


static void update_editor_menu_items(const gchar *search_word, const gchar **suggs, gsize n_suggs)
{
	GtkWidget *menu_item, *menu, *sub_menu;
	GSList *node = NULL;
	gchar *label;
	gsize i;

	menu = init_editor_submenu();
	sub_menu = menu;

	/* display 5 suggestions on top level, 20 more in sub menu */
	for (i = 0; i < MIN(n_suggs, 25); i++)
	{
		if (i >= 5 && menu == sub_menu)
		{
			/* create "More..." sub menu */
			if (sc_info->show_editor_menu_item_sub_menu)
			{
				menu_item = gtk_separator_menu_item_new();
				gtk_widget_show(menu_item);
				gtk_menu_shell_append(GTK_MENU_SHELL(sub_menu), menu_item);
			}

			menu_item = gtk_menu_item_new_with_label(_("More..."));
			gtk_widget_show(menu_item);
			gtk_menu_shell_append(GTK_MENU_SHELL(sub_menu), menu_item);
			menu_item_ref(menu_item);

			sub_menu = gtk_menu_new();
			gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), sub_menu);
		}
		menu_item = gtk_menu_item_new_with_label(suggs[i]);
		gtk_widget_show(menu_item);
		gtk_container_add(GTK_CONTAINER(sub_menu), menu_item);
		if (menu == sub_menu)
		{
			/* Remember menu items to delete only for the top-level, the nested menu items are
			 * destroyed recursively via the sub menu */
			menu_item_ref(menu_item);
		}
		g_signal_connect(menu_item, "activate", G_CALLBACK(menu_suggestion_item_activate_cb), NULL);
	}
	if (suggs == NULL)
	{
		menu_item = gtk_menu_item_new_with_label(_("(No Suggestions)"));
		gtk_widget_set_sensitive(menu_item, FALSE);
		gtk_widget_show(menu_item);
		gtk_container_add(GTK_CONTAINER(menu), menu_item);
		menu_item_ref(menu_item);
	}
	if (sc_info->show_editor_menu_item_sub_menu)
	{
		menu_item = gtk_separator_menu_item_new();
		gtk_widget_show(menu_item);
		gtk_container_add(GTK_CONTAINER(menu), menu_item);
	}

	label = g_strdup_printf(_("Add \"%s\" to Dictionary"), search_word);
	menu_item = image_menu_item_new(GTK_STOCK_ADD, label);
	gtk_widget_show(menu_item);
	gtk_container_add(GTK_CONTAINER(menu), menu_item);
	menu_item_ref(menu_item);
	g_signal_connect(menu_item, "activate",
		G_CALLBACK(menu_addword_item_activate_cb), GINT_TO_POINTER(FALSE));

	menu_item = image_menu_item_new(GTK_STOCK_REMOVE, _("Ignore All"));
	gtk_widget_show(menu_item);
	gtk_container_add(GTK_CONTAINER(menu), menu_item);
	menu_item_ref(menu_item);
	g_signal_connect(menu_item, "activate",
		G_CALLBACK(menu_addword_item_activate_cb), GINT_TO_POINTER(TRUE));

	g_free(label);

	/* re-order menu items: above all menu items are append but for the top-level menu items
	 * we want them to appear at the top of the editor menu */
	if (! sc_info->show_editor_menu_item_sub_menu)
	{
		gpointer child;
		/* final separator */
		menu_item = gtk_separator_menu_item_new();
		gtk_widget_show(menu_item);
		gtk_container_add(GTK_CONTAINER(menu), menu_item);
		menu_item_ref(menu_item);
		/* re-order */
		i = 0;
		foreach_slist(node, sc_info->edit_menu_items)
		{
			child = node->data;
			gtk_menu_reorder_child(GTK_MENU(menu), GTK_WIDGET(child), i);
			i++;
		}
	}
}


void sc_gui_update_editor_menu_cb(GObject *obj, const gchar *word, gint pos,
								  GeanyDocument *doc, gpointer user_data)
{
	gchar *search_word;

	g_return_if_fail(doc != NULL && doc->is_valid);

	/* hide the submenu in any case, we will reshow it again if we actually found something */
	if (sc_info->edit_menu != NULL)
		gtk_widget_hide(sc_info->edit_menu);
	if (sc_info->edit_menu_sep != NULL)
		gtk_widget_hide(sc_info->edit_menu_sep);
	/* clean previously added items to the editor menu */
	if (sc_info->edit_menu_items != NULL)
	{
		g_slist_free_full(sc_info->edit_menu_items, (GDestroyNotify) gtk_widget_destroy);
		sc_info->edit_menu_items = NULL;
	}

	if (! sc_info->show_editor_menu_item)
		return;

	/* if we have a selection, prefer it over the current word */
	if (sci_has_selection(doc->editor->sci))
		search_word = sci_get_selection_contents(doc->editor->sci);
	else
		search_word = g_strdup(word);

	/* ignore numbers or words starting with digits and non-text */
	if (EMPTY(search_word) || isdigit(*search_word) || ! sc_speller_is_text(doc, pos))
	{
		g_free(search_word);
		return;
	}

	/* ignore too long search words */
	if (strlen(search_word) > 100)
	{
		GtkWidget *menu_item, *menu;

		menu = init_editor_submenu();
		menu_item = gtk_menu_item_new_with_label(
			_("Search term is too long to provide\nspelling suggestions in the editor menu."));
		gtk_widget_set_sensitive(menu_item, FALSE);
		gtk_widget_show(menu_item);
		gtk_container_add(GTK_CONTAINER(menu), menu_item);
		menu_item_ref(menu_item);

		menu_item = gtk_menu_item_new_with_label(_("Perform Spell Check"));
		gtk_widget_show(menu_item);
		gtk_container_add(GTK_CONTAINER(menu), menu_item);
		menu_item_ref(menu_item);
		g_signal_connect(menu_item, "activate", G_CALLBACK(perform_spell_check_cb), doc);

		g_free(search_word);
		return;
	}

	if (sc_speller_dict_check(search_word) != 0)
	{
		gsize n_suggs;
		gchar **suggs;

		suggs = sc_speller_dict_suggest(search_word, &n_suggs);

		clickinfo.pos = pos;
		clickinfo.doc = doc;
		SETPTR(clickinfo.word, search_word);

		update_editor_menu_items(search_word, (const gchar**) suggs, n_suggs);

		if (suggs != NULL)
			sc_speller_dict_free_string_list(suggs);
	}
	else
	{
		g_free(search_word); /* search_word is free'd via clickinfo.word otherwise */
	}
}


static void indicator_clear_on_line(GeanyDocument *doc, gint line_number)
{
	gint start_pos, length;

	g_return_if_fail(doc != NULL);

	start_pos = sci_get_position_from_line(doc->editor->sci, line_number);
	length = sci_get_line_length(doc->editor->sci, line_number);

	sci_indicator_set(doc->editor->sci, GEANY_INDICATOR_ERROR);
	sci_indicator_clear(doc->editor->sci, start_pos, length);
}


static gboolean check_lines(gpointer data)
{
	GeanyDocument *doc = check_line_data.doc;

	/* since we're in an timeout callback, the document may have been closed */
	if (DOC_VALID (doc))
	{
		gint line_number = check_line_data.line_number;
		gint line_count = check_line_data.line_count;
		gint i;

		for (i = 0; i < line_count; i++)
		{
			indicator_clear_on_line(doc, line_number);
			if (sc_speller_process_line(doc, line_number) != 0)
			{
				if (sc_info->use_msgwin)
					msgwin_switch_tab(MSG_MESSAGE, FALSE);
			}
			line_number++;
		}
	}
	check_line_data.check_while_typing_idle_source_id = 0;
	return FALSE;
}


static gboolean need_delay(void)
{
	static gint64 time_prev = 0; /* time in microseconds */
	gint64 time_now;
	GTimeVal t;
	const gint timeout = 500; /* delay in milliseconds */
	gboolean ret = FALSE;

	g_get_current_time(&t);

	time_now = ((gint64) t.tv_sec * G_USEC_PER_SEC) + t.tv_usec;

	/* delay keypresses for 0.5 seconds */
	if (time_now < (time_prev + (timeout * 1000)))
		return TRUE;

	if (check_line_data.check_while_typing_idle_source_id == 0)
	{
		check_line_data.check_while_typing_idle_source_id =
			plugin_timeout_add(geany_plugin, timeout, check_lines, NULL);
		ret = TRUE;
	}

	/* set current time for the next key press */
	time_prev = time_now;

	return ret;
}


static void check_on_text_changed(GeanyDocument *doc, gint position, gint lines_added)
{
	gint line_number;
	gint line_count;

	/* Iterating over all lines which changed as indicated by lines_added. lines_added is 0
	 * if only one line has changed, in this case set line_count to 1. Otherwise, iterating over all
	 * new lines makes spell checking work for pasted text. */
	line_count = MAX(1, lines_added);

	line_number = sci_get_line_from_position(doc->editor->sci, position);
	/* TODO: storing these information in the global check_line_data struct isn't that good.
	 * The data gets overwritten when a new line is inserted and so there is a chance that the
	 * previous line is not checked to the end. One solution could be to simple maintain a list
	 * of line numbers which needs to be checked and do this in the timeout handler. */
	check_line_data.doc = doc;
	check_line_data.line_number = line_number;
	check_line_data.line_count = line_count;

	/* check only once in a while */
	if (! need_delay())
		check_lines(NULL);
}


gboolean sc_gui_editor_notify(GObject *object, GeanyEditor *editor,
							  SCNotification *nt, gpointer data)
{
	if (! sc_info->check_while_typing)
		return FALSE;

	if (nt->modificationType & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT))
	{
		check_on_text_changed(editor->document, nt->position, nt->linesAdded);
	}

	return FALSE;
}


static void update_labels(void)
{
	gchar *label;

	label = g_strdup_printf(_("Default (%s)"),
		(sc_info->default_language != NULL) ? sc_info->default_language : _("unknown"));

	gtk_menu_item_set_label(GTK_MENU_ITEM(sc_info->submenu_item_default), label);

	g_free(label);

	if (sc_info->toolbar_button != NULL)
	{
		gchar *text = g_strdup_printf(
			_("Toggle spell check (current language: %s)"),
			(sc_info->default_language != NULL) ? sc_info->default_language : _("unknown"));
		gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(sc_info->toolbar_button), text);
		g_free(text);
	}
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
		SETPTR(sc_info->default_language, g_strdup(gdata));
		sc_speller_reinit_enchant_dict();
		sc_gui_update_menu();
		update_labels();
	}

	perform_check(doc);
}


void sc_gui_kb_run_activate_cb(guint key_id)
{
	perform_check(document_get_current());
}


void sc_gui_kb_toggle_typing_activate_cb(guint key_id)
{
	sc_info->check_while_typing = ! sc_info->check_while_typing;

	perform_spell_check_toggle();

	sc_gui_update_toolbar();
}


static void free_editor_menu_items(void)
{
	if (sc_info->edit_menu != NULL)
	{
		gtk_widget_destroy(sc_info->edit_menu);
		sc_info->edit_menu = NULL;
	}
	if (sc_info->edit_menu_sep != NULL)
	{
		gtk_widget_destroy(sc_info->edit_menu_sep);
		sc_info->edit_menu_sep = NULL;
	}
	if (sc_info->edit_menu_items != NULL)
	{
		g_slist_free_full(sc_info->edit_menu_items, (GDestroyNotify) gtk_widget_destroy);
		sc_info->edit_menu_items = NULL;
	}
}


void sc_gui_recreate_editor_menu(void)
{
	free_editor_menu_items();
	if (sc_info->show_editor_menu_item_sub_menu)
	{
		sc_info->edit_menu = ui_image_menu_item_new(GTK_STOCK_SPELL_CHECK, _("Spelling Suggestions"));
		gtk_container_add(GTK_CONTAINER(geany->main_widgets->editor_menu), sc_info->edit_menu);
		gtk_menu_reorder_child(GTK_MENU(geany->main_widgets->editor_menu), sc_info->edit_menu, 0);

		sc_info->edit_menu_sep = gtk_separator_menu_item_new();
		gtk_container_add(GTK_CONTAINER(geany->main_widgets->editor_menu), sc_info->edit_menu_sep);
		gtk_menu_reorder_child(GTK_MENU(geany->main_widgets->editor_menu), sc_info->edit_menu_sep, 1);

		gtk_widget_show_all(sc_info->edit_menu);
	}
}


void sc_gui_update_menu(void)
{
	GtkWidget *menu_item;
	guint i;
	static gboolean need_init = TRUE;
	GSList *group = NULL;
	gchar *label;

	if (need_init)
	{
		gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), sc_info->menu_item);
		need_init = FALSE;
	}

	if (sc_info->main_menu != NULL)
		gtk_widget_destroy(sc_info->main_menu);

	sc_info->main_menu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(sc_info->menu_item), sc_info->main_menu);

	sc_info->submenu_item_default = gtk_menu_item_new_with_label(NULL);
	gtk_container_add(GTK_CONTAINER(sc_info->main_menu), sc_info->submenu_item_default);
	g_signal_connect(sc_info->submenu_item_default, "activate",
		G_CALLBACK(menu_item_toggled_cb), NULL);

	update_labels();

	menu_item = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(sc_info->main_menu), menu_item);

	sc_ignore_callback = TRUE;
	for (i = 0; i < sc_info->dicts->len; i++)
	{
		label = g_ptr_array_index(sc_info->dicts, i);
		menu_item = gtk_radio_menu_item_new_with_label(group, label);
		group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(menu_item));
		if (utils_str_equal(sc_info->default_language, label))
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item), TRUE);
		gtk_container_add(GTK_CONTAINER(sc_info->main_menu), menu_item);
		g_signal_connect(menu_item, "toggled", G_CALLBACK(menu_item_toggled_cb), label);
	}
	sc_ignore_callback = FALSE;
	gtk_widget_show_all(sc_info->main_menu);
}


void sc_gui_init(void)
{
	clickinfo.word = NULL;
	sc_info->edit_menu_items = NULL;
	sc_info->edit_menu = NULL;
	sc_info->edit_menu_sep = NULL;
	sc_info->edit_menu_items = NULL;

	sc_gui_recreate_editor_menu();
}


void sc_gui_free(void)
{
	g_free(clickinfo.word);
	if (check_line_data.check_while_typing_idle_source_id != 0)
		g_source_remove(check_line_data.check_while_typing_idle_source_id);
	if (sc_info->toolbar_button != NULL)
		gtk_widget_destroy(GTK_WIDGET(sc_info->toolbar_button));
	free_editor_menu_items();
}
