/*
 *      breakpoints.c
 *      
 *      Copyright 2010 Alexander Petukhov <Alexander(dot)Petukhov(at)mail(dot)ru>
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

#include "breakpoint.h"
#include "breakpoints.h"
#include "utils.h"
#include "markers.h"
#include "debug.h"
#include "bptree.h"

/* container for break-for-file g_tree GTree-s */
GHashTable* files = NULL;

/*
 * Functions for breakpoint iteration support
 */

/*
 * Iterates through GTree for the particular file
 */
gboolean tree_foreach(gpointer key, gpointer value, gpointer data)
{
	((breaks_iterate_function)data)(value);
	return FALSE;
}

/*
 * Iterates through hash table of GTree-s
 */
void hash_table_foreach(gpointer key, gpointer value, gpointer user_data)
{
	g_tree_foreach((GTree*)value, tree_foreach, user_data);
}

/*
 * Helper functions
 */

/*
 * lookup for breakpoint
 * arguments:
 * 		file - breakpoints filename
 * 		line - breakpoints line
 */
breakpoint* lookup_breakpoint(gchar* file, int line)
{
	breakpoint* bp = NULL;
	GTree* tree = NULL;
	if (tree = (GTree*)g_hash_table_lookup(files, file))
		bp = g_tree_lookup(tree, (gconstpointer)line);

	return bp;
}

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
	if (a == b)
		return 0;
	else
		return a > b ? 1 : -1; 
}

/*
 * Handlers for breakpoints activities.
 * Are called directly from breaks_set_... functions
 * or from async_callback if break was set asyncronously.
 * 
 */

/*
 * handles new break creation
 * arguments:
 * 		bp		- breakpoint to handle
 * 		success - success of operation
 */
void handle_break_new(breakpoint* bp, gboolean success)
{
	if (success)
	{
		/* add to breakpoints tab */
		bptree_add_breakpoint(bp);
		/* add marker */
		markers_add_breakpoint(bp);
	}
	else
		dialogs_show_msgbox(GTK_MESSAGE_ERROR, debug_error_message());
}

/*
 * handles break removing
 * arguments:
 * 		bp		- breakpoint to handle
 * 		success - success of operation
 */
void handle_break_remove(breakpoint* bp, gboolean success)
{
	if (success)
	{
		/* remove marker */
		markers_remove_breakpoint(bp);
		/* remove from breakpoints tab */
		bptree_remove_breakpoint(bp);
		/* remove from internal storage */
		GTree *tree = g_hash_table_lookup(files,bp->file);
		g_tree_remove(tree, (gconstpointer)bp->line);
	}
	else
		dialogs_show_msgbox(GTK_MESSAGE_ERROR, debug_error_message());
}

/*
 * handles breakpoints hits count set
 * arguments:
 * 		bp		- breakpoint to handle
 * 		success - success of operation
 */
void handle_hitscount_set(breakpoint* bp, gboolean success)
{
	if (success)
		bptree_set_hitscount(bp->iter, bp->hitscount);
	else
		dialogs_show_msgbox(GTK_MESSAGE_ERROR, debug_error_message());
}

/*
 * handles breakpoints condition set
 * arguments:
 * 		bp		- breakpoint to handle
 * 		success - success of operation
 */
void handle_condition_set(breakpoint* bp, gboolean success)
{
	if (success)
	{
		/* set condition in breaks tree */
		bptree_set_condition(bp->iter, bp->condition);
	}
	else
	{
		/* revert to old condition (taken from tree) */
		gchar* oldcondition = bptree_get_condition(bp->iter);
		strcpy(bp->condition, oldcondition);
		g_free(oldcondition);
		/* show error message */
		dialogs_show_msgbox(GTK_MESSAGE_ERROR, debug_error_message());
	}
}

/*
 * handles breakpoints enabled/disabled switch
 * arguments:
 * 		bp		- breakpoint to handle
 * 		success - success of operation
 */
void handle_switch(breakpoint* bp, gboolean success)
{
	/* remove old and set new marker */
	if (bp->enabled)
	{
		markers_remove_breakpoint_disabled(bp->file, bp->line);
		markers_add_breakpoint(bp);
	}
	else
	{
		markers_remove_breakpoint(bp);
		markers_add_breakpoint_disabled(bp->file, bp->line);
	}
	/* set checkbox in breaks tree */
	bptree_set_enabled(bp->iter, bp->enabled);
}

/*
 * Async action callback.
 * Called from debug module, when debugging session is interrupted
 * and ready to perform action specified by "bsa" with "bp" breakpoint
 * (activity type is passed to debug module when calling its "request_interrupt" function
 * and then is passed to "async_callback")
 * arguments:
 * 		bp	- breakpoint to handle
 * 		bsa	- type of activity (remove/add/change property)
 */
void async_callback(breakpoint* bp, break_set_activity bsa)
{
	/* perform requested operation */
	gboolean success = BSA_REMOVE == bsa ? debug_remove_break(bp) : debug_set_break(bp, bsa);
	
	/* handle relevant case */
	if (BSA_REMOVE == bsa)
		handle_break_remove(bp, success);
	else if (BSA_NEW_BREAK == bsa)
		handle_break_new(bp, success);
	else if (BSA_UPDATE_ENABLE == bsa)
		handle_switch(bp, success);
	else if (BSA_UPDATE_HITS_COUNT == bsa)
		handle_hitscount_set(bp, success);
	else if (BSA_UPDATE_CONDITION == bsa)
		handle_condition_set(bp, success);
	
	/* resume debug session */
	debug_run();
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
void breaks_destroy()
{
	/* remove all markers */
	breaks_iterate((breaks_iterate_function)markers_remove_breakpoint);
	/* free storage */
	g_hash_table_destroy(files);
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
void breaks_add(char* file, int line, char* condition, int enabled, int hitscount)
{
	/* do not process async break manipulation on modules
	that do not support async interuppt */
	enum dbs state = debug_get_state();
	if (DBS_RUNNING == state &&  !debug_supports_async_breaks())
		return;
	
	/* allocate memory */
	breakpoint* bp = break_new_full(file, line, condition, enabled, hitscount);
	
	/* check whether GTree for this file exists and create if not*/
	GTree *tree;
	if (!(tree = g_hash_table_lookup(files, bp->file)))
	{
		char *newfile = g_strdup(bp->file);
		tree = g_tree_new_full(compare_func, NULL, NULL, (GDestroyNotify)g_free);
		g_hash_table_insert(files, newfile, tree);
	}
	
	/* insert to internal storage */
	g_tree_insert(tree, (gpointer)bp->line, (gpointer)bp);

	/* handle creation instantly if debugger is idle or stopped
	and request debug module interruption overwise */
	if (DBS_IDLE == state || DBS_STOPPED == state)
		handle_break_new(bp, DBS_IDLE == state || debug_set_break(bp, BSA_NEW_BREAK));
	else if (DBS_STOP_REQUESTED != state)
		debug_request_interrupt(async_callback, bp, BSA_NEW_BREAK);
}

/*
 * Remove breakpoint.
 * arguments:
 * 		file - breakpoints filename
 * 		line - breakpoints line
 */
void breaks_remove(char* file, int line)
{
	/* do not process async break manipulation on modules
	that do not support async interuppt */
	enum dbs state = debug_get_state();
	if (DBS_RUNNING == state &&  !debug_supports_async_breaks())
		return;

	/* lookup for breakpoint */
	breakpoint* bp = NULL;
	if (!(bp = lookup_breakpoint(file, line)))
		return;

	/* handle removing instantly if debugger is idle or stopped
	and request debug module interruption overwise */
	if (DBS_IDLE == state || DBS_STOPPED == state)
		handle_break_remove(bp, DBS_IDLE == state || debug_remove_break(bp));
	else if (DBS_STOP_REQUESTED != state)
		debug_request_interrupt(async_callback, bp, BSA_REMOVE);
}

/*
 * Switch breakpoints state.
 * arguments:
 * 		file - breakpoints filename
 * 		line - breakpoints line
 */
void breaks_switch(char* file, int line)
{
	/* do not process async break manipulation on modules
	that do not support async interuppt */
	enum dbs state = debug_get_state();
	if (DBS_RUNNING == state &&  !debug_supports_async_breaks())
		return;

	/* lookup for breakpoint */
	breakpoint* bp = NULL;
	if (!(bp = lookup_breakpoint(file, line)))
		return;
	
	/* change activeness */
	bp->enabled = !bp->enabled;
	
	/* handle switching instantly if debugger is idle or stopped
	and request debug module interruption overwise */
	if (DBS_IDLE == state || DBS_STOPPED == state)
		handle_switch(bp, state == DBS_IDLE || debug_set_break(bp, BSA_UPDATE_ENABLE));
	else if (DBS_STOP_REQUESTED != state)
		debug_request_interrupt(async_callback, bp, BSA_UPDATE_ENABLE);
}

/*
 * Set breakpoints hits count.
 * arguments:
 * 		file - breakpoints filename
 * 		line - breakpoints line
 * 		count - breakpoints hitscount
 */
void breaks_set_hits_count(char* file, int line, int count)
{
	/* do not process async break manipulation on modules
	that do not support async interuppt */
	enum dbs state = debug_get_state();
	if (DBS_RUNNING == state &&  !debug_supports_async_breaks())
		return;

	/* lookup for breakpoint */
	breakpoint* bp = NULL;
	if (!(bp = lookup_breakpoint(file, line)))
		return;
	
	/* change hits count */
	bp->hitscount = count;
	
	/* handle setting hits count instantly if debugger is idle or stopped
	and request debug module interruption overwise */
	if (state == DBS_IDLE || state == DBS_STOPPED)
		handle_hitscount_set(bp, state == DBS_IDLE || debug_set_break(bp, BSA_UPDATE_HITS_COUNT));
	else if (state != DBS_STOP_REQUESTED)
		debug_request_interrupt(async_callback, bp, BSA_UPDATE_HITS_COUNT);
}

/*
 * Set breakpoints condition.
 * arguments:
 * 		file - breakpoints filename
 * 		line - breakpoints line
 * 		condition - breakpoints line
 */
void breaks_set_condition(char* file, int line, char* condition)
{
	/* do not process async break manipulation on modules
	that do not support async interuppt */
	enum dbs state = debug_get_state();
	if (DBS_RUNNING == state &&  !debug_supports_async_breaks())
		return;

	/* lookup for breakpoint */
	breakpoint* bp = NULL;
	if (!(bp = lookup_breakpoint(file, line)))
		return;
	
	/* change condition */
	strcpy(bp->condition, condition);
	
	/* handle setting condition instantly if debugger is idle or stopped
	and request debug module interruption overwise */
	if (state == DBS_IDLE || state == DBS_STOPPED)
		handle_condition_set(bp, DBS_IDLE == state || debug_set_break(bp, BSA_UPDATE_CONDITION));
	else if (state != DBS_STOP_REQUESTED)
		debug_request_interrupt(async_callback, bp, BSA_UPDATE_CONDITION);
}

/*
 * Checks whether breakpoint is set.
 * arguments:
 * 		file - breakpoints filename
 * 		line - breakpoints line
 */
gboolean breaks_is_set(char* file, int line)
{
	/* first look for the tree for the given file */
	GTree *tree;
	if (!(tree = g_hash_table_lookup(files, file)))
		return FALSE;
	else
	{
		/* lookup for the break in GTree*/
		gpointer p = g_tree_lookup(tree, (gconstpointer)line);
		return p && ((breakpoint*)p)->enabled;
	}
}

/*
 * Get breakpoints GTree for the given file
 * arguments:
 * 		file - file name to get breaks for 
 */
GTree *breaks_get_for_document(char* file)
{
	return g_hash_table_lookup(files, file);
}

/*
 * Get breakpoints widget
 */
GtkWidget* breaks_get_widget()
{
	return bptree_get_widget();
}

/*
 * Calls specified function on every breakpoint
 * arguments:
 * 		bif - function to call 
 */
void breaks_iterate(breaks_iterate_function bif)
{
	g_hash_table_foreach(files, hash_table_foreach, (gpointer)bif);
}
