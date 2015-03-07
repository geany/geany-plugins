/*
 *  thread.c
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

#define _WIN32_WINNT 0x0501  /* for DebugBreakProcess(), must be before any #include-s */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "common.h"

#ifdef G_OS_UNIX
#include <signal.h>
#else
#include <limits.h>
#include <windows.h>
#endif

enum
{
	GROUP_ID,
	GROUP_PID
};

static ScpTreeStore *groups;

void on_thread_group_started(GArray *nodes)
{
	const char *gid = parse_lead_value(nodes);
	const char *pid = parse_find_value(nodes, "pid");

	ui_set_statusbar(TRUE, _("Thread group %s started."), pid ? pid : gid);

	iff (pid, "no pid")
	{
		GtkTreeIter iter;

		iff (store_find(groups, &iter, GROUP_ID, gid), "%s: gid not found", gid)
			scp_tree_store_set(groups, &iter, GROUP_PID, pid, -1);
	}
}

void on_thread_group_exited(GArray *nodes)
{
	const char *gid = parse_lead_value(nodes);
	const char *exit_code = parse_find_value(nodes, "exit-code");
	GString *status = g_string_new(_("Thread group "));
	GtkTreeIter iter;

	if (store_find(groups, &iter, GROUP_ID, gid))
	{
		const char *pid;

		scp_tree_store_get(groups, &iter, GROUP_PID, &pid, -1);
		if (pid)
		{
			g_string_append(status, pid);
			scp_tree_store_set(groups, &iter, GROUP_PID, NULL, -1);
		}
	}
	else
	{
		dc_error("%s: gid not found", gid);
		g_string_append(status, gid);
	}

	g_string_append(status, _(" exited"));
	if (exit_code)
	{
		g_string_append_printf(status, _(" with exit code %s"), exit_code);
	#ifdef G_OS_UNIX
		if (terminal_show_on_error)
			terminal_standalone(TRUE);
	#endif
	}
	ui_set_statusbar(TRUE, _("%s."), status->str);
	g_string_free(status, TRUE);
}

void on_thread_group_added(GArray *nodes)
{
	GtkTreeIter iter;
	scp_tree_store_append(groups, &iter, NULL);
	scp_tree_store_set(groups, &iter, GROUP_ID, parse_lead_value(nodes), -1);
}

void on_thread_group_removed(GArray *nodes)
{
	const char *gid = parse_lead_value(nodes);
	GtkTreeIter iter;

	iff (store_find(groups, &iter, GROUP_ID, gid), "%s: gid not found", gid)
		scp_tree_store_remove(groups, &iter);
}

enum
{
	THREAD_ID,
	THREAD_FILE,
	THREAD_LINE,
	THREAD_PID,
	THREAD_GROUP_ID,
	THREAD_STATE,
	THREAD_BASE_NAME,
	THREAD_FUNC,
	THREAD_ADDR,
	THREAD_TARGET_ID,
	THREAD_CORE
};

static gint thread_ident_compare(ScpTreeStore *store, GtkTreeIter *a, GtkTreeIter *b,
	gpointer gdata)
{
	const char *s1, *s2;

	scp_tree_store_get(store, a, GPOINTER_TO_INT(gdata), &s1, -1);
	scp_tree_store_get(store, b, GPOINTER_TO_INT(gdata), &s2, -1);

	if (s1 && s2)
	{
		const char *p1, *p2;

		for (p1 = s1; *p1 && !isdigit(*p1); p1++);
		for (p2 = s2; *p2 && !isdigit(*p2); p2++);

		if (p1 - s1 == p2 - s2 && !memcmp(s1, s2, p1 - s1))
			return atoi(p1) - atoi(p2);
	}

	return g_strcmp0(s1, s2);
}

static ScpTreeStore *store;
static GtkTreeSelection *selection;

static gboolean find_thread(const char *tid, GtkTreeIter *iter)
{
	if (G_LIKELY(store_find(store, iter, THREAD_ID, tid)))
		return TRUE;

	dc_error("%s: tid not found", tid);
	return FALSE;
}

static const gchar *RUNNING;
static const gchar *STOPPED;

static void auto_select_thread(void)
{
	GtkTreeIter iter;

	if (store_find(store, &iter, THREAD_STATE, STOPPED))
	{
		utils_tree_set_cursor(selection, &iter, -1);
		view_seek_selected(selection, FALSE, SK_EXECUTE);
	}
}

guint thread_count = 0;
const char *thread_id = NULL;
ThreadState thread_state = THREAD_BLANK;
guint thread_prompt = 0;

const char *thread_group_id(void)
{
	GtkTreeIter iter;
	const char *gid = NULL;

	if (gtk_tree_selection_get_selected(selection, NULL, &iter))
		scp_tree_store_get(store, &iter, THREAD_GROUP_ID, &gid, -1);

	return gid;
}

static void thread_iter_unmark(GtkTreeIter *iter, gpointer gdata)
{
	const char *file;
	const gchar *state;
	gint line;
	gboolean stopped;

	scp_tree_store_get(store, iter, THREAD_FILE, &file, THREAD_LINE, &line, THREAD_STATE,
		&state, -1);
	stopped = !strcmp(state, STOPPED);
	thread_prompt += gdata ? -stopped : !stopped;

	if (GPOINTER_TO_INT(gdata) != 2)
		utils_mark(file, line, FALSE, MARKER_EXECUTE);
}

static void thread_iter_running(GtkTreeIter *iter, const char *tid)
{
	thread_iter_unmark(iter, GINT_TO_POINTER(TRUE + pref_keep_exec_point));

	scp_tree_store_set(store, iter, THREAD_STATE, RUNNING, pref_keep_exec_point ? -1 :
		THREAD_FILE, NULL, THREAD_LINE, 0, THREAD_BASE_NAME, NULL, THREAD_FUNC, NULL,
		THREAD_ADDR, NULL, THREAD_CORE, NULL, -1);

	if (thread_id)
	{
		if (!tid)
			scp_tree_store_get(store, iter, THREAD_ID, &tid, -1);

		if (!strcmp(tid, thread_id))
			thread_state = THREAD_RUNNING;
	}
}

gboolean thread_select_on_running;
gboolean thread_select_on_stopped;
gboolean thread_select_on_exited;
gboolean thread_select_follow;

void on_thread_running(GArray *nodes)
{
	const char *tid = parse_find_value(nodes, "thread-id");

	iff (tid, "no tid")
	{
		gboolean was_stopped = thread_state >= THREAD_STOPPED;

		if (!strcmp(tid, "all"))
			store_foreach(store, (GFunc) thread_iter_running, NULL);
		else
		{
			GtkTreeIter iter;

			if (find_thread(tid, &iter))
				thread_iter_running(&iter, tid);
		}

		if (thread_select_on_running && was_stopped && thread_state == THREAD_RUNNING)
			auto_select_thread();
	}
}

static void thread_parse_extra(GArray *nodes, GtkTreeIter *iter, const char *name, int column)
{
	const char *value = parse_find_value(nodes, name);

	if (value)
		scp_tree_store_set(store, iter, column, value, -1);
}

static void thread_parse_frame(GArray *frame, const char *tid, GtkTreeIter *iter)
{
	ParseLocation loc;

	parse_location(frame, &loc);
	if (!loc.addr)
		loc.addr = "??";

	thread_iter_unmark(iter, NULL);
	scp_tree_store_set(store, iter, THREAD_FILE, loc.file, THREAD_LINE, loc.line,
		THREAD_STATE, STOPPED, THREAD_BASE_NAME, loc.base_name, THREAD_FUNC, loc.func,
		THREAD_ADDR, loc.addr, -1);

	if (!g_strcmp0(tid, thread_id))
	{
		if (loc.line)
		{
			thread_state = THREAD_AT_SOURCE;
			utils_seek(loc.file, loc.line, FALSE, SK_EXEC_MARK);
		}
		else
		{
			thread_state = THREAD_AT_ASSEMBLER;
			view_dirty(VIEW_CONSOLE);
		}
	}
	else
		utils_mark(loc.file, loc.line, TRUE, MARKER_EXECUTE);

	parse_location_free(&loc);
}

typedef struct _StopData
{
	const char *tid;
	GtkTreeIter iter;
	gboolean found;
} StopData;

static void thread_iter_stopped(GtkTreeIter *iter, StopData *sd)
{
	const char *tid = sd->tid, *addr;
	const gchar *state;

	scp_tree_store_get(store, iter, THREAD_STATE, &state, THREAD_ADDR, &addr,
		tid ? -1 : THREAD_ID, &tid, -1);

	if (strcmp(state, STOPPED))
		thread_prompt++;
	scp_tree_store_set(store, iter, THREAD_STATE, STOPPED, -1);

	if (!g_strcmp0(tid, thread_id))
	{
		if (!addr)
			thread_state = THREAD_QUERY_FRAME;

		views_data_dirty(DS_BUSY);
	}
	else if (!addr)
		view_dirty(VIEW_THREADS);

	if (!sd->found)
	{
		sd->iter = *iter;
		sd->found = TRUE;
	}
}

static void thread_node_stopped(const ParseNode *node, StopData *sd)
{
	iff (node->type == PT_VALUE, "stopped-threads: contains array")
	{
		GtkTreeIter iter;

		sd->tid = (const char *) node->value;

		if (find_thread(sd->tid, &iter))
			thread_iter_stopped(&iter, sd);
	}
}

void on_thread_stopped(GArray *nodes)
{
	extern gint break_async;
	const char *tid = parse_find_value(nodes, "thread-id");
	const ParseNode *stopped = parse_find_node(nodes, "stopped-threads");
	StopData sd;

	if (tid)
	{
		sd.found = find_thread(tid, &sd.iter);

		if (sd.found)
		{
			GArray *frame = parse_find_array(nodes, "frame");

			if (frame)
				thread_parse_frame(frame, tid, &sd.iter);

			thread_parse_extra(nodes, &sd.iter, "core", THREAD_CORE);
		}
	}
	else
	{
		dc_error("no tid");
		sd.found = FALSE;
	}

	iff (stopped, "no stopped")
	{
		sd.tid = NULL;

		if (stopped->type == PT_VALUE)
		{
			const char *tid = (const char *) stopped->value;

			if (!strcmp(tid, "all"))
				store_foreach(store, (GFunc) thread_iter_stopped, &sd);
			else
			{
				GtkTreeIter iter;

				if (find_thread(tid, &iter))
				{
					sd.tid = tid;
					thread_iter_stopped(&iter, &sd);
				}
			}
		}
		else
			parse_foreach((GArray *) stopped->value, (GFunc) thread_node_stopped, &sd);
	}

	if (thread_select_on_stopped && thread_state <= THREAD_RUNNING && sd.found)
	{
		utils_tree_set_cursor(selection, &sd.iter, -1);
		view_seek_selected(selection, FALSE, SK_EXECUTE);
	}

	if (!g_strcmp0(parse_find_value(nodes, "reason"), "signal-received"))
		plugin_blink();

	if (break_async < TRUE)
		view_dirty(VIEW_BREAKS);
}

static char *gdb_thread = NULL;

static void set_gdb_thread(const char *tid, gboolean select)
{
	g_free(gdb_thread);
	gdb_thread = g_strdup(tid);

	if (select)
	{
		GtkTreeIter iter;

		if (find_thread(gdb_thread, &iter))
			utils_tree_set_cursor(selection, &iter, -1);
	}
}

void on_thread_created(GArray *nodes)
{
	const char *tid = parse_find_value(nodes, "id");
	const char *gid = parse_find_value(nodes, "group-id");

	if (!thread_count++)
	{
		/* startup */
		breaks_reset();
		registers_show(TRUE);
	#ifdef G_OS_UNIX
		terminal_clear();
		if (terminal_auto_show)
			terminal_standalone(TRUE);
	#endif
		if (option_open_panel_on_start)
			open_debug_panel();
	}

	iff (tid, "no tid")
	{
		GtkTreeIter iter;
		const char *pid = NULL;

		iff (gid, "no gid")
			iff (store_find(groups, &iter, GROUP_ID, gid), "%s: gid not found", gid)
				scp_tree_store_get(groups, &iter, GROUP_PID, &pid, -1);

		scp_tree_store_append_with_values(store, &iter, NULL, THREAD_ID, tid, THREAD_STATE,
			"", THREAD_GROUP_ID, gid, THREAD_PID, pid, -1);
		debug_send_format(N, "04-thread-info %s", tid);

		if (thread_count == 1)
			set_gdb_thread(tid, TRUE);
	}
}

void on_thread_exited(GArray *nodes)
{
	const char *tid = parse_find_value(nodes, "id");

	iff (tid, "no tid")
	{
		GtkTreeIter iter;

		if (!g_strcmp0(tid, gdb_thread))
			set_gdb_thread(NULL, FALSE);

		if (find_thread(tid, &iter))
		{
			gboolean was_selected = !g_strcmp0(tid, thread_id);

			thread_iter_unmark(&iter, GINT_TO_POINTER(TRUE));
			scp_tree_store_remove(store, &iter);
			if (was_selected && thread_select_on_exited)
				auto_select_thread();
		}
	}

	iff (thread_count, "extra exit")
	{
		if (!--thread_count)
		{
			/* shutdown */
			registers_show(FALSE);
		#ifdef G_OS_UNIX
			if (terminal_auto_hide)
				terminal_standalone(FALSE);
		#endif
			on_debug_auto_exit();
		}
	}
}

void on_thread_selected(GArray *nodes)
{
	set_gdb_thread(parse_lead_value(nodes), thread_select_follow);
}

static void thread_parse(GArray *nodes, const char *tid, gboolean stopped)
{
	GtkTreeIter iter;

	if (find_thread(tid, &iter))
	{
		if (stopped)
		{
			GArray *frame = parse_find_array(nodes, "frame");

			iff (frame, "no frame")
				thread_parse_frame(frame, tid, &iter);
		}
		else
		{
			const gchar *state;

			scp_tree_store_get(store, &iter, THREAD_STATE, &state, -1);
			if (strcmp(state, RUNNING))
				thread_iter_running(&iter, tid);
		}

		thread_parse_extra(nodes, &iter, "target-id", THREAD_TARGET_ID);
		thread_parse_extra(nodes, &iter, "core", THREAD_CORE);
	}
}

static void thread_node_parse(const ParseNode *node, G_GNUC_UNUSED gpointer gdata)
{
	iff (node->type == PT_ARRAY, "threads: contains value")
	{
		GArray *nodes = (GArray *) node->value;
		const char *tid = parse_find_value(nodes, "id");
		const char *state = parse_find_value(nodes, "state");

		iff (tid && state, "no tid or state")
			thread_parse(nodes, tid, strcmp(state, "running"));
	}
}

static const char *thread_info_parse(GArray *nodes, gboolean select)
{
	const char *tid = parse_find_value(nodes, "current-thread-id");

	parse_foreach(parse_lead_array(nodes), (GFunc) thread_node_parse, NULL);

	if (tid)
		set_gdb_thread(tid, select);

	return tid;
}

void on_thread_info(GArray *nodes)
{
	thread_info_parse(nodes, thread_select_follow);
}

void on_thread_follow(GArray *nodes)
{
	if (!thread_info_parse(nodes, TRUE))
		dc_error("no current tid");
}

void on_thread_frame(GArray *nodes)
{
	thread_parse(nodes, parse_grab_token(nodes), TRUE);
}

static void thread_iter_mark(GtkTreeIter *iter, GeanyDocument *doc)
{
	const char *file;
	gint line;

	scp_tree_store_get(store, iter, THREAD_FILE, &file, THREAD_LINE, &line, -1);

	if (line && !utils_filenamecmp(file, doc->real_path))
		sci_set_marker_at_line(doc->editor->sci, line - 1, MARKER_EXECUTE);
}

void threads_mark(GeanyDocument *doc)
{
	if (doc->real_path)
		store_foreach(store, (GFunc) thread_iter_mark, doc);
}

void threads_clear(void)
{
	store_foreach(store, (GFunc) thread_iter_unmark, GINT_TO_POINTER(TRUE));
	store_clear(groups);
	store_clear(store);
	set_gdb_thread(NULL, FALSE);
	thread_count = 0;
}

void threads_delta(ScintillaObject *sci, const char *real_path, gint start, gint delta)
{
	GtkTreeIter iter;
	gboolean valid = scp_tree_store_get_iter_first(store, &iter);

	while (valid)
	{
		const char *file;
		gint line;

		scp_tree_store_get(store, &iter, THREAD_FILE, &file, THREAD_LINE, &line, -1);

		if (--line >= 0 && start <= line && !utils_filenamecmp(file, real_path))
			utils_move_mark(sci, line, start, delta, MARKER_EXECUTE);

		valid = scp_tree_store_iter_next(store, &iter);
	}
}

gboolean threads_update(void)
{
	debug_send_command(N, "04-thread-info");
	return TRUE;
}

void thread_query_frame(char token)
{
	debug_send_format(T, "0%c%s-stack-info-frame", token, thread_id);
}

void thread_synchronize(void)
{
	if (thread_id && g_strcmp0(thread_id, gdb_thread))
		debug_send_format(N, "04-thread-select %s", thread_id);
}

static void on_thread_selection_changed(GtkTreeSelection *selection,
	G_GNUC_UNUSED gpointer gdata)
{
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(selection, NULL, &iter))
	{
		const gchar *state;
		gint line;
		const char *addr;

		scp_tree_store_get(store, &iter, THREAD_ID, &thread_id, THREAD_STATE, &state,
			THREAD_LINE, &line, THREAD_ADDR, &addr, -1);

		if (strcmp(state, STOPPED))
		{
			thread_state = *state ? THREAD_RUNNING : THREAD_BLANK;
		}
		else if (addr)
		{
			if (line)
				thread_state = THREAD_AT_SOURCE;
			else
			{
				thread_state = THREAD_AT_ASSEMBLER;
				view_dirty(VIEW_CONSOLE);
			}
		}
		else
		{
			thread_state = THREAD_STOPPED;

			if (debug_state() & DS_DEBUG)
				thread_query_frame('4');
			else
				thread_state = THREAD_QUERY_FRAME;
		}

		frame_id = "0";
	}
	else
	{
		thread_id = frame_id = NULL;
		thread_state = THREAD_BLANK;
	}

	views_data_dirty(debug_state());
	update_state(debug_state());  /* may be a different state */
}

static void thread_seek_selected(gboolean focus)
{
	view_seek_selected(selection, focus, SK_DEFAULT);
}

static void on_thread_refresh(G_GNUC_UNUSED const MenuItem *menu_item)
{
	debug_send_command(N, "-thread-info");
}

static void on_thread_unsorted(G_GNUC_UNUSED const MenuItem *menu_item)
{
	scp_tree_store_set_sort_column_id(store, GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID,
		GTK_SORT_ASCENDING);
}

static void on_thread_view_source(G_GNUC_UNUSED const MenuItem *menu_item)
{
	thread_seek_selected(FALSE);
}

static void on_thread_synchronize(const MenuItem *menu_item)
{
	if (menu_item)
		debug_send_command(N, "02-thread-info");
	else if (thread_id)
		debug_send_format(N, "-thread-select %s", thread_id);
	else
		plugin_blink();
}

#ifdef G_OS_UNIX
static void send_signal(int sig)
{
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(selection, NULL, &iter))
	{
		const char *pid;

		scp_tree_store_get(store, &iter, THREAD_PID, &pid, -1);
		if (kill(atoi(pid), sig) == -1)
			show_errno("kill(pid)");
	}
	else
		plugin_beep();
}

static void on_thread_interrupt(G_GNUC_UNUSED const MenuItem *menu_item)
{
	send_signal(SIGINT);  /* -exec-interrupt signals geany/scope  */
}

static void on_thread_terminate(G_GNUC_UNUSED const MenuItem *menu_item)
{
	send_signal(SIGTERM);
}

#ifndef NSIG
#define NSIG 0xFF
#endif

static void on_thread_send_signal(G_GNUC_UNUSED const MenuItem *menu_item)
{
	gdouble value = 1;

	if (dialogs_show_input_numeric(_("Send Signal"), _("Enter signal #:"), &value, 1, NSIG,
		1))
	{
		send_signal(value);
	}
}
#else  /* G_OS_UNIX */
static HANDLE iter_to_handle(GtkTreeIter *iter)
{
	const char *pid;
	HANDLE hid;

	scp_tree_store_get(store, iter, THREAD_PID, &pid, -1);
	hid = OpenProcess(PROCESS_ALL_ACCESS, FALSE, atoi(pid));
	if (!hid)
		show_errno("OpenProcess");

	return hid;
}

static void on_thread_interrupt(G_GNUC_UNUSED const MenuItem *menu_item)
{
	GtkTreeIter iter;
	HANDLE hid;

	gtk_tree_selection_get_selected(selection, NULL, &iter);
	hid = iter_to_handle(&iter);

	if (hid)
	{
		if (!DebugBreakProcess(hid))
			show_errno("DebugBreakProcess");
		CloseHandle(hid);
	}
}

static void on_thread_terminate(G_GNUC_UNUSED const MenuItem *menu_item)
{
	gdouble value = 1;

	if (dialogs_show_input_numeric(_("Terminate Process"), _("Enter exit code:"), &value, 1,
		UINT_MAX, 1))
	{
		GtkTreeIter iter;

		if (gtk_tree_selection_get_selected(selection, NULL, &iter))
		{
			HANDLE hid = iter_to_handle(&iter);

			if (hid)
			{
				if (!TerminateProcess(hid, value))
					show_errno("TerminateProcess");
				CloseHandle(hid);
			}
		}
		else
			plugin_beep();
	}
}
#endif  /* G_OS_UNIX */

gboolean thread_show_group;

static void on_thread_show_group(const MenuItem *menu_item)
{
	on_menu_update_boolean(menu_item);
	view_column_set_visible("thread_group_id_column", thread_show_group);
}

gboolean thread_show_core;

static void on_thread_show_core(const MenuItem *menu_item)
{
	on_menu_update_boolean(menu_item);
	view_column_set_visible("thread_core_column", thread_show_core);
}

#define DS_VIEWABLE (DS_ACTIVE | DS_EXTRA_2)
#define DS_SIGNABLE (DS_ACTIVE | DS_EXTRA_3)

static MenuItem thread_menu_items[] =
{
	{ "thread_refresh",           on_thread_refresh,        DS_SENDABLE, NULL, NULL },
	{ "thread_unsorted",          on_thread_unsorted,       0,           NULL, NULL },
	{ "thread_view_source",       on_thread_view_source,    DS_VIEWABLE, NULL, NULL },
	{ "thread_synchronize",       on_thread_synchronize,    DS_SENDABLE, NULL, NULL },
	{ "thread_interrupt",         on_thread_interrupt,      DS_SIGNABLE, NULL, NULL },
	{ "thread_terminate",         on_thread_terminate,      DS_SIGNABLE, NULL, NULL },
#ifdef G_OS_UNIX
	{ "thread_send_signal",       on_thread_send_signal,    DS_SIGNABLE, NULL, NULL },
#endif
	{ "thread_auto_select",       on_menu_display_booleans, 0, NULL, GINT_TO_POINTER(4) },
	{ "thread_select_on_running", on_menu_update_boolean, 0, NULL, &thread_select_on_running },
	{ "thread_select_on_stopped", on_menu_update_boolean, 0, NULL, &thread_select_on_stopped },
	{ "thread_select_on_exited",  on_menu_update_boolean, 0, NULL, &thread_select_on_exited },
	{ "thread_select_follow",     on_menu_update_boolean, 0, NULL, &thread_select_follow },
	{ "thread_show_columns",      on_menu_display_booleans, 0, NULL, GINT_TO_POINTER(2) },
	{ "thread_show_group",        on_thread_show_group,   0, NULL, &thread_show_group },
	{ "thread_show_core",         on_thread_show_core,    0, NULL, &thread_show_core },
	{ NULL, NULL, 0, NULL, NULL }
};

static guint thread_menu_extra_state(void)
{
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(selection, NULL, &iter))
	{
		const char *pid, *file;

		scp_tree_store_get(store, &iter, THREAD_PID, &pid, THREAD_FILE, &file, -1);
		return ((file != NULL) << DS_INDEX_2) | ((utils_atoi0(pid) > 0) << DS_INDEX_3);
	}

	return 0;
}

static MenuInfo thread_menu_info = { thread_menu_items, thread_menu_extra_state, 0 };

static void on_thread_synchronize_button_release(GtkWidget *widget, GdkEventButton *event,
	GtkWidget *menu)
{
	menu_shift_button_release(widget, event, menu, on_thread_synchronize);
}

void thread_init(void)
{
	GtkTreeView *tree = view_create("thread_view", &store, &selection);
	GtkWidget *menu = menu_select("thread_menu", &thread_menu_info, selection);

	view_set_sort_func(store, THREAD_ID, store_gint_compare);
	view_set_sort_func(store, THREAD_FILE, store_seek_compare);
	view_set_line_data_func("thread_line_column", "thread_line", THREAD_LINE);
	view_set_sort_func(store, THREAD_PID, thread_ident_compare);
	view_set_sort_func(store, THREAD_GROUP_ID, thread_ident_compare);
	view_set_sort_func(store, THREAD_TARGET_ID, thread_ident_compare);
	gtk_widget_set_has_tooltip(GTK_WIDGET(tree), TRUE);
	g_signal_connect(tree, "query-tooltip", G_CALLBACK(on_view_query_base_tooltip),
		get_column("thread_base_name_column"));

	groups = SCP_TREE_STORE(get_object("thread_group_store"));
	scp_tree_store_set_sort_column_id(groups, GROUP_ID, GTK_SORT_ASCENDING);
	RUNNING = _("Running");
	STOPPED = _("Stopped");
	g_signal_connect(tree, "key-press-event", G_CALLBACK(on_view_key_press),
		thread_seek_selected);
	g_signal_connect(tree, "button-press-event", G_CALLBACK(on_view_button_1_press),
		thread_seek_selected);

	g_signal_connect(selection, "changed", G_CALLBACK(on_thread_selection_changed), NULL);
	g_signal_connect(get_widget("thread_synchronize"), "button-release-event",
		G_CALLBACK(on_thread_synchronize_button_release), menu);
#ifndef G_OS_UNIX
	gtk_widget_hide(get_widget("thread_send_signal"));
#endif
}

void thread_finalize(void)
{
	store_foreach(store, (GFunc) thread_iter_unmark, NULL);
	set_gdb_thread(NULL, FALSE);
}
