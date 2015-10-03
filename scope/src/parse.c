/*
 *  parse.c
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

static void parse_node_free(ParseNode *node, G_GNUC_UNUSED gpointer gdata)
{
	if (node->type == PT_ARRAY)
	{
		parse_foreach((GArray *) node->value, (GFunc) parse_node_free, NULL);
		g_array_free((GArray *) node->value, TRUE);
	}
}

void parse_foreach(GArray *nodes, GFunc func, gpointer gdata)
{
	const ParseNode *node = (const ParseNode *) nodes->data;
	const ParseNode *end = node + nodes->len;

	for (; node < end; node++)
		func((gpointer) node, gdata);
}

static void target_feature_node_check(const ParseNode *node, G_GNUC_UNUSED gpointer gdata)
{
	if (!strcmp((const char *) node->value, "async"))
		pref_gdb_async_mode = TRUE;
}

static void on_target_features(GArray *nodes)
{
	pref_gdb_async_mode = FALSE;
	parse_foreach(parse_lead_array(nodes), (GFunc) target_feature_node_check, NULL);
}

static void on_quiet_error(G_GNUC_UNUSED GArray *nodes)
{
	plugin_blink();
}

static void on_data_modified(G_GNUC_UNUSED GArray *nodes)
{
	views_data_dirty(DS_BUSY);
}

typedef struct _ParseRoute
{
	const char *prefix;
	void (*callback)(GArray *nodes);
	char mark;
	char newline;
	guint args;
} ParseRoute;

static const ParseRoute parse_routes[] =
{
	{ "*running,",                    on_thread_running,       '\0', '\0', 0 },
	{ "*stopped,reason=\"exited",     NULL,                    '\0', '\0', 0 },
	{ "*stopped,reason=\"breakpoint", on_break_stopped,        '\0', '\0', 0 },
	{ "*stopped,",                    on_thread_stopped,       '\0', '\0', 0 },
	{ "=thread-created,",             on_thread_created,       '\0', '\0', 0 },
	{ "=thread-exited,",              on_thread_exited,        '\0', '\0', 0 },
	{ "=thread-selected,id=\"",       on_thread_selected,      '\0', '\0', 1 },
	{ "=thread-group-started,id=\"",  on_thread_group_started, '\0', '\0', 1 },
	{ "=thread-group-exited,id=\"",   on_thread_group_exited,  '\0', '\0', 1 },
	{ "=thread-group-added,id=\"",    on_thread_group_added,   '\0', '\0', 1 },
	{ "=thread-group-removed,id=\"",  on_thread_group_removed, '\0', '\0', 1 },
	{ "=breakpoint-created,bkpt={",   on_break_created,        '\0', '\0', 1 },
	{ "=breakpoint-modified,bkpt={",  on_break_created,        '\0', '\0', 1 },
	{ "=breakpoint-deleted,id=\"",    on_break_deleted,        '\0', '\0', 1 },
	{ "^done,bkpt={",                 on_break_inserted,       '\0', '\0', 1 },
	{ "^done,wpt={",                  on_break_inserted,       '\0', '\0', 1 },
	{ "^done,hw-awpt={",              on_break_inserted,       '\0', '\0', 1 },
	{ "^done,hw-rwpt={",              on_break_inserted,       '\0', '\0', 1 },
	{ "^done,threads=[",              on_thread_follow,        '2',  '\0', 1 },
	{ "^done,threads=[",              on_thread_info,          '\0', '\0', 1 },
	{ "^done,new-thread-id=\"",       on_thread_selected,      '\0', '\0', 1 },
	{ "^done,BreakpointTable={",      on_break_list,           '\0', '\0', 1 },
	{ "^done,frame={",                on_stack_follow,         '2',  '\0', 0 },
	{ "^done,frame={",                on_thread_frame,         '4',  '\0', 0 },
	{ "^done,stack=[",                on_stack_frames,         '*',  '\0', 1 },
	{ "^done,stack-args=[",           on_stack_arguments,      '*',  '\0', 1 },
	{ "^done,variables=[",            on_local_variables,      '*',  '\0', 1 },
	{ "^done,line=\"",                on_debug_list_source,    '2',  '\0', 2 },
	{ "^done,value=\"",               on_inspect_evaluate,     '2',  '\0', 1 },
	{ "^done,value=\"",               on_tooltip_value,        '3',  '\0', 1 },
	{ "^done,value=\"",               on_inspect_evaluate,     '4',  '\0', 1 },
	{ "^done,value=\"",               on_watch_value,          '6',  '\0', 1 },
	{ "^done,value=\"",               on_inspect_assign,       '7',  '\0', 1 },
	{ "^done,value=\"",               on_menu_evaluate_value,  '8',  '\0', 1 },
	{ "^done,name=\"",                on_inspect_variable,     '7',  '\0', 1 },
	{ "^done,format=\"",              on_inspect_format,       '7',  '\0', 1 },
	{ "^done,numchild=\"",            on_inspect_children,     '7',  '\0', 2 },
	{ "^done,ndeleted=\"",            on_inspect_ndeleted,     '7',  '\0', 0 },
	{ "^done,path_expr=\"",           on_inspect_path_expr,    '4',  '\0', 1 },
	{ "^done,changelist=[",           on_inspect_changelist,   '\0', '\0', 1 },
	{ "^done,memory=[",               on_memory_read_bytes,    '\0', '\0', 1 },
	{ "^done,features=[",             on_break_features,       '5',  '\0', 1 },
	{ "^done,features=[",             on_target_features,      '7',  '\0', 1 },
	{ "^done,register-names=[",       on_register_names,       '\0', '\0', 1 },
	{ "^done,changed-registers=[",    on_register_changes,     '\0', '\0', 1 },
	{ "^done,register-values=[",      on_register_values,      '*',  '\0', 1 },
	{ "^running",                     on_debug_loaded,         '1',  '\0', 0 },
	{ "^done",                        on_debug_loaded,         '1',  '\0', 0 },
	{ "^done",                        on_break_done,           '2',  '\0', 0 },
	{ "^done",                        on_debug_auto_run,       '5',  '\0', 0 },
	{ "^done",                        on_data_modified,        '7',  '\0', 0 },
	{ "^error,",                      on_debug_load_error,     '1',  '\n', 0 },
	{ "^error,",                      on_tooltip_error,        '3',  '\0', 0 },
	{ "^error",                       on_quiet_error,          '4',  '\0', 0 },
	{ "^error,",                      on_watch_error,          '6',  '\t', 0 },
	{ "^error,",                      on_debug_error,          '\0', '\n', 0 },
	{ "^exit",                        on_debug_exit,           '\0', '\0', 0 },
	{ NULL, NULL, '\0', '\0', 0 }
};

static char *parse_error(const char *text)
{
	dc_error("%s", text);
	return NULL;
}

char *parse_string(char *text, char newline)
{
	char *out = text;

	while (*++text != '"')
	{
		if (*text == '\\')
		{
			switch (*++text)
			{
				case '\\' :
				case '"' : break;
				case 'n' :
				case 'N' : if (newline) { *text = newline; break; }
				case 't' :
				case 'T' : if (newline) { *text = '\t'; break; }
				default : text--;
			}
		}

		*out++ = *text;

		if (*text == '\0')
			return parse_error("\" expected");
	}

	*out = '\0';
	return text + 1;
}

/* no name checks: break script and break list w/ multi-location violate tuple/list syntax */
static char *parse_text(GArray *nodes, char *text, char end, char newline)
{
	do
	{
		ParseNode node;

		/* gdb.info says "string", but... */
		if (isalpha(*++text) || *text == '_')
		{
			node.name = text;

			while (isalnum(*++text) || (strchr("_.-", *text) && *text));
			if (*text != '=')
				return parse_error("= expected");

			*text++ = '\0';
		}
		else
			node.name = "";

		if (*text == '"')
		{
			node.type = PT_VALUE;
			node.value = text;
			text = parse_string(text, newline);

			if (!text && !newline)
			{
				parse_foreach(nodes, (GFunc) parse_node_free, NULL);
				g_array_set_size(nodes, 0);
				return NULL;
			}
		}
		else
		{
			static const char ends[2] = { ']', '}' };

			gboolean brace = *text == '{';
			GArray *array;

			if (!brace && *text != '[')
				return parse_error("\" { or [ expected");

			array = g_array_new(FALSE, FALSE, sizeof(ParseNode));
			node.type = PT_ARRAY;
			node.value = array;

			if (text[1] == ends[brace])
				text += 2;
			else
				text = parse_text(array, text, ends[brace], newline);
		}

		if (end != '\0' || node.type == PT_VALUE || strcmp(node.name, "time"))
			g_array_append_val(nodes, node);

		if (!text)
			return NULL;

	} while (*text == ',');

	return *text == end ? text + (end != '\0') : parse_error(", or end expected");
}

void parse_message(char *message, const char *token)
{
	const ParseRoute *route;

	for (route = parse_routes; route->prefix; route++)
		if (g_str_has_prefix(message, route->prefix))
			if (!route->mark || (token && (route->mark == '*' || route->mark == *token)))
				break;

	if (route->callback)
	{
		GArray *nodes = g_array_new(FALSE, FALSE, sizeof(ParseNode));
		const char *comma = strchr(route->prefix, ',');

		if (comma)
			parse_text(nodes, message + (comma - route->prefix), '\0', route->newline);

		iff (nodes->len >= route->args, "missing argument(s)")
		{
			if (token)
			{
				ParseNode node = { "=token", PT_VALUE, (char *) token + 1 };
				g_array_append_val(nodes, node);
			}

			route->callback(nodes);
		}

		parse_foreach(nodes, (GFunc) parse_node_free, NULL);
		g_array_free(nodes, TRUE);
	}
}

static char *parse_value(char *text, gint mr_mode)
{
	GString *t = g_string_sized_new(strlen(text));
	const char *s, *start = t->str;

	for (s = text; *s; s++)
	{
		switch (*s)
		{
			case '"' :
			case '\'' :
			{
				const char *q = s;

				while (*++s && *s != *q)
				{
					if (*s == '\\' && s[1])
						s++;
				}

				if (*s && mr_mode >= MR_MODIFY)
				{
					char *end;

					if (strtol(start, &end, 0), end > start)
					{
						if (mr_mode == MR_MODSTR)
							g_string_truncate(t, start - t->str);
						else
						{
							if (t->len > 0 && isspace(t->str[t->len - 1]))
								g_string_truncate(t, t->len - 1);
							continue;
						}
					}
					else if (*q == '"')
					{
						g_string_append(t, "{ ");
						start = q + 1;

						while (++q != s)
						{
							if (q > start)
								g_string_append(t, ", ");

							g_string_append_c(t, '\'');
							g_string_append_c(t, *q);

							if (*q == '\\')
							{
								guint i = 0;

								do
									g_string_append_c(t, *++q);
								while (i++ < 2 && isdigit(*q));
							}
							g_string_append_c(t, '\'');
						}

						g_string_append(t, " }");
						continue;
					}
				}

				g_string_append_len(t, q, s - q);

				if (*s == '\0')
				{
					dc_error("%c expected", *q);
					return g_string_free(t, mr_mode == MR_EDITVC);
				}
				break;
			}
			case '{' :
			case ',' : start = t->str + t->len + 1 + (isspace(s[1]) != 0); break;
			case '=' :
			{
				if (isspace(s[1]))
					s++;

				if (mr_mode != MR_NEUTRAL)
					g_string_truncate(t, start - t->str);
				else
				{
					if (t->len > 0 && isspace(t->str[t->len - 1]))
						t->str[t->len - 1] = '=';
					else
						g_string_append_c(t, '=');
				}
				continue;
			}
			case '.' : if (strncmp(s, "...", 3)) break;
			case '<' :
			{
				if (mr_mode == MR_EDITVC)
					return g_string_free(t, TRUE);
			}
		}

		g_string_append_c(t, *s);
	}

	return g_string_free(t, FALSE);
}

gchar *parse_get_display_from_7bit(const char *text, gint hb_mode, gint mr_mode)
{
	char *locale, *parsed;
	gchar *display;

	hb_mode = opt_hb_mode(hb_mode);
	mr_mode = opt_mr_mode(mr_mode);

	locale = hb_mode == HB_LOCALE || (hb_mode == HB_UTF8 && mr_mode <= MR_NEUTRAL) ?
		utils_get_locale_from_7bit(text) : g_strdup(text);
	parsed = locale && (mr_mode != MR_NEUTRAL || !option_long_mr_format) ?
		parse_value(locale, mr_mode) : g_strdup(locale);
	display = utils_get_display_from_locale(parsed, hb_mode);

	g_free(parsed);
	g_free(locale);
	return display;
}

const ParseNode *parse_find_node(GArray *nodes, const char *name)
{
	const ParseNode *node = (const ParseNode *) nodes->data;
	const ParseNode *end = node + nodes->len;

	for (; node < end; node++)
		if (!strcmp(node->name, name))
			return node;

	return NULL;
}

const void *parse_find_node_type(GArray *nodes, const char *name, ParseNodeType type)
{
	const ParseNode *node = parse_find_node(nodes, name);

	if (node)
	{
		iff (node->type == type, "%s: found %s", name, type == PT_VALUE ? "array" : "value")
			return node->value;
	}

	return NULL;
}

const char *parse_grab_token(GArray *nodes)
{
	const ParseNode *node = parse_find_node(nodes, "=token");
	const char *token = NULL;

	if (node)
	{
		token = (const char *) node->value;
		g_array_remove_index(nodes, node - (const ParseNode *) nodes->data);
	}

	return token;
}

gchar *parse_get_error(GArray *nodes)
{
	gchar *error = parse_find_value(nodes, "msg");
	return error && *error ? utils_get_utf8_from_locale(error) :
		g_strdup(_("Undefined GDB error."));
}

#define MAXLEN 0x7FF
static GString *errors;
static guint errors_id = 0;
static guint error_count = 0;

static gboolean errors_show(G_GNUC_UNUSED gpointer gdata)
{
	errors_id = 0;
	error_count = 0;
	show_error("%s", errors->str);
	return FALSE;
}

void on_error(GArray *nodes)
{
	gchar *error = parse_get_error(nodes);

	if (errors_id)
		g_string_append_c(errors, '\n');
	else
		g_string_truncate(errors, 0);

	g_string_append(errors, error);
	error_count++;
	g_free(error);

	if (errors_id)
	{
		if (errors->len > MAXLEN || error_count >= 8)
		{
			g_source_remove(errors_id);
			errors_show(NULL);
		}
	}
	else
		errors_id = plugin_timeout_add(geany_plugin, 25, errors_show, NULL);
}

void parse_location(GArray *nodes, ParseLocation *loc)
{
	const char *file = parse_find_locale(nodes, "file");
	const char *line = parse_find_value(nodes, "line");

	loc->base_name = utils_get_utf8_from_locale(file);
	loc->func = parse_find_locale(nodes, "func");
	loc->addr = parse_find_value(nodes, "addr");
	loc->file = parse_find_locale(nodes, "fullname");
	loc->line = utils_atoi0(line);

	if (loc->file)
	{
		if (!loc->base_name)
			loc->base_name = utils_get_utf8_basename(loc->file);

		if (!g_path_is_absolute(loc->file))
			loc->file = NULL;
	}

	if (!loc->file || G_UNLIKELY(loc->line < 0))
		loc->line = 0;
}

static ScpTreeStore *parse_modes;
enum { MODE_NAME = MODE_ENTRY + 1 };

static gboolean parse_mode_load(GKeyFile *config, const char *section)
{
	char *name = utils_key_file_get_string(config, section, "name");
	gint hb_mode = utils_get_setting_integer(config, section, "hbit", HB_DEFAULT);
	gint mr_mode = utils_get_setting_integer(config, section, "member", MR_DEFAULT);
	gboolean entry = utils_get_setting_boolean(config, section, "entry", TRUE);
	gboolean valid = FALSE;

	if (name && (unsigned) hb_mode < HB_COUNT && (unsigned) mr_mode < MR_MODIFY)
	{
		scp_tree_store_append_with_values(parse_modes, NULL, NULL, MODE_NAME, name,
			MODE_HBIT, hb_mode, MODE_MEMBER, mr_mode, MODE_ENTRY, entry, -1);
		valid = TRUE;
	}

	g_free(name);
	return valid;
}

static gboolean parse_mode_save(GKeyFile *config, const char *section, GtkTreeIter *iter)
{
	const char *name;
	gint hb_mode, mr_mode;
	gboolean entry;

	scp_tree_store_get(parse_modes, iter, MODE_NAME, &name, MODE_HBIT, &hb_mode,
		MODE_MEMBER, &mr_mode, MODE_ENTRY, &entry, -1);

	if (hb_mode == HB_DEFAULT && mr_mode == MR_DEFAULT && entry)
		return FALSE;

	g_key_file_set_string(config, section, "name", name);
	g_key_file_set_integer(config, section, "hbit", hb_mode);
	g_key_file_set_integer(config, section, "member", mr_mode);
	g_key_file_set_boolean(config, section, "entry", entry);
	return TRUE;
}

char *parse_mode_reentry(const char *name)
{
	return g_str_has_suffix(name, "@entry") ? g_strndup(name, strlen(name) - 6) :
		g_strdup_printf("%s@entry", name);
}

static char *parse_mode_pm_name(const char *name)
{
	return g_strndup(name, strlen(name) - (g_str_has_suffix(name, "@entry") ? 6 : 0));
}

gint parse_mode_get(const char *name, gint mode)
{
	char *pm_name = parse_mode_pm_name(name);
	GtkTreeIter iter;
	gint value;

	if (store_find(parse_modes, &iter, MODE_NAME, pm_name))
		scp_tree_store_get(parse_modes, &iter, mode, &value, -1);
	else
		value = mode == MODE_HBIT ? HB_DEFAULT : mode == MODE_MEMBER ? MR_DEFAULT : TRUE;

	g_free(pm_name);
	return value;
}

void parse_mode_update(const char *name, gint mode, gint value)
{
	char *pm_name = parse_mode_pm_name(name);
	GtkTreeIter iter;

	if (!store_find(parse_modes, &iter, MODE_NAME, name))
	{
		scp_tree_store_append_with_values(parse_modes, &iter, NULL, MODE_NAME, pm_name,
			MODE_HBIT, HB_DEFAULT, MODE_MEMBER, MR_DEFAULT, MODE_ENTRY, TRUE, -1);
	}

	g_free(pm_name);
	scp_tree_store_set(parse_modes, &iter, mode, value, -1);
}

gboolean parse_variable(GArray *nodes, ParseVariable *var, const char *children)
{
	const char *name = parse_find_locale(nodes, "name");

	iff (name, "no name")
	{
		var->name = name;
		var->value = parse_find_value(nodes, "value");
		var->expr = NULL;

		if (children)
		{
			var->expr = parse_find_locale(nodes, "exp");
			var->children = parse_find_value(nodes, children);
			var->numchild = utils_atoi0(var->children);
		}

		var->hb_mode = parse_mode_get(var->expr ? var->expr : name, MODE_HBIT);
		var->mr_mode = parse_mode_get(var->expr ? var->expr : name, MODE_MEMBER);
		var->display = parse_get_display_from_7bit(var->value, var->hb_mode, var->mr_mode);
		return TRUE;
	}

	return FALSE;
}

void parse_load(GKeyFile *config)
{
	store_clear(parse_modes);
	utils_load(config, "parse", parse_mode_load);
}

void parse_save(GKeyFile *config)
{
	store_save(parse_modes, config, "parse", parse_mode_save);
}

void parse_init(void)
{
	errors = g_string_sized_new(MAXLEN);
	parse_modes = SCP_TREE_STORE(get_object("parse_mode_store"));
	scp_tree_store_set_sort_column_id(parse_modes, MODE_NAME, GTK_SORT_ASCENDING);
}

void parse_finalize(void)
{
	g_string_free(errors, TRUE);
}
