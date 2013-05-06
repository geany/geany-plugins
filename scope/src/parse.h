/*
 *  parse.h
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

#ifndef PARSE_H

typedef enum _ParseNodeType
{
	PT_VALUE,  /* string */
	PT_ARRAY
} ParseNodeType;

typedef struct _ParseNode
{
	const char *name;
	ParseNodeType type;
	void *value;
} ParseNode;

void parse_foreach(GArray *nodes, GFunc func, gpointer gdata);
char *parse_string(char *text, char newline);
void parse_message(char *message, const char *token);
gchar *parse_get_display_from_7bit(const char *text, gint hb_mode, gint mr_mode);

const ParseNode *parse_find_node(GArray *nodes, const char *name);
const void *parse_find_node_type(GArray *nodes, const char *name, ParseNodeType type);
#define parse_find_value(nodes, name) \
	((char *) parse_find_node_type((nodes), (name), PT_VALUE))
#define parse_find_locale(nodes, name) utils_7bit_to_locale(parse_find_value((nodes), (name)))
#define parse_find_array(nodes, name) \
	((GArray *) parse_find_node_type((nodes), (name), PT_ARRAY))
const char *parse_grab_token(GArray *nodes);  /* removes token from nodes */
#define parse_lead_array(nodes) ((GArray *) ((ParseNode *) nodes->data)->value)
#define parse_lead_value(nodes) ((char *) ((ParseNode *) nodes->data)->value)

gchar *parse_get_error(GArray *nodes);
void on_error(GArray *nodes);

typedef struct _ParseLocation
{
	gchar *base_name;
	const char *func;
	const char *addr;
	const char *file;
	gint line;
} ParseLocation;

void parse_location(GArray *nodes, ParseLocation *loc);
#define parse_location_free(loc) g_free((loc)->base_name)

enum
{
	HB_DEFAULT,
	HB_7BIT,
	HB_LOCALE,
	HB_UTF8,
	HB_COUNT
};

enum
{
	MR_COMPACT,
	MR_NEUTRAL,
	MR_DEFAULT,
	MR_MODIFY,
	MR_MODSTR,
	MR_EDITVC
};

enum
{
	MODE_HBIT,
	MODE_MEMBER,
	MODE_ENTRY
};

typedef struct _ParseMode
{
	char *name;
	gint hb_mode;
	gint mr_mode;
	gboolean entry;
} ParseMode;

typedef struct _ParseVariable
{
	const char *name;
	const char *value;
	gint hb_mode;
	gint mr_mode;
	gchar *display;
	/* if children */
	const char *expr;
	const char *children;
	gint numchild;
} ParseVariable;

char *parse_mode_reentry(const char *name);
gint parse_mode_get(const char *name, gint mode);
void parse_mode_update(const char *name, gint mode, gint value);
gboolean parse_variable(GArray *nodes, ParseVariable *var, const char *children);
#define parse_variable_free(var) g_free((var)->display)

void parse_load(GKeyFile *config);
void parse_save(GKeyFile *config);

void parse_init(void);
void parse_finalize(void);

#define PARSE_H 1
#endif
