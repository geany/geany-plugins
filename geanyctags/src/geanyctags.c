/*
 *	  Copyright 2010-2014 Jiri Techet <techet@gmail.com>
 *
 *	  This program is free software; you can redistribute it and/or modify
 *	  it under the terms of the GNU General Public License as published by
 *	  the Free Software Foundation; either version 2 of the License, or
 *	  (at your option) any later version.
 *
 *	  This program is distributed in the hope that it will be useful,
 *	  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	  GNU General Public License for more details.
 *
 *	  You should have received a copy of the GNU General Public License
 *	  along with this program; if not, write to the Free Software
 *	  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <sys/time.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <geanyplugin.h>

#include "readtags.h"

#include <errno.h>
#include <glib/gstdio.h>

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>


/* Pre-GTK 2.24 compatibility */
#ifndef GTK_COMBO_BOX_TEXT
#	define GTK_COMBO_BOX_TEXT GTK_COMBO_BOX
#	define gtk_combo_box_text_new gtk_combo_box_new_text
#	define gtk_combo_box_text_append_text gtk_combo_box_append_text
#endif


PLUGIN_VERSION_CHECK(226)
PLUGIN_SET_TRANSLATABLE_INFO(LOCALEDIR, GETTEXT_PACKAGE,
	"GeanyCtags",
	_("Ctags generation and search plugin for geany projects"),
	VERSION,
	"Jiri Techet <techet@gmail.com>")

GeanyPlugin *geany_plugin;
GeanyData *geany_data;


static GtkWidget *s_context_fdec_item, *s_context_fdef_item, *s_context_sep_item,
	*s_gt_item, *s_sep_item, *s_ft_item;

static struct
{
	GtkWidget *widget;

	GtkWidget *combo;
	GtkWidget *combo_match;
	GtkWidget *case_sensitive;
	GtkWidget *declaration;
} s_ft_dialog = {NULL, NULL, NULL, NULL, NULL};


enum
{
	KB_FIND_TAG,
	KB_GENERATE_TAGS,
	KB_COUNT
};



static void set_widgets_sensitive(gboolean sensitive)
{
	gtk_widget_set_sensitive(GTK_WIDGET(s_gt_item), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(s_ft_item), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(s_context_fdec_item), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(s_context_fdef_item), sensitive);
}

static void on_project_open(G_GNUC_UNUSED GObject * obj, GKeyFile * config, G_GNUC_UNUSED gpointer user_data)
{
	set_widgets_sensitive(TRUE);
}

static void on_project_close(G_GNUC_UNUSED GObject * obj, G_GNUC_UNUSED gpointer user_data)
{
	set_widgets_sensitive(FALSE);
}

static void on_project_save(G_GNUC_UNUSED GObject * obj, GKeyFile * config, G_GNUC_UNUSED gpointer user_data)
{
	set_widgets_sensitive(TRUE);
}

PluginCallback plugin_callbacks[] = {
	{"project-open", (GCallback) & on_project_open, TRUE, NULL},
	{"project-close", (GCallback) & on_project_close, TRUE, NULL},
	{"project-save", (GCallback) & on_project_save, TRUE, NULL},
	{NULL, NULL, FALSE, NULL}
};

void plugin_help (void)
{
	utils_open_browser (DOCDIR "/" PLUGIN "/README");
}

static void spawn_cmd(const gchar *cmd, const gchar *dir)
{
	GError *error = NULL;
	gchar **argv = NULL;
	gchar *working_dir;
	gchar *utf8_working_dir;
	gchar *utf8_cmd_string;
	gchar *out;
	gint exitcode;
	gboolean success;
	GString *output;

#ifndef G_OS_WIN32
	/* run within shell so we can use pipes */
	argv = g_new0(gchar *, 4);
	argv[0] = g_strdup("/bin/sh");
	argv[1] = g_strdup("-c");
	argv[2] = g_strdup(cmd);
	argv[3] = NULL;
#endif

	utf8_cmd_string = utils_get_utf8_from_locale(cmd);
	utf8_working_dir = g_strdup(dir);
	working_dir = utils_get_locale_from_utf8(utf8_working_dir);

	msgwin_clear_tab(MSG_MESSAGE);
	msgwin_switch_tab(MSG_MESSAGE, TRUE);
	msgwin_msg_add(COLOR_BLUE, -1, NULL, _("%s (in directory: %s)"), utf8_cmd_string, utf8_working_dir);
	g_free(utf8_working_dir);
	g_free(utf8_cmd_string);

	output = g_string_new(NULL);
#ifndef G_OS_WIN32
	success = spawn_sync(working_dir, NULL, argv, NULL,
			NULL, NULL, output, &exitcode, &error);
#else
	success = spawn_sync(working_dir, cmd, NULL, NULL,
			NULL, output, NULL, &exitcode, &error);
#endif
	out = g_string_free(output, FALSE);
	if (!success || exitcode != 0)
	{
		if (error != NULL)
		{
			msgwin_msg_add(COLOR_RED, -1, NULL, _("Process execution failed (%s)"), error->message);
			g_error_free(error);
		}
		msgwin_msg_add(COLOR_RED, -1, NULL, "%s", out);
	}
	else
	{
		msgwin_msg_add(COLOR_BLACK, -1, NULL, "%s", out);
	}

	g_strfreev(argv);
	g_free(working_dir);
	g_free(out);
}

static gchar *get_tags_filename(void)
{
	gchar *ret = NULL;

	if (geany_data->app->project)
	{
		ret = utils_remove_ext_from_filename(geany_data->app->project->file_name);
		SETPTR(ret, g_strconcat(ret, ".tags", NULL));
	}
	return ret;
}

static gchar *generate_find_string(GeanyProject *prj)
{
	gchar *ret;

	ret = g_strdup("find -L . -not -path '*/\\.*'");

	if (!EMPTY(prj->file_patterns))
	{
		guint i;

		SETPTR(ret, g_strconcat(ret, " \\( -name \"", prj->file_patterns[0], "\"", NULL));
		for (i = 1; prj->file_patterns[i]; i++)
			SETPTR(ret, g_strconcat(ret, " -o -name \"", prj->file_patterns[i], "\"", NULL));
		SETPTR(ret, g_strconcat(ret, " \\)", NULL));
	}
	return ret;
}


static void
on_generate_tags(GtkMenuItem *menuitem, gpointer user_data)
{
	GeanyProject *prj;

	prj = geany_data->app->project;
	if (prj)
	{
		gchar *cmd;
		gchar *tag_filename;

		tag_filename = get_tags_filename();

#ifndef G_OS_WIN32
		gchar *find_string = generate_find_string(prj);
		cmd = g_strconcat(find_string,
			" | ctags --totals --fields=fKsSt --extra=-fq --c-kinds=+p --sort=foldcase --excmd=number -L - -f ",
			tag_filename, NULL);
		g_free(find_string);
#else
		/* We don't have find and | on windows, generate tags for all files in the project (-R recursively) */
		
		/* Unfortunately, there's a bug in ctags - when run with -R, the first line is
		 * empty, ctags doesn't recognize the tags file as a valid ctags file and
		 * refuses to overwrite it. Therefore, we need to delete the tags file manually. */
		g_unlink(tag_filename);

		cmd = g_strconcat("ctags.exe -R --totals --fields=fKsSt --extra=-fq --c-kinds=+p --sort=foldcase --excmd=number -f ",
			tag_filename, NULL);
#endif

		spawn_cmd(cmd, prj->base_path);

		g_free(cmd);
		g_free(tag_filename);
	}
}

static void show_entry(tagEntry *entry)
{
	const gchar *kind;
	const gchar *signature;
	const gchar *scope;
	const gchar *file;
	const gchar *name;
	gchar *scope_str;
	gchar *kind_str;

	file = entry->file;
	if (!file)
		file = "";

	name = entry->name;
	if (!name)
		name = "";

	signature = tagsField(entry, "signature");
	if (!signature)
		signature = "";

	scope = tagsField(entry, "class");
	if (!scope)
		scope = tagsField(entry, "struct");
	if (!scope)
		scope = tagsField(entry, "union");
	if (!scope)
		scope = tagsField(entry, "enum");

	if (scope)
		scope_str = g_strconcat(scope, "::", NULL);
	else
		scope_str = g_strdup("");

	kind = entry->kind;
	if (kind)
	{
		kind_str = g_strconcat(kind, ":  ", NULL);
		SETPTR(kind_str, g_strdup_printf("%-14s", kind_str));
	}
	else
		kind_str = g_strdup("");

	msgwin_msg_add(COLOR_BLACK, -1, NULL, "%s:%lu:\n    %s%s%s%s", file,
		entry->address.lineNumber, kind_str, scope_str, name, signature);

	g_free(scope_str);
	g_free(kind_str);
}


static gchar *get_selection(void)
{
	gchar *ret = NULL;
	GeanyDocument *doc = document_get_current();
	GeanyEditor *editor;

	if (!doc)
		return NULL;

	editor = doc->editor;

	if (sci_has_selection(editor->sci))
		ret = sci_get_selection_contents(editor->sci);
	else
		ret = editor_get_word_at_pos(editor, -1, GEANY_WORDCHARS);

	return ret;
}

/* TODO: Not possible to do it the way below because some of the API is private 
 * in Geany. This means the cursor has to be placed at the symbol first and
 * then right-clicked (right-clicking without having the cursor at the symbol
 * doesn't work) */
 
/*
static gchar *get_selection()
{
	gchar *ret = NULL;
	GeanyDocument *doc = document_get_current();

	if (!doc)
		return NULL;

	if (!sci_has_selection(doc->editor->sci))
		sci_set_current_position(doc->editor->sci, editor_info.click_pos, FALSE);

	if (sci_has_selection(doc->editor->sci))
		return sci_get_selection_contents(doc->editor->sci);

	gint len = sci_get_selected_text_length(doc->editor->sci);

	ret = g_malloc(len + 1);
	sci_get_selected_text(doc->editor->sci, ret);

	editor_find_current_word(doc->editor, -1,
		editor_info.current_word, GEANY_MAX_WORD_LENGTH, NULL);

	return editor_info.current_word != 0 ? g_strdup(editor_info.current_word) : NULL;
}
*/

typedef enum
{
	MATCH_FULL,
	MATCH_PREFIX,
	MATCH_PATTERN
} MatchType;

static gboolean find_first(tagFile *tf, tagEntry *entry, const gchar *name, MatchType match_type)
{
	gboolean ret;

	if (match_type == MATCH_PATTERN)
		ret = tagsFirst(tf, entry) == TagSuccess;
	else
	{
		int options = TAG_IGNORECASE;

		options |= match_type == MATCH_PREFIX ? TAG_PARTIALMATCH : TAG_FULLMATCH;
		ret = tagsFind(tf, entry, name, options) == TagSuccess;
	}
	return ret;
}

static gboolean find_next(tagFile *tf, tagEntry *entry, MatchType match_type)
{
	gboolean ret;

	if (match_type == MATCH_PATTERN)
		ret = tagsNext(tf, entry) == TagSuccess;
	else
		ret = tagsFindNext(tf, entry) == TagSuccess;
	return ret;
}

static gboolean filter_tag(tagEntry *entry, GPatternSpec *name, gboolean declaration, gboolean case_sensitive)
{
	gboolean filter = TRUE;
	gchar *entry_name;

	if (!EMPTY(entry->kind))
	{
		gboolean is_prototype;

		is_prototype = g_strcmp0(entry->kind, "prototype") == 0;
		filter = (declaration && !is_prototype) || (!declaration && is_prototype);
		if (filter)
			return TRUE;
	}

	if (case_sensitive)
		entry_name = g_strdup(entry->name);
	else
		entry_name = g_utf8_strdown(entry->name, -1);

	filter = !g_pattern_match_string(name, entry_name);

	g_free(entry_name);

	return filter;
}

static void find_tags(const gchar *name, gboolean declaration, gboolean case_sensitive, MatchType match_type)
{
	tagFile *tf;
	GeanyProject *prj;
	gchar *tag_filename = NULL;
	tagEntry entry;
	tagFileInfo info;

	prj = geany_data->app->project;
	if (!prj)
		return;

	msgwin_clear_tab(MSG_MESSAGE);
	msgwin_set_messages_dir(prj->base_path);

	tag_filename = get_tags_filename();
	tf = tagsOpen(tag_filename, &info);

	if (tf)
	{
		if (find_first(tf, &entry, name, match_type))
		{
			GPatternSpec *name_pat;
			gchar *name_case;
			gchar *path = NULL; 
			gint num = 0;

			if (case_sensitive)
				name_case = g_strdup(name);
			else
				name_case = g_utf8_strdown(name, -1);

			SETPTR(name_case, g_strconcat("*", name_case, "*", NULL));
			name_pat = g_pattern_spec_new(name_case);

			if (!filter_tag(&entry, name_pat, declaration, case_sensitive))
			{
				path = g_build_filename(prj->base_path, entry.file, NULL);
				show_entry(&entry);
				num++;
			}
			
			while (find_next(tf, &entry, match_type))
			{
				if (!filter_tag(&entry, name_pat, declaration, case_sensitive))
				{
					if (!path)
						path = g_build_filename(prj->base_path, entry.file, NULL);
					show_entry(&entry);
					num++;
				}
			}
			
			if (num == 1)
			{
				GeanyDocument *doc = document_open_file(path, FALSE, NULL, NULL);
				if (doc != NULL)
				{
					navqueue_goto_line(document_get_current(), doc, entry.address.lineNumber);
					gtk_widget_grab_focus(GTK_WIDGET(doc->editor->sci));
				}
			}

			g_pattern_spec_free(name_pat);
			g_free(name_case);
			g_free(path);
		}

		tagsClose(tf);
	}

	msgwin_switch_tab(MSG_MESSAGE, TRUE);

	g_free(tag_filename);
}

static void on_find_declaration(GtkMenuItem *menuitem, gpointer user_data)
{
	gchar *name;

	name = get_selection();
	if (name)
		find_tags(name, TRUE, TRUE, MATCH_FULL);
	g_free(name);
}

static void on_find_definition(GtkMenuItem *menuitem, gpointer user_data)
{
	gchar *name;

	name = get_selection();
	if (name)
		find_tags(name, FALSE, TRUE, MATCH_FULL);
	g_free(name);
}

static void create_dialog_find_file(void)
{
	GtkWidget *label, *vbox, *ebox, *entry;
	GtkSizeGroup *size_group;

	if (s_ft_dialog.widget)
		return;

	s_ft_dialog.widget = gtk_dialog_new_with_buttons(
		_("Find Tag"), GTK_WINDOW(geany->main_widgets->window),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL);
	gtk_dialog_add_button(GTK_DIALOG(s_ft_dialog.widget), "gtk-find", GTK_RESPONSE_ACCEPT);
	gtk_dialog_set_default_response(GTK_DIALOG(s_ft_dialog.widget), GTK_RESPONSE_ACCEPT);

	vbox = ui_dialog_vbox_new(GTK_DIALOG(s_ft_dialog.widget));
	gtk_box_set_spacing(GTK_BOX(vbox), 9);

	size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	label = gtk_label_new_with_mnemonic(_("_Search for:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_size_group_add_widget(size_group, label);

	s_ft_dialog.combo = gtk_combo_box_text_new_with_entry();
	entry = gtk_bin_get_child(GTK_BIN(s_ft_dialog.combo));

	ui_entry_add_clear_icon(GTK_ENTRY(entry));
	gtk_entry_set_width_chars(GTK_ENTRY(entry), 40);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
	ui_entry_add_clear_icon(GTK_ENTRY(entry));
	gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);

	ebox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(ebox), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(ebox), s_ft_dialog.combo, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), ebox, TRUE, FALSE, 0);

	label = gtk_label_new_with_mnemonic(_("_Match type:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_size_group_add_widget(size_group, label);

	s_ft_dialog.combo_match = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(s_ft_dialog.combo_match), _("exact"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(s_ft_dialog.combo_match), _("prefix"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(s_ft_dialog.combo_match), _("pattern"));
	gtk_combo_box_set_active(GTK_COMBO_BOX(s_ft_dialog.combo_match), 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), s_ft_dialog.combo_match);

	ebox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(ebox), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(ebox), s_ft_dialog.combo_match, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), ebox, TRUE, FALSE, 0);

	s_ft_dialog.case_sensitive = gtk_check_button_new_with_mnemonic(_("C_ase sensitive"));
	gtk_button_set_focus_on_click(GTK_BUTTON(s_ft_dialog.case_sensitive), FALSE);

	s_ft_dialog.declaration = gtk_check_button_new_with_mnemonic(_("_Declaration"));
	gtk_button_set_focus_on_click(GTK_BUTTON(s_ft_dialog.declaration), FALSE);

	g_object_unref(G_OBJECT(size_group));	/* auto destroy the size group */

	gtk_container_add(GTK_CONTAINER(vbox), s_ft_dialog.case_sensitive);
	gtk_container_add(GTK_CONTAINER(vbox), s_ft_dialog.declaration);
	gtk_widget_show_all(vbox);
}

static void on_find_tag(GtkMenuItem *menuitem, gpointer user_data)
{
	gchar *selection;
	GtkWidget *entry;

	create_dialog_find_file();

	entry = gtk_bin_get_child(GTK_BIN(s_ft_dialog.combo));

	selection = get_selection();
	if (selection)
		gtk_entry_set_text(GTK_ENTRY(entry), selection);
	g_free(selection);

	gtk_widget_grab_focus(entry);

	if (gtk_dialog_run(GTK_DIALOG(s_ft_dialog.widget)) == GTK_RESPONSE_ACCEPT)
	{
		const gchar *name;
		gboolean case_sensitive, declaration;
		MatchType match_type;

		name = gtk_entry_get_text(GTK_ENTRY(entry));
		case_sensitive = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(s_ft_dialog.case_sensitive));
		declaration = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(s_ft_dialog.declaration));
		match_type = gtk_combo_box_get_active(GTK_COMBO_BOX(s_ft_dialog.combo_match));

		ui_combo_box_add_to_history(GTK_COMBO_BOX_TEXT(s_ft_dialog.combo), name, 0);

		find_tags(name, declaration, case_sensitive, match_type);
	}

	gtk_widget_hide(s_ft_dialog.widget);
}

static gboolean kb_callback(guint key_id)
{
	switch (key_id)
	{
		case KB_FIND_TAG:
			on_find_tag(NULL, NULL);
			return TRUE;
		case KB_GENERATE_TAGS:
			on_generate_tags(NULL, NULL);
			return TRUE;
	}
	return FALSE;
}

void plugin_init(G_GNUC_UNUSED GeanyData * data)
{
	GeanyKeyGroup *key_group = plugin_set_key_group(geany_plugin, "GeanyCtags", KB_COUNT, kb_callback);
	
	s_context_sep_item = gtk_separator_menu_item_new();
	gtk_widget_show(s_context_sep_item);
	gtk_menu_shell_prepend(GTK_MENU_SHELL(geany->main_widgets->editor_menu), s_context_sep_item);

	s_context_fdec_item = gtk_menu_item_new_with_mnemonic(_("Find Tag Declaration (geanyctags)"));
	gtk_widget_show(s_context_fdec_item);
	gtk_menu_shell_prepend(GTK_MENU_SHELL(geany->main_widgets->editor_menu), s_context_fdec_item);
	g_signal_connect((gpointer) s_context_fdec_item, "activate", G_CALLBACK(on_find_declaration), NULL);

	s_context_fdef_item = gtk_menu_item_new_with_mnemonic(_("Find Tag Definition (geanyctags)"));
	gtk_widget_show(s_context_fdef_item);
	gtk_menu_shell_prepend(GTK_MENU_SHELL(geany->main_widgets->editor_menu), s_context_fdef_item);
	g_signal_connect((gpointer) s_context_fdef_item, "activate", G_CALLBACK(on_find_definition), NULL);

	s_sep_item = gtk_separator_menu_item_new();
	gtk_widget_show(s_sep_item);
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->project_menu), s_sep_item);

	s_gt_item = gtk_menu_item_new_with_mnemonic(_("Generate tags"));
	gtk_widget_show(s_gt_item);
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->project_menu), s_gt_item);
	g_signal_connect((gpointer) s_gt_item, "activate", G_CALLBACK(on_generate_tags), NULL);
	keybindings_set_item(key_group, KB_GENERATE_TAGS, NULL,
		0, 0, "generate_tags", _("Generate tags"), s_gt_item);

	s_ft_item = gtk_menu_item_new_with_mnemonic(_("Find tag..."));
	gtk_widget_show(s_ft_item);
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->project_menu), s_ft_item);
	g_signal_connect((gpointer) s_ft_item, "activate", G_CALLBACK(on_find_tag), NULL);
	keybindings_set_item(key_group, KB_FIND_TAG, NULL,
		0, 0, "find_tag", _("Find tag"), s_ft_item);

	set_widgets_sensitive(geany_data->app->project != NULL);
}

void plugin_cleanup(void)
{
	gtk_widget_destroy(s_context_fdec_item);
	gtk_widget_destroy(s_context_fdef_item);
	gtk_widget_destroy(s_context_sep_item);

	gtk_widget_destroy(s_ft_item);
	gtk_widget_destroy(s_gt_item);
	gtk_widget_destroy(s_sep_item);

	if (s_ft_dialog.widget)
		gtk_widget_destroy(s_ft_dialog.widget);
	s_ft_dialog.widget = NULL;
}
