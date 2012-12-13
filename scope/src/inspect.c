/*
 *  inspect.c
 *
 *  Copyright 2012 Dimitar Toshkov Zhekov <dimitar.zhekov@gmail.com>
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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

enum
{
	INSPECT_VAR1,
	INSPECT_DISPLAY,
	INSPECT_VALUE,
	INSPECT_HB_MODE,
	INSPECT_SCID,
	INSPECT_EXPR,
	INSPECT_NAME,
	INSPECT_FRAME,
	INSPECT_RUN_APPLY,
	INSPECT_START,
	INSPECT_COUNT,
	INSPECT_EXPAND,
	INSPECT_NUMCHILD,
	INSPECT_FORMAT,
	INSPECT_PATH_EXPR
};

enum
{
	FORMAT_NATURAL,
	FORMAT_DECIMAL,
	FORMAT_HEX,
	FORMAT_OCTAL,
	FORMAT_BINARY,
	FORMAT_COUNT
};

static GtkTreeView *tree;
static GtkTreeStore *store;
static GtkTreeModel *model;
static GtkTreeSelection *selection;
static gint scid_gen = 0;

static void remove_children(GtkTreeIter *parent)
{
	GtkTreeIter iter;
	gboolean valid = gtk_tree_model_iter_children(model, &iter, parent);

	while (valid)
		valid = gtk_tree_store_remove(store, &iter);
}

static void append_stub(GtkTreeIter *parent, const gchar *text, gboolean expand)
{
	GtkTreeIter iter;
	gtk_tree_store_append(store, &iter, parent);
	gtk_tree_store_set(store, &iter, INSPECT_EXPR, text, INSPECT_EXPAND, expand, -1);
}

#define append_ellipsis(parent, expand) append_stub((parent), _("..."), (expand))

static gboolean inspect_find_recursive(GtkTreeIter *iter, gint i, const char *key)
{
	do
	{
		gboolean flag = !key;

		if (flag)
		{
			gint scid;

			gtk_tree_model_get(model, iter, INSPECT_SCID, &scid, -1);
			if (scid == i)
				return TRUE;
		}
		else
		{
			const char *var1;
			size_t len;

			gtk_tree_model_get(model, iter, INSPECT_VAR1, &var1, -1);
			len = var1 ? strlen(var1) : 0;

			if (var1 && !strncmp(key, var1, len))
			{
				if (key[len] == '\0')
					return TRUE;

				flag = key[len] == '.' && key[len + 1];
			}
		}

		if (flag)
		{
			GtkTreeIter child;

			if (gtk_tree_model_iter_children(model, &child, iter) &&
				inspect_find_recursive(&child, i, key))
			{
				*iter = child;
				return TRUE;
			}
		}
	} while (gtk_tree_model_iter_next(model, iter));

	return FALSE;
}

static gboolean inspect_find(GtkTreeIter *iter, gboolean string, const char *key)
{
	if (gtk_tree_model_get_iter_first(model, iter) &&
		inspect_find_recursive(iter, atoi(key), string ? key : NULL))
	{
		return TRUE;
	}

	if (!string)
		dc_error("%s: i_scid not found", key);

	return FALSE;
}

static gint inspect_get_scid(GtkTreeIter *iter)
{
	gint scid;

	gtk_tree_model_get(model, iter, INSPECT_SCID, &scid, -1);
	if (!scid)
		gtk_tree_store_set(store, iter, INSPECT_SCID, scid = ++scid_gen, -1);

	return scid;
}

static void inspect_expand(GtkTreeIter *iter)
{
	const char *var1;
	gint scid, start, count, numchild;
	gchar *s;

	scid = inspect_get_scid(iter);
	gtk_tree_model_get(model, iter, INSPECT_VAR1, &var1, INSPECT_START, &start,
		INSPECT_COUNT, &count, INSPECT_NUMCHILD, &numchild, -1);
	s = g_strdup_printf("%d", start);
	debug_send_format(N, "07%c%d%d-var-list-children 1 %s %d %d", '0' + (int) strlen(s) - 1,
		start, scid, var1, start, count ? start + count : numchild);
	g_free(s);
}

static void on_jump_to_menu_item_activate(GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer gdata)
{
	GtkTreeIter iter;
	const gchar *expr = gtk_menu_item_get_label(menuitem);

	if (model_find(model, &iter, INSPECT_EXPR, expr))
		gtk_tree_selection_select_iter(selection, &iter);
}

static GtkWidget *jump_to_item;
static GtkContainer *jump_to_menu;
static const gchar *jump_to_expr;

static void on_inspect_row_inserted(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter,
	G_GNUC_UNUSED gpointer gdata)
{
	if (gtk_tree_path_get_depth(path) == 1)
	{
		GtkWidget *item;
		const gint *index = gtk_tree_path_get_indices(path);

		gtk_tree_model_get(model, iter, INSPECT_EXPR, &jump_to_expr, -1);
		item = gtk_menu_item_new_with_label(jump_to_expr ? jump_to_expr : "");
		gtk_widget_show(item);
		gtk_menu_shell_insert(GTK_MENU_SHELL(jump_to_menu), item, *index);
		g_signal_connect(item, "activate", G_CALLBACK(on_jump_to_menu_item_activate), NULL);
	}
}

static void on_inspect_row_changed(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter,
	G_GNUC_UNUSED gpointer gdata)
{
	if (!jump_to_expr && gtk_tree_path_get_depth(path) == 1)
	{
		const gint *index = gtk_tree_path_get_indices(path);
		GList *list = gtk_container_get_children(jump_to_menu);
		gpointer item = g_list_nth_data(list, *index);

		gtk_tree_model_get(model, iter, INSPECT_EXPR, &jump_to_expr, -1);
		if (jump_to_expr)
			gtk_menu_item_set_label(GTK_MENU_ITEM(item), jump_to_expr);
		g_list_free(list);
	}
}

static void on_inspect_row_deleted(GtkTreeModel *model, GtkTreePath *path,
	G_GNUC_UNUSED gpointer gdata)
{
	if (gtk_tree_path_get_depth(path) == 1)
	{
		const gint *index = gtk_tree_path_get_indices(path);
		GList *list = gtk_container_get_children(jump_to_menu);
		gpointer item = g_list_nth_data(list, *index);
		GtkTreeIter iter;

		gtk_container_remove(jump_to_menu, GTK_WIDGET(item));
		if (!gtk_tree_model_get_iter_first(model, &iter))
			gtk_widget_set_sensitive(jump_to_item, FALSE);
		g_list_free(list);
	}
}

static const MenuItem *apply_item;

static void inspect_redisplay(GtkTreeIter *iter, ParseVariable *var)
{
	gtk_tree_model_get(model, iter, INSPECT_HB_MODE, &var->hb_mode, -1);
	g_free(var->display);
	var->display = utils_get_display_from_7bit(var->value, var->hb_mode);
}

static gint inspect_variable_store(GtkTreeIter *iter, const ParseVariable *var)
{
	gint format;
	gboolean expand;

	gtk_tree_model_get(model, iter, INSPECT_EXPAND, &expand, INSPECT_FORMAT, &format, -1);
	gtk_tree_store_set(store, iter, INSPECT_VAR1, var->name, INSPECT_DISPLAY,
		var->display ? var->display : "??", INSPECT_VALUE, var->value, INSPECT_NUMCHILD,
		var->numchild, -1);

	if (var->numchild)
	{
		append_ellipsis(iter, TRUE);
		if (expand)
			inspect_expand(iter);
	}

	return format;
}

static const char *const inspect_formats[FORMAT_COUNT] = { "natural", "decimal", "hex",
	"octal", "binary" };

void on_inspect_variable(GArray *nodes)
{
	GtkTreeIter iter;
	const char *token = parse_grab_token(nodes);

	iff (model_find(model, &iter, INSPECT_SCID, token), "%s: no vid", token)
	{
		ParseVariable var;
		gint format;

		parse_variable(nodes, &var, "numchild");
		inspect_redisplay(&iter, &var);
		remove_children(&iter);

		if ((format = inspect_variable_store(&iter, &var)) != FORMAT_NATURAL)
		{
			debug_send_format(N, "07%s-var-set-format %s %s", token, var.name,
				inspect_formats[format]);
		}

		if (gtk_tree_selection_iter_is_selected(selection, &iter))
			menu_item_set_active(apply_item, TRUE);

		parse_variable_free(&var);
	}
}

void on_inspect_format(GArray *nodes)
{
	const char *s = parse_lead_value(nodes);
	const char *value = parse_find_value(nodes, "value");
	gint format;

	for (format = FORMAT_NATURAL; format < FORMAT_COUNT; format++)
		if (!strcmp(inspect_formats[format], s))
			break;

	iff (value && format < FORMAT_COUNT, "no value or bad format")
	{
		const char *token = parse_grab_token(nodes);
		GtkTreeIter iter;

		if (inspect_find(&iter, FALSE, token))
		{
			gchar *display;
			gint hb_mode;

			gtk_tree_model_get(model, &iter, INSPECT_HB_MODE, &hb_mode, -1);
			display = utils_get_display_from_7bit(value, hb_mode);
			gtk_tree_store_set(store, &iter, INSPECT_DISPLAY, display, INSPECT_VALUE,
				&value, INSPECT_FORMAT, format, -1);
			g_free(display);
		}
	}
}

static void inspect_node_append(const ParseNode *node, GtkTreeIter *parent)
{
	GArray *nodes = (GArray *) node->value;
	ParseVariable var;

	if (node->type == PT_VALUE || !parse_variable(nodes, &var, "numchild"))
		append_stub(parent, _("invalid data"), FALSE);
	else
	{
		GtkTreeIter iter;

		gtk_tree_store_append(store, &iter, parent);
		inspect_variable_store(&iter, &var);
		gtk_tree_store_set(store, &iter, INSPECT_EXPR, var.expr ? var.expr : var.name,
			INSPECT_HB_MODE, var.hb_mode, INSPECT_FORMAT, FORMAT_NATURAL, -1);
		parse_variable_free(&var);
	}
}

void on_inspect_children(GArray *nodes)
{
	char *token = (char *) parse_grab_token(nodes);
	size_t size = *token - '0' + 2;

	iff (strlen(token) >= size + 1, "bad token")
	{
		GtkTreeIter iter;

		if (inspect_find(&iter, FALSE, token + size))
		{
			gint from, numchild, end;
			const char *var1;
			GtkTreePath *path = gtk_tree_model_get_path(model, &iter);

			token[size] = '\0';
			from = atoi(token + 1);
			remove_children(&iter);

			if ((nodes = parse_find_array(nodes, "children")) == NULL)
				append_stub(&iter, _("no children in range"), FALSE);
			else
			{
				if (from)
					append_ellipsis(&iter, FALSE);

				gtk_tree_model_get(model, &iter, INSPECT_VAR1, &var1,
					INSPECT_NUMCHILD, &numchild, -1);

				array_foreach(nodes, (GFunc) inspect_node_append, &iter);
				end = from + nodes->len;

				if (nodes->len && (from || end < numchild))
				{
					debug_send_format(N, "04-var-set-update-range %s %d %d", var1,
						from, end);
				}

				if (nodes->len ? end < numchild : !from)
					append_ellipsis(&iter, FALSE);
			}

			gtk_tree_view_expand_row(tree, path, FALSE);
			gtk_tree_path_free(path);
		}
	}
}

static void inspect_iter_clear(GtkTreeIter *iter, G_GNUC_UNUSED gpointer gdata)
{
	remove_children(iter);
	gtk_tree_store_set(store, iter, INSPECT_DISPLAY, NULL, INSPECT_VALUE, NULL,
		INSPECT_VAR1, NULL, INSPECT_NUMCHILD, 0, INSPECT_PATH_EXPR, NULL, -1);

	if (gtk_tree_selection_iter_is_selected(selection, iter))
		menu_item_set_active(apply_item, FALSE);
}

void on_inspect_ndeleted(GArray *nodes)
{
	const char *token = parse_grab_token(nodes);

	iff (*token <= '1', "%s: invalid i_oper", token)
	{
		GtkTreeIter iter;
		
		if (inspect_find(&iter, FALSE, token + 1))
		{
			if (*token == '0')
				inspect_iter_clear(&iter, NULL);
			else
				gtk_tree_store_remove(store, &iter);
		}
	}
}

void on_inspect_value(GArray *nodes)
{
	const char *token = parse_grab_token(nodes);
	GtkTreeIter iter;

	if (inspect_find(&iter, FALSE, token))
	{
		const char *value = parse_lead_value(nodes);
		gint hb_mode;
		gchar *display;

		gtk_tree_model_get(model, &iter, INSPECT_HB_MODE, &hb_mode, -1);
		display = utils_get_display_from_7bit(value, hb_mode);
		gtk_tree_store_set(store, &iter, INSPECT_DISPLAY, display, INSPECT_VALUE, value,
			-1);
		g_free(display);
	}
}

void on_inspect_path_expr(GArray *nodes)
{
	const char *token = parse_grab_token(nodes);
	GtkTreeIter iter;

	if (inspect_find(&iter, FALSE, token))
		gtk_tree_store_set(store, &iter, INSPECT_PATH_EXPR, parse_lead_value(nodes), -1);
}

static void inspect_node_change(const ParseNode *node, G_GNUC_UNUSED gpointer gdata)
{
	iff (node->type == PT_ARRAY, "changelist: contains value")
	{
		GArray *nodes = (GArray *) node->value;
		ParseVariable var;
		GtkTreeIter iter;

		if (parse_variable(nodes, &var, "new_numchild") &&
			inspect_find(&iter, TRUE, var.name))
		{
			const char *in_scope = parse_find_value(nodes, "in_scope");

			if (!g_strcmp0(in_scope, "false"))
			{
				gtk_tree_store_set(store, &iter, INSPECT_DISPLAY, _("out of scope"),
					INSPECT_VALUE, NULL, -1);
			}
			else if (!g_strcmp0(in_scope, "invalid"))
			{
				debug_send_format(N, "070%d-var-delete %s", inspect_get_scid(&iter),
					var.name);
			}
			else
			{
				inspect_redisplay(&iter, &var);

				if (var.children)
				{
					remove_children(&iter);
					inspect_variable_store(&iter, &var);
				}
				else
				{
					gtk_tree_store_set(store, &iter, INSPECT_DISPLAY,
						var.display ? var.display : "??", INSPECT_VALUE,
						var.value, -1);
				}
			}
		}

		parse_variable_free(&var);
	}
}

void on_inspect_changelist(GArray *nodes)
{
	array_foreach(parse_lead_array(nodes), (GFunc) inspect_node_change, NULL);
}

static void inspect_apply(GtkTreeIter *iter)
{
	gint scid;
	const gchar *expr;
	const char *name, *frame;
	char *locale;

	gtk_tree_model_get(model, iter, INSPECT_EXPR, &expr, INSPECT_SCID, &scid,
		INSPECT_NAME, &name, INSPECT_FRAME, &frame, -1);
	locale = utils_get_locale_from_utf8(expr);
	debug_send_format(F, "07%d-var-create %s %s %s", scid, name, frame, locale);
	g_free(locale);
}

void on_inspect_signal(const char *name)
{
	GtkTreeIter iter;

	iff (isalpha(*name), "%s: invalid var name", name)
	{
		iff (model_find(model, &iter, INSPECT_NAME, name), "%s: var not found", name)
		{
			const char *var1;

			gtk_tree_model_get(model, &iter, INSPECT_VAR1, &var1, -1);

			iff (!var1, "%s: already applied", name)
				inspect_apply(&iter);
		}
	}
}

void inspects_clear(void)
{
	model_foreach(model, (GFunc) inspect_iter_clear, NULL);
}

gboolean inspects_update(void)
{
	debug_send_command(N, "04-var-update 1 *");
	return TRUE;
}

static GtkNotebook *geany_sidebar;
static GtkWidget *inspect_page;

gboolean inspects_current(void)
{
	gint page_num = gtk_notebook_get_current_page(geany_sidebar);
	return gtk_notebook_get_nth_page(geany_sidebar, page_num) == inspect_page;
}

static void inspect_iter_apply(GtkTreeIter *iter, G_GNUC_UNUSED gpointer gdata)
{
	const char *frame;
	gboolean run_apply;

	gtk_tree_model_get(model, iter, INSPECT_FRAME, &frame, INSPECT_RUN_APPLY, &run_apply,
		-1);

	if (run_apply && !isdigit(*frame))
		inspect_apply(iter);
}

void inspects_apply(void)
{
	model_foreach(model, (GFunc) inspect_iter_apply, NULL);
}

static gboolean inspect_name_valid(const char *name)
{
	return !strcmp(name, "-") || isalpha(*name);
}

static gboolean inspect_frame_valid(const char *frame)
{
	char *end;
	strtol(frame, &end, 0);
	return !strcmp(frame, "*") || !strcmp(frame, "@") || (end > frame && *end == '\0');
}

static GtkWidget *inspect_dialog;
static GtkEntry *inspect_expr;
static GtkEntry *inspect_name;
static GtkEntry *inspect_frame;
static GtkToggleButton *inspect_run_apply;
static GtkWidget *inspect_ok;

static void on_inspect_entry_changed(G_GNUC_UNUSED GtkEditable *editable,
	G_GNUC_UNUSED gpointer gdata)
{
	const char *frame = gtk_entry_get_text(inspect_frame);
	const char *expr = gtk_entry_get_text(inspect_expr);

	gtk_widget_set_sensitive(GTK_WIDGET(inspect_run_apply), !isdigit(*frame));
	gtk_widget_set_sensitive(inspect_ok,
		inspect_name_valid(gtk_entry_get_text(inspect_name)) &&
		inspect_frame_valid(frame) && *utils_skip_spaces(expr));
}

static void on_inspect_ok_button_clicked(G_GNUC_UNUSED GtkButton *button,
	G_GNUC_UNUSED gpointer gdata)
{
	GtkTreeIter iter;
	const char *name = gtk_entry_get_text(inspect_name);

	if ((strcmp(name, "-") && model_find(model, &iter, INSPECT_NAME, name)) ||
		inspect_find(&iter, TRUE, name))
	{
		show_error(_("Duplicate inspect variable name."));
	}
	else
		gtk_dialog_response(GTK_DIALOG(inspect_dialog), GTK_RESPONSE_ACCEPT);
}

static void inspect_dialog_store(GtkTreeIter *iter)
{
	const gchar *expr = gtk_entry_get_text(inspect_expr);

	gtk_tree_store_set(store, iter, INSPECT_EXPR, expr, INSPECT_PATH_EXPR, expr,
		INSPECT_NAME, gtk_entry_get_text(inspect_name),
		INSPECT_FRAME, gtk_entry_get_text(inspect_frame),
		INSPECT_RUN_APPLY, gtk_toggle_button_get_active(inspect_run_apply), -1);
}

void inspect_add(const gchar *text)
{
	gtk_entry_set_text(inspect_expr, text ? text : "");
	gtk_entry_set_text(inspect_name, "-");
	gtk_toggle_button_set_active(inspect_run_apply, FALSE);
	on_inspect_entry_changed(NULL, NULL);
	gtk_widget_grab_focus(GTK_WIDGET(inspect_expr));

	if (gtk_dialog_run(GTK_DIALOG(inspect_dialog)) == GTK_RESPONSE_ACCEPT)
	{
		GtkTreeIter iter;
		const gchar *expr = gtk_entry_get_text(inspect_expr);
		const ParseMode *pm = parse_mode_find(expr);

		gtk_tree_store_append(store, &iter, NULL);
		if (debug_state() != DS_INACTIVE)
			gtk_widget_set_sensitive(jump_to_item, TRUE);
		inspect_dialog_store(&iter);
		gtk_tree_store_set(store, &iter, INSPECT_HB_MODE, pm->hb_mode, INSPECT_SCID,
			++scid_gen, INSPECT_FORMAT, FORMAT_NATURAL, INSPECT_COUNT,
			option_inspect_count, INSPECT_EXPAND, option_inspect_expand, -1);
		gtk_tree_selection_select_iter(selection, &iter);

		if (debug_state() & DS_DEBUG)
			inspect_apply(&iter);
	}
}

static void on_inspect_display_edited(G_GNUC_UNUSED GtkCellRendererText *renderer,
	gchar *path_str, gchar *new_text, G_GNUC_UNUSED gpointer gdata)
{
	if (validate_column(new_text, TRUE))
	{
		if (debug_state() & DS_SENDABLE)
		{
			GtkTreeIter iter;
			char *format;

			gtk_tree_model_get_iter_from_string(model, &iter, path_str);
			format = g_strdup_printf("07%d%s", inspect_get_scid(&iter),
				"-var-assign %s %s");
			view_display_edited(model, &iter, new_text, format);
			g_free(format);
		}
		else
			plugin_beep();
	}
}

static const TreeCell inspect_cells[] =
{
	{ "inspect_display", G_CALLBACK(on_inspect_display_edited) },
	{ NULL, NULL }
};

static GObject *inspect_display;

void inspects_update_state(DebugState state)
{
	static gboolean last_active = FALSE;
	gboolean active = state != DS_INACTIVE;
	GtkTreeIter iter;
	const char *var1 = NULL;
	gint numchild = 0;

	if ((state & DS_SENDABLE) && gtk_tree_selection_get_selected(selection, NULL, &iter))
	{
		gtk_tree_model_get(model, &iter, INSPECT_VAR1, &var1, INSPECT_NUMCHILD, &numchild,
			-1);
	}
	g_object_set(inspect_display, "editable", var1 && !numchild, NULL);

	if (active != last_active)
	{
		gtk_widget_set_sensitive(jump_to_item, active &&
			gtk_tree_model_get_iter_first(model, &iter));
		last_active = active;
	}
}

void inspects_delete_all(void)
{
	gtk_tree_store_clear(store);
	scid_gen = 0;
}

static gboolean inspect_load(GKeyFile *config, const char *section)
{
	char *name = utils_key_file_get_string(config, section, "name");
	gchar *expr = utils_key_file_get_string(config, section, "expr");
	gint hb_mode = g_key_file_get_integer(config, section, "hbit", NULL);
	char *frame = utils_key_file_get_string(config, section, "frame");
	gboolean run_apply = g_key_file_get_boolean(config, section, "run_apply", NULL);
	gint start = g_key_file_get_integer(config, section, "start", NULL);
	gint count = g_key_file_get_integer(config, section, "count", NULL);
	gboolean expand = g_key_file_get_boolean(config, section, "expand", NULL);
	gint format = g_key_file_get_integer(config, section, "format", NULL);
	gboolean valid = FALSE;

	if (name && inspect_name_valid(name) && expr && (unsigned) hb_mode < HB_COUNT &&
		frame && inspect_frame_valid(frame) && (unsigned) start <= EXPAND_MAX &&
		(unsigned) count <= EXPAND_MAX && (unsigned) format < FORMAT_COUNT)
	{
		GtkTreeIter iter;

		gtk_tree_store_append(store, &iter, NULL);
		gtk_tree_store_set(store, &iter, INSPECT_EXPR, expr, INSPECT_PATH_EXPR, expr,
			INSPECT_HB_MODE, hb_mode, INSPECT_SCID, ++scid_gen, INSPECT_NAME, name,
			INSPECT_FRAME, frame, INSPECT_RUN_APPLY, run_apply, INSPECT_START, start,
			INSPECT_COUNT, count, INSPECT_EXPAND, expand, INSPECT_FORMAT, format, -1);
		valid = TRUE;
	}

	g_free(frame);
	g_free(expr);
	g_free(name);
	return valid;
}

void inspects_load(GKeyFile *config)
{
	inspects_delete_all();
	utils_load(config, "inspect", inspect_load);
}

static gboolean inspect_save(GKeyFile *config, const char *section, GtkTreeIter *iter)
{
	gint hb_mode, start, count, format;
	const char *name, *frame;
	const gchar *expr;
	gboolean run_apply, expand;

	gtk_tree_model_get(model, iter, INSPECT_EXPR, &expr, INSPECT_HB_MODE, &hb_mode,
		INSPECT_NAME, &name, INSPECT_FRAME, &frame, INSPECT_RUN_APPLY, &run_apply,
		INSPECT_START, &start, INSPECT_COUNT, &count, INSPECT_EXPAND, &expand,
		INSPECT_FORMAT, &format, -1);
	g_key_file_set_string(config, section, "name", name);
	g_key_file_set_string(config, section, "expr", expr);
	g_key_file_set_integer(config, section, "hbit", hb_mode);
	g_key_file_set_string(config, section, "frame", frame);
	g_key_file_set_boolean(config, section, "run_apply", run_apply);
	g_key_file_set_integer(config, section, "start", start);
	g_key_file_set_integer(config, section, "count", count);
	g_key_file_set_boolean(config, section, "expand", expand);
	g_key_file_set_integer(config, section, "format", format);
	return TRUE;
}

void inspects_save(GKeyFile *config)
{
	model_save(model, config, "inspect", inspect_save);
}

static void on_inspect_refresh(G_GNUC_UNUSED const MenuItem *menu_item)
{
	debug_send_command(N, "-var-update 1 *");
}

static void on_inspect_add(G_GNUC_UNUSED const MenuItem *menu_item)
{
	GtkTreeIter iter;
	const gchar *expr = NULL;

	if (gtk_tree_selection_get_selected(selection, NULL, &iter))
		gtk_tree_model_get(model, &iter, INSPECT_PATH_EXPR, &expr, -1);

	inspect_add(expr);
}

static void on_inspect_edit(G_GNUC_UNUSED const MenuItem *menu_item)
{
	GtkTreeIter iter;
	const gchar *expr;
	char *name;
	const char *frame;
	gboolean run_apply;

	gtk_tree_selection_get_selected(selection, NULL, &iter);
	gtk_tree_model_get(model, &iter, INSPECT_EXPR, &expr, INSPECT_NAME, &name,
		INSPECT_FRAME, &frame, INSPECT_RUN_APPLY, &run_apply, -1);
	name = strdup(name);
	gtk_tree_store_set(store, &iter, INSPECT_NAME, "-", -1);  /* for duplicate name check */

	gtk_entry_set_text(inspect_expr, expr);
	gtk_entry_set_text(inspect_name, name);
	gtk_entry_set_text(inspect_frame, frame);
	gtk_toggle_button_set_active(inspect_run_apply, run_apply);
	on_inspect_entry_changed(NULL, NULL);

	if (gtk_dialog_run(GTK_DIALOG(inspect_dialog)) == GTK_RESPONSE_ACCEPT)
	{
		jump_to_expr = NULL;
		inspect_dialog_store(&iter);
	}
	else
		gtk_tree_store_set(store, &iter, INSPECT_NAME, name, -1);

	g_free(name);
}

static void on_inspect_apply(G_GNUC_UNUSED const MenuItem *menu_item)
{
	GtkTreeIter iter;
	const char *var1;
	gint scid;

	gtk_tree_selection_get_selected(selection, NULL, &iter);
	gtk_tree_model_get(model, &iter, INSPECT_SCID, &scid, INSPECT_VAR1, &var1, -1);

	if (var1)
		debug_send_format(N, "070%d-var-delete %s", scid, var1);
	else
		inspect_apply(&iter);
}

static GtkWidget *expand_dialog;
static GtkSpinButton *expand_start;
static GtkSpinButton *expand_count;
static GtkToggleButton *expand_automatic;

static void on_inspect_expand(G_GNUC_UNUSED const MenuItem *menu_item)
{
	GtkTreeIter iter;
	const char *name;
	gint start, count;
	gboolean expand;

	gtk_tree_selection_get_selected(selection, NULL, &iter);
	gtk_tree_model_get(model, &iter, INSPECT_NAME, &name, INSPECT_START, &start,
		INSPECT_COUNT, &count, INSPECT_EXPAND, &expand, -1);
	gtk_spin_button_set_value(expand_start, start);
	gtk_spin_button_set_value(expand_count, count);
	gtk_toggle_button_set_active(expand_automatic, expand);
	gtk_widget_set_sensitive(GTK_WIDGET(expand_automatic), name != NULL);

	if (gtk_dialog_run(GTK_DIALOG(expand_dialog)) == GTK_RESPONSE_ACCEPT)
	{
		gtk_tree_store_set(store, &iter,
			INSPECT_START, gtk_spin_button_get_value_as_int(expand_start),
			INSPECT_COUNT, gtk_spin_button_get_value_as_int(expand_count),
			INSPECT_EXPAND, gtk_toggle_button_get_active(expand_automatic), -1);

		if (debug_state() & DS_SENDABLE)
			inspect_expand(&iter);
		else
			plugin_beep();
	}
}

static void on_inspect_copy(const MenuItem *menu_item)
{
	menu_copy(selection, menu_item);
}

static void on_inspect_format_display(const MenuItem *menu_item)
{
	menu_mode_display(selection, menu_item, INSPECT_FORMAT);
}

static void on_inspect_format_update(const MenuItem *menu_item)
{
	GtkTreeIter iter;
	gint format = GPOINTER_TO_INT(menu_item->gdata);
	const char *var1;

	gtk_tree_selection_get_selected(selection, NULL, &iter);
	gtk_tree_model_get(model, &iter, INSPECT_VAR1, &var1, -1);

	if (var1)
	{
		debug_send_format(N, "07%d-var-set-format %s %s", inspect_get_scid(&iter), var1,
			inspect_formats[format]);
	}
	else
		gtk_tree_store_set(store, &iter, INSPECT_FORMAT, format, -1);
}

static void on_inspect_hbit_display(const MenuItem *menu_item)
{
	menu_hbit_display(selection, menu_item);
}

static void inspect_hbit_update_iter(GtkTreeIter *iter, gint hb_mode)
{
	const char *value;
	gchar *display;

	gtk_tree_model_get(model, iter, INSPECT_VALUE, &value, -1);
	display = utils_get_display_from_7bit(value, hb_mode);
	gtk_tree_store_set(store, iter, INSPECT_HB_MODE, hb_mode, value ? INSPECT_DISPLAY : -1,
		display, -1);
	g_free(display);
}

static void on_inspect_hbit_update(const MenuItem *menu_item)
{
	GtkTreeIter iter;
	const char *expr, *name;
	gint hb_mode = GPOINTER_TO_INT(menu_item->gdata);

	gtk_tree_selection_get_selected(selection, NULL, &iter);
	gtk_tree_model_get(model, &iter, INSPECT_EXPR, &expr, INSPECT_NAME, &name, -1);
	inspect_hbit_update_iter(&iter, hb_mode);
	parse_mode_update(expr, MODE_HBIT, hb_mode);

	if (name)
	{
		char *reverse = parse_mode_reentry(expr);

		if (model_find(model, &iter, INSPECT_EXPR, reverse))
			inspect_hbit_update_iter(&iter, hb_mode);
		g_free(reverse);
	}
}

static void on_inspect_delete(G_GNUC_UNUSED const MenuItem *menu_item)
{
	GtkTreeIter iter;
	const char *var1;

	gtk_tree_selection_get_selected(selection, NULL, &iter);
	gtk_tree_model_get(model, &iter, INSPECT_VAR1, &var1, -1);

	if (var1)
		debug_send_format(N, "071%d-var-delete %s", inspect_get_scid(&iter), var1);
	else
		gtk_tree_store_remove(store, &iter);
}

#define DS_FRESHABLE DS_SENDABLE
#define DS_INSPECTABLE DS_NOT_BUSY
#define DS_EDITABLE (DS_BASICS | DS_EXTRA_2)
#define DS_APPLIABLE (DS_SENDABLE | DS_EXTRA_3)
#define DS_EXPANDABLE (DS_SENDABLE | DS_EXTRA_4)
#define DS_COPYABLE (DS_BASICS | DS_EXTRA_1)
#define DS_FORMATABLE (DS_NOT_BUSY | DS_EXTRA_1)
#define DS_REPARSABLE (DS_BASICS | DS_EXTRA_1)
#define DS_DELETABLE (DS_NOT_BUSY | DS_EXTRA_1)

#define FORMAT_ITEM(format, FORMAT) \
	{ ("inspect_format_"format), on_inspect_format_update, DS_FORMATABLE, NULL, \
		GINT_TO_POINTER(FORMAT) }

static MenuItem inspect_menu_items[] =
{
	{ "inspect_refresh",   on_inspect_refresh,        DS_FRESHABLE,   NULL, NULL },
	{ "inspect_add",       on_inspect_add,            DS_INSPECTABLE, NULL, NULL },
	{ "inspect_edit",      on_inspect_edit,           DS_EDITABLE,    NULL, NULL },
	{ "inspect_apply",     on_inspect_apply,          DS_APPLIABLE,   NULL, NULL },
	{ "inspect_expand",    on_inspect_expand,         DS_EXPANDABLE,  NULL, NULL },
	{ "inspect_copy",      on_inspect_copy,           DS_COPYABLE,    NULL, NULL },
	{ "inspect_format",    on_inspect_format_display, DS_FORMATABLE,  NULL, NULL },
	FORMAT_ITEM("natural", FORMAT_NATURAL),
	FORMAT_ITEM("decimal", FORMAT_DECIMAL),
	FORMAT_ITEM("hex",     FORMAT_HEX),
	FORMAT_ITEM("octal",   FORMAT_OCTAL),
	FORMAT_ITEM("binary",  FORMAT_BINARY),
	MENU_HBIT_ITEMS(inspect),
	{ "inspect_delete",    on_inspect_delete, DS_DELETABLE, NULL, NULL },
	{ NULL, NULL, 0, NULL, NULL }
};

static guint inspect_menu_extra_state(void)
{
	GtkTreeIter iter;
	guint state = gtk_tree_selection_get_selected(selection, NULL, &iter) << DS_INDEX_1;

	if (state)
	{
		const char *var1, *name;
		gint numchild;

		gtk_tree_model_get(model, &iter, INSPECT_VAR1, &var1, INSPECT_NAME, &name,
			INSPECT_NUMCHILD, &numchild, -1);

		if (name || var1)
		{
			return state | ((name && !var1) << DS_INDEX_2) |
				((name != NULL) << DS_INDEX_3) | ((numchild != 0) << DS_INDEX_4);
		}
	}

	return 0;
}

static MenuInfo inspect_menu_info = { inspect_menu_items, inspect_menu_extra_state, 0 };

static void on_inspect_selection_changed(G_GNUC_UNUSED GtkTreeSelection *selection,
	G_GNUC_UNUSED gpointer gdata)
{
	GtkTreeIter iter;
	const char *name = NULL;

	if (gtk_widget_get_visible(inspect_dialog))
		gtk_widget_hide(inspect_dialog);
	else if (gtk_widget_get_visible(expand_dialog))
		gtk_widget_hide(expand_dialog);

	if (gtk_tree_selection_get_selected(selection, NULL, &iter))
		gtk_tree_model_get(model, &iter, INSPECT_NAME, &name, -1);
	gtk_tree_view_set_reorderable(tree, name != NULL);

	inspects_update_state(debug_state());
}

static gboolean inspect_test_expand_row(G_GNUC_UNUSED GtkTreeView *tree_view,
	GtkTreeIter *iter, G_GNUC_UNUSED GtkTreePath *path, G_GNUC_UNUSED gpointer gdata)
{
	GtkTreeIter child;
	const char *var1;
	gboolean expand;

	gtk_tree_model_iter_children(model, &child, iter);
	gtk_tree_model_get(model, &child, INSPECT_VAR1, &var1, INSPECT_EXPAND, &expand, -1);

	if (var1 || !expand)
		return FALSE;

	if (debug_state() & DS_SENDABLE)
		inspect_expand(iter);
	else
		plugin_blink();

	return TRUE;
}

static gboolean on_inspect_key_press(G_GNUC_UNUSED GtkWidget *widget, GdkEventKey *event,
	G_GNUC_UNUSED gpointer gdata)
{
	if (ui_is_keyval_enter_or_return(event->keyval))
	{
		menu_item_execute(&inspect_menu_info, apply_item, FALSE);
		return TRUE;
	}

	return menu_insert_delete(event, &inspect_menu_info, "inspect_add", "inspect_delete");
}

gboolean on_inspect_button_press(GtkWidget *widget, GdkEventButton *event,
	G_GNUC_UNUSED gpointer gdata)
{
	if (event->button == 1 && event->type == GDK_2BUTTON_PRESS)
	{
		utils_handle_button_press(widget, event);
		menu_item_execute(&inspect_menu_info, apply_item, FALSE);
		return TRUE;
	}

	return FALSE;
}

gboolean on_inspect_drag_motion(G_GNUC_UNUSED GtkWidget *widget,
	G_GNUC_UNUSED GdkDragContext *context, gint x, gint y, G_GNUC_UNUSED guint time,
	G_GNUC_UNUSED gpointer gdata)
{
	GtkTreePath *path;
	GtkTreeViewDropPosition pos;

	if (gtk_tree_view_get_dest_row_at_pos(tree, x, y, &path, &pos))
	{
		GtkTreeIter iter;
		const char *name;

		gtk_tree_model_get_iter(model, &iter, path);
		gtk_tree_path_free(path);
		gtk_tree_model_get(model, &iter, INSPECT_NAME, &name, -1);

		if (!name || pos == GTK_TREE_VIEW_DROP_INTO_OR_BEFORE ||
			pos == GTK_TREE_VIEW_DROP_INTO_OR_AFTER)
		{
			g_signal_stop_emission_by_name(tree, "drag-motion");
		}
	}

	return FALSE;
}

static void on_inspect_menu_show(G_GNUC_UNUSED GtkWidget *widget, G_GNUC_UNUSED gpointer gdata)
{
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(selection, NULL, &iter))
	{
		const char *var1, *path_expr;

		gtk_tree_model_get(model, &iter, INSPECT_VAR1, &var1, INSPECT_PATH_EXPR,
			&path_expr, -1);
		menu_item_set_active(apply_item, var1 != NULL);

		if (var1 && !path_expr && (debug_state() & DS_SENDABLE))
		{
			debug_send_format(N, "04%d-var-info-path-expression %s",
				inspect_get_scid(&iter), var1);
		}
	}
}

static void on_geany_sidebar_switch_page(G_GNUC_UNUSED GtkNotebook *notebook, gpointer page,
	G_GNUC_UNUSED gint page_num, G_GNUC_UNUSED gpointer gdata)
{
	if (page == inspect_page)
		view_inspect_update();
}

void inspect_init(void)
{
	GtkWidget *menu;

	jump_to_item = get_widget("inspect_jump_to_item");
	jump_to_menu = GTK_CONTAINER(get_widget("inspect_jump_to_menu"));
	apply_item = menu_item_find(inspect_menu_items, "inspect_apply");

	inspect_page = get_widget("inspect_page");
	geany_sidebar = GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook);
	g_signal_connect(geany_sidebar, "switch-page", G_CALLBACK(on_geany_sidebar_switch_page),
		NULL);
	gtk_notebook_append_page(geany_sidebar, inspect_page, get_widget("inspect_label"));

	tree = view_connect("inspect_view", &model, &selection, inspect_cells, "inspect_window",
		&inspect_display);
	g_signal_connect(tree, "test-expand-row", G_CALLBACK(inspect_test_expand_row), NULL);
	g_signal_connect(tree, "key-press-event", G_CALLBACK(on_inspect_key_press), NULL);
	g_signal_connect(tree, "button-press-event", G_CALLBACK(on_inspect_button_press), NULL);
	g_signal_connect(tree, "drag-motion", G_CALLBACK(on_inspect_drag_motion), NULL);

	store = GTK_TREE_STORE(model);
	g_signal_connect(model, "row-inserted", G_CALLBACK(on_inspect_row_inserted), NULL);
	g_signal_connect(model, "row-changed", G_CALLBACK(on_inspect_row_changed), NULL);
	g_signal_connect(model, "row-deleted", G_CALLBACK(on_inspect_row_deleted), NULL);

	g_signal_connect(selection, "changed", G_CALLBACK(on_inspect_selection_changed), NULL);
	menu = menu_select("inspect_menu", &inspect_menu_info, selection);
	g_signal_connect(menu, "show", G_CALLBACK(on_inspect_menu_show), NULL);

	inspect_dialog = dialog_connect("inspect_dialog");
	inspect_name = GTK_ENTRY(get_widget("inspect_name_entry"));
	validator_attach(GTK_EDITABLE(inspect_name), VALIDATOR_NOSPACE);
	g_signal_connect(inspect_name, "changed", G_CALLBACK(on_inspect_entry_changed), NULL);
	inspect_frame = GTK_ENTRY(get_widget("inspect_frame_entry"));
	validator_attach(GTK_EDITABLE(inspect_frame), VALIDATOR_VARFRAME);
	g_signal_connect(inspect_frame, "changed", G_CALLBACK(on_inspect_entry_changed), NULL);
	inspect_expr = GTK_ENTRY(get_widget("inspect_expr_entry"));
	g_signal_connect(inspect_expr, "changed", G_CALLBACK(on_inspect_entry_changed), NULL);
	inspect_run_apply = GTK_TOGGLE_BUTTON(get_widget("inspect_run_apply"));
	inspect_ok = get_widget("inspect_ok");
	g_signal_connect(inspect_ok, "clicked", G_CALLBACK(on_inspect_ok_button_clicked), NULL);
	gtk_widget_grab_default(inspect_ok);

	expand_dialog = dialog_connect("expand_dialog");
	expand_start = GTK_SPIN_BUTTON(get_widget("expand_start_spin"));
	expand_count = GTK_SPIN_BUTTON(get_widget("expand_count_spin"));
	expand_automatic = GTK_TOGGLE_BUTTON(get_widget("expand_automatic"));
	gtk_widget_grab_default(get_widget("expand_ok"));
}

void inspect_finalize(void)
{
	gtk_widget_destroy(inspect_page);
	gtk_widget_destroy(inspect_dialog);
	gtk_widget_destroy(expand_dialog);
}
