/*
 *  conterm.h
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

#ifndef CONTERM_H

#ifdef G_OS_UNIX
void on_terminal_show(const MenuItem *menu_item);
void terminal_clear(void);
void terminal_standalone(gboolean alone);

extern gboolean terminal_auto_show;
extern gboolean terminal_auto_hide;
extern gboolean terminal_show_on_error;
#endif  /* G_OS_UNIX */

/* pseudo-fd; 3 = prompt, 4 = scope error */
extern void (*dc_output)(int fd, const char *text, gint length);
extern void (*dc_output_nl)(int fd, const char *text, gint length);
void dc_error(const char *format, ...) G_GNUC_PRINTF(1, 2);

void dc_clear(void);
gboolean dc_update(void);

void conterm_load_config(void);  /* Geany VTE config */
void conterm_apply_config(void);

void conterm_init(void);
void conterm_finalize(void);

#define CONTERM_H 1
#endif
