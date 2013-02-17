/*
 *  break.h
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

#ifndef BREAK_H

void on_break_inserted(GArray *nodes);
void on_break_done(GArray *nodes);
void on_break_list(GArray *nodes);  /* or info */
void on_break_stopped(GArray *nodes);  /* thread */
void on_break_created(GArray *nodes);  /* or modified */
void on_break_deleted(GArray *nodes);
void on_break_features(GArray *nodes);

void breaks_mark(GeanyDocument *doc);
void breaks_clear(void);
void breaks_reset(void);
void breaks_apply(void);
void breaks_query_async(GString *commands);
void breaks_delta(ScintillaObject *sci, const char *real_path, gint start, gint delta,
	gboolean active);
guint breaks_active(void);

void on_break_toggle(const MenuItem *menu_item);
gboolean breaks_update(void);
void breaks_delete_all(void);
void breaks_load(GKeyFile *config);
void breaks_save(GKeyFile *config);

void break_init(void);
void break_finalize(void);

#define BREAK_H 1
#endif
