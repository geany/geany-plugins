/*
 *
 *		dconfig.c
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
 *		Working with debugger configs.
 */

#include <sys/stat.h>
#include <memory.h>

#include "geanyplugin.h"
extern GeanyFunctions	*geany_functions;
extern GeanyPlugin		*geany_plugin;

#include "dconfig.h"
#include "breakpoint.h"
#include "debug.h"
#include "watch_model.h"
#include "wtree.h"
#include "breakpoints.h"
#include "tpage.h"

#define CONFIG_NAME ".debugger"

/* config file markers */
#define ENVIRONMENT_MARKER	"[ENV]"
#define BREAKPOINTS_MARKER	"[BREAK]"
#define WATCH_MARKER		"[WATCH]"

/* maximus config file line length */
#define MAXLINE 1000

static gchar	*target = NULL;
static int		module = -1; 
static gchar	*args = NULL;
static GList	*env = NULL;
static GList	*breaks = NULL;
static GList	*watches = NULL;

/*
 * reads line from a file
 */
int readline(FILE *file, gchar *buffer, int buffersize)
{
	gchar c;
	int read = 0;
	while (buffersize && fread(&c, 1, 1, file) && '\n' != c)
	{
		buffer[read++] = c;
		buffersize--;
	}
	buffer[read] = '\0';

	return read;
} 

/*
 * checks whether a config fileis founs in the folder
 */
gboolean 	dconfig_is_found_at(gchar *folder)
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
	dconfig_clear();
	
	gchar *path = g_build_path(G_DIR_SEPARATOR_S, folder, CONFIG_NAME, NULL);
	FILE *file = fopen(path, "r");
	if (!file)
	{
		return FALSE;
	}

	/* target */
	gchar buffer[FILENAME_MAX];
	if(!readline(file, buffer, FILENAME_MAX - 1))
	{
		memset(target, 0, FILENAME_MAX * sizeof(gchar));
	}
	target = g_strdup(buffer);

	/* debugger */
	gchar debugger[FILENAME_MAX];
	if(readline(file, debugger, FILENAME_MAX - 1))
	{
		module = debug_get_module_index(debugger);
		if (-1 == module)
		{
			module = 0;
		}
	}
		
	/* arguments */
	if(!readline(file, buffer, FILENAME_MAX - 1))
	{
		memset(buffer, 0, FILENAME_MAX * sizeof(gchar));
	}
	args = g_strdup(buffer);

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
				breakpoint *bp = break_new_full(_path, nline, condition, enabled, hitscount);
				breaks = g_list_append(breaks, bp);
			}
		}
		else if (!strcmp(line, ENVIRONMENT_MARKER))
		{
			gchar name[MAXLINE], value[1000];
			if(readline(file, name, MAXLINE) && readline(file, value, MAXLINE))
			{
				env = g_list_append(env, name);
				env = g_list_append(env, value);
			}
		}
		else if (!strcmp(line, WATCH_MARKER))
		{
			gchar watch[MAXLINE];
			if(readline(file, watch, MAXLINE))
			{
				watches = g_list_append(watches, g_strdup(watch));
			}
		}
	}
	
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

	return (gboolean)config;
}

/*
 * gets target
 */
gchar* dconfig_target_get()
{
	return target;
}

/*
 * sets target
 */
void dconfig_target_set(gchar *newvalue)
{
	if (target)
	{
		g_free(target);
	}
	target = newvalue;
}

/*
 * gets debugger module index
 */
int dconfig_module_get()
{
	return module;
}

/*
 * sets debugger module index
 */
void dconfig_module_set(int newvalue)
{
	module = newvalue;
}

/*
 * gets command line arguments
 */
gchar* dconfig_args_get()
{
	return args;
}

/*
 * sets command line arguments
 */
void dconfig_args_set(gchar *newvalue)
{
	if (args)
	{
		g_free(args);
	}
	args = newvalue;
}

/*
 * gets environment variables
 */
GList* dconfig_env_get()
{
	return env;
}

/*
 * removes all environment variables
 */
void dconfig_env_clear()
{
	g_list_foreach(env, (GFunc)g_free, NULL);
	g_list_free(env);
	env = NULL;
}

/*
 * gets breakpoints
 */
GList*	dconfig_breaks_get()
{
	return breaks;
}

/*
 * clears breakpoints
 */
void dconfig_breaks_clear()
{
	g_list_foreach(breaks, (GFunc)g_free, NULL);
	g_list_free(breaks);
	breaks = NULL;
}

/*
 * gets watches
 */
GList* dconfig_watches_get()
{
	return watches;
}

/*
 * clears watches
 */
void dconfig_watches_clear()
{
	g_list_foreach(watches, (GFunc)g_free, NULL);
	g_list_free(watches);
	watches = NULL;
}

/*
 * clears all config values
 */
void dconfig_clear()
{
	dconfig_target_set(NULL);
	dconfig_module_set(0);
	dconfig_args_set(NULL);
	dconfig_env_clear();

	dconfig_breaks_clear();
	dconfig_watches_clear();
}
