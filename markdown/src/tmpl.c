/*
 * tmpl.c - Part of the Geany Markdown plugin
 *
 * Copyright 2012 Matthew Brush <mbrush@codebrainz.ca>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include <glib.h>
#include <string.h>
#include "tmpl.h"

struct MarkdownTemplate {
  gchar *name;
  gchar *filename;
  gchar *base_dir;
  gchar *text;
  gsize  text_len;
};

static void markdown_template_load_file(MarkdownTemplate *tmpl)
{
  GError *error = NULL;

  g_return_if_fail(tmpl);

  g_free(tmpl->text);
  tmpl->text = NULL;
  tmpl->text_len = 0;

  if (!g_file_get_contents(tmpl->filename, &(tmpl->text), &(tmpl->text_len), &error)) {
    g_warning("Unable to load template file '%s': %s", tmpl->filename,
      error->message);
    g_error_free(error); error = NULL;
    tmpl->text = g_strdup("");
    tmpl->text_len = 0;
  }
}

MarkdownTemplate *markdown_template_new(const gchar *tmpl_name,
  const gchar *tmpl_file)
{
  MarkdownTemplate *tmpl = g_slice_new0(MarkdownTemplate);
  if (tmpl) {
    markdown_template_set_name(tmpl, tmpl_name);
    markdown_template_set_filename(tmpl, tmpl_file);
  }
  return tmpl;
}

void markdown_template_free(MarkdownTemplate *tmpl)
{
  if (tmpl) {
    g_free(tmpl->name);
    g_free(tmpl->filename);
    g_free(tmpl->base_dir);
    g_free(tmpl->text);
    g_slice_free1(sizeof(MarkdownTemplate), tmpl);
  }
}

const gchar *markdown_template_get_name(MarkdownTemplate *tmpl)
{
  g_return_val_if_fail(tmpl, NULL);
  return (const gchar *) tmpl->name;
}

void markdown_template_set_name(MarkdownTemplate *tmpl, const gchar *name)
{
  g_return_if_fail(tmpl);
  g_free(tmpl->name);
  tmpl->name = name ? g_strdup(name) : g_strdup("");
}

const gchar *markdown_template_get_filename(MarkdownTemplate *tmpl)
{
  g_return_val_if_fail(tmpl, NULL);
  return (const gchar *) tmpl->filename;
}

const gchar *markdown_template_get_base_dir(MarkdownTemplate *tmpl)
{
  g_return_val_if_fail(tmpl, NULL);
  return (const gchar *) tmpl->base_dir;
}

void markdown_template_set_filename(MarkdownTemplate *tmpl, const gchar *filename)
{
  g_return_if_fail(tmpl);
  g_free(tmpl->filename);
  tmpl->filename = filename ? g_strdup(filename) : g_strdup("");
  tmpl->base_dir = filename ? g_path_get_dirname(tmpl->filename) : g_strdup("");
}

const gchar *markdown_template_get_text(MarkdownTemplate *tmpl, gsize *len)
{
  g_return_val_if_fail(tmpl, NULL);

  /* This is here to delay loading the file and allocating a bunch of
   * memory until the template text actually needs to be read. */
  if (tmpl->text == NULL || tmpl->text_len == 0) {
    markdown_template_load_file(tmpl);
  }

  if (len) {
    *len = tmpl->text_len;
  }

  return (const gchar *) tmpl->text;
}

gchar *markdown_template_replace(MarkdownTemplate *tmpl, const gchar *replacement,
  gsize *replaced_len)
{
  gchar *repl, **parts;
  const gchar *tmpl_text;

  g_return_val_if_fail(tmpl, NULL);

  tmpl_text = markdown_template_get_text(tmpl, NULL);
  parts = g_strsplit(tmpl_text, MARKDOWN_TEMPLATE_STRING, 0);
  repl = g_strjoinv(replacement, parts);
  g_strfreev(parts);

  if (replaced_len) {
    *replaced_len = strlen(repl);
  }

  return repl;
}
