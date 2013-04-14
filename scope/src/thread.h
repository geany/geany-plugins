/*
 *  thread.h
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

#ifndef THREAD_H

void on_thread_group_started(GArray *nodes);
void on_thread_group_exited(GArray *nodes);
void on_thread_group_added(GArray *nodes);
void on_thread_group_removed(GArray *nodes);

typedef enum _ThreadState
{
	THREAD_BLANK,  /* none or created */
	THREAD_RUNNING,
	THREAD_STOPPED,  /* unknown frame */
	THREAD_QUERY_FRAME,
	THREAD_AT_SOURCE,
	THREAD_AT_ASSEMBLER
} ThreadState;

extern const char *thread_id;
extern ThreadState thread_state;
const char *thread_group_id(void);

void on_thread_running(GArray *nodes);
void on_thread_stopped(GArray *nodes);
void on_thread_created(GArray *nodes);
void on_thread_exited(GArray *nodes);
void on_thread_selected(GArray *nodes);
void on_thread_info(GArray *nodes);
void on_thread_follow(GArray *nodes);
void on_thread_frame(GArray *nodes);

void threads_mark(GeanyDocument *doc);
void threads_clear(void);
void threads_delta(ScintillaObject *sci, const char *real_path, gint start, gint delta);
gboolean threads_update(void);

void thread_query_frame(char token);
void thread_synchronize(void);

void thread_init(void);
void thread_finalize(void);

#define THREAD_H 1
#endif
