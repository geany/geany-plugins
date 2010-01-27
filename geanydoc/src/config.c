/*
 *  config.c
 *
 *  Copyright 2008 Yura Siamashka <yurand2@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>

#include "geanyplugin.h"

#include "geanydoc.h"

extern GeanyData *geany_data;
extern GeanyFunctions *geany_functions;

const gchar defaults[] =
	"[C]\n"
	"internal = false\n"
	"command0 = man -P \"col -b\" -S 2:3:5 '%w'\n"
	"command1 = devhelp -s '%w'\n"
	"[C++]\n"
	"internal = false\n"
	"command0 = man -P \"col -b\" -S 2:3:5 '%w'\n"
	"command1 = devhelp -s '%w'\n"
	"[PHP]\n"
	"internal = false\n"
	"command0 = firefox \"http://www.php.net/%w\"\n"
	"[Sh]\n"
	"internal = true\n"
	"command0 = man -P \"col -b\" -S 1:4:5:6:7:8:9 '%w'\n"
	"[Python]\n"
	"internal = true\n"
	"command0 = pydoc '%w'\n"
	"[None]\n"
	"internal = false\n" "command0 = firefox \"http://www.google.com/search?q=%w\"\n";

static GKeyFile *config = NULL;
static gchar *config_file = NULL;

void
config_init()
{
	config_file = g_build_filename(geany->app->configdir, "plugins", "geanydoc", NULL);
	utils_mkdir(config_file, TRUE);

	setptr(config_file, g_build_filename(config_file, "geanydoc.conf", NULL));

	config = g_key_file_new();
	if (!g_key_file_load_from_file(config, config_file, G_KEY_FILE_KEEP_COMMENTS, NULL))
	{
		g_key_file_load_from_data(config, defaults, sizeof(defaults),
					  G_KEY_FILE_KEEP_COMMENTS, NULL);
	}
}

void
config_uninit()
{
	g_free(config_file);
	config_file = NULL;
	g_key_file_free(config);
	config = NULL;
}

GKeyFile *
config_clone()
{
	GKeyFile *ret;
	gchar *txt = g_key_file_to_data(config, NULL, NULL);
	ret = g_key_file_new();
	g_key_file_load_from_data(ret, txt, strlen(txt), G_KEY_FILE_KEEP_COMMENTS, NULL);
	g_free(txt);
	return ret;
}

void
config_set(GKeyFile * cfg)
{
	gchar *data;

	g_key_file_free(config);
	config = cfg;

	data = g_key_file_to_data(config, NULL, NULL);
	utils_write_file(config_file, data);
	g_free(data);
}

gchar *
config_get_command(const gchar * lang, gint cmd_num, gboolean * intern)
{
	gchar *ret, *tmp;
	gchar *key = g_strdup_printf("command%d", cmd_num);
	ret = utils_get_setting_string(config, lang, key, "");
	g_free(key);
	if (!NZV(ret))
		return ret;
	key = g_strdup_printf("command%d", cmd_num + 1);
	tmp = utils_get_setting_string(config, lang, key, "");
	g_free(key);
	if (NZV(tmp))
		*intern = TRUE;
	else
		*intern = utils_get_setting_boolean(config, lang, "internal", FALSE);
	g_free(tmp);
	return ret;
}
