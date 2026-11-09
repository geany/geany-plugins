/*
 * geanyenv.c — Geany plugin: per-project environment variables
 *
 * Reads <project_base_path>/geany.env on project open and sets the
 * listed environment variables via g_setenv(), making them available
 * to ALL spawned processes (treebrowser launches, file tools,
 * geanycli VTE terminals, geanyagent terminal, etc.).
 *
 * File format (geany.env):
 *   # comment lines (ignored)
 *   KEY=value
 *   JAVA_HOME=/opt/java/25
 *   PATH=${JAVA_HOME}/bin:${MAVEN_HOME}/bin:${PATH}
 *
 * ${VAR} and $VAR expansions are resolved left-to-right against the
 * already-updated environment so later lines can reference earlier ones.
 *
 * On project close the environment is restored to its previous state.
 *
 * Copyright 2025 teknopaul
 * License: GPL-2.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <geanyplugin.h>

GeanyPlugin  *geany_plugin;
GeanyData    *geany_data;

#define ENV_FILENAME "geany.env"

/* Maps gchar *key → gchar *old_value (NULL if the var was unset before). */
static GHashTable *saved_env = NULL;


/* ------------------------------------------------------------------ */
/* String expansion: replace ${VAR} and $VAR with g_getenv() values   */

static gchar *expand_vars(const gchar *value)
{
    GString *result = g_string_new(NULL);
    const gchar *p = value;

    while (*p) {
        if (*p == '$') {
            p++;
            gboolean braced = (*p == '{');
            if (braced) p++;

            const gchar *start = p;
            while (*p && (braced ? *p != '}' : (g_ascii_isalnum(*p) || *p == '_')))
                p++;

            gchar *varname = g_strndup(start, p - start);
            if (braced && *p == '}')
                p++;

            const gchar *val = g_getenv(varname);
            if (val)
                g_string_append(result, val);
            g_free(varname);
        } else {
            g_string_append_c(result, *p);
            p++;
        }
    }

    return g_string_free(result, FALSE);
}


/* ------------------------------------------------------------------ */
/* Load geany.env and apply to the process environment                */

static void env_load(const gchar *base_path)
{
    if (!base_path || !*base_path)
        return;

    gchar *path = g_build_filename(base_path, ENV_FILENAME, NULL);
    gchar *contents = NULL;

    if (!g_file_get_contents(path, &contents, NULL, NULL)) {
        g_free(path);
        return;
    }
    g_free(path);

    /* Lazily create the save table */
    if (!saved_env)
        saved_env = g_hash_table_new_full(g_str_hash, g_str_equal,
                                          g_free, g_free);
    else
        g_hash_table_remove_all(saved_env);

    gchar **lines = g_strsplit(contents, "\n", -1);
    g_free(contents);

    for (gint i = 0; lines[i]; i++) {
        gchar *line = g_strstrip(lines[i]);

        /* skip comments and empty lines */
        if (!*line || *line == '#')
            continue;

        gchar *eq = strchr(line, '=');
        if (!eq)
            continue;

        gchar *key = g_strndup(line, eq - line);
        g_strstrip(key);
        if (!*key) {
            g_free(key);
            continue;
        }

        /* Save current value before overwriting */
        if (!g_hash_table_contains(saved_env, key)) {
            const gchar *old = g_getenv(key);
            g_hash_table_insert(saved_env,
                                g_strdup(key),
                                old ? g_strdup(old) : NULL);
        }

        gchar *raw_val = g_strstrip(g_strdup(eq + 1));
        gchar *expanded = expand_vars(raw_val);
        g_free(raw_val);

        g_setenv(key, expanded, TRUE);

        g_free(expanded);
        g_free(key);
    }

    g_strfreev(lines);

    /* Restart the agent terminal so it picks up the new environment.
     * Emits geanyagent-restart on geany->object if geanyagent is loaded.
     * New geanycli terminal tabs inherit automatically via utils_copy_environment. */
    if (g_signal_lookup("geanyagent-restart", G_OBJECT_TYPE(geany->object)))
        g_signal_emit_by_name(geany->object, "geanyagent-restart");
}


/* Restore the environment to what it was before env_load() */
static void env_restore(void)
{
    if (!saved_env)
        return;

    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, saved_env);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        if (value)
            g_setenv((const gchar *)key, (const gchar *)value, TRUE);
        else
            g_unsetenv((const gchar *)key);
    }

    g_hash_table_remove_all(saved_env);
}


/* ------------------------------------------------------------------ */
/* Signal handlers                                                     */

static void on_project_open(G_GNUC_UNUSED GObject *obj,
                             G_GNUC_UNUSED GKeyFile *config,
                             G_GNUC_UNUSED gpointer data)
{
    GeanyApp *app = geany->app;
    if (app->project && app->project->base_path)
        env_load(app->project->base_path);
}

static void on_project_close(G_GNUC_UNUSED GObject *obj,
                              G_GNUC_UNUSED gpointer data)
{
    env_restore();
}


/* ------------------------------------------------------------------ */
/* Plugin lifecycle                                                    */

static gboolean ge_init(GeanyPlugin *plugin, G_GNUC_UNUSED gpointer data)
{
    geany_plugin = plugin;
    geany_data   = plugin->geany_data;

    /* Apply env for any project already open when the plugin loads */
    GeanyApp *app = geany->app;
    if (app->project && app->project->base_path)
        env_load(app->project->base_path);

    plugin_signal_connect(plugin, geany->object, "project-open", FALSE,
                          G_CALLBACK(on_project_open), NULL);
    plugin_signal_connect(plugin, geany->object, "project-close", FALSE,
                          G_CALLBACK(on_project_close), NULL);

    return TRUE;
}

static void ge_cleanup(G_GNUC_UNUSED GeanyPlugin *plugin,
                       G_GNUC_UNUSED gpointer data)
{
    env_restore();
    if (saved_env) {
        g_hash_table_destroy(saved_env);
        saved_env = NULL;
    }
}


/* ------------------------------------------------------------------ */

G_MODULE_EXPORT void geany_load_module(GeanyPlugin *plugin)
{
    plugin->info->name        = "Project Env";
    plugin->info->description =
        "Sets environment variables from <project>/geany.env on project open. "
        "Variables are available to all spawned processes: treebrowser, "
        "file tools, CLI terminals, and AI agent. "
        "Supports ${VAR} expansion. Restores previous environment on close.";
    plugin->info->version     = "0.1";
    plugin->info->author      = "teknopaul";

    plugin->funcs->init    = ge_init;
    plugin->funcs->cleanup = ge_cleanup;

    GEANY_PLUGIN_REGISTER(plugin, 235);
}
