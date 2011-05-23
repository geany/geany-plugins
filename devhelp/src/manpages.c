#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <glib.h>

#include "devhelpplugin.h"

#ifdef HAVE_MAN

#define DEVHELP_MANPAGE_BUF_SIZE 4096


static GList *temp_files = NULL;


/* Locates the path to the manpage found for the term and section. */
static gchar *find_manpage(const gchar *term, const gchar *section)
{
	FILE *fp;
	gint len, retcode=0;
	gchar *cmd, buf[PATH_MAX];

	g_return_val_if_fail(term != NULL, NULL);

	if (section == NULL)
		cmd = g_strdup_printf("man -S 3:2:1:8:5:4:7:6 --where '%s'", term);
	else
		cmd = g_strdup_printf("man --where %s '%s'", section, term);

	if ((fp = popen(cmd, "r")) == NULL)
	{
		g_free(cmd);
		return NULL;
	}

	g_free(cmd);
	len = fread(buf, sizeof(gchar), PATH_MAX, fp);
	retcode = pclose(fp);

	buf[PATH_MAX - 1] = '\0';

	if (strlen(buf) == 0 || retcode != 0)
		return NULL;

	return g_strstrip(g_strdup(buf));
}


/* Read the text output from man or NULL. */
static gchar *devhelp_plugin_man(const gchar *filename)
{
	FILE *fp;
	gint size = DEVHELP_MANPAGE_BUF_SIZE;
	gchar buf[DEVHELP_MANPAGE_BUF_SIZE] = { 0 };
	gchar *text=NULL, *cmd;

	g_return_val_if_fail(filename != NULL, NULL);

	cmd = g_strdup_printf("man -S 3:2:1:8:5:4:7:6 -P\"col -b\" \"%s\"", filename);

	fp = popen(cmd, "r");
	g_free(cmd);
	if (fp == NULL)
		return NULL;

	while (fgets(buf, DEVHELP_MANPAGE_BUF_SIZE-1, fp) != NULL)
	{
		text = g_realloc(text, size);
		strncat(text, buf, DEVHELP_MANPAGE_BUF_SIZE);
		size += DEVHELP_MANPAGE_BUF_SIZE;
	}

	pclose(fp);
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
	fseek(fp, 0, SEEK_SET);

	text = devhelp_plugin_man(man_fn);
	if (text == NULL)
	{
		g_free(man_fn);
		g_free(tmp_fn);
		fclose(fp);
		return NULL;
	}
	html_text = g_strdup_printf("<html><head><title>%s</title></head>"
					"<body><pre>%s</pre></body></html>", term, text);
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
