/*
 *      breakpoints.c
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
 * 		Functions for manipulatins breakpoints and quering breakpoints state.
 * 		Modifying functions do all job regarding markers and 
 * 		entries in breaks tree view in the debugger panel 
 */

#include <string.h>

#include "geanyplugin.h"
extern GeanyFunctions	*geany_functions;

#include "breakpoints.h"
#include "utils.h"
#include "markers.h"
#include "debug.h"
#include "bptree.h"
#include "dconfig.h"

/* container for break-for-file g_tree GTree-s */
GHashTable* files = NULL;

/*
 * Functions for breakpoint iteration support
 */

typedef void 	(*breaks_iterate_function)(void* bp);

/*
 * Iterates through GTree for the particular file
 */
gboolean tree_foreach_call_function(gpointer key, gpointer value, gpointer data)
{
	((breaks_iterate_function)data)(value);
	return FALSE;
}

/*
 * Iterates through hash table of GTree-s
 */
void hash_table_foreach_call_function(gpointer key, gpointer value, gpointer user_data)
{
	g_tree_foreach((GTree*)value, tree_foreach_call_function, user_data);
}

/*
 * Iterates through GTree
 * adding each item to GList that is passed through data variable
 */
gboolean tree_foreach_add_to_list(gpointer key, gpointer value, gpointer data)
{
	GList **list = (GList**)data;
	*list = g_list_append(*list, value);
	return FALSE;
}

/*
 * Iterates through hash table of GTree-s
 * calling list collection functions on each tree
 */
void hash_table_foreach_add_to_list(gpointer key, gpointer value, gpointer user_data)
{
	g_tree_foreach((GTree*)value, tree_foreach_add_to_list, user_data);
}

/*
 * functions to perform markers and tree vew operation when breakpoint
 * is finally updated/added/removed
 */
static void on_add(breakpoint *bp)
{
	/* add to breakpoints tab */
	bptree_add_breakpoint(bp);
	/* add marker */
	markers_add_breakpoint(bp);
}
static void on_remove(breakpoint *bp)
{
	/* remove marker */
	markers_remove_breakpoint(bp);
	/* remove from breakpoints tab */
	bptree_remove_breakpoint(bp);
	/* remove from internal storage */
	GTree *tree = g_hash_table_lookup(files, bp->file);
	g_tree_remove(tree, GINT_TO_POINTER(bp->line));
}
static void on_set_hits_count(breakpoint *bp)
{
	bptree_set_hitscount(bp);
	markers_remove_breakpoint(bp);
	markers_add_breakpoint(bp);
}
static void on_set_condition(breakpoint* bp)
{
	/* set condition in breaks tree */
	bptree_set_condition(bp);
	markers_remove_breakpoint(bp);
	markers_add_breakpoint(bp);
}
static void on_switch(breakpoint *bp)
{
	/* remove old and set new marker */
	markers_remove_breakpoint(bp);
	markers_add_breakpoint(bp);
	
	/* set checkbox in breaks tree */
	bptree_set_enabled(bp);
}
static void on_set_enabled_list(GList *breaks, gboolean enabled)
{
	GList *iter = breaks;
	while (iter)
	{
		breakpoint *bp = (breakpoint*)iter->data;
		
		if (bp->enabled ^ enabled)
		{
			bp->enabled = enabled;
			
			/* remove old and set new marker */
			markers_remove_breakpoint(bp);
			markers_add_breakpoint(bp);
			
			/* set checkbox in breaks tree */
			bptree_set_enabled(bp);
		}
		iter = iter->next;
	}
}
static void on_remove_list(GList *list)
{
	GList *iter;
	for (iter = list; iter; iter = iter->next)
	{
		on_remove((breakpoint*)iter->data);
	}
}

/*
 * Helper functions
 */

/*
 * compare pointers as integers
 * return value similar to strcmp
 * arguments:
 * 		a -	first integer
 * 		b -	second integer
 * 		user_data - not used
 */
gint compare_func(gconstpointer a, gconstpointer b, gpointer user_data)
{
	return GPOINTER_TO_INT(a) - GPOINTER_TO_INT(b);
}

/*
 * functions that are called when a breakpoint is altered while debuginng session is active.
 * Therefore, these functions try to alter break in debug session first and if successful -
 * do what on_... do or simply call on_... function directly
 */
static void breaks_add_debug(breakpoint* bp)
{
	if (debug_set_break(bp, BSA_NEW_BREAK))
	{
		/* add markers, update treeview */
		on_add(bp);
		/* mark config for saving */
		config_set_debug_changed();
	}
	else
		dialogs_show_msgbox(GTK_MESSAGE_ERROR, "%s", debug_error_message());
}
static void breaks_remove_debug(breakpoint* bp)
{
	if (debug_remove_break(bp))
	{
		/* remove markers, update treeview */
		on_remove(bp);
		/* mark config for saving */
		config_set_debug_changed();
	}
	else
		dialogs_show_msgbox(GTK_MESSAGE_ERROR, "%s", debug_error_message());
}
static void breaks_set_hits_count_debug(breakpoint* bp)
{
	if (debug_set_break(bp, BSA_UPDATE_HITS_COUNT))
	{
		on_set_hits_count(bp);
		/* mark config for saving */
		config_set_debug_changed();
	}
	else
		dialogs_show_msgbox(GTK_MESSAGE_ERROR, "%s", debug_error_message());
}
static void breaks_set_condition_debug(breakpoint* bp)
{
	if (debug_set_break(bp, BSA_UPDATE_CONDITION))
	{
		on_set_condition(bp);
		/* mark config for saving */
		config_set_debug_changed();
	}
	else
	{
		/* revert to old condition (taken from tree) */
		gchar* oldcondition = bptree_get_condition(bp);
		strcpy(bp->condition, oldcondition);
		g_free(oldcondition);
		/* show error message */
		dialogs_show_msgbox(GTK_MESSAGE_ERROR, "%s", debug_error_message());
	}
}
static void breaks_switch_debug(breakpoint* bp)
{
	if (debug_set_break(bp, BSA_UPDATE_ENABLE))
	{
		on_switch(bp);
		/* mark config for saving */
		config_set_debug_changed();
	}
	else
	{
		bp->enabled = !bp->enabled;
		dialogs_show_msgbox(GTK_MESSAGE_ERROR, "%s", debug_error_message());
	}
}
static void breaks_set_disabled_list_debug(GList *list)
{
	GList *iter;
	for (iter = list; iter; iter = iter->next)
	{
		breakpoint *bp = (breakpoint*)iter->data;
		if (bp->enabled)
		{
			bp->enabled = FALSE;
			if (debug_set_break(bp, BSA_UPDATE_ENABLE))
			{
				on_switch(bp);
			}
			else
			{
				bp->enabled = TRUE;
			}
		}
	}
	g_list_free(list);

	config_set_debug_changed();
}
static void breaks_set_enabled_list_debug(GList *list)
{
	GList *iter;
	for (iter = list; iter; iter = iter->next)
	{
		breakpoint *bp = (breakpoint*)iter->data;
		if (!bp->enabled)
		{
			bp->enabled = TRUE;
			if (debug_set_break(bp, BSA_UPDATE_ENABLE))
			{
				on_switch(bp);
			}
			else
			{
				bp->enabled = FALSE;
			}
		}
	}
	g_list_free(list);

	config_set_debug_changed();
}
static void breaks_remove_list_debug(GList *list)
{
	GList *iter;
	for (iter = list; iter; iter = iter->next)
	{
		breakpoint *bp = (breakpoint*)iter->data;
		if (debug_remove_break(bp))
		{
			on_remove((breakpoint*)iter->data);
		}
	}
	g_list_free(list);

	config_set_debug_changed();
}

/*
 * Init breaks related data.
 * arguments:
 * 		cb - callback to call on breakpoints tree view double click 
 */
gboolean breaks_init(move_to_line_cb cb)
{
	/* create breakpoints storage */
	files = g_hash_table_new_full(
		g_str_hash,
		g_str_equal,
		(GDestroyNotify)g_free,
		(GDestroyNotify)g_tree_destroy);

	/* create breaks tab page control */
	bptree_init(cb);

	return TRUE;
}

/*
 * Frees breaks related data.
 */
void breaks_destroy(void)
{
	/* remove all markers */
	GList *breaks, *iter;
	breaks = iter = breaks_get_all();
	while (iter)
	{
		markers_remove_breakpoint((breakpoint*)iter->data);
		iter = iter->next;
	}
	g_list_free(breaks);
	
	/* free storage */
	g_hash_table_destroy(files);
	
	/* destroy breaks tree data */
	bptree_destroy();
}

/*
 * Add new breakpoint.
 * arguments:
 * 		file - breakpoints filename
 * 		line - breakpoints line
 * 		condition - breakpoints line
 * 		enabled - is new breakpoint enabled
 * 		hitscount - breakpoints hitscount
 */
void breaks_add(const char* file, int line, char* condition, int enabled, int hitscount)
{
	/* do not process async break manipulation on modules
	that do not support async interuppt */
	enum dbs state = debug_get_state();
	if (DBS_RUNNING == state &&  !debug_supports_async_breaks())
		return;
	
	/* allocate memory */
	breakpoint* bp = break_new_full(file, line, condition, enabled, hitscount);
	
	/* check whether GTree for this file exists and create if doesn't */
	GTree *tree;
	if (!(tree = g_hash_table_lookup(files, bp->file)))
	{
		char *newfile = g_strdup(bp->file);
		tree = g_tree_new_full(compare_func, NULL, NULL, (GDestroyNotify)g_free);
		g_hash_table_insert(files, newfile, tree);
	}
	
	/* insert to internal storage */
	g_tree_insert(tree, GINT_TO_POINTER(bp->line), bp);

	/* handle creation instantly if debugger is idle or stopped
	and request debug module interruption overwise */
	if (DBS_IDLE == state)
	{
		on_add(bp);
		config_set_debug_changed();
	}
	else if (DBS_STOPPED == state)
		breaks_add_debug(bp);
	else if (DBS_STOP_REQUESTED != state)
		debug_request_interrupt((bs_callback)breaks_add_debug, (gpointer)bp);
}

/*
 * Remove breakpoint.
 * arguments:
 * 		file - breakpoints filename
 * 		line - breakpoints line
 */
void breaks_remove(const char* file, int line)
{
	/* do not process async break manipulation on modules
	that do not support async interuppt */
	enum dbs state = debug_get_state();
	if (DBS_RUNNING == state &&  !debug_supports_async_breaks())
		return;

	/* lookup for breakpoint */
	breakpoint* bp = NULL;
	if (!(bp = breaks_lookup_breakpoint(file, line)))
		return;

	/* handle removing instantly if debugger is idle or stopped
	and request debug module interruption overwise */
	if (DBS_IDLE == state)
	{
		on_remove(bp);
		config_set_debug_changed();
	}
	else if (DBS_STOPPED == state)
		breaks_remove_debug(bp);
	else if (DBS_STOP_REQUESTED != state)
		debug_request_interrupt((bs_callback)breaks_remove_debug, (gpointer)bp);
}

/*
 * Remove all breakpoints in the list.
 * arguments:
 * 		list - list f breakpoints
 */
void breaks_remove_list(GList *list)
{
	/* do not process async break manipulation on modules
	that do not support async interuppt */
	enum dbs state = debug_get_state();
	if (DBS_RUNNING == state &&  !debug_supports_async_breaks())
		return;

	/* handle removing instantly if debugger is idle or stopped
	and request debug module interruption overwise */
	if (DBS_IDLE == state)
	{
		on_remove_list(list);
		g_list_free(list);
		
		config_set_debug_changed();
	}
	else if (DBS_STOPPED == state)
		breaks_remove_list_debug(list);
	else if (DBS_STOP_REQUESTED != state)
		debug_request_interrupt((bs_callback)breaks_remove_list_debug, (gpointer)list);
}

/*
 * Removes all breakpoints.
 * arguments:
 */
void breaks_remove_all(void)
{
	g_hash_table_foreach(files, hash_table_foreach_call_function, (gpointer)on_remove);
	g_hash_table_remove_all(files);
}

/*
 * sets all breakpoints fo the file enabled or disabled.
 * arguments:
 * 		file - list of breakpoints
 * 		enabled - anble or disable breakpoints
 */
void breaks_set_enabled_for_file(const char *file, gboolean enabled)
{
	/* do not process async break manipulation on modules
	that do not support async interuppt */
	enum dbs state = debug_get_state();
	if (DBS_RUNNING == state &&  !debug_supports_async_breaks())
		return;

	GList *breaks = breaks_get_for_document(file);

	/* handle switching instantly if debugger is idle or stopped
	and request debug module interruption overwise */
	if (DBS_IDLE == state)
	{
		on_set_enabled_list(breaks, enabled);
		g_list_free(breaks);
		config_set_debug_changed();
	}
	else if (DBS_STOPPED == state)
		enabled ? breaks_set_enabled_list_debug(breaks) : breaks_set_disabled_list_debug(breaks);
	else if (DBS_STOP_REQUESTED != state)
		debug_request_interrupt((bs_callback)(enabled ? breaks_set_enabled_list_debug : breaks_set_disabled_list_debug), (gpointer)breaks);
}

/*
 * Switch breakpoints state.
 * arguments:
 * 		file - breakpoints filename
 * 		line - breakpoints line
 */
void breaks_switch(const char* file, int line)
{
	/* do not process async break manipulation on modules
	that do not support async interuppt */
	enum dbs state = debug_get_state();
	if (DBS_RUNNING == state &&  !debug_supports_async_breaks())
		return;

	/* lookup for breakpoint */
	breakpoint* bp = NULL;
	if (!(bp = breaks_lookup_breakpoint(file, line)))
		return;
	
	/* change activeness */
	bp->enabled = !bp->enabled;
	
	/* handle switching instantly if debugger is idle or stopped
	and request debug module interruption overwise */
	if (DBS_IDLE == state)
	{
		on_switch(bp);
		config_set_debug_changed();
	}
	else if (DBS_STOPPED == state)
		breaks_switch_debug(bp);
	else if (DBS_STOP_REQUESTED != state)
		debug_request_interrupt((bs_callback)breaks_switch_debug, (gpointer)bp);
}

/*
 * Set breakpoints hits count.
 * arguments:
 * 		file - breakpoints filename
 * 		line - breakpoints line
 * 		count - breakpoints hitscount
 */
void breaks_set_hits_count(const char* file, int line, int count)
{
	/* do not process async break manipulation on modules
	that do not support async interuppt */
	enum dbs state = debug_get_state();
	if (DBS_RUNNING == state &&  !debug_supports_async_breaks())
		return;

	/* lookup for breakpoint */
	breakpoint* bp = NULL;
	if (!(bp = breaks_lookup_breakpoint(file, line)))
		return;
	
	/* change hits count */
	bp->hitscount = count;
	
	/* handle setting hits count instantly if debugger is idle or stopped
	and request debug module interruption overwise */
	if (state == DBS_IDLE)
	{
		on_set_hits_count(bp);
		config_set_debug_changed();
	}
	else if(state == DBS_STOPPED)
		breaks_set_hits_count_debug(bp);
	else if (state != DBS_STOP_REQUESTED)
		debug_request_interrupt((bs_callback)breaks_set_hits_count_debug, (gpointer)bp);
}

/*
 * Set breakpoints condition.
 * arguments:
 * 		file - breakpoints filename
 * 		line - breakpoints line
 * 		condition - breakpoints line
 */
void breaks_set_condition(const char* file, int line, const char* condition)
{
	/* do not process async break manipulation on modules
	that do not support async interuppt */
	enum dbs state = debug_get_state();
	if (DBS_RUNNING == state &&  !debug_supports_async_breaks())
		return;

	/* lookup for breakpoint */
	breakpoint* bp = NULL;
	if (!(bp = breaks_lookup_breakpoint(file, line)))
		return;
	
	/* change condition */
	strcpy(bp->condition, condition);
	
	/* handle setting condition instantly if debugger is idle or stopped
	and request debug module interruption overwise */
	if (state == DBS_IDLE)
	{
		on_set_condition(bp);
		config_set_debug_changed();
	}
	else if (state == DBS_STOPPED)
		breaks_set_condition_debug(bp);
	else if (state != DBS_STOP_REQUESTED)
		debug_request_interrupt((bs_callback)breaks_set_condition_debug, (gpointer)bp);
}

/*
 * Moves a breakpoint from to another line
 * arguments:
 * 		file - breakpoints filename
 * 		line_from - old line number
 * 		line_to - new line number
 */
void breaks_move_to_line(const char* file, int line_from, int line_to)
{
	/* first look for the tree for the given file */
	GTree *tree = NULL;
	if ( (tree = g_hash_table_lookup(files, file)) )
	{
		/* lookup for the break in GTree*/
		breakpoint *bp = (breakpoint*)g_tree_lookup(tree, GINT_TO_POINTER(line_from));
		if (bp)
		{
			g_tree_steal(tree, GINT_TO_POINTER(line_from));
			bp->line = line_to;
			g_tree_insert(tree, GINT_TO_POINTER(line_to), bp);

			/* mark config for saving */
			config_set_debug_changed();
		}
	}
}

/*
 * Checks whether breakpoint is set.
 * arguments:
 * 		file - breakpoints filename
 * 		line - breakpoints line
 */
break_state	breaks_get_state(const char* file, int line)
{
	break_state bs = BS_NOT_SET;
	
	/* first look for the tree for the given file */
	GTree *tree;
	if ( (tree = g_hash_table_lookup(files, file)) )
	{
		breakpoint *bp = g_tree_lookup(tree, GINT_TO_POINTER(line));
		if (bp)
		{
			bs = bp->enabled ?  BS_ENABLED : BS_DISABLED;
		}
	}

	return bs;
}

/*
 * Get breakpoints GTree for the given file
 * arguments:
 * 		file - file name to get breaks for 
 */
GList* breaks_get_for_document(const char* file)
{
	GList *breaks = NULL;
	GTree *tree = g_hash_table_lookup(files, file);
	if (tree)
	{
		g_tree_foreach(tree, tree_foreach_add_to_list, &breaks);
	}
	return breaks;
}

/*
 * lookup for breakpoint
 * arguments:
 * 		file - breakpoints filename
 * 		line - breakpoints line
 */
breakpoint* breaks_lookup_breakpoint(const gchar* file, int line)
{
	breakpoint* bp = NULL;
	GTree* tree = NULL;
	if ( (tree = (GTree*)g_hash_table_lookup(files, file)) )
		bp = g_tree_lookup(tree, GINT_TO_POINTER(line));

	return bp;
}

/*
 * Gets all breakpoints
 * arguments:
 */
GList* breaks_get_all(void)
{
	GList *breaks  = NULL;
	g_hash_table_foreach(files, hash_table_foreach_add_to_list, &breaks);
	return breaks;
}
