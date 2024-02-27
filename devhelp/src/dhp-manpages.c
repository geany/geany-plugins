#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <glib.h>


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <geanyplugin.h>

#include "dhp.h"


#define DEVHELP_PLUGIN_MANPAGE_SECTIONS "3:2:1:8:5:4:7:6"
#define DEVHELP_PLUGIN_MANPAGE_PAGER "col -b"

#define DEVHELP_PLUGIN_MANPAGE_HTML_TEMPLATE \
	"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd\">\n" \
	"<html>\n"									\
	"  <head>\n"								\
	"    <title>%s</title>\n"					\
	"    <style type=\"text/css\">\n"			\
	"      .man_text {\n"						\
	"        /*font-family: sans;*/\n"			\
	"      }\n"									\
	"    </style>\n"							\
	"  </head>\n"								\
	"  <body>\n"								\
	"    <pre class=\"man_text\">%s</pre>\n"	\
	"  </body>\n"								\
	"</html>\n"


/* Locates the path to the manpage found for the term and section. */
static gchar *devhelp_plugin_find_manpage_path(DevhelpPlugin *self, const gchar *term, const gchar *section)
{
	gint retcode=0;
	gchar *cmd, *path=NULL;
	const gchar *man_path;

	g_return_val_if_fail(self != NULL, NULL);
	g_return_val_if_fail(term != NULL, NULL);

	if ((man_path = devhelp_plugin_get_man_prog_path(self)) == NULL)
		man_path = "man";

	if (section == NULL)
	{
		cmd = g_strdup_printf("%s -S %s --where '%s'", man_path,
				DEVHELP_PLUGIN_MANPAGE_SECTIONS, term);
	}
	else
		cmd = g_strdup_printf("%s --where %s '%s'", man_path, section, term);

	if (!g_spawn_command_line_sync(cmd, &path, NULL, &retcode, NULL))
	{
		g_free(cmd);
		return NULL;
	}

	g_free(cmd);

	if (retcode != 0)
	{
		g_free(path);
		return NULL;
	}

	return g_strstrip(path);
}


/* Read the text output from man or NULL. */
static gchar *devhelp_plugin_read_man_text(DevhelpPlugin *self, const gchar *filename)
{
	gint retcode=0;
	gchar *cmd, *text=NULL;
	const gchar *man_path;

	g_return_val_if_fail(self != NULL, NULL);
	g_return_val_if_fail(filename != NULL, NULL);

	if ((man_path = devhelp_plugin_get_man_prog_path(self)) == NULL)
		man_path = "man";

	cmd = g_strdup_printf("%s -P\"%s\" \'%s\'", man_path,
			DEVHELP_PLUGIN_MANPAGE_PAGER, filename);

	if (!g_spawn_command_line_sync(cmd, &text, NULL, &retcode, NULL))
	{
		g_free(cmd);
		return NULL;
	}

	g_free(cmd);

	if (retcode != 0)
	{
		g_free(text);
		return NULL;
	}

	return text;
}


/**
 * Searches for a manual page, and if it finds one, writes its text into a
 * <pre> section in an HTML file, and returns the URI of the HTML file which
 * can be loaded into the webview.
 *
 * @param self Devhelp plugin.
 * @param term The search term to look for.
 * @param section The manual page section to look in or NULL.
 *
 * @return The URI to a temporary HTML file containing the man page text or
 * NULL on error.
 */
gchar *devhelp_plugin_manpages_search(DevhelpPlugin *self, const gchar *term, const gchar *section)
{
	FILE *fp = NULL;
	gint fd = -1;
	gsize len;
	gchar *man_fn = NULL, *tmp_fn = NULL, *uri = NULL;
	gchar *text = NULL, *html_text = NULL;
	const gchar *tmpl = "devhelp_manpage_XXXXXX.html";

	g_return_val_if_fail(self != NULL, NULL);
	g_return_val_if_fail(term != NULL, NULL);

	if ((man_fn = devhelp_plugin_find_manpage_path(self, term, section)) == NULL)
		goto error;

	if ((fd = g_file_open_tmp(tmpl, &tmp_fn, NULL)) == -1)
		goto error;

	if ((fp = fdopen(fd, "w")) == NULL)
		goto error;

	if ((text = devhelp_plugin_read_man_text(self, man_fn)) == NULL)
		goto error;

	html_text = g_strdup_printf(DEVHELP_PLUGIN_MANPAGE_HTML_TEMPLATE, term, text);

	len = strlen(html_text);
	if (fwrite(html_text, sizeof(gchar), len, fp) != len)
		goto error;

	devhelp_plugin_add_temp_file(self, tmp_fn);

	uri = g_filename_to_uri(tmp_fn, NULL, NULL);

	g_free(man_fn);
	g_free(tmp_fn);
	g_free(text);
	g_free(html_text);

	fclose(fp);

	return uri;

error:
	g_free(man_fn);
	g_free(tmp_fn);
	g_free(text);
	g_free(html_text);
	g_free(uri);
	if (fp != NULL)
		fclose(fp);
	return NULL;
}


/**
 * Removes temporary files made by the plugin and frees the stored filenames
 * and the list used to hold them.
 *
 * @param self Devhelp plugin.
 */
void devhelp_plugin_remove_manpages_temp_files(DevhelpPlugin *self)
{
	GList *temp_files, *iter;

	g_return_if_fail(self != NULL);

	temp_files = devhelp_plugin_get_temp_files(self);

	if (temp_files == NULL)
		return;

	for (iter = temp_files; iter != NULL; iter = iter->next)
	{
		if (remove(iter->data) == -1)
			g_warning("Unable to delete temp file: %s", strerror(errno));
		g_free(iter->data);
	}

	g_list_free(temp_files);
}


/**
 * Adds a filename to the list of temp files to be deleted later.
 *
 * @param self Devhelp plugin.
 * @param filename The filename to track for future removal.
 */
void devhelp_plugin_add_temp_file(DevhelpPlugin *self, const gchar *filename)
{
	GList *temp_files;

	g_return_if_fail(self != NULL);

	temp_files = devhelp_plugin_get_temp_files(self);
	temp_files = g_list_append(temp_files, g_strdup(filename));
}

/**
 * Search for a term in Manual Pages and activate/show the plugin's UI stuff.
 *
 * @param dhplug	Devhelp plugin
 * @param term		The string to search for
 */
void devhelp_plugin_search_manpages(DevhelpPlugin *self, const gchar *term)
{
	gchar *man_fn;

	g_return_if_fail(self != NULL);
	g_return_if_fail(term != NULL);

	if ((man_fn = devhelp_plugin_manpages_search(self, term, NULL)) == NULL)
		return;

	devhelp_plugin_set_webview_uri(self, man_fn);

	g_free(man_fn);

	devhelp_plugin_activate_webview_tab(self);
}
