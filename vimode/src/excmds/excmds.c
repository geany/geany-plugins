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

#include "excmds/excmds.h"
#include "utils.h"

void excmd_save(CmdContext *c, ExCmdParams *p)
{
	c->cb->on_save(p->force);
}


void excmd_save_all(CmdContext *c, ExCmdParams *p)
{
	c->cb->on_save_all(p->force);
}


void excmd_quit(CmdContext *c, ExCmdParams *p)
{
	c->cb->on_quit(p->force);
}


void excmd_save_quit(CmdContext *c, ExCmdParams *p)
{
	if (c->cb->on_save(p->force))
		c->cb->on_quit(p->force);
}


void excmd_save_all_quit(CmdContext *c, ExCmdParams *p)
{
	if (c->cb->on_save_all(p->force))
		c->cb->on_quit(p->force);
}


void excmd_repeat_subst(CmdContext *c, ExCmdParams *p)
{
	const gchar *flags = p->param1;
	if (!flags)
		flags = "g";
	perform_substitute(c->sci, c->substitute_text, p->range_from, p->range_to, flags);
}


void excmd_repeat_subst_orig_flags(CmdContext *c, ExCmdParams *p)
{
	perform_substitute(c->sci, c->substitute_text, p->range_from, p->range_to, NULL);
}
