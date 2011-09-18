/*
 *
 *		pconfig.c
 *      
 *      Copyright 2011 Alexander Petukhov <devel(at)apetukhov.ru>
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
 *		Debug paned config.
 */
 
#include <sys/stat.h>

#include "geanyplugin.h"
extern GeanyFunctions	*geany_functions;
extern GeanyData		*geany_data;

#include "pconfig.h"
#include "tabs.h"

/* config file path */
static gchar *config_path = NULL;

/* config GKeyFile */
static GKeyFile *key_file = NULL;

/* saving interval */
#define SAVING_INTERVAL 2000000

/* saving thread staff */
static GMutex *change_config_mutex;
static GCond *cond;
static GThread *saving_thread;
static gboolean config_changed = FALSE;

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
		if (config_changed)
		{
			gchar *config_data = g_key_file_to_data(key_file, NULL, NULL);
			g_file_set_contents(config_path, config_data, -1, NULL);
			g_free(config_data);

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
 *	set default values in the GKeyFile
 */
void pconfig_set_defaults(GKeyFile *kf)
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
 *	initialize
 */
void pconfig_init()
{
	/* read config */
	gchar *config_dir = g_build_path(G_DIR_SEPARATOR_S, geany_data->app->configdir, "plugins", "debugger", NULL);
	config_path = g_build_path(G_DIR_SEPARATOR_S, config_dir, "debugger.conf", NULL);
	
	g_mkdir_with_parents(config_dir, S_IRUSR | S_IWUSR | S_IXUSR);
	g_free(config_dir);

	key_file = g_key_file_new();
	if (!g_key_file_load_from_file(key_file, config_path, G_KEY_FILE_NONE, NULL))
	{
		pconfig_set_defaults(key_file);
		gchar *data = g_key_file_to_data(key_file, NULL, NULL);
		g_file_set_contents(config_path, data, -1, NULL);
		g_free(data);
	}

	change_config_mutex = g_mutex_new();
	cond = g_cond_new();
	saving_thread = g_thread_create(saving_thread_func, NULL, TRUE, NULL);
}	

/*
 *	destroy
 */
void pconfig_destroy()
{
	g_cond_signal(cond);
	/* ??? g_thread_join(saving_thread); */	
	
	g_mutex_free(change_config_mutex);
	g_cond_free(cond);

	g_free(config_path);
}

/*
 *	set one or several config values
 */
void pconfig_set(int config_part, gpointer config_value, ...)
{
	g_mutex_lock(change_config_mutex);
	
	va_list ap;
	va_start(ap, config_value);
	
	while(config_part)
	{
		switch (config_part)
		{
			case CP_TABBED_MODE:
			{
				g_key_file_set_boolean(key_file, "tabbed_mode", "enabled", (gboolean)config_value);
				break;
			}
			case CP_OT_TABS:
			{
				int *array = (int*)config_value;
				g_key_file_set_integer_list(key_file, "one_panel_mode", "tabs", array + 1, array[0]);
				break;
			}
			case CP_OT_SELECTED:
			{
				g_key_file_set_integer(key_file, "one_panel_mode", "selected_tab_index", (int)config_value);
				break;
			}
			case CP_TT_LTABS:
			{
				int *array = (int*)config_value;
				g_key_file_set_integer_list(key_file, "two_panels_mode", "left_tabs", array + 1, array[0]);
				break;
			}
			case CP_TT_LSELECTED:
			{
				g_key_file_set_integer(key_file, "two_panels_mode", "left_selected_tab_index", (int)config_value);
				break;
			}
			case CP_TT_RTABS:
			{
				int *array = (int*)config_value;
				g_key_file_set_integer_list(key_file, "two_panels_mode", "right_tabs", array + 1, array[0]);
				break;
			}
			case CP_TT_RSELECTED:
			{
				g_key_file_set_integer(key_file, "two_panels_mode", "right_selected_tab_index", (int)config_value);
				break;
			}
		}
		
		config_part = va_arg(ap, int);
		if (config_part)
		{
			config_value = va_arg(ap, gpointer);
		}
	}
	
	config_changed = TRUE;
	g_mutex_unlock(change_config_mutex);
}

/*
 *	config parts getters
 */

gboolean pconfig_get_tabbed()
{
	return g_key_file_get_boolean(key_file, "tabbed_mode", "enabled", NULL);
}
int* pconfig_get_tabs(gsize *length)
{
	return g_key_file_get_integer_list(key_file, "one_panel_mode", "tabs", length, NULL);
}
int pconfig_get_selected_tab_index()
{
	return g_key_file_get_integer(key_file, "one_panel_mode", "selected_tab_index", NULL);
}
int* pconfig_get_left_tabs(gsize *length)
{
	return g_key_file_get_integer_list(key_file, "two_panels_mode", "left_tabs", length, NULL);
}
int pconfig_get_left_selected_tab_index()
{
	return g_key_file_get_integer(key_file, "two_panels_mode", "left_selected_tab_index", NULL);
}
int* pconfig_get_right_tabs(gsize *length)
{
	return g_key_file_get_integer_list(key_file, "two_panels_mode", "right_tabs", length, NULL);
}
int	pconfig_get_right_selected_tab_index()
{
	return g_key_file_get_integer(key_file, "two_panels_mode", "right_selected_tab_index", NULL);
}
