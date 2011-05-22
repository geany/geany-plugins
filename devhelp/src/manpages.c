#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <glib.h>

#include "manpages.h"

#define DEVHELP_MANPAGE_NUM_SECTIONS 8
/* In order of most likely sections */
static gint sections[DEVHELP_MANPAGE_NUM_SECTIONS] = { 3, 2, 1, 8, 5, 4, 7, 6 };

#define DEVHELP_MANPAGE_BUF_SIZE 4096

static GList *temp_files = NULL;


/* Locates the path to the manpage found for the term and section. */
static gchar *find_manpage(const gchar *term, const gchar *section)
{
	FILE *fp;
	gint i, len, retcode=0;
	gchar *cmd, buf[PATH_MAX];

	g_return_val_if_fail(term != NULL, NULL);

	if (section != NULL) /* user-specified section */
	{
		cmd = g_strdup_printf("man --where %s '%s'", section, term);
		if ((fp = popen(cmd, "r")) != NULL)
		{
			g_free(cmd);

			len = fread(buf, sizeof(gchar), PATH_MAX, fp);
			retcode = pclose(fp);
		}
		g_free(cmd);
	}
	else
	{

		/* try in order of most-likely sections */
		for (i = 0; i < DEVHELP_MANPAGE_NUM_SECTIONS; i++)
		{
			cmd = g_strdup_printf("man --where %d '%s'", sections[i], term);
			if ((fp = popen(cmd, "r")) == NULL)
			{
				g_free(cmd);
				continue;
			}

			g_free(cmd);
			len = fread(buf, sizeof(gchar), PATH_MAX, fp);
			retcode = pclose(fp);

			if (retcode != 0)
				continue;
			else
				break;
		}

		/* try without section if all else fails */
		if (retcode != 0)
		{
			cmd = g_strdup_printf("man --where '%s'", term);
			if ((fp = popen(cmd, "r")) == NULL)
			{
				g_free(cmd);
				return NULL;
			}

			g_free(cmd);
			len = fread(buf, sizeof(gchar), PATH_MAX, fp);
			retcode = pclose(fp);
		}
	}

	buf[PATH_MAX - 1] = '\0';

	if (strlen(buf) == 0 || retcode != 0)
		return NULL;

	return g_strstrip(g_strdup(buf));
}


/* Finds the full URI to the manpage for term and section. */
/*
static gchar *find_manpage_uri(const gchar *term, const gchar *section)
{
	gchar *uri, *fn;

	g_return_val_if_fail(term != NULL, NULL);

	fn = find_manpage(term, section);
	if (fn == NULL)
		return NULL;

	uri = g_strdup_printf("file://%s", fn);
	g_free(fn);

	return uri;
}
*/


/* Read the text output from man2html or NULL. */
/*
static gchar *devhelp_plugin_man2html(const gchar *filename)
{
	FILE *fp;
	gint size = DEVHELP_MANPAGE_BUF_SIZE;
	gchar buf[DEVHELP_MANPAGE_BUF_SIZE] = { 0 };
	gchar *text=NULL, *cmd;

	g_return_val_if_fail(filename != NULL, NULL);

	cmd = g_strdup_printf("man2html '%s'", filename);

	fp = popen(cmd, "r");
	g_free(cmd);
	if (fp == NULL)
		return NULL;

	while(fgets(buf, DEVHELP_MANPAGE_BUF_SIZE-1, fp) != NULL)
	{
		text = g_realloc(text, size);
		strncat(text, buf, DEVHELP_MANPAGE_BUF_SIZE);
		size += DEVHELP_MANPAGE_BUF_SIZE;
	}

	pclose(fp);
	return text;
}
*/


/* Read the text output from man or NULL. */
static gchar *devhelp_plugin_man(const gchar *filename)
{
	FILE *fp;
	gint size = DEVHELP_MANPAGE_BUF_SIZE;
	gchar buf[DEVHELP_MANPAGE_BUF_SIZE] = { 0 };
	gchar *text=NULL, *cmd;

	g_return_val_if_fail(filename != NULL, NULL);

	cmd = g_strdup_printf("man \"%s\"", filename);

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
	return g_strstrip(text);
}


/* Gets the manpage content as text, writes it to a temp file and returns
 * the path to the tempfile */
gchar *devhelp_plugin_manpages_search(const gchar *term, const gchar *section)
{
	FILE *fp;
	gint fd, len;
	gchar *man_fn, *tmp_fn, *uri, *text;
	const gchar *tmpl = "devhelp_manpage_XXXXXX.txt";

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

	len = strlen(text);
	if (fwrite(text, sizeof(gchar), len, fp) != len)
	{
		g_free(man_fn);
		g_free(tmp_fn);
		g_free(text);
		fclose(fp);
		return NULL;
	}

	g_free(man_fn);
	g_free(text);
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
