#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <glib.h>

#include "devhelpplugin.h"

#ifdef HAVE_MAN


#define DEVHELP_MANPAGE_SECTIONS "3:2:1:8:5:4:7:6"
#define DEVHELP_MANPAGE_PAGER "col -b"

#define DEVHELP_MANPAGE_HTML_TEMPLATE \
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


static GList *temp_files = NULL;


/* Locates the path to the manpage found for the term and section. */
static gchar *find_manpage(const gchar *term, const gchar *section)
{
	gint retcode=0;
	gchar *cmd, *text=NULL;

	g_return_val_if_fail(term != NULL, NULL);

	if (section == NULL)
		cmd = g_strdup_printf("man -S %s --where '%s'",
					DEVHELP_MANPAGE_SECTIONS, term);
	else
		cmd = g_strdup_printf("man --where %s '%s'", section, term);

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

	return g_strstrip(text);
}


/* Read the text output from man or NULL. */
static gchar *devhelp_plugin_man(const gchar *filename)
{
	gint retcode=0;
	gchar *cmd, *text=NULL;

	g_return_val_if_fail(filename != NULL, NULL);

	cmd = g_strdup_printf("man -P\"%s\" \'%s\'", DEVHELP_MANPAGE_PAGER, filename);

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


/* Gets the manpage content as text, writes it to a temp file and returns
 * the path to the tempfile */
gchar *devhelp_plugin_manpages_search(const gchar *term, const gchar *section)
{
	FILE *fp;
	gint fd, len;
	gchar *man_fn, *tmp_fn, *uri, *text, *html_text;
	const gchar *tmpl = "devhelp_manpage_XXXXXX.html";

	man_fn = find_manpage(term, section);
	if (man_fn == NULL)
		return NULL;

	tmp_fn = NULL;
	if ((fd = g_file_open_tmp(tmpl, &tmp_fn, NULL)) == -1)
	{
		g_free(man_fn);
		g_free(tmp_fn); /* is NULL? */
		return NULL;
	}

	fp = fdopen(fd, "w");

	text = devhelp_plugin_man(man_fn);
	if (text == NULL)
	{
		g_free(man_fn);
		g_free(tmp_fn);
		fclose(fp);
		return NULL;
	}
	html_text = g_strdup_printf(DEVHELP_MANPAGE_HTML_TEMPLATE, term, text);
	g_free(text);

	len = strlen(html_text);
	if (fwrite(html_text, sizeof(gchar), len, fp) != len)
	{
		g_free(man_fn);
		g_free(tmp_fn);
		g_free(html_text);
		fclose(fp);
		return NULL;
	}

	g_free(man_fn);
	g_free(html_text);
	fclose(fp);

	temp_files = g_list_append(temp_files, tmp_fn);
	uri = g_strdup_printf("file://%s", tmp_fn);

	return uri;
}


void devhelp_plugin_remove_manpages_temp_files(void)
{
	GList *iter;

	if (temp_files == NULL)
		return;

	for (iter = temp_files; iter != NULL; iter = iter->next)
	{
		if (remove(iter->data) == -1)
			g_warning("Unable to delete temp file: %s", strerror(errno));
		g_free(iter->data);
	}

	g_list_free(temp_files);

	temp_files = NULL;
}

#endif /* HAVE_MAN */
