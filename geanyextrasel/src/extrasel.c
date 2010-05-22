/*
 *  extrasel.c
 *
 *  Copyright 2010 Dimitar Toshkov Zhekov <jimmy@is-vn.bg>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <gdk/gdkkeysyms.h>

#include "geanyplugin.h"

GeanyPlugin	*geany_plugin;
GeanyData	*geany_data;
GeanyFunctions	*geany_functions;

PLUGIN_VERSION_CHECK(150)

PLUGIN_SET_INFO(_("Extra Selection"), _("Column mode, select to line / matching brace."),
		"0.1", "Dimitar Toshkov Zhekov <jimmy@is-vn.bg>");

/* Keybinding(s) */
enum
{
	COLUMN_MODE_KB,
	GOTO_LINE_EXTEND_KB,
	BRACE_MATCH_EXTEND_KB,
	COUNT_KB
};

PLUGIN_KEY_GROUP(extra_select, COUNT_KB)

static GtkWidget *main_menu_item = NULL;
static GtkWidget *column_mode_item;
static GtkWidget *goto_line_item;
static GtkWidget *brace_match_item;

static gboolean column_mode = FALSE;

typedef struct _command_key
{
	guint key;
	guint keypad;
	gint command;
} command_key;

static const command_key command_keys[] =
{
	{ GDK_Left,   GDK_KP_Left,   SCI_WORDLEFT },
	{ GDK_Right,  GDK_KP_Right,  SCI_WORDRIGHTEND },
	{ GDK_Home,   GDK_KP_Home,   SCI_DOCUMENTSTART },
	{ GDK_End,    GDK_KP_End,    SCI_DOCUMENTEND },
	{ GDK_Up,     GDK_KP_Up,     SCI_PARAUP },
	{ GDK_Down,   GDK_KP_Down,   SCI_PARADOWN },
	{ 0, 0, 0 }
};

/* not #defined in 0.18 */
#define sci_get_anchor(sci) scintilla_send_message((sci), SCI_GETANCHOR, 0, 0)
#define sci_set_anchor(sci, pos) scintilla_send_message((sci), SCI_SETANCHOR, (pos), 0)

static void column_mode_command(ScintillaObject *sci, int command)
{
	gboolean set_anchor = sci_get_selection_mode(sci) != SC_SEL_RECTANGLE;
	int anchor;

	if (set_anchor)
		anchor = sci_get_anchor(sci);
	sci_set_selection_mode(sci, SC_SEL_RECTANGLE);
	sci_send_command(sci, command);
	if (set_anchor)
		sci_set_anchor(sci, anchor);
	sci_send_command(sci, SCI_CANCEL);
}

static gboolean on_key_press_event(GtkWidget *widget, GdkEventKey *event, G_GNUC_UNUSED
	gpointer user_data)
{
	guint state = GDK_CONTROL_MASK | GDK_SHIFT_MASK | (column_mode ? 0 : GDK_MOD1_MASK);

	if ((event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_MOD1_MASK)) == state)
	{
		const command_key *ck;

		for (ck = command_keys; ck->key; ck++)
		{
			if (event->keyval == ck->key || event->keyval == ck->keypad)
			{
				GeanyDocument *doc = document_get_current();

				if (doc)
				{
					ScintillaObject *sci = doc->editor->sci;

					if (gtk_window_get_focus(GTK_WINDOW(widget)) == sci)
					{
						column_mode_command(sci, ck->command);
						return TRUE;
					}
				}
				/* no document or not focused */
				break;
			}
		}
	}

	return FALSE;
}

typedef struct _select_key
{
	gint key;
	gint stream;
	gint rectangle;
} select_key;

static const select_key select_keys[] =
{
	{ SCK_DOWN  | (SCMOD_SHIFT << 16),  SCI_LINEDOWNEXTEND,   SCI_LINEDOWNRECTEXTEND },
	{ SCK_UP    | (SCMOD_SHIFT << 16),  SCI_LINEUPEXTEND,     SCI_LINEUPRECTEXTEND },
	{ SCK_LEFT  | (SCMOD_SHIFT << 16),  SCI_CHARLEFTEXTEND,   SCI_CHARLEFTRECTEXTEND  },
	{ SCK_RIGHT | (SCMOD_SHIFT << 16),  SCI_CHARRIGHTEXTEND,  SCI_CHARRIGHTRECTEXTEND },
	{ SCK_HOME  | (SCMOD_SHIFT << 16),  SCI_VCHOMEEXTEND,     SCI_VCHOMERECTEXTEND },
	{ SCK_END   | (SCMOD_SHIFT << 16),  SCI_LINEENDEXTEND,    SCI_LINEENDRECTEXTEND },
	{ SCK_PRIOR | (SCMOD_SHIFT << 16),  SCI_PAGEUPEXTEND,     SCI_PAGEUPRECTEXTEND },
	{ SCK_NEXT  | (SCMOD_SHIFT << 16),  SCI_PAGEDOWNEXTEND,   SCI_PAGEDOWNRECTEXTEND },
	{ 0, 0, 0 }
};

/* not #defined in 0.18 */
#define sci_clear_cmdkey(sci, key) scintilla_send_message((sci), SCI_CLEARCMDKEY, (key), 0)
#define sci_assign_cmdkey(sci, key, command) \
	scintilla_send_message((sci), SCI_ASSIGNCMDKEY, (key), (command))

static void assign_column_keys(ScintillaObject *sci)
{
	const select_key *sk;

	for (sk = select_keys; sk->key; sk++)
	{
		if (column_mode)
		{
			sci_clear_cmdkey(sci, sk->key | (SCMOD_ALT << 16));
			sci_assign_cmdkey(sci, sk->key, sk->rectangle);
		}
		else
		{
			sci_assign_cmdkey(sci, sk->key, sk->stream);
			sci_assign_cmdkey(sci, sk->key | (SCMOD_ALT << 16), sk->rectangle);
		}
	}
}

static void on_document_switch(G_GNUC_UNUSED GObject *obj, GeanyDocument *doc,
	G_GNUC_UNUSED gpointer user_data)
{
	assign_column_keys(doc->editor->sci);
}

PluginCallback plugin_callbacks[] =
{
	{ "document-new", (GCallback) &on_document_switch, FALSE, NULL },
	{ "document-open", (GCallback) &on_document_switch, FALSE, NULL },
	{ "document-activate", (GCallback) &on_document_switch, FALSE, NULL },
	{ NULL, NULL, FALSE, NULL }
};

static void on_extra_select_activate(G_GNUC_UNUSED GtkMenuItem *menuitem,
	G_GNUC_UNUSED gpointer gdata)
{
	gtk_widget_set_sensitive(goto_line_item, document_get_current() != NULL);
	gtk_widget_set_sensitive(brace_match_item, document_get_current() != NULL);
}

static void on_column_mode_toggled(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED
	gpointer gdata)
{
	GeanyDocument *doc = document_get_current();

	column_mode = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(column_mode_item));
	if (doc)
	{
		ScintillaObject *sci = doc->editor->sci;
		int select_mode = sci_get_selection_mode(sci);
		int anchor = sci_get_anchor(sci);
		int cursor = sci_get_current_position(sci);

		assign_column_keys(sci);
		if (column_mode && select_mode == SC_SEL_STREAM)
		{
			sci_set_selection_mode(sci, SC_SEL_RECTANGLE);
			sci_set_anchor(sci, anchor);
			/* sci_set_current_pos() clears the selection, bypass */
			scintilla_send_message(sci, SCI_SETCURRENTPOS, cursor, 0);
		}
		else if (!column_mode && select_mode == SC_SEL_RECTANGLE)
		{
			sci_set_selection_mode(sci, SC_SEL_STREAM);
			scintilla_send_message(sci, SCI_SETSEL, anchor, cursor);
		}
		sci_send_command(sci, SCI_CANCEL);
	}
}

static void on_column_mode_key(G_GNUC_UNUSED guint key_id)
{
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(column_mode_item), !column_mode);
}

static void doit_and_select(guint group_id, guint key_id)
{
	GeanyDocument *doc = document_get_current();

	if (doc)
	{
		ScintillaObject *sci = doc->editor->sci;
		int before = sci_get_current_position(sci), after;

		keybindings_send_command(group_id, key_id);
		after = sci_get_current_position(sci);
		if (before != after)
			sci_set_anchor(sci, before);
	}
}

static void on_goto_line_activate(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED
	gpointer gdata)
{
	doit_and_select(GEANY_KEY_GROUP_GOTO, GEANY_KEYS_GOTO_LINE);
}

static void on_goto_line_key(G_GNUC_UNUSED guint key_id)
{
	on_goto_line_activate(NULL, NULL);
}

static void on_brace_match_activate(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED
	gpointer gdata)
{
	doit_and_select(GEANY_KEY_GROUP_GOTO, GEANY_KEYS_GOTO_MATCHINGBRACE);
}

static void on_brace_match_key(G_GNUC_UNUSED guint key_id)
{
	on_brace_match_activate(NULL, NULL);
}

void plugin_init(G_GNUC_UNUSED GeanyData *data)
{
	GtkWidget *extra_select_menu;

	main_locale_init(LOCALEDIR, GETTEXT_PACKAGE);

	main_menu_item = gtk_menu_item_new_with_mnemonic(_("E_xtra selection"));
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), main_menu_item);
	g_signal_connect(main_menu_item, "activate", G_CALLBACK(on_extra_select_activate), NULL);
	extra_select_menu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(main_menu_item), extra_select_menu);

	column_mode_item = gtk_check_menu_item_new_with_mnemonic(_("_Column mode"));
	gtk_container_add(GTK_CONTAINER(extra_select_menu), column_mode_item);
	g_signal_connect(column_mode_item, "toggled", G_CALLBACK(on_column_mode_toggled), NULL);
	keybindings_set_item(plugin_key_group, COLUMN_MODE_KB, on_column_mode_key, 0, 0,
		"column_mode", _("Column mode"), column_mode_item);

	goto_line_item = gtk_menu_item_new_with_mnemonic(_("Select to _Line"));
	gtk_container_add(GTK_CONTAINER(extra_select_menu), goto_line_item);
	g_signal_connect(goto_line_item, "activate", G_CALLBACK(on_goto_line_activate), NULL);
	keybindings_set_item(plugin_key_group, GOTO_LINE_EXTEND_KB, on_goto_line_key, 0, 0,
		"goto_line_extend", _("Select to line"), goto_line_item);

	brace_match_item = gtk_menu_item_new_with_mnemonic(_("Select to Matching _Brace"));
	gtk_container_add(GTK_CONTAINER(extra_select_menu), brace_match_item);
	g_signal_connect(brace_match_item, "activate", G_CALLBACK(on_brace_match_activate),
		NULL);
	keybindings_set_item(plugin_key_group, BRACE_MATCH_EXTEND_KB, on_brace_match_key, 0, 0,
		"brace_match_extend", _("Select to matching brace"), brace_match_item);

	gtk_widget_show_all(main_menu_item);

	plugin_signal_connect(geany_plugin, G_OBJECT(geany->main_widgets->window),
		"key-press-event", FALSE, G_CALLBACK(on_key_press_event), NULL);
}

/* for 0.18 compatibility */
#ifndef foreach_document
#define foreach_document documents_foreach
#endif

void plugin_cleanup(void)
{
	guint i;

	column_mode = FALSE;
	foreach_document (i)
		assign_column_keys(documents[i]->editor->sci);
	gtk_widget_destroy(main_menu_item);
}
