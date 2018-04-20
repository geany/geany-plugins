/*
 * Copyright 2018 Jiri Techet <techet@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "excmd-runner.h"
#include "excmd-params.h"
#include "excmds/excmds.h"
#include "utils.h"

#include <string.h>
#include <ctype.h>


typedef struct {
	ExCmd cmd;
	const gchar *name;
} ExCmdDef;


ExCmdDef ex_cmds[] = {
	{excmd_save, "w"},
	{excmd_save, "write"},
	{excmd_save, "up"},
	{excmd_save, "update"},

	{excmd_save_all, "wall"},

	{excmd_quit, "q"},
	{excmd_quit, "quit"},
	{excmd_quit, "quita"},
	{excmd_quit, "quitall"},
	{excmd_quit, "qa"},
	{excmd_quit, "qall"},
	{excmd_quit, "cq"},
	{excmd_quit, "cquit"},
	
	{excmd_save_quit, "wq"},
	{excmd_save_quit, "x"},
	{excmd_save_quit, "xit"},
	{excmd_save_quit, "exi"},
	{excmd_save_quit, "exit"},

	{excmd_save_all_quit, "xa"},
	{excmd_save_all_quit, "xall"},
	{excmd_save_all_quit, "wqa"},
	{excmd_save_all_quit, "wqall"},
	{excmd_save_all_quit, "x"},
	{excmd_save_all_quit, "xit"},

	{excmd_repeat_subst, "s"},
	{excmd_repeat_subst, "substitute"},
	{excmd_repeat_subst, "&"},
	{excmd_repeat_subst_orig_flags, "&&"},

	{NULL, NULL}
};


typedef enum
{
	TK_END,
	TK_EOL,
	TK_ERROR,

	TK_PLUS,
	TK_MINUS,
	TK_NUMBER,
	TK_COLON,
	TK_SEMICOLON,
	TK_DOT,
	TK_DOLLAR,
	TK_VISUAL_START,
	TK_VISUAL_END,
	TK_PATTERN,

	TK_STAR,
	TK_PERCENT,
} TokenType;


typedef enum
{
	ST_START,
	ST_AFTER_NUMBER,
	ST_BEFORE_END
} State;


typedef struct
{
	TokenType type;
	gint num;
	gchar *str;
} Token;


static void init_tk(Token *tk, TokenType type, gint num, gchar *str)
{
	tk->type = type;
	tk->num = num;
	g_free(tk->str);
	tk->str = str;
}


static TokenType get_simple_token_type(gchar c)
{
	switch (c)
	{
		case '+':
			return TK_PLUS;
		case '-':
			return TK_MINUS;
		case ';':
			return TK_SEMICOLON;
		case ',':
			return TK_COLON;
		case '.':
			return TK_DOT;
		case '$':
			return TK_DOLLAR;
		case '*':
			return TK_STAR;
		case '%':
			return TK_PERCENT;
		default:
			break;
	}
	return TK_ERROR;
}


static void next_token(const gchar **p, Token *tk)
{
	TokenType type;

	while (isspace(**p))
		(*p)++;

	if (**p == '\0')
	{
		init_tk(tk, TK_EOL, 0, NULL);
		return;
	}

	if (isdigit(**p))
	{
		gint num = 0;
		while (isdigit(**p))
		{
			num = 10 * num + (**p - '0');
			(*p)++;
		}
		init_tk(tk, TK_NUMBER, num, NULL);
		return;
	}

	type = get_simple_token_type(**p);
	if (type != TK_ERROR)
	{
		(*p)++;
		init_tk(tk, type, 0, NULL);
		return;
	}

	if (**p == '/' || **p == '?')
	{
		gchar c = **p;
		gchar begin[2] = {c, '\0'};
		GString *s = g_string_new(begin);
		(*p)++;
		while (**p != c && **p != '\0')
		{
			g_string_append_c(s, **p);
			(*p)++;
		}
		if (**p == c)
			(*p)++;
		init_tk(tk, TK_PATTERN, 0, s->str);
		g_string_free(s, FALSE);
		return ;
	}

	if (**p == '\'')
	{
		(*p)++;
		if (**p == '<')
		{
			(*p)++;
			init_tk(tk, TK_VISUAL_START, 0, NULL);
			return;
		}
		else if (**p == '>')
		{
			(*p)++;
			init_tk(tk, TK_VISUAL_END, 0, NULL);
			return;
		}
		else
		{
			init_tk(tk, TK_ERROR, 0, NULL);
			return;
		}
	}

	init_tk(tk, TK_END, 0, NULL);
	return;
}


static gboolean parse_ex_range(const gchar **p, CmdContext *ctx, gint *from, gint *to)
{
	Token *tk = g_new0(Token, 1);
	State state = ST_START;
	gint num = 0;
	gboolean neg = FALSE;
	gint count = 0;
	gboolean success = TRUE;

	next_token(p, tk);

	while (TRUE)
	{
		if (state == ST_START)
		{
			switch (tk->type)
			{
				case TK_PLUS:
					state = ST_AFTER_NUMBER;
					break;
				case TK_MINUS:
					state = ST_AFTER_NUMBER;
					neg = !neg;
					break;
				case TK_NUMBER:
					state = ST_AFTER_NUMBER;
					num = tk->num - 1;
					break;
				case TK_DOT:
					state = ST_AFTER_NUMBER;
					num = GET_CUR_LINE(ctx->sci);
					break;
				case TK_DOLLAR:
					state = ST_AFTER_NUMBER;
					num = SSM(ctx->sci, SCI_GETLINECOUNT, 0, 0) - 1;
					break;
				case TK_VISUAL_START:
				{
					state = ST_AFTER_NUMBER;
					gint min = MIN(ctx->sel_anchor, SSM(ctx->sci, SCI_GETCURRENTPOS, 0, 0));
					num = SSM(ctx->sci, SCI_LINEFROMPOSITION, min, 0);
					break;
				}
				case TK_VISUAL_END:
				{
					state = ST_AFTER_NUMBER;
					gint max = MAX(ctx->sel_anchor, SSM(ctx->sci, SCI_GETCURRENTPOS, 0, 0));
					num = SSM(ctx->sci, SCI_LINEFROMPOSITION, max, 0);
					break;
				}
				case TK_PATTERN:
				{
					gint pos = perform_search(ctx->sci, tk->str, ctx->num, FALSE);
					num = SSM(ctx->sci, SCI_LINEFROMPOSITION, pos, 0);
					state = ST_AFTER_NUMBER;
					break;
				}

				case TK_PERCENT:
					state = ST_BEFORE_END;
					*to = 0;
					num = SSM(ctx->sci, SCI_GETLINECOUNT, 0, 0) - 1;
					count++;
					break;
				case TK_STAR:
				{
					gint pos = MIN(ctx->sel_anchor, SSM(ctx->sci, SCI_GETCURRENTPOS, 0, 0));
					*to = SSM(ctx->sci, SCI_LINEFROMPOSITION, pos, 0);
					pos = MAX(ctx->sel_anchor, SSM(ctx->sci, SCI_GETCURRENTPOS, 0, 0));
					num = SSM(ctx->sci, SCI_LINEFROMPOSITION, pos, 0);
					state = ST_BEFORE_END;
					count++;
					break;
				}

				case TK_SEMICOLON:
				case TK_COLON:
					//we don't have number yet, ignore
					break;
				default:
					goto finish;
			}
		}
		else if (state == ST_AFTER_NUMBER || state == ST_BEFORE_END)
		{
			if (tk->type == TK_SEMICOLON || tk->type == TK_COLON ||
				tk->type == TK_END || tk->type == TK_EOL)
			{
				num = MAX(num, 0);
				num = MIN(num, SSM(ctx->sci, SCI_GETLINECOUNT, 0, 0) - 1);
				if (tk->type == TK_SEMICOLON || tk->type == TK_EOL)
					goto_nonempty(ctx->sci, num, TRUE);

				*from = *to;
				*to = num;

				neg = FALSE;
				num = 0;
				count++;
				state = ST_START;
			}
			else if (state == ST_AFTER_NUMBER)
			{
				switch (tk->type)
				{
					case TK_PLUS:
						break;
					case TK_MINUS:
						neg = !neg;
						break;
					case TK_NUMBER:
						num += neg ? -tk->num : tk->num;
						neg = FALSE;
						break;
					default:
						goto finish;
				}
			}
			else
				goto finish;
		}
		next_token(p, tk);
	}

finish:
	if (tk->type != TK_EOL && tk->type != TK_END)
		success = FALSE;
	g_free(tk->str);
	g_free(tk);
	if (count == 0)
		*from = *to = GET_CUR_LINE(ctx->sci);
	else if (count == 1)
		*from = *to;
	return success;
}


static void perform_simple_ex_cmd(CmdContext *ctx, const gchar *cmd)
{
	ExCmdParams params;
	gchar **parts, **part;
	gchar *cmd_name = NULL;
	gchar *param1 = NULL;

	params.range_from = 0;
	params.range_to = 0;

	if (strlen(cmd) < 1)
		return;

	if (!parse_ex_range(&cmd, ctx, &params.range_from, &params.range_to))
		return;

	if (g_str_has_prefix(cmd, "s/") || g_str_has_prefix(cmd, "substitute/"))
	{
		g_free(ctx->substitute_text);
		ctx->substitute_text = g_strdup(cmd);
		perform_substitute(ctx->sci, cmd, params.range_from, params.range_to, NULL);
		return;
	}

	parts = g_strsplit(cmd, " ", 0);

	for (part = parts; *part; part++)
	{
		if (strlen(*part) != 0)
		{
			if (!cmd_name)
				cmd_name = *part;
			else if (!param1)
				param1 = *part;
		}
	}

	if (cmd_name)
	{
		gint i;

		params.param1 = param1;
		params.force = FALSE;
		if (cmd_name[strlen(cmd_name)-1] == '!')
		{
			cmd_name[strlen(cmd_name)-1] = '\0';
			params.force = TRUE;
		}

		for (i = 0; ex_cmds[i].cmd != NULL; i++)
		{
			ExCmdDef *def = &ex_cmds[i];
			if (strcmp(def->name, cmd_name) == 0)
			{
				def->cmd(ctx, &params);
				break;
			}
		}
	}

	g_strfreev(parts);
}


void excmd_perform(CmdContext *ctx, const gchar *cmd)
{
	guint len = strlen(cmd);

	if (cmd == NULL || len < 1)
		return;

	switch (cmd[0])
	{
		case ':':
			perform_simple_ex_cmd(ctx, cmd + 1);
			break;
		case '/':
		case '?':
		{
			gint pos;
			if (len == 1)
			{
				if (ctx->search_text && strlen(ctx->search_text) > 1)
					ctx->search_text[0] = cmd[0];
			}
			else
			{
				g_free(ctx->search_text);
				ctx->search_text = g_strdup(cmd);
			}
			pos = perform_search(ctx->sci, ctx->search_text, ctx->num, FALSE);
			if (pos >= 0)
				SET_POS(ctx->sci, pos, TRUE);
			break;
		}
	}
}
