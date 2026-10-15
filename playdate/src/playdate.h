/*
 *      playdate.h - Geany Playdate plugin
 *
 *      Copyright (C) 2026 geany-playdate contributors
 *
 *      This program is free software: you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, either version 3 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      See LICENSE and THIRD_PARTY.md.
 */

#ifndef PLAYDATE_PLUGIN_H
#define PLAYDATE_PLUGIN_H 1

#include <geanyplugin.h>

/* Provided by playdate.c; Geany's GEANY() macro expects geany_data. */
extern GeanyPlugin *geany_plugin;
extern GeanyData *geany_data;

#define PD_PLUGIN_NAME        "Playdate"
#define PD_PLUGIN_VERSION     "0.1.0"
#define PD_PLUGIN_DESCRIPTION "Build and run Playdate games (pdc + Simulator)"
#define PD_PLUGIN_AUTHOR      "Davi Campos Gonçalves"

#define PD_CONFIG_GROUP       "playdate"
#define PD_DEFAULT_SOURCE     "Source"
#define PD_DEFAULT_OUTPUT     "Output.pdx"

typedef struct
{
	gchar *sdk_path;      /* empty = use PLAYDATE_SDK_PATH */
	gchar *source;        /* relative to project root, default Source */
	gchar *output;        /* relative to project root, default Output.pdx */
	gchar *product_name;  /* optional, informational */

	/* pdc flags */
	gboolean strip;
	gboolean no_compress;
	gboolean verbose;
	gboolean quiet;
	gboolean skip_unknown;
} PDConfig;

typedef struct
{
	GeanyPlugin *plugin;
	PDConfig config;
} PDData;

/* config.c */
void pd_config_init_defaults(PDConfig *cfg);
void pd_config_load(PDConfig *cfg);
void pd_config_save(const PDConfig *cfg);
void pd_config_free(PDConfig *cfg);
gchar *pd_config_get_config_path(void);

/* Returns a newly allocated SDK path (env first, then config), or NULL. */
gchar *pd_resolve_sdk_path(const PDConfig *cfg);

/* Returns a newly allocated project root for the current document, or NULL. */
gchar *pd_resolve_project_root(void);

/* Fully resolved source / output paths (caller frees). */
gchar *pd_resolve_source_path(const PDConfig *cfg);
gchar *pd_resolve_output_path(const PDConfig *cfg);

/* build.c */
void pd_build_project(PDData *data, gboolean run_after);
void pd_run_simulator(PDData *data);
void pd_clean_output(PDData *data);
void pd_open_simulator_only(PDData *data);

/* errors.c */
void pd_errors_begin(void);
void pd_errors_parse_line(const gchar *line);
void pd_errors_end(gint exit_status);

/* tags.c */
gboolean pd_tags_install(void);

#endif /* PLAYDATE_PLUGIN_H */
