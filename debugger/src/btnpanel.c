/*
 *      btnpanel.c
 *      
 *      Copyright 2010 Alexander Petukhov <devel(at)apetukhov.ru>
 *      
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *      
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *      
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

/*
 * 		buttons panel
 */

#include <sys/stat.h>

#include "geanyplugin.h"
extern GeanyFunctions	*geany_functions;
extern GeanyPlugin		*geany_plugin;

#include "gui.h"
#include "breakpoints.h"
#include "debug.h"
#include "dconfig.h"
#include "tpage.h"
#include "watch_model.h"
#include "wtree.h"
#include "dpaned.h"
#include "btnpanel.h"

#define CP_BUTTONS_PAD 5
#define CONFIG_NAME ".debugger"

static GtkWidget *loadbtn = NULL;

static GtkWidget *runbtn = NULL;
static GtkWidget *restartbtn = NULL;
static GtkWidget *stopbtn = NULL;

static GtkWidget *stepoverbtn = NULL;
static GtkWidget *stepinbtn = NULL;
static GtkWidget *stepoutbtn = NULL;
static GtkWidget *runcursorbtn = NULL;

static GtkWidget *tabbtn = NULL;
static GtkWidget *optbtn = NULL;

static gboolean debugging = FALSE;

/*
 * load config button handler
 */
void on_config_load(GtkButton *button, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	if (!doc && !doc->real_path)
	{
		dialogs_show_msgbox(GTK_MESSAGE_ERROR, _("Error reading config file"));
	}
	else
	{
		gchar *folder = g_path_get_dirname(DOC_FILENAME(doc));
		if (!dconfig_load(folder))
		{
			dialogs_show_msgbox(GTK_MESSAGE_ERROR, _("Error reading config file"));
		}
		g_free(folder);
	}
}

/*
 * clear config values button handler
 */
void on_config_clear(GtkButton *button, gpointer user_data)
{
	/* target page */
	tpage_clear();

	/* breakpoints */
	breaks_remove_all();

	/* watches */
	wtree_remove_all();
}

/*
 * calls settings dialog
 */
void on_settings(GtkButton *button, gpointer user_data)
{
	plugin_show_configure(geany_plugin);
}

/*
 * gets current file and line and calls debug function
 */
void on_execute_until(GtkButton *button, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	if (doc)
	{
		int line = sci_get_current_line(doc->editor->sci) + 1;
		debug_execute_until(DOC_FILENAME(doc), line);
	}
}

/*
 * create and initialize buttons panel
 */
GtkWidget* btnpanel_create(on_toggle cb)
{
	GtkWidget *vbox = gtk_vbox_new(FALSE, CP_BUTTONS_PAD);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 7);

	GtkWidget *hbutton_box = gtk_hbox_new(FALSE, CP_BUTTONS_PAD);

	runbtn = create_button("run.gif", _("Run"));
	g_signal_connect(G_OBJECT(runbtn), "clicked", G_CALLBACK (debug_run), (gpointer)TRUE);

	gtk_box_pack_start(GTK_BOX(hbutton_box), runbtn, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbutton_box, FALSE, TRUE, 0);
	
	hbutton_box = gtk_hbox_new(TRUE, CP_BUTTONS_PAD);

	restartbtn = create_button("restart.gif", _("Restart"));
	g_signal_connect(G_OBJECT(restartbtn), "clicked", G_CALLBACK (debug_restart), (gpointer)TRUE);

	stopbtn = create_button("stop.gif", _("Stop"));
	g_signal_connect(G_OBJECT(stopbtn), "clicked", G_CALLBACK (debug_stop), (gpointer)TRUE);

	gtk_box_pack_start(GTK_BOX(hbutton_box), restartbtn, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbutton_box), stopbtn, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbutton_box, FALSE, TRUE, 0);

	hbutton_box = gtk_hbox_new(TRUE, CP_BUTTONS_PAD);

	stepoverbtn = create_button("step_over.gif", _("Step over"));
	g_signal_connect(G_OBJECT(stepoverbtn), "clicked", G_CALLBACK (debug_step_over), (gpointer)TRUE);

	stepinbtn = create_button("step_in.png", _("Step into"));
	g_signal_connect(G_OBJECT(stepinbtn), "clicked", G_CALLBACK (debug_step_into), (gpointer)TRUE);

	gtk_box_pack_start(GTK_BOX(hbutton_box), stepoverbtn, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbutton_box), stepinbtn, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbutton_box, FALSE, TRUE, 0);

	hbutton_box = gtk_hbox_new(TRUE, CP_BUTTONS_PAD);

	stepoutbtn = create_button("step_out.gif", _("Step out"));
	g_signal_connect(G_OBJECT(stepoutbtn), "clicked", G_CALLBACK (debug_step_out), (gpointer)TRUE);

	runcursorbtn = create_button("run_to_cursor.gif", _("Run to cursor"));
	g_signal_connect(G_OBJECT(runcursorbtn), "clicked", G_CALLBACK (on_execute_until), (gpointer)TRUE);

	gtk_box_pack_start(GTK_BOX(hbutton_box), stepoutbtn, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbutton_box), runcursorbtn, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbutton_box, FALSE, TRUE, 0);

	GtkWidget *vbox_panels_buttons = gtk_vbox_new(FALSE, 0);
	GtkWidget *vbutton_box = gtk_vbox_new(TRUE, CP_BUTTONS_PAD);

	loadbtn = create_stock_button(GTK_STOCK_OPEN, _("Load settings"));
	g_signal_connect(G_OBJECT(loadbtn), "clicked", G_CALLBACK (on_config_load), (gpointer)TRUE);
	gtk_box_pack_start(GTK_BOX(vbutton_box), loadbtn, FALSE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(vbox_panels_buttons), vbutton_box, TRUE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), vbox_panels_buttons, TRUE, FALSE, 0);

	vbutton_box = gtk_vbox_new(TRUE, CP_BUTTONS_PAD);
	tabbtn = create_toggle_button("tabs.gif", _("Tabs"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tabbtn), dpaned_get_tabbed());
	g_signal_connect(G_OBJECT(tabbtn), "toggled", G_CALLBACK(cb), NULL);
	gtk_box_pack_start(GTK_BOX(vbox), tabbtn, FALSE, FALSE, 0);

	optbtn = create_stock_button(GTK_STOCK_PREFERENCES, "Настройки");
	g_signal_connect(G_OBJECT(optbtn), "clicked", G_CALLBACK (on_settings), NULL);
	gtk_box_pack_start(GTK_BOX(vbox), optbtn, FALSE, FALSE, 0);

	btnpanel_set_debug_state(DBS_IDLE);

	return vbox;
}

/*
 * disable load/save config buttons on document close
 */
void btnpanel_on_document_close()
{
	gtk_widget_set_sensitive(loadbtn, FALSE);
}

/*
 * enable load/save config buttons on document activate
 * if page is not readonly and have config
 */
void btnpanel_on_document_activate(GeanyDocument *doc)
{
	if (debugging)
		return;
	
	if (!doc || !doc->real_path)
	{
		btnpanel_on_document_close();
		return;
	}

	gchar *dirname = g_path_get_dirname(DOC_FILENAME(doc));
	gchar *config = g_build_path(G_DIR_SEPARATOR_S, dirname, CONFIG_NAME, NULL);
	struct stat st;
	gtk_widget_set_sensitive(loadbtn, !stat(config, &st));
	
	g_free(config);
}

/*
 * set buttons sensitive based on whether it is a config file
 * in the current folder
 */
void btnpanel_set_have_config(gboolean haveconfig)
{
	gtk_widget_set_sensitive(loadbtn, haveconfig);
}

/*
 * set buttons sensitive based on debugger state
 */
void btnpanel_set_debug_state(enum dbs state)
{
	debugging = (DBS_IDLE != state);

	if (DBS_STOPPED == state)
	{
		set_button_image(runbtn, "continue.png");
		gtk_widget_set_tooltip_text(runbtn, _("Continue"));
	}
	else
	{
		set_button_image(runbtn, "run.gif");		
		gtk_widget_set_tooltip_text(runbtn, _("Run"));
	}
	
	gtk_widget_set_sensitive(runbtn, DBS_IDLE == state || DBS_STOPPED == state);
	gtk_widget_set_sensitive(restartbtn, DBS_IDLE != state);
	gtk_widget_set_sensitive(stopbtn, DBS_IDLE != state);
	
	gtk_widget_set_sensitive(stepoverbtn, DBS_STOPPED == state);
	gtk_widget_set_sensitive(stepinbtn, DBS_STOPPED == state);
	gtk_widget_set_sensitive(stepoutbtn, DBS_STOPPED == state);
	gtk_widget_set_sensitive(runcursorbtn, DBS_STOPPED == state);

	if (DBS_IDLE == state)
	{
		btnpanel_on_document_activate(document_get_current());
	}
	else
	{
		gtk_widget_set_sensitive(loadbtn, FALSE);
	}
}
