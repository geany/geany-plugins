/*
 *      config.c - preferences and path resolution for geany-playdate
 */

#include "playdate.h"

#include <glib/gstdio.h>
#include <stdlib.h>
#include <string.h>


void pd_config_init_defaults(PDConfig *cfg)
{
	g_free(cfg->sdk_path);
	g_free(cfg->source);
	g_free(cfg->output);
	g_free(cfg->product_name);

	cfg->sdk_path = g_strdup("");
	cfg->source = g_strdup(PD_DEFAULT_SOURCE);
	cfg->output = g_strdup(PD_DEFAULT_OUTPUT);
	cfg->product_name = g_strdup("");
	cfg->strip = FALSE;
	cfg->no_compress = FALSE;
	cfg->verbose = FALSE;
	cfg->quiet = FALSE;
	cfg->skip_unknown = FALSE;
}


gchar *pd_config_get_config_path(void)
{
	return g_build_filename(geany->app->configdir, "plugins", "geany-playdate",
		"playdate.conf", NULL);
}


void pd_config_load(PDConfig *cfg)
{
	GKeyFile *kf = g_key_file_new();
	gchar *path = pd_config_get_config_path();

	pd_config_init_defaults(cfg);

	if (g_key_file_load_from_file(kf, path, G_KEY_FILE_NONE, NULL))
	{
		gchar *s;

		s = utils_get_setting_string(kf, PD_CONFIG_GROUP, "sdk_path", "");
		g_free(cfg->sdk_path);
		cfg->sdk_path = s;

		s = utils_get_setting_string(kf, PD_CONFIG_GROUP, "source", PD_DEFAULT_SOURCE);
		g_free(cfg->source);
		cfg->source = s;

		s = utils_get_setting_string(kf, PD_CONFIG_GROUP, "output", PD_DEFAULT_OUTPUT);
		g_free(cfg->output);
		cfg->output = s;

		s = utils_get_setting_string(kf, PD_CONFIG_GROUP, "product_name", "");
		g_free(cfg->product_name);
		cfg->product_name = s;

		cfg->strip = utils_get_setting_boolean(kf, PD_CONFIG_GROUP, "strip", FALSE);
		cfg->no_compress = utils_get_setting_boolean(kf, PD_CONFIG_GROUP, "no_compress", FALSE);
		cfg->verbose = utils_get_setting_boolean(kf, PD_CONFIG_GROUP, "verbose", FALSE);
		cfg->quiet = utils_get_setting_boolean(kf, PD_CONFIG_GROUP, "quiet", FALSE);
		cfg->skip_unknown = utils_get_setting_boolean(kf, PD_CONFIG_GROUP, "skip_unknown", FALSE);
	}

	g_key_file_free(kf);
	g_free(path);
}


void pd_config_save(const PDConfig *cfg)
{
	GKeyFile *kf = g_key_file_new();
	gchar *path = pd_config_get_config_path();
	gchar *dir = g_path_get_dirname(path);
	gchar *data;
	gsize len;

	g_mkdir_with_parents(dir, 0755);

	g_key_file_set_string(kf, PD_CONFIG_GROUP, "sdk_path", cfg->sdk_path ? cfg->sdk_path : "");
	g_key_file_set_string(kf, PD_CONFIG_GROUP, "source", cfg->source ? cfg->source : PD_DEFAULT_SOURCE);
	g_key_file_set_string(kf, PD_CONFIG_GROUP, "output", cfg->output ? cfg->output : PD_DEFAULT_OUTPUT);
	g_key_file_set_string(kf, PD_CONFIG_GROUP, "product_name", cfg->product_name ? cfg->product_name : "");
	g_key_file_set_boolean(kf, PD_CONFIG_GROUP, "strip", cfg->strip);
	g_key_file_set_boolean(kf, PD_CONFIG_GROUP, "no_compress", cfg->no_compress);
	g_key_file_set_boolean(kf, PD_CONFIG_GROUP, "verbose", cfg->verbose);
	g_key_file_set_boolean(kf, PD_CONFIG_GROUP, "quiet", cfg->quiet);
	g_key_file_set_boolean(kf, PD_CONFIG_GROUP, "skip_unknown", cfg->skip_unknown);

	data = g_key_file_to_data(kf, &len, NULL);
	g_file_set_contents(path, data, (gssize) len, NULL);

	g_free(data);
	g_free(dir);
	g_free(path);
	g_key_file_free(kf);
}


void pd_config_free(PDConfig *cfg)
{
	g_free(cfg->sdk_path);
	g_free(cfg->source);
	g_free(cfg->output);
	g_free(cfg->product_name);
	memset(cfg, 0, sizeof(*cfg));
}


gchar *pd_resolve_sdk_path(const PDConfig *cfg)
{
	const gchar *env = g_getenv("PLAYDATE_SDK_PATH");

	if (!EMPTY(env))
		return g_strdup(env);

	if (!EMPTY(cfg->sdk_path))
		return g_strdup(cfg->sdk_path);

	return NULL;
}


/*
 * Project root is the directory that *contains* the source folder, not the
 * source folder itself. Playdate layout is typically:
 *
 *   MyGame/
 *   ├── Source/          (or source/)
 *   │   ├── main.lua
 *   │   └── pdxinfo
 *   └── MyGame.pdx/      (output)
 *
 * A naive "first dir with pdxinfo" walk stops at Source/ and then
 * concatenates the relative source again (Source/Source).
 */
static gboolean basename_is_source_dir(const gchar *dir, const gchar *configured)
{
	gchar *base = g_path_get_basename(dir);
	gboolean match = FALSE;

	if (!EMPTY(configured) && g_strcmp0(base, configured) == 0)
		match = TRUE;
	else if (g_strcmp0(base, "source") == 0 || g_strcmp0(base, "Source") == 0)
		match = TRUE;

	g_free(base);
	return match;
}


static gboolean dir_has_source_child(const gchar *dir, const gchar *configured)
{
	gchar *child;

	if (!EMPTY(configured))
	{
		child = g_build_filename(dir, configured, NULL);
		if (g_file_test(child, G_FILE_TEST_IS_DIR))
		{
			g_free(child);
			return TRUE;
		}
		g_free(child);
	}

	child = g_build_filename(dir, "source", NULL);
	if (g_file_test(child, G_FILE_TEST_IS_DIR))
	{
		g_free(child);
		return TRUE;
	}
	g_free(child);

	child = g_build_filename(dir, "Source", NULL);
	if (g_file_test(child, G_FILE_TEST_IS_DIR))
	{
		g_free(child);
		return TRUE;
	}
	g_free(child);

	return FALSE;
}


gchar *pd_resolve_project_root(void)
{
	GeanyDocument *doc = document_get_current();
	const gchar *configured = NULL;
	gchar *start = NULL;
	gchar *dir = NULL;
	gint depth = 0;

	if (doc == NULL || doc->real_path == NULL)
		return NULL;

	/* Prefer an explicit source name from config so a custom folder matches. */
	configured = NULL; /* caller may pass via cfg later; auto is enough for root */

	start = g_path_get_dirname(doc->real_path);
	dir = g_strdup(start);

	while (dir != NULL && depth < 32)
	{
		/* We're inside the source directory → root is its parent. */
		if (basename_is_source_dir(dir, configured))
		{
			gchar *parent = g_path_get_dirname(dir);
			g_free(start);
			return parent;
		}

		/* This directory contains a source/Source child → it's the root. */
		if (dir_has_source_child(dir, configured))
		{
			g_free(start);
			return dir;
		}

		{
			gchar *parent = g_path_get_dirname(dir);
			if (g_strcmp0(parent, dir) == 0)
			{
				g_free(parent);
				break;
			}
			g_free(dir);
			dir = parent;
		}
		depth++;
	}

	g_free(dir);
	/* fall back to the document directory */
	return start;
}


static gchar *join_root(const gchar *root, const gchar *rel)
{
	if (g_path_is_absolute(rel))
		return g_strdup(rel);
	return g_build_filename(root, rel, NULL);
}


/* Empty source means "auto-detect Source/ or source/ under project root". */
gchar *pd_resolve_source_path(const PDConfig *cfg)
{
	gchar *root = pd_resolve_project_root();
	gchar *result;

	if (root == NULL)
		return NULL;

	if (EMPTY(cfg->source))
	{
		gchar *cand;
		const gchar *names[] = { "Source", "source", NULL };
		guint i;

		for (i = 0; names[i] != NULL; i++)
		{
			cand = g_build_filename(root, names[i], NULL);
			if (g_file_test(cand, G_FILE_TEST_IS_DIR))
			{
				g_free(root);
				return cand;
			}
			g_free(cand);
		}

		/* last resort: the project root itself */
		return root;
	}

	/* Configured default "Source": fall back to lowercase if that's what exists. */
	if (g_strcmp0(cfg->source, PD_DEFAULT_SOURCE) == 0)
	{
		gchar *pref = g_build_filename(root, cfg->source, NULL);
		gchar *alt = g_build_filename(root, "source", NULL);

		if (!g_file_test(pref, G_FILE_TEST_IS_DIR) && g_file_test(alt, G_FILE_TEST_IS_DIR))
		{
			g_free(pref);
			g_free(root);
			return alt;
		}
		g_free(alt);
		g_free(pref);
	}

	result = join_root(root, cfg->source);
	g_free(root);
	return result;
}


/* Empty output means "use existing *.pdx if any, else Output.pdx". */
gchar *pd_resolve_output_path(const PDConfig *cfg)
{
	gchar *root = pd_resolve_project_root();
	gchar *result;

	if (root == NULL)
		return NULL;

	if (EMPTY(cfg->output))
	{
		GDir *dir = g_dir_open(root, 0, NULL);
		const gchar *name;

		if (dir != NULL)
		{
			while ((name = g_dir_read_name(dir)) != NULL)
			{
				if (g_str_has_suffix(name, ".pdx"))
				{
					result = g_build_filename(root, name, NULL);
					g_dir_close(dir);
					g_free(root);
					return result;
				}
			}
			g_dir_close(dir);
		}

		result = g_build_filename(root, PD_DEFAULT_OUTPUT, NULL);
		g_free(root);
		return result;
	}

	result = join_root(root, cfg->output);
	g_free(root);
	return result;
}
