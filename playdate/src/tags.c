/*
 *      tags.c - install Playdate Lua API tags for Geany autocomplete
 */

#include "playdate.h"
#include "tags_data.h"

#include <glib/gstdio.h>
#include <string.h>

/* Minimal ctags (extended) line helper. */
static void tag(GString *out, const gchar *name, const gchar *kind, const gchar *sig)
{
	g_string_append_printf(out, "%s\t/PlaydateSDK\t1;\"\tkind:%s", name, kind);
	if (sig != NULL)
		g_string_append_printf(out, "\tsignature:%s", sig);
	g_string_append_c(out, '\n');
}


/* Append a single ctags line for a Lua definition if we can see one. */
static void scan_lua_defs(GString *out, const gchar *path)
{
	gchar *contents = NULL;
	gsize len = 0;
	gchar **lines;
	guint i;

	if (!g_file_get_contents(path, &contents, &len, NULL))
		return;

	lines = g_strsplit(contents, "\n", -1);
	for (i = 0; lines[i] != NULL; i++)
	{
		const gchar *ln = lines[i];
		const gchar *p = ln;

		while (*p == ' ' || *p == '\t')
			p++;

		/* function name(...)  |  function obj:method(...)  |  function obj.method(...) */
		if (g_str_has_prefix(p, "function "))
		{
			const gchar *name = p + 9;
			const gchar *end = name;
			gchar *ident;

			while (*end && *end != '(' && *end != ' ' && *end != '\t' && *end != '{')
				end++;
			ident = g_strndup(name, (gsize) (end - name));
			if (*ident != '\0')
				tag(out, ident, "function", "()");
			g_free(ident);
		}
		/* local function name( */
		else if (g_str_has_prefix(p, "local function "))
		{
			const gchar *name = p + 15;
			const gchar *end = name;
			gchar *ident;

			while (*end && *end != '(' && *end != ' ' && *end != '\t')
				end++;
			ident = g_strndup(name, (gsize) (end - name));
			if (*ident != '\0')
				tag(out, ident, "function", "()");
			g_free(ident);
		}
		/* M.foo = function  /  playdate.foo = function */
		else
		{
			const gchar *eq = strstr(p, "= function");
			if (eq != NULL && eq > p)
			{
				const gchar *start = p;
				const gchar *end = eq;

				while (end > start && (*(end - 1) == ' ' || *(end - 1) == '\t'))
					end--;
				/* walk back over identifier / dots / colons */
				while (end > start)
				{
					gchar c = *(end - 1);
					if (g_ascii_isalnum(c) || c == '_' || c == '.' || c == ':')
						end--;
					else
						break;
				}
				if (end > start)
				{
					gchar *ident = g_strndup(start, (gsize) (end - start));
					if (*ident != '\0')
						tag(out, ident, "function", "()");
					g_free(ident);
				}
			}
		}
	}

	g_strfreev(lines);
	g_free(contents);
}


static void scan_corelibs(GString *out, const gchar *sdk)
{
	gchar *corelibs = g_build_filename(sdk, "CoreLibs", NULL);
	GDir *dir;
	const gchar *name;

	if (!g_file_test(corelibs, G_FILE_TEST_IS_DIR))
	{
		g_free(corelibs);
		return;
	}

	dir = g_dir_open(corelibs, 0, NULL);
	if (dir == NULL)
	{
		g_free(corelibs);
		return;
	}

	while ((name = g_dir_read_name(dir)) != NULL)
	{
		if (g_str_has_suffix(name, ".lua"))
		{
			gchar *path = g_build_filename(corelibs, name, NULL);
			scan_lua_defs(out, path);
			g_free(path);
		}
	}

	g_dir_close(dir);
	g_free(corelibs);
}


gboolean pd_tags_install(void)
{
	gchar *tags_dir;
	gchar *dest;
	GString *tags;
	GError *error = NULL;
	gboolean ok;

	tags_dir = g_build_filename(geany->app->configdir, "tags", NULL);
	dest = g_build_filename(tags_dir, "playdate.lua.tags", NULL);

	g_mkdir_with_parents(tags_dir, 0755);

	/* Full API surface generated from notpeter/playdate-luacats stub.lua. */
	tags = g_string_new(PD_TAGS_DATA);

	/* Append anything extra we can parse from local CoreLibs. */
	{
		const gchar *env = g_getenv("PLAYDATE_SDK_PATH");
		if (!EMPTY(env))
			scan_corelibs(tags, env);
	}

	ok = g_file_set_contents(dest, tags->str, (gssize) tags->len, &error);
	if (!ok && error != NULL)
	{
		msgwin_compiler_add(COLOR_RED, _("Playdate: could not write tags: %s"), error->message);
		g_error_free(error);
	}
	else
	{
		ui_set_statusbar(TRUE,
			_("Playdate: API tags installed → %s (reload Geany or restart to pick them up)"),
			dest);
		msgwin_compiler_add(COLOR_BLUE, _("Playdate: installed %s (%u bytes)"),
			dest, (guint) tags->len);
	}

	g_string_free(tags, TRUE);
	g_free(dest);
	g_free(tags_dir);
	return ok;
}
