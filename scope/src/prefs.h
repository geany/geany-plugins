/*
 *  prefs.h
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

#ifndef PREFS_H

extern gchar *pref_gdb_executable;
extern gboolean pref_gdb_async_mode;
#ifndef G_OS_UNIX
extern gboolean pref_async_break_bugs;
#endif
extern gboolean pref_var_update_bug;

extern gboolean pref_auto_view_source;
extern gboolean pref_keep_exec_point;
extern gint pref_visual_beep_length;
#ifdef G_OS_UNIX
extern gboolean pref_debug_console_vte;
#endif

extern gint pref_sci_marker_first;
extern gint pref_sci_caret_policy;
extern gint pref_sci_caret_slop;
extern gboolean pref_unmark_current_line;

extern gboolean pref_scope_goto_cursor;
extern gboolean pref_seek_with_navqueue;
extern gint pref_panel_tab_pos;
extern gint pref_show_recent_items;
extern gint pref_show_toolbar_items;

extern gint pref_tooltips_fail_action;
extern gint pref_tooltips_send_delay;
extern gint pref_tooltips_length;

extern gint pref_memory_bytes_per_line;
extern gchar *pref_memory_font;

#ifdef G_OS_UNIX
extern gboolean pref_terminal_padding;
extern gint pref_terminal_window_x;
extern gint pref_terminal_window_y;
extern gint pref_terminal_width;
extern gint pref_terminal_height;
#endif

/* geany terminal preferences */
extern gboolean pref_vte_blinken;
extern gchar *pref_vte_emulation;
extern gchar *pref_vte_font;
extern gint pref_vte_scrollback;

#if !GTK_CHECK_VERSION(3, 14, 0)
extern GdkColor pref_vte_colour_fore;
extern GdkColor pref_vte_colour_back;
#else
extern GdkRGBA pref_vte_colour_fore;
extern GdkRGBA pref_vte_colour_back;
#endif

void prefs_apply(GeanyDocument *doc);
char *prefs_file_name(void);

void prefs_init(void);
void prefs_finalize(void);

#define PREFS_H 1
#endif
