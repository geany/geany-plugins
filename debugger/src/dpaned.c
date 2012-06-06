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

#include "dpaned.h"
#include "tabs.h"
#include "breakpoints.h"
#include "debug.h"
#include "btnpanel.h"
#include "stree.h"
#include "dconfig.h"

#define NOTEBOOK_GROUP 438948394
#define HPANED_BORDER_WIDTH 4

#define CONNECT_PAGE_SIGNALS(X) \
	switch_left_handler_id = g_signal_connect(G_OBJECT(debug_notebook_left), "switch-page", G_CALLBACK(on_change_current_page), NULL); \
	switch_right_handler_id = g_signal_connect(G_OBJECT(debug_notebook_right), "switch-page", G_CALLBACK(on_change_current_page), NULL); \
	reorder_left_handler_id = g_signal_connect(G_OBJECT(debug_notebook_left), "page-reordered", G_CALLBACK(on_page_reordered), NULL); \
	reorder_right_handler_id = g_signal_connect(G_OBJECT(debug_notebook_right), "page-reordered", G_CALLBACK(on_page_reordered), NULL); \
	add_left_handler_id = g_signal_connect(G_OBJECT(debug_notebook_left), "page-added", G_CALLBACK(on_page_added), NULL); \
	add_right_handler_id = g_signal_connect(G_OBJECT(debug_notebook_right), "page-added", G_CALLBACK(on_page_added), NULL); \
	remove_left_handler_id = g_signal_connect(G_OBJECT(debug_notebook_left), "page-removed", G_CALLBACK(on_page_removed), NULL); \
	remove_right_handler_id = g_signal_connect(G_OBJECT(debug_notebook_right), "page-removed", G_CALLBACK(on_page_removed), NULL);

#define DISCONNECT_PAGE_SIGNALS(X) \
	g_signal_handler_disconnect(G_OBJECT(debug_notebook_left), switch_left_handler_id); \
	g_signal_handler_disconnect(G_OBJECT(debug_notebook_right), switch_right_handler_id); \
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

/* pane */
static GtkWidget *hpaned = NULL;

/* left and right notebooks */
static GtkWidget *debug_notebook_left = NULL;
static GtkWidget *debug_notebook_right = NULL;

/* notebook signal handler ids */
static gulong allocate_handler_id;
static gulong switch_left_handler_id;
static gulong switch_right_handler_id;
static gulong reorder_left_handler_id;
static gulong reorder_right_handler_id;
static gulong add_left_handler_id;
static gulong add_right_handler_id;
static gulong remove_left_handler_id;
static gulong remove_right_handler_id;

/*
 *	first allocation handler to properly set paned position
 */
static void on_size_allocate(GtkWidget *widget,GdkRectangle *allocation, gpointer   user_data)
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
	gboolean is_left = (GTK_NOTEBOOK(debug_notebook_left) == notebook);
	gboolean is_tabbed = config_get_tabbed();
	
	int *tabs = NULL;
	gsize length;

	if (!is_tabbed)
		tabs = config_get_tabs(&length);
	else if (is_left)
		tabs = config_get_left_tabs(&length);
	else
		tabs = config_get_right_tabs(&length);
	
	int *array = g_malloc((length + 2) * sizeof(int));
	int *new_tabs = array + 1;
	memcpy(new_tabs, tabs, length * sizeof(int));
	memmove(new_tabs + page_num + 1, new_tabs + page_num, (length - page_num) * sizeof(int)); 

	GtkWidget *page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(is_left ? debug_notebook_left : debug_notebook_right), page_num);
	tab_id id = tabs_get_tab_id(page);
	new_tabs[page_num] = id;
	
	int config_part;
	if (!is_tabbed)
		config_part = CP_OT_TABS;
	else if (is_left)
		config_part = CP_TT_LTABS;
	else
		config_part = CP_TT_RTABS;

	array[0] = length + 1;
	memcpy(array + 1, new_tabs, length + 1);
	config_set_panel(config_part, array, 0);

	g_free(tabs);
	g_free(array);
}

/*
 *	page reordered event handler
 */
static void on_page_reordered(GtkNotebook *notebook, GtkWidget *child, guint page_num, gpointer user_data)
{
	gboolean is_left = (GTK_NOTEBOOK(debug_notebook_left) == notebook);
	gboolean is_tabbed = config_get_tabbed();

	int *tabs = NULL;
	gsize length;

	if (!is_tabbed)
		tabs = config_get_tabs(&length);
	else if (is_left)
		tabs = config_get_left_tabs(&length);
	else
		tabs = config_get_right_tabs(&length);

	int prev_index;
	GtkWidget *page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(is_left ? debug_notebook_left : debug_notebook_right), page_num);
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
	
	int config_part_tabs;
	int config_part_selected_index;
	if (!is_tabbed)
	{
		config_part_tabs = CP_OT_TABS;
		config_part_selected_index = CP_OT_SELECTED;
	}
	else if (is_left)
	{
		config_part_tabs = CP_TT_LTABS;
		config_part_selected_index = CP_TT_LSELECTED;
	}
	else
	{
		config_part_tabs = CP_TT_RTABS;
		config_part_selected_index = CP_TT_RSELECTED;
	}
	
	int *array = g_malloc((length + 1) * sizeof(int));
	array[0] = length;
	memcpy(array + 1, tabs, length * sizeof(int));
	
	config_set_panel(
		config_part_tabs, array,
		config_part_selected_index, &page_num,
		0
	);
	
	g_free(tabs);
	g_free(array);
}

/*
 *	page removed event handler
 */
static void on_page_removed(GtkNotebook *notebook, GtkWidget *child, guint page_num, gpointer user_data)
{
	gboolean is_left = (GTK_NOTEBOOK(debug_notebook_left) == notebook);
	gboolean is_tabbed = config_get_tabbed();

	int *tabs = NULL;
	gsize length;

	if (!is_tabbed)
		tabs = config_get_tabs(&length);
	else if (is_left)
		tabs = config_get_left_tabs(&length);
	else
		tabs = config_get_right_tabs(&length);

	memmove(tabs + page_num, tabs + page_num + 1, (length - page_num - 1) * sizeof(int)); 
	memmove(tabs + 1, tabs, (length - 1) * sizeof(int)); 
	tabs[0] = length - 1;
	
	int config_part;
	if (!is_tabbed)
		config_part = CP_OT_TABS;
	else if (is_left)
		config_part = CP_TT_LTABS;
	else
		config_part = CP_TT_RTABS;
	
	config_set_panel(config_part, tabs, 0);

	g_free(tabs);
}

/*
 *	active page changed event handler
 */
static gboolean on_change_current_page(GtkNotebook *notebook, gpointer arg1, guint arg2, gpointer user_data)
{
	gboolean is_left = (GTK_NOTEBOOK(debug_notebook_left) == notebook);
	gboolean is_tabbed = config_get_tabbed();

	int config_part;
	if (!is_tabbed)
		config_part = CP_OT_SELECTED;
	else if (is_left)
		config_part = CP_TT_LSELECTED;
	else
		config_part = CP_TT_RSELECTED;
	
	config_set_panel(config_part, (gpointer)&arg2, 0);

	return TRUE;
}

/*
 *	create GUI and check config
 */
void dpaned_init(void)
{
	/* create paned */
	hpaned = gtk_hpaned_new();
	
	/* create notebooks */
	debug_notebook_left = gtk_notebook_new();
	debug_notebook_right = gtk_notebook_new();

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

	gboolean is_tabbed = config_get_tabbed();
	if (is_tabbed)
	{
		gsize length;
		int *tab_ids, i;

		/* left */
		tab_ids = config_get_left_tabs(&length);
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
		tab_ids = config_get_right_tabs(&length);
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

		gtk_notebook_set_current_page(GTK_NOTEBOOK(debug_notebook_left), config_get_left_selected_tab_index());
		gtk_notebook_set_current_page(GTK_NOTEBOOK(debug_notebook_right),config_get_right_selected_tab_index());
	}
	else
	{
		g_object_ref(debug_notebook_right);
		gtk_container_remove(GTK_CONTAINER(hpaned), debug_notebook_right);
		
		gsize length;
		int *tab_ids = config_get_tabs(&length);
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
		gtk_notebook_set_current_page(GTK_NOTEBOOK(debug_notebook_left), config_get_selected_tab_index());
	}
		
	CONNECT_PAGE_SIGNALS();
	CONNECT_ALLOCATED_PAGE_SIGNALS();
}

/*
 *	free local data, disconnect signals
 */
void dpaned_destroy(void)
{
	DISCONNECT_PAGE_SIGNALS();
}

/*
 *	gets widget
 */
GtkWidget* dpaned_get_paned(void)
{
	return hpaned;
}

/*
 *	sets tabs mode
 */
void dpaned_set_tabbed(gboolean tabbed)
{
	DISCONNECT_PAGE_SIGNALS();
	
	if (!tabbed)
	{
		g_object_ref(debug_notebook_right);
		gtk_container_remove(GTK_CONTAINER(hpaned), debug_notebook_right);
		
		gsize length;
		int *tab_ids = config_get_tabs(&length);
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

		gtk_notebook_set_current_page(GTK_NOTEBOOK(debug_notebook_left), config_get_selected_tab_index());

		gtk_widget_show_all(hpaned);
	}
	else
	{
		gtk_paned_add2(GTK_PANED(hpaned), debug_notebook_right);
		g_object_unref(debug_notebook_right);

		gsize length;
		int *tab_ids = config_get_right_tabs(&length);
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

		gtk_notebook_set_current_page(GTK_NOTEBOOK(debug_notebook_left), config_get_left_selected_tab_index());
		gtk_notebook_set_current_page(GTK_NOTEBOOK(debug_notebook_right), config_get_right_selected_tab_index());

		gtk_widget_show_all(hpaned);
	}
	
	CONNECT_PAGE_SIGNALS();

	config_set_panel(CP_TABBED_MODE, (gpointer)&tabbed, 0);
}
