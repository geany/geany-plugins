/*
 *  extrasel.c
 *
 *  Copyright 2010 Dimitar Toshkov Zhekov <dimitar.zhekov@gmail.com>
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

PLUGIN_VERSION_CHECK(189)

PLUGIN_SET_INFO(_("Extra Selection"), _("Column mode, select to line / brace / anchor."),
	"0.4", "Dimitar Toshkov Zhekov <dimitar.zhekov@gmail.com>")

/* Keybinding(s) */
enum
{
	COLUMN_MODE_KB,
	GOTO_LINE_EXTEND_KB,
	BRACE_MATCH_EXTEND_KB,
	SET_ANCHOR_KB,
	ANCHOR_EXTEND_KB,
	ANCHOR_RECTEXTEND_KB,
	COUNT_KB
};

PLUGIN_KEY_GROUP(extra_select, COUNT_KB)

static GtkWidget *main_menu_item = NULL;
static GtkCheckMenuItem *column_mode_item;
static GtkWidget *anchor_rect_select_item;
static gpointer *go_to_line1_item = NULL;

static gint column_mode = FALSE;
static gboolean plugin_ignore_callback = FALSE;
static gint select_anchor = 0, select_space = 0;

/* Common functions / macros */

ScintillaObject *scintilla_get_current(void)
{
	GeanyDocument *doc = document_get_current();
	return doc ? doc->editor->sci : NULL;
}

#define SSM(s, m, w, l) scintilla_send_message(s, m, w, l)
#define sci_get_anchor(sci) SSM(sci, SCI_GETANCHOR, 0, 0)
#define sci_set_anchor(sci, anchor) SSM(sci, SCI_SETANCHOR, anchor, 0)
#define sci_get_main_selection(sci) SSM(sci, SCI_GETMAINSELECTION, 0, 0)
#define sci_rectangle_selection(sci) (sci_get_selection_mode(sci) == SC_SEL_RECTANGLE || \
	sci_get_selection_mode(sci) == SC_SEL_THIN)
#define sci_virtual_space(sci, ACTION, OBJECT, space) \
	(sci_rectangle_selection(sci) ? \
	SSM(sci, ACTION##RECTANGULARSELECTION##OBJECT##VIRTUALSPACE, space, 0) : \
	SSM(sci, ACTION##SELECTIONN##OBJECT##VIRTUALSPACE, sci_get_main_selection(sci), space))
#define sci_set_virtual_space(sci, OBJECT, space) \
	do { if (space) sci_virtual_space(sci, SCI_SET, OBJECT, space); } while(0)
#define sci_get_anchor_space(sci) sci_virtual_space(sci, SCI_GET, ANCHOR, 0)
#define sci_set_anchor_space(sci, space) sci_set_virtual_space(sci, ANCHOR, space)
#define sci_get_cursor_space(sci) sci_virtual_space(sci, SCI_GET, CARET, 0)
#define sci_set_cursor_space(sci, space) sci_set_virtual_space(sci, CARET, space)

static void create_selection(ScintillaObject *sci, int anchor, int anchor_space,
	gboolean rectangle)
{
	int cursor = sci_get_current_position(sci);
	int cursor_space = sci_get_cursor_space(sci);

	if (rectangle)
	{
		sci_set_selection_mode(sci, SC_SEL_RECTANGLE);
		sci_set_anchor(sci, anchor);
		/* sci_set_current_position() sets anchor = cursor, bypass */
		scintilla_send_message(sci, SCI_SETCURRENTPOS, cursor, 0);
	}
	else
	{
		sci_set_selection_mode(sci, SC_SEL_STREAM);
		scintilla_send_message(sci, SCI_SETSEL, anchor, cursor);
	}

	sci_set_anchor_space(sci, anchor_space);
	sci_set_cursor_space(sci, cursor_space);
	sci_send_command(sci, SCI_CANCEL);
}

static void convert_selection(ScintillaObject *sci, gboolean rectangle)
{
	if (sci_has_selection(sci))
		create_selection(sci, sci_get_anchor(sci), sci_get_anchor_space(sci), rectangle);
}

static void on_extra_select_activate(G_GNUC_UNUSED GtkMenuItem *menuitem,
	G_GNUC_UNUSED gpointer gdata)
{
	if (column_mode != gtk_check_menu_item_get_active(column_mode_item))
	{
		plugin_ignore_callback = TRUE;
		gtk_check_menu_item_set_active(column_mode_item, column_mode);
		plugin_ignore_callback = FALSE;
		gtk_widget_set_sensitive(anchor_rect_select_item, !column_mode);
	}
}

/* New rectangle selection keys */

typedef struct _command_key
{
	guint key;
	guint keypad;
	int command;
} command_key;

static const command_key command_keys[] =
{
	{ GDK_Up,     GDK_KP_Up,     SCI_PARAUP },
	{ GDK_Down,   GDK_KP_Down,   SCI_PARADOWN },
	{ GDK_Left,   GDK_KP_Left,   SCI_WORDLEFT },
	{ GDK_Right,  GDK_KP_Right,  SCI_WORDRIGHTEND },
	{ GDK_Home,   GDK_KP_Home,   SCI_DOCUMENTSTART },
	{ GDK_End,    GDK_KP_End,    SCI_DOCUMENTEND },
	{ GDK_Prior,  GDK_KP_Prior,  0 },
	{ GDK_Next,   GDK_KP_Next,   0 },
	{ 0, 0, 0 }
};

static void column_mode_command(ScintillaObject *sci, int command)
{
	gboolean convert = !sci_rectangle_selection(sci);
	int anchor;
	int anchor_space;

	if (convert)
	{
		anchor = sci_get_anchor(sci);
		anchor_space = sci_get_anchor_space(sci);
	}
	sci_set_selection_mode(sci, SC_SEL_RECTANGLE);
	sci_send_command(sci, command);
	if (convert)
	{
		sci_set_anchor(sci, anchor);
		sci_set_anchor_space(sci, anchor_space);
	}
	sci_send_command(sci, SCI_CANCEL);
}

static gboolean on_key_press_event(GtkWidget *widget, GdkEventKey *event, G_GNUC_UNUSED
	gpointer user_data)
{
	guint mask = GDK_CONTROL_MASK | GDK_SHIFT_MASK | (column_mode ? 0 : GDK_MOD1_MASK);
	guint state = event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_MOD1_MASK);

	if (state == mask)
	{
		const command_key *ck;

		for (ck = command_keys; ck->command; ck++)
		{
			if (event->keyval == ck->key || event->keyval == ck->keypad)
			{
				ScintillaObject *sci = scintilla_get_current();

				if (sci && gtk_window_get_focus(GTK_WINDOW(widget)) == GTK_WIDGET(sci))
				{
					column_mode_command(sci, ck->command);
					return TRUE;
				}
				break;
			}
		}
	}
	else if (!column_mode && state == GDK_SHIFT_MASK)
	{
		const command_key *ck;

		for (ck = command_keys; ck->key; ck++)
		{
			if (event->keyval == ck->key || event->keyval == ck->keypad)
			{
				ScintillaObject *sci = scintilla_get_current();

				if (sci && sci_has_selection(sci) && sci_rectangle_selection(sci) &&
					gtk_window_get_focus(GTK_WINDOW(widget)) == GTK_WIDGET(sci))
				{
					/* not exactly a bug, yet... */
					convert_selection(sci, FALSE);
				}
				break;
			}
		}
	}

	return FALSE;
}

/* Column mode */

typedef struct _select_key
{
	int key;
	int stream;
	int rectangle;
} select_key;

#define sci_clear_cmdkey(sci, key) scintilla_send_message((sci), SCI_CLEARCMDKEY, (key), 0)
#define sci_assign_cmdkey(sci, key, command) \
	scintilla_send_message((sci), SCI_ASSIGNCMDKEY, (key), (command))

static select_key select_keys[] =
{
	{ SCK_HOME  | (SCMOD_SHIFT << 16),  SCI_NULL,             SCI_NULL },
	{ SCK_UP    | (SCMOD_SHIFT << 16),  SCI_LINEUPEXTEND,     SCI_LINEUPRECTEXTEND },
	{ SCK_DOWN  | (SCMOD_SHIFT << 16),  SCI_LINEDOWNEXTEND,   SCI_LINEDOWNRECTEXTEND },
	{ SCK_LEFT  | (SCMOD_SHIFT << 16),  SCI_CHARLEFTEXTEND,   SCI_CHARLEFTRECTEXTEND  },
	{ SCK_RIGHT | (SCMOD_SHIFT << 16),  SCI_CHARRIGHTEXTEND,  SCI_CHARRIGHTRECTEXTEND },
	{ SCK_END   | (SCMOD_SHIFT << 16),  SCI_LINEENDEXTEND,    SCI_LINEENDRECTEXTEND },
	{ SCK_PRIOR | (SCMOD_SHIFT << 16),  SCI_PAGEUPEXTEND,     SCI_PAGEUPRECTEXTEND },
	{ SCK_NEXT  | (SCMOD_SHIFT << 16),  SCI_PAGEDOWNEXTEND,   SCI_PAGEDOWNRECTEXTEND },
	{ 0, 0, 0 }
};

static void assign_select_keys(ScintillaObject *sci)
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

static void on_column_mode_toggled(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED
	gpointer gdata)
{
	ScintillaObject *sci = scintilla_get_current();

	if (sci && !plugin_ignore_callback)
	{
		column_mode = gtk_check_menu_item_get_active(column_mode_item);
		assign_select_keys(sci);
		g_object_set_data(G_OBJECT(sci), "column_mode", GINT_TO_POINTER(column_mode));
		if (sci_has_selection(sci) && sci_rectangle_selection(sci) != column_mode)
			convert_selection(sci, column_mode);
		gtk_widget_set_sensitive(anchor_rect_select_item, !column_mode);
	}
}

static void on_column_mode_key(G_GNUC_UNUSED guint key_id)
{
	gtk_check_menu_item_set_active(column_mode_item, !column_mode);
}

static void on_document_create(G_GNUC_UNUSED GObject *obj, G_GNUC_UNUSED GeanyDocument *doc,
	G_GNUC_UNUSED gpointer user_data)
{
	column_mode = FALSE;
	select_anchor = select_space = 0;
}

static void on_document_activate(G_GNUC_UNUSED GObject *obj, GeanyDocument *doc,
	G_GNUC_UNUSED gpointer user_data)
{
	ScintillaObject *sci = doc->editor->sci;

	column_mode = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(sci), "column_mode"));
	select_anchor = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(sci), "select_anchor"));
	select_space = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(sci), "select_space"));
}

static void update_home_key(void)
{
	if (geany_data->editor_prefs->smart_home_key)
	{
		select_keys->stream = SCI_VCHOMEEXTEND;
		select_keys->rectangle = SCI_VCHOMERECTEXTEND;
	}
	else
	{
		select_keys->stream = SCI_HOMEEXTEND;
		select_keys->rectangle = SCI_HOMERECTEXTEND;
	}
}

static void on_settings_change(G_GNUC_UNUSED GKeyFile *config)
{
	update_home_key();

	if (column_mode)
	{
		ScintillaObject *sci = scintilla_get_current();

		if (sci)
			assign_select_keys(sci);
	}
}

/* Select to line / matching brace */

static void doit_and_select(guint group_id, guint key_id)
{
	ScintillaObject *sci = scintilla_get_current();

	if (sci)
	{
		int before = sci_get_current_position(sci), after;

		if (key_id != GEANY_KEYS_GOTO_LINE || !geany_data->toolbar_prefs->visible)
			keybindings_send_command(group_id, key_id);
		else if (go_to_line1_item)
			g_signal_emit_by_name(go_to_line1_item, "activate");
		else
		{
			if (geany_data->prefs->beep_on_errors)
				gdk_beep();
			return;
		}

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

/* Set anchor / select to anchor */

static void save_selection(ScintillaObject *sci)
{
	g_object_set_data(G_OBJECT(sci), "select_anchor", GINT_TO_POINTER(select_anchor));
	g_object_set_data(G_OBJECT(sci), "select_space", GINT_TO_POINTER(select_space));
}

static gboolean on_editor_notify(G_GNUC_UNUSED GObject *obj, GeanyEditor *editor,
	SCNotification *nt, G_GNUC_UNUSED gpointer user_data)
{
	if (nt->nmhdr.code == SCN_MODIFIED)
	{
		if (nt->modificationType & SC_MOD_INSERTTEXT)
		{
			if (nt->position < select_anchor)
			{
				select_anchor += nt->length;
				select_space = 0;
				save_selection(editor->sci);
			}
		}
		else if (nt->modificationType & SC_MOD_DELETETEXT)
		{
			if (nt->position < select_anchor)
			{
				if (nt->position + nt->length < select_anchor)
					select_anchor -= nt->length;
				else
					select_anchor = nt->position;
				select_space = 0;
				save_selection(editor->sci);
			}
		}
	}

	return FALSE;
}

static void on_set_anchor_activate(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED
	gpointer gdata)
{
	ScintillaObject *sci = scintilla_get_current();

	if (sci)
	{
		select_anchor = sci_get_current_position(sci);
		select_space = sci_get_cursor_space(sci);
		save_selection(sci);
	}
}

static void on_set_anchor_key(G_GNUC_UNUSED guint key_id)
{
	on_set_anchor_activate(NULL, NULL);
}

static void select_to_anchor(gboolean rectangle)
{
	ScintillaObject *sci = scintilla_get_current();

	if (sci)
		create_selection(sci, select_anchor, select_space, rectangle);
}

static void on_select_to_anchor_activate(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED
	gpointer gdata)
{
	select_to_anchor(column_mode);
}

static void on_select_to_anchor_key(G_GNUC_UNUSED guint key_id)
{
	on_select_to_anchor_activate(NULL, NULL);
}

static void on_select_rectangle_activate(G_GNUC_UNUSED GtkMenuItem *menuitem,
	G_GNUC_UNUSED gpointer gdata)
{
	if (!column_mode)
		select_to_anchor(TRUE);
}

static void on_select_rectangle_key(G_GNUC_UNUSED guint key_id)
{
	on_select_rectangle_activate(NULL, NULL);
}

/* Plugin globals */

PluginCallback plugin_callbacks[] =
{
	{ "document-new", (GCallback) &on_document_create, FALSE, NULL },
	{ "document-open", (GCallback) &on_document_create, FALSE, NULL },
	{ "document-activate", (GCallback) &on_document_activate, FALSE, NULL },
	{ "save-settings", (GCallback) &on_settings_change, FALSE, NULL },
	{ "editor-notify", (GCallback) &on_editor_notify, FALSE, NULL },
	{ NULL, NULL, FALSE, NULL }
};

void plugin_init(G_GNUC_UNUSED GeanyData *data)
{
	GtkContainer *menu;
	GtkWidget *item;

	main_locale_init(LOCALEDIR, GETTEXT_PACKAGE);

	item = gtk_menu_item_new_with_mnemonic(_("E_xtra Selection"));
	main_menu_item = item;
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_extra_select_activate), NULL);
	ui_add_document_sensitive(item);
	menu = GTK_CONTAINER(gtk_menu_new());
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), GTK_WIDGET(menu));

	item = gtk_check_menu_item_new_with_mnemonic(_("_Column Mode"));
	column_mode_item = GTK_CHECK_MENU_ITEM(item);
	gtk_container_add(menu, item);
	g_signal_connect(item, "toggled", G_CALLBACK(on_column_mode_toggled), NULL);
	keybindings_set_item(plugin_key_group, COLUMN_MODE_KB, on_column_mode_key, 0, 0,
		"column_mode", _("Column mode"), item);

	item = gtk_menu_item_new_with_mnemonic(_("Select to _Line"));
	gtk_container_add(menu, item);
	g_signal_connect(item, "activate", G_CALLBACK(on_goto_line_activate), NULL);
	keybindings_set_item(plugin_key_group, GOTO_LINE_EXTEND_KB, on_goto_line_key, 0, 0,
		"goto_line_extend", _("Select to line"), item);

	item = gtk_menu_item_new_with_mnemonic(_("Select to Matching _Brace"));
	gtk_container_add(menu, item);
	g_signal_connect(item, "activate", G_CALLBACK(on_brace_match_activate), NULL);
	keybindings_set_item(plugin_key_group, BRACE_MATCH_EXTEND_KB, on_brace_match_key, 0, 0,
		"brace_match_extend", _("Select to matching brace"), item);

	gtk_container_add(menu, gtk_separator_menu_item_new());

	item = gtk_menu_item_new_with_mnemonic(_("_Set Anchor"));
	gtk_container_add(menu, item);
	g_signal_connect(item, "activate", G_CALLBACK(on_set_anchor_activate), NULL);
	keybindings_set_item(plugin_key_group, SET_ANCHOR_KB, on_set_anchor_key, 0, 0,
		"set_anchor", _("Set anchor"), item);

	item = gtk_menu_item_new_with_mnemonic(_("Select to _Anchor"));
	gtk_container_add(menu, item);
	g_signal_connect(item, "activate", G_CALLBACK(on_select_to_anchor_activate), NULL);
	keybindings_set_item(plugin_key_group, ANCHOR_EXTEND_KB, on_select_to_anchor_key, 0, 0,
		"select_to_anchor", _("Select to anchor"), item);

	item = gtk_menu_item_new_with_mnemonic(_("_Rectangle Select to Anchor"));
	anchor_rect_select_item = item;
	gtk_container_add(menu, item);
	g_signal_connect(item, "activate", G_CALLBACK(on_select_rectangle_activate), NULL);
	keybindings_set_item(plugin_key_group, ANCHOR_RECTEXTEND_KB, on_select_rectangle_key, 0,
		0, "rect_select_to_anchor", _("Rectangle select to anchor"), item);

	gtk_widget_show_all(main_menu_item);

	go_to_line1_item = g_object_get_data((gpointer) geany->main_widgets->window,
		"go_to_line1");

	update_home_key();
	plugin_signal_connect(geany_plugin, G_OBJECT(geany->main_widgets->window),
		"key-press-event", FALSE, G_CALLBACK(on_key_press_event), NULL);
}

void plugin_cleanup(void)
{
	guint i;

	gtk_widget_destroy(main_menu_item);
	column_mode = FALSE;

	foreach_document(i)
	{
		ScintillaObject *sci = documents[i]->editor->sci;

		assign_select_keys(sci);
		g_object_steal_data(G_OBJECT(sci), "column_mode");
		g_object_steal_data(G_OBJECT(sci), "select_anchor");
		g_object_steal_data(G_OBJECT(sci), "select_space");
	}
}
