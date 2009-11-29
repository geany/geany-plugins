/*
 *      ao_tasks.c - this file is part of Addons, a Geany plugin
 *
 *      Copyright 2009 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * $Id$
 */


#include <gtk/gtk.h>
#include <glib-object.h>

#include "geanyplugin.h"

#include "addons.h"
#include "ao_tasks.h"

#include <gdk/gdkkeysyms.h>


/* TODO make tokens configurable */
const gchar *tokens[] = { "TODO", "FIXME", NULL };


typedef struct _AoTasksPrivate AoTasksPrivate;

#define AO_TASKS_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), \
	AO_TASKS_TYPE, AoTasksPrivate))

struct _AoTasks
{
	GObject parent;
};

struct _AoTasksClass
{
	GObjectClass parent_class;
};

struct _AoTasksPrivate
{
	gboolean enable_tasks;

	GtkListStore *store;
	GtkWidget *tree;

	GtkWidget *page;
	GtkWidget *popup_menu;
};

enum
{
	PROP_0,
	PROP_ENABLE_TASKS
};

enum
{
	TLIST_COL_FILENAME,
	TLIST_COL_DISPLAY_FILENAME,
	TLIST_COL_LINE,
	TLIST_COL_NAME,
	TLIST_COL_TOOLTIP,
	TLIST_COL_MAX
};

static void ao_tasks_finalize  			(GObject *object);
static void ao_tasks_show				(AoTasks *t);
static void ao_tasks_hide				(AoTasks *t);

G_DEFINE_TYPE(AoTasks, ao_tasks, G_TYPE_OBJECT);


static void ao_tasks_set_property(GObject *object, guint prop_id,
								  const GValue *value, GParamSpec *pspec)
{
	AoTasksPrivate *priv = AO_TASKS_GET_PRIVATE(object);

	switch (prop_id)
	{
		case PROP_ENABLE_TASKS:
		{
			gboolean new_val = g_value_get_boolean(value);
			if (new_val && ! priv->enable_tasks)
				ao_tasks_show(AO_TASKS(object));
			if (! new_val && priv->enable_tasks)
				ao_tasks_hide(AO_TASKS(object));

			priv->enable_tasks = new_val;
			break;
		}
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
			break;
	}
}


static void ao_tasks_class_init(AoTasksClass *klass)
{
	GObjectClass *g_object_class;

	g_object_class = G_OBJECT_CLASS(klass);
	g_object_class->finalize = ao_tasks_finalize;
	g_object_class->set_property = ao_tasks_set_property;
	g_type_class_add_private(klass, sizeof(AoTasksPrivate));

	g_object_class_install_property(g_object_class,
									PROP_ENABLE_TASKS,
									g_param_spec_boolean(
									"enable-tasks",
									"enable-tasks",
									"Whether to show list of defined tasks",
									TRUE,
									G_PARAM_WRITABLE));
}


static void ao_tasks_finalize(GObject *object)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_AO_TASKS(object));

	ao_tasks_hide(AO_TASKS(object));

	G_OBJECT_CLASS(ao_tasks_parent_class)->finalize(object);
}


static gboolean ao_tasks_selection_changed_cb(gpointer widget)
{
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
	GtkTreeIter iter;
	GtkTreeModel *model;

	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gint line;
		gchar *filename, *locale_filename;
		GeanyDocument *doc;

		gtk_tree_model_get(model, &iter, TLIST_COL_LINE, &line, TLIST_COL_FILENAME, &filename, -1);
		locale_filename = utils_get_locale_from_utf8(filename);
		doc = document_open_file(locale_filename, FALSE, NULL, NULL);
		if (doc != NULL)
		{
			sci_goto_line(doc->editor->sci, line - 1, TRUE);
			gtk_widget_grab_focus(GTK_WIDGET(doc->editor->sci));
		}
		g_free(filename);
		g_free(locale_filename);
	}
	return FALSE;
}


static gboolean ao_tasks_button_press_cb(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	if (event->button == 1)
	{	/* allow reclicking of a treeview item */
		g_idle_add(ao_tasks_selection_changed_cb, widget);
	}
	else if (event->button == 3)
	{
		AoTasksPrivate *priv = AO_TASKS_GET_PRIVATE(data);
		gtk_menu_popup(GTK_MENU(priv->popup_menu), NULL, NULL, NULL, NULL,
				event->button, event->time);
		/* don't return TRUE here, otherwise the selection won't be changed */
	}
	return FALSE;
}


static gboolean ao_tasks_key_press_cb(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event->keyval == GDK_Return ||
		event->keyval == GDK_ISO_Enter ||
		event->keyval == GDK_KP_Enter ||
		event->keyval == GDK_space)
	{
		g_idle_add(ao_tasks_selection_changed_cb, widget);
	}
	if ((event->keyval == GDK_F10 && event->state & GDK_SHIFT_MASK) || event->keyval == GDK_Menu)
	{
		GdkEventButton button_event;

		button_event.time = event->time;
		button_event.button = 3;

		ao_tasks_button_press_cb(widget, &button_event, data);
		return TRUE;
	}
	return FALSE;
}


static void ao_tasks_hide(AoTasks *t)
{
	AoTasksPrivate *priv = AO_TASKS_GET_PRIVATE(t);

	if (priv->page)
	{
		gtk_widget_destroy(priv->page);
		priv->page = NULL;
	}
	if (priv->popup_menu)
	{
		gtk_widget_destroy(priv->popup_menu);
		priv->popup_menu = NULL;
	}
}


static void popup_update_item_click_cb(GtkWidget *button, AoTasks *t)
{
	ao_tasks_update(t, NULL);
}


static void popup_hide_item_click_cb(GtkWidget *button, AoTasks *t)
{
	keybindings_send_command(GEANY_KEY_GROUP_VIEW, GEANY_KEYS_VIEW_MESSAGEWINDOW);
}


static GtkWidget *create_popup_menu(AoTasks *t)
{
	GtkWidget *item, *menu;

	menu = gtk_menu_new();

	item = ui_image_menu_item_new(GTK_STOCK_REFRESH, _("_Update"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(popup_update_item_click_cb), t);

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	item = gtk_menu_item_new_with_mnemonic(_("_Hide Message Window"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(popup_hide_item_click_cb), t);

	return menu;
}


static void ao_tasks_show(AoTasks *t)
{
	GtkCellRenderer *text_renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;
	GtkTreeSortable *sortable;
	AoTasksPrivate *priv = AO_TASKS_GET_PRIVATE(t);

	priv->store = gtk_list_store_new(TLIST_COL_MAX,
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING);
	priv->tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(priv->store));

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->tree));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);

	/* selection handling */
	g_signal_connect(priv->tree, "button-press-event", G_CALLBACK(ao_tasks_button_press_cb), t);
	g_signal_connect(priv->tree, "key-press-event", G_CALLBACK(ao_tasks_key_press_cb), t);

	text_renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("File"));
	gtk_tree_view_column_pack_start(column, text_renderer, TRUE);
	gtk_tree_view_column_set_attributes(column, text_renderer, "text",
		TLIST_COL_DISPLAY_FILENAME, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(priv->tree), column);

	text_renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Line"));
	gtk_tree_view_column_pack_start(column, text_renderer, TRUE);
	gtk_tree_view_column_set_attributes(column, text_renderer, "text", TLIST_COL_LINE, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(priv->tree), column);

	text_renderer = gtk_cell_renderer_text_new();
	g_object_set(text_renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Task"));
	gtk_tree_view_column_pack_start(column, text_renderer, TRUE);
	gtk_tree_view_column_set_attributes(column, text_renderer, "text", TLIST_COL_NAME, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(priv->tree), column);

	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(priv->tree), TRUE);
	gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(priv->tree), TRUE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(priv->tree), TLIST_COL_DISPLAY_FILENAME);

	/* sorting */
	/* TODO improve sorting: sort by filename, then line number; make header clicks sort the data */
	sortable = GTK_TREE_SORTABLE(GTK_TREE_MODEL(priv->store));
	gtk_tree_sortable_set_sort_column_id(sortable, TLIST_COL_DISPLAY_FILENAME, GTK_SORT_ASCENDING);

	ui_widget_modify_font_from_string(priv->tree, geany->interface_prefs->tagbar_font);

	/* GTK 2.12 tooltips */
	if (gtk_check_version(2, 12, 0) == NULL)
		g_object_set(priv->tree, "has-tooltip", TRUE, "tooltip-column", TLIST_COL_TOOLTIP, NULL);

	/* scrolled window */
	priv->page = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(priv->page),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	gtk_container_add(GTK_CONTAINER(priv->page), priv->tree);

	gtk_widget_show_all(priv->page);
	gtk_notebook_append_page(
		GTK_NOTEBOOK(ui_lookup_widget(geany->main_widgets->window, "notebook_info")),
		priv->page,
		gtk_label_new(_("Tasks")));

	priv->popup_menu = create_popup_menu(t);

	/* initial update */
	ao_tasks_update(t, NULL);
}


void ao_tasks_activate(AoTasks *t)
{
	AoTasksPrivate *priv = AO_TASKS_GET_PRIVATE(t);

	if (priv->enable_tasks)
	{
		GtkNotebook *notebook = GTK_NOTEBOOK(
			ui_lookup_widget(geany->main_widgets->window, "notebook_info"));
		gint page_number = gtk_notebook_page_num(notebook, priv->page);

		gtk_notebook_set_current_page(notebook, page_number);
		gtk_widget_grab_focus(priv->tree);
	}
}


void ao_tasks_update(AoTasks *t, G_GNUC_UNUSED GeanyDocument *cur_doc)
{
	guint i, lines, line;
	gchar *line_buf, *context, *display_name, *tooltip;
	const gchar **token;
	GeanyDocument *doc;
	AoTasksPrivate *priv = AO_TASKS_GET_PRIVATE(t);

	/* TODO this could be improved to only update the currently loaded/saved document instead of
	 * iterating over all documents. */
	gtk_list_store_clear(priv->store);

	for (i = 0; i < geany->documents_array->len; i++)
	{
		doc = document_index(i);
		if (doc->is_valid)
		{
			display_name = document_get_basename_for_display(doc, -1);
			lines = sci_get_line_count(doc->editor->sci);
			for (line = 0; line < lines; line++)
			{
				line_buf = g_strstrip(sci_get_line(doc->editor->sci, line));
				token = tokens;
				while (*token != NULL)
				{
					if (NZV(*token) && strstr(line_buf, *token) != NULL)
					{
						context = g_strstrip(sci_get_line(doc->editor->sci, line + 1));
						setptr(context, g_strconcat(
							_("Context:"), "\n", line_buf, "\n", context, NULL));
						tooltip = g_markup_escape_text(context, -1);

						gtk_list_store_insert_with_values(priv->store, NULL, -1,
							TLIST_COL_FILENAME, DOC_FILENAME(doc),
							TLIST_COL_DISPLAY_FILENAME, display_name,
							TLIST_COL_LINE, line + 1,
							TLIST_COL_NAME, line_buf,
							TLIST_COL_TOOLTIP, tooltip,
							-1);
						g_free(context);
						g_free(tooltip);
					}
					token++;
				}
				g_free(line_buf);
			}
			g_free(display_name);
		}
	}
}


static void ao_tasks_init(AoTasks *self)
{
	AoTasksPrivate *priv = AO_TASKS_GET_PRIVATE(self);

	priv->page = NULL;
}


AoTasks *ao_tasks_new(gboolean enable)
{
	return g_object_new(AO_TASKS_TYPE, "enable-tasks", enable, NULL);
}
