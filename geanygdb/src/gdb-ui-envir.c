/*
 * gdb-ui-envir.c - Environment and option settings for a GTK-based GDB user interface.
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
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA. *
 */


#include <string.h>

#include "geanyplugin.h"
#include "gdb-io.h"
#include "gdb-ui.h"



static GtkWidget *
newlabel(gchar * s)
{
	GtkWidget *w = gtk_label_new(s);
	gtk_misc_set_alignment(GTK_MISC(w), 0.0f, 0.0f);
	return w;
}

#define label(w,s) \
gtk_box_pack_start(vbox, newlabel(s), TRUE, TRUE, 0); \
gtk_box_pack_start(vbox, w, TRUE, TRUE, 0); \
gtk_entry_set_activates_default(GTK_ENTRY(w), TRUE);

static gboolean
same_str(const gchar * a, const gchar * b)
{
	if ((!a) && (!b))
	{
		return TRUE;
	}
	if (!a)
	{
		return (!b) || (!*b);
	}
	if (!b)
	{
		return (!a) || (!*a);
	}
	return g_str_equal(a, b);
}



static gchar *
fixup_path(const gchar * path)
{
	if (path && *path)
	{
		gchar **dirs = g_strsplit(path, ":", 0);
		if (dirs)
		{
			gchar *rv = NULL;
			gint i;
			for (i = 0; dirs[i]; i++)
			{
				if (strchr(dirs[i], ' '))
				{
					gchar *tmp = g_strconcat("\"", dirs[i], "\"", NULL);
					g_free(dirs[i]);
					dirs[i] = tmp;
				}
			}
			rv = g_strjoinv(" ", dirs);
			g_strfreev(dirs);
			return rv;
		}
	}
	return g_strdup("");
}



void
gdbui_env_dlg(const GdbEnvironInfo * env)
{
	GtkWidget *dlg = gtk_dialog_new_with_buttons(_("Environment settings"),
						     GTK_WINDOW(gdbui_setup.main_window),
						     GTK_DIALOG_MODAL |
						     GTK_DIALOG_DESTROY_WITH_PARENT,
						     GTK_STOCK_OK, GTK_RESPONSE_OK,
						     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL);

	GtkBox *vbox = GTK_BOX(GTK_DIALOG(dlg)->vbox);
	GtkWidget *cwd_box = gtk_entry_new();
	GtkWidget *path_box = gtk_entry_new();
	GtkWidget *args_box = gtk_entry_new();
	GtkWidget *dirs_box = gtk_entry_new();

	gtk_window_set_transient_for(GTK_WINDOW(dlg), GTK_WINDOW(gdbui_setup.main_window));
	gtk_window_set_position(GTK_WINDOW(dlg), GTK_WIN_POS_CENTER);
	gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);

	gtk_entry_set_text(GTK_ENTRY(cwd_box), env->cwd ? env->cwd : "");
	gtk_entry_set_text(GTK_ENTRY(path_box), env->path ? env->path : "");
	gtk_entry_set_text(GTK_ENTRY(args_box), env->args ? env->args : "");
	gtk_entry_set_text(GTK_ENTRY(dirs_box), env->dirs ? env->dirs : "");

	label(args_box, _("\n Command-line arguments passed to target program:"));
	label(dirs_box, _("\n Search path for source files:"));
	label(cwd_box, _("\n Working directory for target program:"));
	label(path_box, _("\n Search path for executables:"));

	gtk_widget_show_all(dlg);
	gtk_widget_set_usize(dlg, (gdk_screen_get_width(gdk_screen_get_default()) / 2) * 1, 0);
	if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK)
	{
		const gchar *cwd = gtk_entry_get_text(GTK_ENTRY(cwd_box));
		const gchar *path = gtk_entry_get_text(GTK_ENTRY(path_box));
		const gchar *args = gtk_entry_get_text(GTK_ENTRY(args_box));
		const gchar *dirs = gtk_entry_get_text(GTK_ENTRY(dirs_box));
		if (!same_str(cwd, env->cwd))
		{
			gdbio_send_cmd("-environment-cd %s\n", cwd);
		}
		if (!same_str(path, env->path))
		{
			gchar *fixed = fixup_path(path);
			gdbio_send_cmd("-environment-path -r %s\n", fixed);
			g_free(fixed);
		}
		if (!same_str(args, env->args))
		{
			gdbio_send_cmd("-exec-arguments %s\n", args);
		}
		if (!same_str(dirs, env->dirs))
		{
			gchar *fixed = fixup_path(dirs);
			gdbio_send_cmd("-environment-directory -r %s\n", fixed);
			g_free(fixed);
		}
	}
	gtk_widget_destroy(dlg);
}

#define MONO_FONT "monospace 12"

static void
font_click(GtkButton * button, gpointer user_data)
{
	GtkWidget *dlg;
	gchar *fn = NULL;
	gint resp;
	fn = (gchar *) gtk_entry_get_text(GTK_ENTRY(user_data));
	dlg = gtk_font_selection_dialog_new(_("Select Font"));
	if (fn && *fn)
	{
		gtk_font_selection_dialog_set_font_name(GTK_FONT_SELECTION_DIALOG(dlg), fn);
	}
	resp = gtk_dialog_run(GTK_DIALOG(dlg));
	if (resp == GTK_RESPONSE_OK)
	{
		fn = gtk_font_selection_dialog_get_font_name(GTK_FONT_SELECTION_DIALOG(dlg));
		if (fn)
		{
			gtk_entry_set_text(GTK_ENTRY(user_data), fn);
			g_free(fn);
		}
	}
	gtk_widget_destroy(dlg);
}


void
gdbui_opts_dlg()
{
	GtkWidget *dlg = gtk_dialog_new_with_buttons(_("Preferences"),
						     GTK_WINDOW(gdbui_setup.main_window),
						     GTK_DIALOG_MODAL |
						     GTK_DIALOG_DESTROY_WITH_PARENT,
						     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						     GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);

	GtkBox *vbox = GTK_BOX(GTK_DIALOG(dlg)->vbox);
	GtkWidget *hbox;
	GtkWidget *font_btn = gtk_button_new();
//  GtkWidget*font_btn=gtk_button_new_with_label("Choose...");
// GtkWidget*font_btn=gtk_button_new_from_stock(GTK_STOCK_SELECT_FONT);

	GtkWidget *font_box = gtk_entry_new();
	GtkWidget *term_box = gtk_entry_new();
#ifdef STANDALONE
	GtkWidget *top_chk = gtk_check_button_new_with_label(_("Keep debug window on top."));
#endif
	GtkWidget *tip_chk = gtk_check_button_new_with_label(_("Show tooltips."));
	GtkWidget *ico_chk = gtk_check_button_new_with_label(_("Show icons."));

	gtk_button_set_image(GTK_BUTTON(font_btn),
			     gtk_image_new_from_stock(GTK_STOCK_SELECT_FONT, GTK_ICON_SIZE_BUTTON));

	gtk_box_pack_start(vbox, newlabel(_("Font for source code listings:")), FALSE, FALSE, 2);
	if (gdbui_setup.options.mono_font)
	{
		gtk_entry_set_text(GTK_ENTRY(font_box), gdbui_setup.options.mono_font);
	}
	g_signal_connect(G_OBJECT(font_btn), "clicked", G_CALLBACK(font_click), font_box);
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(vbox, hbox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), font_box, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), font_btn, FALSE, FALSE, 0);

	gtk_box_pack_start(vbox, gtk_hseparator_new(), FALSE, FALSE, 8);
	gtk_box_pack_start(vbox, newlabel(_("Terminal program:")), FALSE, FALSE, 2);
	gtk_box_pack_start(vbox, term_box, FALSE, FALSE, 0);
	if (gdbui_setup.options.term_cmd)
	{
		gtk_entry_set_text(GTK_ENTRY(term_box), gdbui_setup.options.term_cmd);
	}

	gtk_box_pack_start(vbox, gtk_hseparator_new(), FALSE, FALSE, 8);
#ifdef STANDALONE
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(top_chk), gdbui_setup.options.stay_on_top);
	gtk_box_pack_start(vbox, top_chk, FALSE, FALSE, 0);
#endif
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tip_chk), gdbui_setup.options.show_tooltips);
	gtk_box_pack_start(vbox, tip_chk, FALSE, FALSE, 0);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ico_chk), gdbui_setup.options.show_icons);
	gtk_box_pack_start(vbox, ico_chk, FALSE, FALSE, 0);


	gtk_widget_show_all(dlg);
	if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK)
	{
		gchar *font = (gchar *) gtk_entry_get_text(GTK_ENTRY(font_box));
		gchar *term = (gchar *) gtk_entry_get_text(GTK_ENTRY(term_box));
		if ((font && *font)
		    && !(gdbui_setup.options.mono_font
			 && g_str_equal(gdbui_setup.options.mono_font, font)))
		{
			g_free(gdbui_setup.options.mono_font);
			gdbui_setup.options.mono_font = g_strdup(font);
		}

		if ((term && *term)
		    && !(gdbui_setup.options.term_cmd
			 && g_str_equal(gdbui_setup.options.term_cmd, term)))
		{
			g_free(gdbui_setup.options.term_cmd);
			gdbui_setup.options.term_cmd = g_strdup(term);
		}
	}
#ifdef STANDALONE
	gdbui_opts.stay_on_top = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(top_chk));
#endif
	gdbui_setup.options.show_tooltips =
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(tip_chk));
	gdbui_setup.options.show_icons = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ico_chk));
	gtk_widget_destroy(dlg);
	if (gdbui_setup.opts_func)
	{
		gdbui_setup.opts_func();
	}
}


void
gdbui_opts_init()
{
	gdbui_setup.options.term_cmd = g_strdup("xterm");
	gdbui_setup.options.mono_font = g_strdup(MONO_FONT);
	gdbui_setup.options.show_tooltips = TRUE;
	gdbui_setup.options.show_icons = TRUE;
#ifdef STANDALONE
	gdbui_setup.options.stay_on_top = FALSE;
#endif
}


void
gdbui_opts_done()
{
	g_free(gdbui_setup.options.mono_font);
	gdbui_setup.options.mono_font = NULL;
}
