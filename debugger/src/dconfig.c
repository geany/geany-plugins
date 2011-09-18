/*
 *
 *		dconfig.c
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
 *		Working with debugger configs.
 */

#include <sys/stat.h>
#include <memory.h>

#include "geanyplugin.h"
extern GeanyFunctions	*geany_functions;
extern GeanyPlugin		*geany_plugin;

#include "dconfig.h"
#include "breakpoints.h"
#include "debug.h"
#include "watch_model.h"
#include "wtree.h"
#include "tpage.h"
#include "bptree.h"

#define CONFIG_NAME ".debugger"

/* config file markers */
#define ENVIRONMENT_MARKER	"[ENV]"
#define BREAKPOINTS_MARKER	"[BREAK]"
#define WATCH_MARKER		"[WATCH]"

/* maximus config file line length */
#define MAXLINE 1000

/* saving interval */
#define SAVING_INTERVAL 2000000

/* saving thread staff */
static GMutex *change_config_mutex;
static GCond *cond;
static GThread *saving_thread;
static gboolean config_changed = FALSE;
static gboolean modifyable = FALSE;

/* the folder, config has been loaded from */
static gchar *current_folder = NULL;

/* forward declaration */
gboolean	dconfig_save(gchar *folder);

/*
 * reads line from a file
 */
int readline(FILE *file, gchar *buffer, int buffersize)
{
	gchar c;
	int read_count = 0;
	while (buffersize && fread(&c, 1, 1, file) && '\n' != c)
	{
		buffer[read_count++] = c;
		buffersize--;
	}
	buffer[read_count] = '\0';

	return read_count;
}

/*
 * function for a config file background saving if changed 
 */
static gpointer saving_thread_func(gpointer data)
{
	GTimeVal interval;
	GMutex *m = g_mutex_new();
	do
	{
		g_mutex_lock(change_config_mutex);
		if (config_changed && current_folder)
		{
			dconfig_save(current_folder);
			config_changed = FALSE;
		}
		g_mutex_unlock(change_config_mutex);

		g_get_current_time(&interval);
		g_time_val_add(&interval, SAVING_INTERVAL);
	}
	while (!g_cond_timed_wait(cond, m, &interval));
	g_mutex_free(m);
	
	return NULL;
}

/*
 * set "changed" flag to save it on "saving_thread" thread
 */
void dconfig_set_changed()
{
	if (!modifyable)
	{
		g_mutex_lock(change_config_mutex);
		config_changed = TRUE;
		g_mutex_unlock(change_config_mutex);
	}
}

/*
 * init config
 */
void dconfig_init()
{
	change_config_mutex = g_mutex_new();
	cond = g_cond_new();

	saving_thread = g_thread_create(saving_thread_func, NULL, TRUE, NULL);
}

/*
 * destroys config
 */
void dconfig_destroy()
{
	g_cond_signal(cond);
	/* ??? g_thread_join(saving_thread); */	
	
	g_mutex_free(change_config_mutex);
	g_cond_free(cond);
	
	if (current_folder)
	{
		g_free(current_folder);
		current_folder = NULL;
	}
}

/*
 * checks whether a config file is founs in the folder
 */
gboolean dconfig_is_found_at(gchar *folder)
{
	gchar *config = g_build_path(G_DIR_SEPARATOR_S, folder, CONFIG_NAME, NULL);
	gboolean res = g_file_test(config, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR);
	g_free(config);

	return res;
}

/*
 * loads config from a folder
 */
gboolean dconfig_load(gchar *folder)
{
	modifyable = TRUE;
	g_mutex_lock(change_config_mutex);

	tpage_clear();
	wtree_remove_all();
	breaks_remove_all();
	
	if (current_folder)
	{
		g_free(current_folder);
	}
	current_folder = g_strdup(folder);
	
	gchar *path = g_build_path(G_DIR_SEPARATOR_S, folder, CONFIG_NAME, NULL);
	FILE *file = fopen(path, "r");
	if (!file)
	{
		config_changed = FALSE;

		modifyable = FALSE;
		g_mutex_unlock(change_config_mutex);

		return FALSE;
	}

	/* target */
	gchar buffer[FILENAME_MAX];
	if(readline(file, buffer, FILENAME_MAX - 1))
	{
		tpage_set_target(buffer);
	}

	/* debugger */
	gchar debugger[FILENAME_MAX];
	if(readline(file, debugger, FILENAME_MAX - 1))
	{
		tpage_set_debugger(debugger);
	}
		
	/* arguments */
	if(readline(file, buffer, FILENAME_MAX - 1))
	{
		tpage_set_commandline(buffer);
	}

	/* breakpoints and environment variables */
	gchar line[MAXLINE];
	while (readline(file, line, MAXLINE))
	{
		if (!strcmp(line, BREAKPOINTS_MARKER))
		{
			/* file */
			gchar _path[FILENAME_MAX];
			readline(file, _path, MAXLINE);
			
			/* line */
			int nline;
			readline(file, line, MAXLINE);
			sscanf(line, "%i", &nline);
			
			/* hitscount */
			int hitscount;
			readline(file, line, MAXLINE);
			sscanf(line, "%d", &hitscount);

			/* condition */
			gchar condition[MAXLINE];
			readline(file, condition, MAXLINE);

			/* enabled */
			gboolean enabled;
			readline(file, line, MAXLINE);
			sscanf(line, "%d", &enabled);
			
			/* check whether file is available */
			struct stat st;
			if(!stat(_path, &st))
			{
				breaks_add(_path, nline, condition, enabled, hitscount);
			}
		}
		else if (!strcmp(line, ENVIRONMENT_MARKER))
		{
			gchar name[MAXLINE], value[1000];
			if(readline(file, name, MAXLINE) && readline(file, value, MAXLINE))
			{
				tpage_add_environment(name, value);
			}
		}
		else if (!strcmp(line, WATCH_MARKER))
		{
			gchar watch[MAXLINE];
			if(readline(file, watch, MAXLINE))
			{
				wtree_add_watch(watch);
			}
		}
	}
	
	bptree_update_file_nodes();

	config_changed = FALSE;

	modifyable = FALSE;
	g_mutex_unlock(change_config_mutex);
	
	return TRUE;
}

/*
 * saves config to a folder
 */
gboolean dconfig_save(gchar *folder)
{
	/* open config file */
	gchar *config_file = g_build_path(G_DIR_SEPARATOR_S, folder, CONFIG_NAME, NULL);		

	FILE *config = fopen(config_file, "w");
	if (config)
	{
		/* get target */
		const gchar *_target = tpage_get_target();
		fprintf(config, "%s\n", _target);
		
		/* debugger type */
		gchar *debugger = tpage_get_debugger();
		fprintf(config, "%s\n", debugger);
		
		/* get command line arguments */
		gchar *_args = tpage_get_commandline();
		fprintf(config, "%s\n", _args);
		g_free(_args);

		/* environment */
		GList *_env = tpage_get_environment();
		GList *iter = _env;
		
		while(iter)
		{
			gchar *name = (gchar*)iter->data;
			iter = iter->next;
			gchar *value = (gchar*)iter->data;

			fprintf(config, "%s\n", ENVIRONMENT_MARKER);
			fprintf(config, "%s\n%s\n", name, value);

			iter = iter->next;
		}
		
		/* breakpoints */
		GList *_breaks = breaks_get_all();
		GList *biter = _breaks;
		while (biter)
		{
			breakpoint *bp = (breakpoint*)biter->data;
			
			fprintf(config, "%s\n", BREAKPOINTS_MARKER);
			fprintf(config, "%s\n%i\n%i\n%s\n%i\n",
				bp->file, bp->line, bp->hitscount, bp->condition, bp->enabled);
					
			biter = biter->next;
		}
		g_list_free(_breaks);
		_breaks = NULL;
		
		/* watches */
		GList *cur_watches = wtree_get_watches();
		biter = cur_watches;
		while (biter)
		{
			gchar *watch = (gchar*)biter->data;
			
			fprintf(config, "%s\n", WATCH_MARKER);
			fprintf(config, "%s\n", watch);
					
			biter = biter->next;
		}
		g_list_foreach(cur_watches, (GFunc)g_free, NULL);
		g_list_free(cur_watches);
		cur_watches = NULL;

		fclose(config);
	}

	g_free(config_file);

	return config ? TRUE : FALSE;
}
