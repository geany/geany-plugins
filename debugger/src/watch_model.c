/*
 *		watch_model.c
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
 *		Watch tree view model.
 * 		Functions for handling variables tree - expanding, refreshing etc.
 */

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif
#include <geanyplugin.h>

#include <string.h>
#include <gtk/gtk.h>

#include "watch_model.h"
#include "breakpoint.h"
#include "debug_module.h"

/* text for the stub item */
#define WATCH_CHILDREN_STUB "..."

extern dbg_module *active_module;

/*
 * removes all children from "parent"
 */
inline static void remove_children(GtkTreeModel *model, GtkTreeIter *parent)
{
	GtkTreeIter child;
	if (gtk_tree_model_iter_children(model, &child, parent))
	{
		while(gtk_tree_store_remove(GTK_TREE_STORE(model), &child))
			;
	}
}

/*
 * adds stub subitem to "parent"
 */
inline static void add_stub(GtkTreeStore *store, GtkTreeIter *parent)
{
	/* add subitem */
	GtkTreeIter stub;
	gtk_tree_store_prepend (store, &stub, parent);
	gtk_tree_store_set (store, &stub,
		W_NAME, WATCH_CHILDREN_STUB,
		W_VALUE, "",
		W_TYPE, "",
		W_INTERNAL, "",
	    W_EXPRESSION, "",
		W_STUB, FALSE,
		W_CHANGED, FALSE,
		W_VT, VT_NONE,
		-1);
	
	/* set W_STUB flag */
	gtk_tree_store_set (
		store,
		parent,
		W_STUB, TRUE,
		-1);
}

/*
 * insert all "vars" members to "parent" iterator in the "tree" as new children
 * mark_changed specifies whether to mark new items as beed changed
 * expand specifies whether to expand to the added children
 */
inline static void append_variables(GtkTreeView *tree, GtkTreeIter *parent, GList *vars,
	gboolean mark_changed, gboolean expand)
{
	int current_position = 0;
	GtkTreeModel *model = gtk_tree_view_get_model(tree);
	GtkTreeStore *store = GTK_TREE_STORE(model);
	GtkTreeIter child;
	GHashTable *ht = NULL;

	/* get existing rows */
	if (gtk_tree_model_iter_n_children(model, parent))
	{
		/* collect all existing rows */
		gtk_tree_model_iter_children(model, &child, parent);
		
		ht = g_hash_table_new_full(g_str_hash, g_str_equal, (GDestroyNotify)g_free, (GDestroyNotify)gtk_tree_row_reference_free);
		
		do
		{
			gchar *name = NULL;
			gtk_tree_model_get(model, &child, W_NAME, &name, -1);
			if (name && strlen(name))
			{
				GtkTreePath *path = gtk_tree_model_get_path(model, &child);
				g_hash_table_insert(ht, name, gtk_tree_row_reference_new(model, path));
				gtk_tree_path_free(path);
			}
		}
		while(gtk_tree_model_iter_next(model, &child));
	}
	
	while (vars)
	{
		variable *v = vars->data;
		
		GtkTreeRowReference *reference = NULL;
		if (ht && (reference = g_hash_table_lookup (ht, v->name->str)))
		{
			GtkTreePath *path = gtk_tree_row_reference_get_path(reference);
			if (current_position != gtk_tree_path_get_indices(path)[0])
			{
				/* move a row if not at it's place */
				GtkTreeIter iter;
				gtk_tree_model_get_iter(model, &iter, path);
				if (current_position)
				{
					GtkTreeIter insert_after;	
					gtk_tree_model_iter_nth_child(model, &insert_after, parent, current_position - 1);
					gtk_tree_store_move_after(store, &iter, &insert_after);
				}
				else
				{
					gtk_tree_store_move_after(store, &iter, NULL);
				}
			}

			gtk_tree_path_free(path);
		}
		else
		{
			gtk_tree_store_insert(store, &child, parent, current_position);
			gtk_tree_store_set (store, &child,
				W_NAME, v->name->str,
				W_VALUE, v->value->str,
				W_TYPE, v->type->str,
				W_INTERNAL, v->internal->str,
				W_EXPRESSION, v->expression->str,
				W_STUB, v->has_children,
				W_CHANGED, mark_changed,
				W_VT, v->vt,
				-1);
			
			/* expand to row if we were asked to */
			if (expand)
			{
				GtkTreePath *path = gtk_tree_model_get_path(model, &child);
				gtk_tree_view_expand_row(tree, path, FALSE);
				gtk_tree_path_free(path);
			}
			
			/* add stub if added child also have children */
			if (v->has_children)
				add_stub(store, &child);
		}
		
		/* move to next variable */
		vars = vars->next;
		current_position++;
	}
	
	if (ht)
	{
		g_hash_table_destroy(ht);
	}
}

/*
 * looks up a variable with corresponding name in the list
 */
inline static GList *lookup_variable(GList *vars, gchar *name)
{
	GList *var = vars;
	while (var)
	{
		variable *v = (variable*)var->data; 
		if (!strcmp(v->name->str, name))
			break;
		var = var->next;
	}
	
	return var;
}

/*
 * frees variable list: removes all data and destroys list
 * used for the lists returned by get_children call
 * who doesn't represent internal debugger module structures
 */
void free_variables_list(GList *vars)
{
	g_list_foreach(vars, (GFunc)variable_free, NULL);
	g_list_free(vars);
}

/*
 * updates iterator according to variable
 */
void update_variable(GtkTreeStore *store, GtkTreeIter *iter, variable *var, gboolean changed)
{
	gtk_tree_store_set (store, iter,
		W_NAME, var->name->str,
		W_VALUE, var->evaluated ? var->value->str : _("Can't evaluate expression"),
		W_TYPE, var->type->str,
		W_INTERNAL, var->internal->str,
		W_EXPRESSION, var->expression->str,
		W_STUB, FALSE,
		W_CHANGED, changed,
		W_VT, var->vt,
		-1);
} 

/*
 * remove stub item and add vars to parent iterator
 */
void expand_stub(GtkTreeView *tree, GtkTreeIter *parent, GList *vars)
{
	GtkTreeModel *model = gtk_tree_view_get_model(tree);
	GtkTreeStore *store = GTK_TREE_STORE(model);
	GtkTreeIter stub;
	gboolean changed;

	/* remember stub iterator */
	gtk_tree_model_iter_children(model, &stub, parent);

	/* check whether arent has been changed */
	gtk_tree_model_get(model, parent,
		W_CHANGED, &changed,
		-1);
	
	/* add all stuff from "vars" */
	append_variables(tree, parent, vars, changed, TRUE);
	
	/* remove stub item */
	gtk_tree_store_remove(store, &stub);
}

/*
 * change watch specified by "iter" as dscribed in "var"
 */
void change_watch(GtkTreeView *tree, GtkTreeIter *iter, gpointer var)
{
	GtkTreeModel *model = gtk_tree_view_get_model(tree);
	GtkTreeStore *store = GTK_TREE_STORE(model);
	
	variable *v = (variable*)var;

	/* update variable */
	update_variable(store, iter, v, FALSE);

	/* if item have children - remove them */ 		
	if (gtk_tree_model_iter_has_child(model, iter))
		remove_children(model, iter);
	
	/* if new watch has children - add stub */
	if (v->has_children)
		add_stub(store, iter);
}

/*
 * update root variables in "tree" according to the "vars" list
 * if root variables have expanded children - walk through them also 
 */
void update_variables(GtkTreeView *tree, GtkTreeIter *parent, GList *vars)
{
	/* tree model and store for the given tree */
	GtkTreeModel *model = gtk_tree_view_get_model(tree);
	GtkTreeStore *store = GTK_TREE_STORE(model);
	
	/* check whether "parent" has children nodes
	if NULL == parent - let's try to get iter to the first node in the tree */
	GtkTreeIter child;
	gboolean haschildren = FALSE;
	gboolean parent_changed = FALSE;
	if (parent)
	{
		gtk_tree_model_get (model, parent,
			W_CHANGED, &parent_changed,
			-1);
		haschildren = gtk_tree_model_iter_children(model, &child, parent);
	}
	else
		haschildren = gtk_tree_model_get_iter_first(model, &child);

	/* walk through all children of "parent" iterator */
	if (haschildren)
	{
		/* if have children - lets check and update their values */
		while (TRUE)
		{
			gchar *name;
			gchar *internal;
			gchar *value;
			GList *var;
			variable *v;
			gboolean changed;

			/* set variable value
			1. get the variable params */
			
			gtk_tree_model_get (
				model,
				&child,
				W_NAME, &name,
				W_INTERNAL, &internal,
				W_VALUE, &value,
				-1);
				
			/* miss empty row in watch tree */
			if (!strlen(name))
				break;
			
			/* 2. find this path is "vars" list */
			var = lookup_variable(vars, name);

			/* 3. check if we have found currect iterator */
			if (!var)
			{
				/* if we haven't - remove current and try to move to the next one
				in the same level */
				
				/* if gtk_tree_store_remove returns "true" - child is set to the next iterator,
				if "false" - it was the last - then, exit the loop */
				if (gtk_tree_store_remove(GTK_TREE_STORE(model), &child))
					continue;
				else
					break;
			}
			
			/* 4. update variable (type, value) */
			v = (variable*)var->data;
			changed = parent_changed || strcmp(value, v->value->str);
			update_variable(store, &child, v, changed && v->evaluated);
			
			/* 5. if item have children - process them */ 		
			if (gtk_tree_model_iter_has_child(model, &child))
			{
				if (!v->has_children)
				{
					/* if children are left from previous variable value - remove all children */
					remove_children(model, &child);
				}
				else
				{
					/* if row isn't expanded - add "..." item
					else - process all children */
					GtkTreePath *path = gtk_tree_model_get_path(model, &child);
					if (!gtk_tree_view_row_expanded(tree, path))
					{
						/* remove all children */
						remove_children(model, &child);
						/* add stub item */
						add_stub(store, &child); 
					}
					else
					{
						/* get children for "parent" item */
						GList *children = active_module->get_children(v->internal->str);
						/* update children */
						update_variables(tree, &child, g_list_copy(children));
						/* frees children list */
						free_variables_list(children);
					}
					gtk_tree_path_free(path);
				}
			}
			else if (v->has_children)
			{
				/* if tree item doesn't have children, but variable has
				(variable type has changed) - add stub */
				add_stub(store, &child); 
			}
			
			/* 6. free name, expression */ 
			g_free(name);
			g_free(internal);
			g_free(value);

			if (!gtk_tree_model_iter_next(model, &child))
				break;
		}
	}

	/* insert items that are left in "vars" list */
	append_variables(tree, parent, vars, !parent || parent_changed, TRUE);
	
	/* remove variables left in the list */
	g_list_free(vars);
}

/*
 * clear all root variables in "tree" removing their children if available
 */
void clear_watch_values(GtkTreeView *tree)
{
	GtkTreeModel *model = gtk_tree_view_get_model(tree);
	GtkTreeStore *store = GTK_TREE_STORE(model);

	GtkTreeIter child;
	if(!gtk_tree_model_get_iter_first(model, &child))
		return;
	
	do
	{
		/* if item have children - process them */ 		
		if (gtk_tree_model_iter_has_child(model, &child))
			remove_children(model, &child);
		
		/* set expression/value/type to empty string */
		gtk_tree_store_set (store, &child,
			W_VALUE, "",
			W_TYPE, "",
			W_INTERNAL, "",
			W_EXPRESSION, "",
			W_STUB, FALSE,
			W_CHANGED, FALSE,
			-1);
	}
	while (gtk_tree_model_iter_next(model, &child));
}

/*
 * set W_NAME field to "name" and all others to empty string
 */
void variable_set_name_only(GtkTreeStore *store, GtkTreeIter *iter, gchar *name)
{
	/* set value/type/internal to empty string */
	gtk_tree_store_set (store, iter,
		W_NAME, name,
		W_VALUE, "",
		W_TYPE, "",
		W_INTERNAL, "",
		W_EXPRESSION, "",
		W_STUB, FALSE,
		W_CHANGED, FALSE,
		W_VT, VT_WATCH,
		-1);
}

/*
 * get names of the root items in the tree view
 */
GList *get_root_items(GtkTreeView *tree)
{
	GtkTreeModel *model = gtk_tree_view_get_model(tree);
	GtkTreeIter child;
	GList *names = NULL;

	if(!gtk_tree_model_get_iter_first(model, &child))
		return NULL;
	
	do
	{
		gchar *name;
		gtk_tree_model_get (
			model,
			&child,
			W_NAME, &name,
			-1);
		
		if (strlen(name))
			names = g_list_append(names, name);
	}
	while (gtk_tree_model_iter_next(model, &child));
	
	return names;
}
