/*
 * gdb-ui.h - A GTK-based user interface for the GNU debugger.
 * Copyright 2008 Jeff Pohlmeyer <yetanothergeek(at)gmail(dot)com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#define gui_loop() while (gtk_events_pending()) {gtk_main_iteration();}


typedef struct
{
	gchar *mono_font;
	gchar *term_cmd;
	gboolean show_tooltips;
	gboolean show_icons;
#ifdef STANDALONE
	gboolean stay_on_top;
#endif
} GdbUiOpts;


typedef struct
{
	gchar *filename;
	gchar *line_num;
	gchar *symbol;
} LocationInfo;




typedef void (*GdbUiLineFunc) (const gchar * filename, const gchar * line, const gchar * reason);
typedef LocationInfo *(*GdbUiLocationFunc) ();
typedef void (*GdbUiOptsFunc) ();



typedef struct
{
	GtkWidget *main_window;
	GdbMsgFunc info_func;
	GdbMsgFunc warn_func;
	GdbUiOptsFunc opts_func;
	GdbUiLineFunc line_func;
	GdbUiLocationFunc location_query;
	GdbUiOpts options;
} GdbUiSetup;

extern GdbUiSetup gdbui_setup;


GtkWidget *gdbui_create_widgets(GtkWidget * parent);
void gdbui_set_tip(GtkWidget * w, gchar * tip);
void gdbui_set_tips(GtkTooltips * tips);
void gdbui_enable(gboolean enabled);

GtkWidget *gdbui_new_dialog(gchar * title);

void gdbui_opts_init();
void gdbui_opts_done();

void gdbui_opts_dlg();

void gdbui_stack_dlg(const GSList * frame_list);
void gdbui_break_dlg(gboolean is_watch);
void gdbui_env_dlg(const GdbEnvironInfo * env);

LocationInfo *gdbui_location_dlg(gchar * title, gboolean is_watch);
void gdbui_free_location_info(LocationInfo * li);
