/*
 * gsdlg.h - Simple GTK dialog wrapper
 *
 * Copyright 2007-2008 Jeff Pohlmeyer <yetanothergeek(at)gmail(dot)com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
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


/*

Defining GSDLG_ALL_IN_ONE will cause this header to also include the
complete "gsdlg.c" sources. This is done by "gsdlg_lua.c" to keep
the implemention details static.

But since the "gsdlg.c" API might also be useful outside of Lua,
it is also possible to build it as a standalone object file, in
which case the GSDLG_API functions will be available externally.

*/

#ifdef GSDLG_ALL_IN_ONE
#define GSDLG_API static
#else
#define GSDLG_API
#endif

#include <gtk/gtk.h>

typedef const gchar* GsDlgStr;

GSDLG_API void gsdlg_text(     GtkDialog *dlg, GsDlgStr key, GsDlgStr value, GsDlgStr label);
GSDLG_API void gsdlg_password( GtkDialog *dlg, GsDlgStr key, GsDlgStr value, GsDlgStr label);
GSDLG_API void gsdlg_textarea( GtkDialog *dlg, GsDlgStr key, GsDlgStr value, GsDlgStr label);
GSDLG_API void gsdlg_file(     GtkDialog *dlg, GsDlgStr key, GsDlgStr value, GsDlgStr label);
GSDLG_API void gsdlg_color(    GtkDialog *dlg, GsDlgStr key, GsDlgStr value, GsDlgStr label);
GSDLG_API void gsdlg_font(     GtkDialog *dlg, GsDlgStr key, GsDlgStr value, GsDlgStr label);

GSDLG_API void gsdlg_group(    GtkDialog *dlg, GsDlgStr key, GsDlgStr value, GsDlgStr label);
GSDLG_API void gsdlg_radio(    GtkDialog *dlg, GsDlgStr key, GsDlgStr value, GsDlgStr label);
GSDLG_API void gsdlg_select(   GtkDialog *dlg, GsDlgStr key, GsDlgStr value, GsDlgStr label);
GSDLG_API void gsdlg_option(   GtkDialog *dlg, GsDlgStr key, GsDlgStr value, GsDlgStr label);

GSDLG_API void gsdlg_checkbox( GtkDialog *dlg, GsDlgStr key, gboolean value, GsDlgStr label);

GSDLG_API void gsdlg_label(    GtkDialog *dlg, GsDlgStr text);
GSDLG_API void gsdlg_heading(  GtkDialog *dlg, GsDlgStr text);
GSDLG_API void gsdlg_hr(       GtkDialog *dlg);


GSDLG_API GtkDialog *gsdlg_new(GsDlgStr title, GsDlgStr* btns);
GSDLG_API GHashTable* gsdlg_run(GtkDialog *dlg, gint *btn, gpointer user_data);

GSDLG_API GtkWindow* gsdlg_toplevel;


typedef void (*GsDlgRunHook) (gboolean running, gpointer user_data);

/*
	If assigned, the cb callback will be called twice by gsdlg_run(),
	first with running=TRUE when the dialog is displayed, and
	then with running=FALSE when it is dismissed.
*/
#ifndef DIALOG_LIB
GSDLG_API void gsdlg_set_run_hook(GsDlgRunHook cb);
#endif


#ifdef GSDLG_ALL_IN_ONE
#include "gsdlg.c"
#endif
