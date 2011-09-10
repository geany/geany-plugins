/*
 *
 *		dpaned.c
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
 *		Working with debug paned.
 */

#include <sys/stat.h>

#include <memory.h>
#include <string.h>

#include "geanyplugin.h"
extern GeanyFunctions	*geany_functions;
extern GeanyData		*geany_data;

#include "tabs.h"
#include "breakpoint.h"
#include "breakpoints.h"
#include "debug.h"
#include "btnpanel.h"
#include "stree.h"

#define NOTEBOOK_GROUP 438948394
#define HPANED_BORDER_WIDTH 4

#define CONNECT_PAGE_SIGNALS(X) \
	swicth_left_handler_id = g_signal_connect(G_OBJECT(debug_notebook_left), "switch-page", G_CALLBACK(on_change_current_page), NULL); \
	swicth_right_handler_id = g_signal_connect(G_OBJECT(debug_notebook_right), "switch-page", G_CALLBACK(on_change_current_page), NULL); \
	g_signal_connect(G_OBJECT(debug_notebook_left), "page-reordered", G_CALLBACK(on_page_reordered), NULL); \
	g_signal_connect(G_OBJECT(debug_notebook_right), "page-reordered", G_CALLBACK(on_page_reordered), NULL); \
	add_left_handler_id = g_signal_connect(G_OBJECT(debug_notebook_left), "page-added", G_CALLBACK(on_page_added), NULL); \
	add_right_handler_id = g_signal_connect(G_OBJECT(debug_notebook_right), "page-added", G_CALLBACK(on_page_added), NULL); \
	remove_left_handler_id = g_signal_connect(G_OBJECT(debug_notebook_left), "page-removed", G_CALLBACK(on_page_removed), NULL); \
	remove_right_handler_id = g_signal_connect(G_OBJECT(debug_notebook_right), "page-removed", G_CALLBACK(on_page_removed), NULL);

#define DISCONNECT_PAGE_SIGNALS(X) \
	g_signal_handler_disconnect(G_OBJECT(debug_notebook_left), swicth_left_handler_id); \
	g_signal_handler_disconnect(G_OBJECT(debug_notebook_right), swicth_right_handler_id); \
	g_signal_handler_disconnect(G_OBJECT(debug_notebook_left), reorder_left_handler_id); \
	g_signal_handler_disconnect(G_OBJECT(debug_notebook_right), reorder_right_handler_id); \
	g_signal_handler_disconnect(G_OBJECT(debug_notebook_left), add_left_handler_id); \
	g_signal_handler_disconnect(G_OBJECT(debug_notebook_right), add_right_handler_id); \
	g_signal_handler_disconnect(G_OBJECT(debug_notebook_left), remove_left_handler_id); \
	g_signal_handler_disconnect(G_OBJECT(debug_notebook_right), remove_right_handler_id);

#define CONNECT_ALLOCATED_PAGE_SIGNALS(X) \
	allocate_handler_id = g_signal_connect(G_OBJECT(hpaned), "size-allocate", G_CALLBACK(on_size_allocate), NULL);

#define DISCONNECT_ALLOCATED_PAGE_SIGNALS(X) \
	g_signal_handler_disconnect(G_OBJECT(hpaned), allocate_handler_id); \

/* config file path */
static gchar *config_path = NULL;

/* config GKeyFile */
static GKeyFile *key_file = NULL;

/* pane */
static GtkWidget *hpaned = NULL;

/* left and right notebooks */
static GtkWidget *debug_notebook_left = NULL;
static GtkWidget *debug_notebook_right = NULL;


/* notebook signal handler ids */

static gulong allocate_handler_id;

static gulong swicth_left_handler_id;
static gulong swicth_right_handler_id;

static gulong reorder_left_handler_id;
static gulong reorder_right_handler_id;

static gulong add_left_handler_id;
static gulong add_right_handler_id;

static gulong remove_left_handler_id;
static gulong remove_right_handler_id;

/* keeps current tabbed mode */
static gboolean is_tabbed; 

/*
 *	save config to a file
 */
static void save_config()
{
	gchar *data = g_key_file_to_data(key_file, NULL, NULL);
	g_file_set_contents(config_path, data, -1, NULL);
	g_free(data);
}

/*
 *	first allocation handler to properly set paned position
 */
void on_size_allocate(GtkWidget *widget,GdkRectangle *allocation, gpointer   user_data)
{
	DISCONNECT_ALLOCATED_PAGE_SIGNALS();

	int position = (allocation->width - 2 * HPANED_BORDER_WIDTH) * 0.5;
	gtk_paned_set_position(GTK_PANED(hpaned), position);
}

/*
 *	page added event handler
 */
static void on_page_added(GtkNotebook *notebook, GtkWidget *child, guint page_num, gpointer user_data)
{
	gboolean left = (GTK_NOTEBOOK(debug_notebook_left) == notebook);
	
	const gchar *group = is_tabbed ? "two_panels_mode" : "one_panel_mode";
	const gchar *tabs_key = is_tabbed ? (left ? "left_tabs" : "right_tabs") : "tabs";

	gsize length;
	int *tabs = g_key_file_get_integer_list(key_file, group, tabs_key, &length, NULL);

	int *new_tabs = g_malloc((length + 1) * sizeof(int));
	memcpy(new_tabs, tabs, length * sizeof(int));
	memmove(new_tabs + page_num + 1, new_tabs + page_num, (length - page_num) * sizeof(int)); 

	GtkWidget *page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(left ? debug_notebook_left : debug_notebook_right), page_num);
	tab_id id = tabs_get_tab_id(page);
	new_tabs[page_num] = id;
	
	g_key_file_set_integer_list(key_file, group, tabs_key, new_tabs, length + 1);

	save_config();

	g_free(tabs);
	g_free(new_tabs);
}

/*
 *	page reordered event handler
 */
static void on_page_reordered(GtkNotebook *notebook, GtkWidget *child, guint page_num, gpointer user_data)
{
	gboolean left = (GTK_NOTEBOOK(debug_notebook_left) == notebook);

	const gchar *group = is_tabbed ? "two_panels_mode" : "one_panel_mode";
	const gchar *tabs_key = is_tabbed ? (left ? "left_tabs" : "right_tabs") : "tabs";
	const gchar *selected_key = is_tabbed ? (left ? "left_selected_tab_index" : "right_selected_tab_index") : "selected_tab_index";

	gsize length;
	int *tabs = g_key_file_get_integer_list(key_file, group, tabs_key, &length, NULL);

	int prev_index;
	GtkWidget *page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(left ? debug_notebook_left : debug_notebook_right), page_num);
	tab_id id = tabs_get_tab_id(page);
	for (prev_index = 0; prev_index < length; prev_index++)
	{
		if (id == tabs[prev_index])
		{
			break;
		}
	}

	int i, min = MIN(prev_index, page_num), max = MAX(prev_index, page_num);
	for (i = min; i < max; i++)
	{
		int tmp = tabs[i];
		tabs[i] = tabs[i + 1];
		tabs[i + 1] = tmp;
	}  
	
	g_key_file_set_integer_list(key_file, group, tabs_key, tabs, length);
	g_key_file_set_integer(key_file, group, selected_key, page_num);

	save_config();

	g_free(tabs);
}

/*
 *	page removed event handler
 */
static void on_page_removed(GtkNotebook *notebook, GtkWidget *child, guint page_num, gpointer user_data)
{
	gboolean left = (GTK_NOTEBOOK(debug_notebook_left) == notebook);

	const gchar *group = is_tabbed ? "two_panels_mode" : "one_panel_mode";
	const gchar *tabs_key = is_tabbed ? (left ? "left_tabs" : "right_tabs") : "tabs";

	gsize length;
	int *tabs = g_key_file_get_integer_list(key_file, group, tabs_key, &length, NULL);
	memmove(tabs + page_num, tabs + page_num + 1, (length - page_num - 1) * sizeof(int)); 

	g_key_file_set_integer_list(key_file, group, tabs_key, tabs, length - 1);
	
	save_config();

	g_free(tabs);
}

/*
 *	active page changed event handler
 */
static gboolean on_change_current_page(GtkNotebook *notebook, gpointer arg1, guint arg2, gpointer user_data)
{
	gboolean left = (GTK_NOTEBOOK(debug_notebook_left) == notebook);

	const gchar *group = is_tabbed ? "two_panels_mode" : "one_panel_mode";
	const gchar *selected_key = is_tabbed ? (left ? "left_selected_tab_index" : "right_selected_tab_index") : "selected_tab_index";

	g_key_file_set_integer(key_file, group, selected_key, arg2);

	save_config();

	return TRUE;
}

/*
 *	set default values in the GKeyFile
 */
void dpaned_set_defaults(GKeyFile *kf)
{
	g_key_file_set_boolean(key_file, "tabbed_mode", "enabled", FALSE);

	int all_tabs[] = { TID_TARGET, TID_BREAKS, TID_LOCALS, TID_WATCH, TID_STACK, TID_TERMINAL, TID_MESSAGES };
	g_key_file_set_integer_list(key_file, "one_panel_mode", "tabs", all_tabs, sizeof(all_tabs) / sizeof(int));
	g_key_file_set_integer(key_file, "one_panel_mode", "selected_tab_index", 0);

	int left_tabs[] = { TID_TARGET, TID_BREAKS, TID_LOCALS, TID_WATCH };
	g_key_file_set_integer_list(key_file, "two_panels_mode", "left_tabs", left_tabs, sizeof(left_tabs) / sizeof(int));
	g_key_file_set_integer(key_file, "two_panels_mode", "left_selected_tab_index", 0);
	int right_tabs[] = { TID_STACK, TID_TERMINAL, TID_MESSAGES };
	g_key_file_set_integer_list(key_file, "two_panels_mode", "right_tabs", right_tabs, sizeof(right_tabs) / sizeof(int));
	g_key_file_set_integer(key_file, "two_panels_mode", "right_selected_tab_index", 0);
}

/*
 *	create GUI and check config
 */
void dpaned_init()
{
	/* create paned */
	hpaned = gtk_hpaned_new();
	gtk_container_set_border_width(GTK_CONTAINER(hpaned), HPANED_BORDER_WIDTH);
	
	/* create notebooks */
	debug_notebook_left = gtk_notebook_new ();
	debug_notebook_right = gtk_notebook_new ();

	/* setup notebooks */
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(debug_notebook_left), TRUE);
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(debug_notebook_right), TRUE);
	gtk_notebook_set_group_id(GTK_NOTEBOOK(debug_notebook_left), NOTEBOOK_GROUP);
	gtk_notebook_set_group_id (GTK_NOTEBOOK(debug_notebook_right), NOTEBOOK_GROUP);
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(debug_notebook_left), GTK_POS_TOP);
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(debug_notebook_right), GTK_POS_TOP);
	
	/* add notebooks */
	gtk_paned_add1(GTK_PANED(hpaned), debug_notebook_left);
	gtk_paned_add2(GTK_PANED(hpaned), debug_notebook_right);

	/* read config */
	gchar *config_dir = g_build_path(G_DIR_SEPARATOR_S, geany_data->app->configdir, "plugins", "debugger", NULL);
	config_path = g_build_path(G_DIR_SEPARATOR_S, config_dir, "debugger.conf", NULL);
	
	g_mkdir_with_parents(config_dir, S_IRUSR | S_IWUSR | S_IXUSR);

	key_file = g_key_file_new();
	if (!g_key_file_load_from_file(key_file, config_path, G_KEY_FILE_NONE, NULL))
	{
		dpaned_set_defaults(key_file);
		gchar *data = g_key_file_to_data(key_file, NULL, NULL);
		g_file_set_contents(config_path, data, -1, NULL);
		g_free(data);
	}
	
	is_tabbed = g_key_file_get_boolean(key_file, "tabbed_mode", "enabled", NULL);
	if (is_tabbed)
	{
		gsize length;
		int *tab_ids, i;

		/* left */
		tab_ids = g_key_file_get_integer_list(key_file, "two_panels_mode", "left_tabs", &length, NULL);
		for (i = 0; i < length; i++)
		{
			GtkWidget *tab = tabs_get_tab((tab_id)tab_ids[i]);
			gtk_notebook_append_page(GTK_NOTEBOOK(debug_notebook_left),
				tab,
				gtk_label_new(tabs_get_label(tab_ids[i])));
			gtk_notebook_set_tab_detachable(GTK_NOTEBOOK(debug_notebook_left), tab, TRUE);
			gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(debug_notebook_left), tab, TRUE);
		}
		g_free(tab_ids);

		/* right */
		tab_ids = g_key_file_get_integer_list(key_file, "two_panels_mode", "right_tabs", &length, NULL);
		for (i = 0; i < length; i++)
		{
			GtkWidget *tab = tabs_get_tab((tab_id)tab_ids[i]);
			gtk_notebook_append_page(GTK_NOTEBOOK(debug_notebook_right),
				tab,
				gtk_label_new(tabs_get_label(tab_ids[i])));
			gtk_notebook_set_tab_detachable(GTK_NOTEBOOK(debug_notebook_right), tab, TRUE);
			gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(debug_notebook_right), tab, TRUE);
		}
		g_free(tab_ids);

		gtk_widget_show_all(hpaned);

		gtk_notebook_set_current_page(GTK_NOTEBOOK(debug_notebook_left),
			g_key_file_get_integer(key_file, "two_panels_mode", "left_selected_tab_index", NULL));
		gtk_notebook_set_current_page(GTK_NOTEBOOK(debug_notebook_right),
			g_key_file_get_integer(key_file, "two_panels_mode", "right_selected_tab_index", NULL));
	}
	else
	{
		g_object_ref(debug_notebook_right);
		gtk_container_remove(GTK_CONTAINER(hpaned), debug_notebook_right);
		
		gsize length;
		int *tab_ids = g_key_file_get_integer_list(key_file, "one_panel_mode", "tabs", &length, NULL);
		int i;
		for (i = 0; i < length; i++)
		{
			GtkWidget *tab = tabs_get_tab((tab_id)tab_ids[i]);
			gtk_notebook_append_page(GTK_NOTEBOOK(debug_notebook_left),
				tab,
				gtk_label_new(tabs_get_label(tab_ids[i])));
			gtk_notebook_set_tab_detachable(GTK_NOTEBOOK(debug_notebook_left), tab, TRUE);
			gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(debug_notebook_left), tab, TRUE);
		}

		gtk_widget_show_all(hpaned);
		gtk_notebook_set_current_page(GTK_NOTEBOOK(debug_notebook_left),
			g_key_file_get_integer(key_file, "one_panel_mode", "selected_tab_index", NULL));
	}
		
	g_free(config_dir);

	CONNECT_PAGE_SIGNALS();
	CONNECT_ALLOCATED_PAGE_SIGNALS();
}

/*
 *	free local data, disconnect signals
 */
void dpaned_destroy()
{
	g_key_file_free(key_file);
	g_free(config_path);
	
	DISCONNECT_PAGE_SIGNALS();
}

/*
 *	gets widget
 */
GtkWidget* dpaned_get_paned()
{
	return hpaned;
}

/*
 *	sets tabs mode
 */
void dpaned_set_tabbed(gboolean tabbed)
{
	is_tabbed = tabbed;
	
	DISCONNECT_PAGE_SIGNALS();
	
	if (!tabbed)
	{
		g_object_ref(debug_notebook_right);
		gtk_container_remove(GTK_CONTAINER(hpaned), debug_notebook_right);
		
		gsize length;
		int *tab_ids = g_key_file_get_integer_list(key_file, "one_panel_mode", "tabs", &length, NULL);
		int i;
		for (i = 0; i < length; i++)
		{
			GtkWidget *tab = tabs_get_tab((tab_id)tab_ids[i]);
			if (-1 == gtk_notebook_page_num(GTK_NOTEBOOK(debug_notebook_left), tab))
			{
				g_object_ref(tab);
				gtk_container_remove(GTK_CONTAINER(debug_notebook_right), tab);
				gtk_notebook_insert_page(GTK_NOTEBOOK(debug_notebook_left), tab, gtk_label_new(tabs_get_label(tab_ids[i])), i);
				g_object_unref(tab);
				gtk_notebook_set_tab_detachable(GTK_NOTEBOOK(debug_notebook_left), tab, TRUE);
				gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(debug_notebook_left), tab, TRUE);
			}
		}

		gtk_notebook_set_current_page(GTK_NOTEBOOK(debug_notebook_left),
			g_key_file_get_integer(key_file, "one_panel_mode", "selected_tab_index", NULL));

		gtk_widget_show_all(hpaned);
	}
	else
	{
		gtk_paned_add2(GTK_PANED(hpaned), debug_notebook_right);
		g_object_unref(debug_notebook_right);

		gsize length;
		int *tab_ids = g_key_file_get_integer_list(key_file, "two_panels_mode", "right_tabs", &length, NULL);
		int i;
		for (i = 0; i < length; i++)
		{
			GtkWidget *tab = tabs_get_tab((tab_id)tab_ids[i]);
			g_object_ref(tab);
			gtk_container_remove(GTK_CONTAINER(debug_notebook_left), tab);
			gtk_notebook_insert_page(GTK_NOTEBOOK(debug_notebook_right), tab, gtk_label_new(tabs_get_label(tab_ids[i])), i);
			g_object_unref(tab);
			gtk_notebook_set_tab_detachable(GTK_NOTEBOOK(debug_notebook_right), tab, TRUE);
				gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(debug_notebook_right), tab, TRUE);
		}

		gtk_notebook_set_current_page(GTK_NOTEBOOK(debug_notebook_left),
			g_key_file_get_integer(key_file, "two_panels_mode", "left_selected_tab_index", NULL));
		gtk_notebook_set_current_page(GTK_NOTEBOOK(debug_notebook_right),
			g_key_file_get_integer(key_file, "two_panels_mode", "right_selected_tab_index", NULL));

		gtk_widget_show_all(hpaned);
	}
	
	CONNECT_PAGE_SIGNALS();

	g_key_file_set_boolean(key_file, "tabbed_mode", "enabled", is_tabbed);

	save_config();
}

/*
 *	gets tabbed mode state
 */
gboolean dpaned_get_tabbed()
{
	return is_tabbed;
}
