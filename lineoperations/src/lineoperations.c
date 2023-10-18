/*
 *      lineoperations.c - Line operations, remove duplicate lines, empty lines,
 *                         lines with only whitespace, sort lines.
 *
 *      Copyright 2015 Sylvan Mostert <smostert.dev@gmail.com>
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
 *      You should have received a copy of the GNU General Public License along
 *      with this program; if not, write to the Free Software Foundation, Inc.,
 *      51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/


#ifdef HAVE_CONFIG_H
    #include "config.h" /* for the gettext domain */
#endif

#include <geanyplugin.h>
#include "Scintilla.h"
#include "lo_fns.h"
#include "lo_prefs.h"


static GtkWidget *main_menu_item = NULL;


/* represents a selection of lines that will have Operation applied to */
struct lo_lines
{
	gboolean is_selection;
	gint     start_line;
	gint     end_line;
};


/* represents a menu item and key binding */
struct lo_menu_item
{
	const gchar *label;
	const gchar *kb_section_name;
	GCallback cb_activate;
	gpointer cb_data;
};


typedef void (*CB_USER_FUNCTION)(GtkMenuItem *menuitem, gpointer gdata);


/* selects lines in document (based on lo_lines struct parameter) */
static void
select_lines(GeanyEditor *editor, struct lo_lines *sel)
{
	/* set the selection to beginning of first line */
	sci_set_selection_start(editor->sci,
							sci_get_position_from_line(editor->sci, sel->start_line));

	/* set the selection to end of last line */
	sci_set_selection_end(editor->sci,
						  sci_get_line_end_position(editor->sci, sel->end_line) +
							  editor_get_eol_char_len(editor));
}


/* get lo_lines struct 'sel' from document */
static void
get_current_sel_lines(ScintillaObject *sci, struct lo_lines *sel)
{
	gint start_posn     = 0;        /* position of selection start */
	gint end_posn       = 0;        /* position of selection end   */

	/* check for selection */
	if (sci_has_selection(sci))
	{
		/* get the start and end *positions* */
		start_posn = sci_get_selection_start(sci);
		end_posn   = sci_get_selection_end  (sci);

		/* get the *line number* of those positions */
		sel->start_line = scintilla_send_message(sci,
									SCI_LINEFROMPOSITION,
									start_posn, 0);

		sel->end_line   = scintilla_send_message(sci,
									SCI_LINEFROMPOSITION,
									end_posn, 0);

		sel->is_selection = TRUE;
	}
	else
	{
		/* if there is no selection, start at first line */
		sel->start_line = 0;
		/* and end at last one */
		sel->end_line   = (sci_get_line_count(sci) - 1);

		sel->is_selection = FALSE;
	}
}


/* altered from geany/src/editor.c, ensure new line at file end */
static void
ensure_final_newline(GeanyEditor *editor, gint *num_lines, struct lo_lines *sel)
{
	gint end_document   = sci_get_position_from_line(editor->sci, (*num_lines));
	gboolean append_newline = end_document >
					sci_get_position_from_line(editor->sci, ((*num_lines) - 1));

	if (append_newline)
	{
		const gchar *eol = editor_get_eol_char(editor);
		sci_insert_text(editor->sci, end_document, eol);

		/* re-adjust the selection */
		(*num_lines)++;
		sel->end_line++;
	}
}


/* set statusbar with message and select altered lines */
static void
user_indicate(GeanyEditor *editor, gint lines_affected, struct lo_lines *sel)
{
	if (lines_affected < 0)
	{
		ui_set_statusbar(FALSE, _("Operation successful! %d lines removed."),
					-lines_affected);

		/* select lines to indicate to user what lines were altered */
		sel->end_line += lines_affected;

		if (sel->is_selection)
			select_lines(editor, sel);
	}
	else if (lines_affected == 0)
	{
		ui_set_statusbar(FALSE, _("Operation successful! No lines removed."));

		/* select lines to indicate to user what lines were altered */
		if (sel->is_selection)
			select_lines(editor, sel);
	}
	else
	{
		ui_set_statusbar(FALSE, _("Operation successful! %d lines affected."),
					lines_affected);

		/* select lines to indicate to user what lines were altered */
		if (sel->is_selection)
			select_lines(editor, sel);
	}
}


/*
 * Menu action for functions with indirect scintilla manipulation
 * e.g. functions requiring **lines array, num_lines, *new_file
 *
 * Use 'action_sci_manip_item()' if possible, since direction
 * manipulation of Scintilla doc is faster/better.
 * Use this if the line operation cannot be easily done with
 * scintilla functions.
*/
static void
action_indir_manip_item(GtkMenuItem *menuitem, gpointer gdata)
{
	/* function pointer to function to be used */
	gint (*func)(gchar **lines, gint num_lines, gchar *new_file) = gdata;
	GeanyDocument *doc    = document_get_current();
	g_return_if_fail(doc != NULL);

	struct lo_lines sel;
	gint   num_chars      = 0;
	gint   i              = 0;
	gint   lines_affected = 0;

	get_current_sel_lines(doc->editor->sci, &sel);
	gint num_lines = (sel.end_line - sel.start_line) + 1;

	/* if last line within selection ensure that the file ends with newline */
	if ((sel.end_line + 1) == sci_get_line_count(doc->editor->sci))
		ensure_final_newline(doc->editor, &num_lines, &sel);

	/* get num_chars and **lines */
	gchar **lines = g_malloc(sizeof(gchar *) * num_lines);
	for (i = 0; i < num_lines; i++)
	{
		num_chars += (sci_get_line_length(doc->editor->sci,
										(i + sel.start_line)));

		lines[i]   = sci_get_line(doc->editor->sci,
										(i + sel.start_line));
	}

	gchar *new_file  = g_malloc(sizeof(gchar) * (num_chars + 1));
	new_file[0]      = '\0';

	/* select lines that will be replaced with array */
	select_lines(doc->editor, &sel);

	sci_start_undo_action(doc->editor->sci);

	lines_affected = func(lines, num_lines, new_file);


	/* set new document */
	sci_replace_sel(doc->editor->sci, new_file);

	/* select affected lines and set statusbar message */
	user_indicate(doc->editor, lines_affected, &sel);

	sci_end_undo_action(doc->editor->sci);

	/* free used memory */
	for (i = 0; i < num_lines; i++)
		g_free(lines[i]);
	g_free(lines);
	g_free(new_file);
}


/*
 * Menu action for functions with direct scintilla manipulation
 * e.g. no need for **lines array, *new_file...
 *
 * Use this if the line operation can be directly done with
 * scintilla functions.
*/
static void
action_sci_manip_item(GtkMenuItem *menuitem, gpointer gdata)
{
	/* function pointer to gdata -- function to be used */
	gint (*func)(ScintillaObject *, gint, gint) = gdata;
	GeanyDocument *doc = document_get_current();
	g_return_if_fail(doc != NULL);

	struct lo_lines sel;
	get_current_sel_lines(doc->editor->sci, &sel);
	gint lines_affected = 0;

	sci_start_undo_action(doc->editor->sci);

	lines_affected = func(doc->editor->sci, sel.start_line, sel.end_line);


	/* put message in ui_statusbar, and highlight lines that were affected */
	user_indicate(doc->editor, lines_affected, &sel);

	sci_end_undo_action(doc->editor->sci);
}


/* List of menu items, also used for keybindings. */
static struct lo_menu_item menu_items[] = {
	{ N_("Remove Duplicate Lines, _Sorted"), "remove_duplicate_lines_s",
	  G_CALLBACK(action_indir_manip_item), (gpointer) rmdupst },
	{ N_("Remove Duplicate Lines, _Ordered"), "remove_duplicate_lines_o",
	  G_CALLBACK(action_indir_manip_item), (gpointer) rmdupln },
	{ N_("Remove _Unique Lines"), "remove_unique_lines",
	  G_CALLBACK(action_indir_manip_item), (gpointer) rmunqln },
	{ N_("Keep _Unique Lines"), "keep_unique_lines",
	  G_CALLBACK(action_indir_manip_item), (gpointer) kpunqln },
	{ NULL },
	{ N_("Remove _Empty Lines"), "remove_empty_lines",
	  G_CALLBACK(action_sci_manip_item), (gpointer) rmemtyln },
	{ N_("Remove _Whitespace Lines"), "remove_whitespace_lines",
	  G_CALLBACK(action_sci_manip_item), (gpointer) rmwhspln },
	{ NULL },
	{ N_("Remove Every _Nth Line"), "remove_every_nth_line",
	  G_CALLBACK(action_sci_manip_item), (gpointer) rmnthln },
	{ NULL },
	{ N_("Sort Lines _Ascending"), "sort_lines_ascending",
	  G_CALLBACK(action_indir_manip_item), (gpointer) sortlnsasc },
	{ N_("Sort Lines _Descending"), "sort_lines_descending",
	  G_CALLBACK(action_indir_manip_item), (gpointer) sortlndesc }
};


/* Keybinding callback */
static void lo_keybinding_callback(guint key_id)
{
	CB_USER_FUNCTION cb_activate;
	g_return_if_fail(key_id < G_N_ELEMENTS(menu_items));
	cb_activate = (CB_USER_FUNCTION)menu_items[key_id].cb_activate;
	cb_activate(NULL, menu_items[key_id].cb_data);
}


/* Initialization */
static gboolean
lo_init(GeanyPlugin *plugin, G_GNUC_UNUSED gpointer gdata)
{
	GeanyData *geany_data = plugin->geany_data;
	GeanyKeyGroup *key_group;
	GtkWidget *submenu;
	guint i;

	lo_init_prefs(plugin);

	main_menu_item = gtk_menu_item_new_with_mnemonic(_("_Line Operations"));
	gtk_widget_show(main_menu_item);

	submenu = gtk_menu_new();
	gtk_widget_show(submenu);

	for (i = 0; i < G_N_ELEMENTS(menu_items); i++)
	{
		GtkWidget *item;

		if (! menu_items[i].label) /* separator */
			item = gtk_separator_menu_item_new();
		else
		{
			item = gtk_menu_item_new_with_mnemonic(_(menu_items[i].label));
			g_signal_connect(item,
							"activate",
							menu_items[i].cb_activate,
							menu_items[i].cb_data);
			ui_add_document_sensitive(item);
		}

		gtk_widget_show(item);
		gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
	}

	gtk_menu_item_set_submenu(GTK_MENU_ITEM(main_menu_item), submenu);
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu),
									main_menu_item);

	/* Setup keybindings. */
	key_group = plugin_set_key_group
		(plugin, "Line Operations", G_N_ELEMENTS(menu_items), NULL);
	for (i = 0; i < G_N_ELEMENTS(menu_items); i++)
	{
		if (menu_items[i].label != NULL)
		{
			keybindings_set_item(key_group, i,
				lo_keybinding_callback, 0, 0, menu_items[i].kb_section_name,
					_(menu_items[i].label), NULL);
		}
	}

	return TRUE;
}


/* Show help */
static void
lo_help (G_GNUC_UNUSED GeanyPlugin *plugin, G_GNUC_UNUSED gpointer pdata)
{
	utils_open_browser("https://plugins.geany.org/lineoperations.html");
}


static void
lo_cleanup(GeanyPlugin *plugin, gpointer pdata)
{
	gtk_widget_destroy(main_menu_item);
	lo_free_info();
}


G_MODULE_EXPORT
void geany_load_module(GeanyPlugin *plugin)
{
	main_locale_init(LOCALEDIR, GETTEXT_PACKAGE);

	plugin->info->name        = _("Line Operations");
	plugin->info->description = _("Line Operations provides a handful of functions that can be applied to a document or selection such as, removing duplicate lines, removing empty lines, removing lines with only whitespace, and sorting lines.");
	plugin->info->version     = "0.3";
	plugin->info->author      = "Sylvan Mostert <smostert.dev@gmail.com>";

	plugin->funcs->init       = lo_init;
	plugin->funcs->cleanup    = lo_cleanup;
	plugin->funcs->configure  = lo_configure;
	plugin->funcs->help       = lo_help;

	GEANY_PLUGIN_REGISTER(plugin, 225);
}
