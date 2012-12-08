/*
 *  program.h
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

#ifndef PROGRAM_H

extern gchar *program_executable;
extern gchar *program_arguments;
extern gchar *program_environment;
extern gchar *program_working_dir;
extern gchar *program_load_script;
extern gboolean program_auto_run_exit;
extern gboolean program_non_stop_mode;
extern gboolean program_temp_breakpoint;
extern gchar *program_temp_break_location;

extern gboolean option_open_panel_on_load;
extern gboolean option_open_panel_on_start;
extern gboolean option_update_all_views;
extern gint option_high_bit_mode;
extern gboolean option_member_names;
extern gboolean option_argument_names;
extern gboolean option_long_mr_format;
extern gboolean option_inspect_expand;
extern gint option_inspect_count;
extern gboolean option_library_messages;
extern gboolean option_editor_tooltips;

/* note: 2 references to mode */
#define opt_hb_mode(hb_mode) ((hb_mode) == HB_DEFAULT ? option_high_bit_mode : (hb_mode))
#define opt_mr_mode(mr_mode) ((mr_mode) == MR_DEFAULT ? option_member_names : (mr_mode))

gboolean recent_menu_items(void);
void program_context_changed(void);
void program_update_state(DebugState state);
void on_program_setup(const MenuItem *menu_item);
void program_load_config(GKeyFile *config);

void program_init(void);
void program_finalize(void);

#define PROGRAM_H 1
#endif
