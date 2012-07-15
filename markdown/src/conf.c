/*
 * conf.c - Part of the Geany Markdown plugin
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
#include "conf.h"
#include "tmpl.h"
#include "tmplmgr.h"

#define MARKDOWN_CONF_DEFAULT "[general]\n" \
                              "template=Default\n" \
                              "readme_shown=false\n" \
                              "position=0\n"

struct MarkdownConf {
  gchar *filename;
  GKeyFile *keyfile;
};

static void markdown_conf_init_defaults(MarkdownConf *conf)
{
  if (!g_key_file_has_group(conf->keyfile, "general") ||
      !g_key_file_has_key(conf->keyfile, "general", "template", NULL))
  {
    g_key_file_set_string(conf->keyfile, "general", "template", "Default");
  }
}

static void markdown_conf_init_file(MarkdownConf *conf)
{
  gchar *dirname = g_path_get_dirname(conf->filename);
  if (!g_file_test(dirname, G_FILE_TEST_IS_DIR)) {
    g_mkdir_with_parents(dirname, 0755);
  }
  if (!g_file_test(conf->filename, G_FILE_TEST_EXISTS)) {
    g_file_set_contents(conf->filename, MARKDOWN_CONF_DEFAULT, -1, NULL);
  }
  g_free(dirname);
}

MarkdownConf *markdown_conf_new(const gchar *conf_filename)
{
  MarkdownConf *conf = g_slice_new0(MarkdownConf);
  if (conf) {
    GError *error = NULL;
    conf->filename = g_strdup(conf_filename);
    markdown_conf_init_file(conf);
    conf->keyfile = g_key_file_new();
    if (!g_key_file_load_from_file(conf->keyfile, conf->filename,
        G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, &error))
    {
      g_warning("Error loading configuration file '%s': %s", conf->filename,
        error->message);
      g_error_free(error); error = NULL;
    }
    markdown_conf_init_defaults(conf);
  }
  return conf;
}

void markdown_conf_free(MarkdownConf *conf)
{
  g_return_if_fail(conf);
  g_free(conf->filename);
  g_key_file_free(conf->keyfile);
}

void markdown_conf_save(MarkdownConf *conf)
{
  g_return_if_fail(conf);

  GError *error = NULL;
  gchar *contents = NULL;
  gsize len = 0;

  contents = g_key_file_to_data(conf->keyfile, &len, &error);

  if (error) {
    g_warning("Error getting config file data: %s", error->message);
    g_error_free(error); error = NULL;
    return;
  }

  if (!g_file_set_contents(conf->filename, contents, len, &error)) {
    g_warning("Error saving config file: %s", error->message);
    g_error_free(error); error = NULL;
  }

  g_free(contents);
}

gchar *markdown_conf_get_template_name(MarkdownConf *conf)
{
  gchar *res;
  GError *error = NULL;

  g_return_val_if_fail(conf, NULL);

  res = g_key_file_get_string(conf->keyfile, "general", "template", &error);
  if (error) {
    g_warning("Unable to get template from config file: %s", error->message);
    g_error_free(error); error = NULL;
  }

  return res;
}

void markdown_conf_set_template_name(MarkdownConf *conf, const gchar *tmpl_name)
{
  g_return_if_fail(conf);
  if (!tmpl_name)
    g_key_file_set_string(conf->keyfile, "general", "template", "Default");
  else
    g_key_file_set_string(conf->keyfile, "general", "template", tmpl_name);
}

gboolean markdown_conf_get_readme_shown(MarkdownConf *conf)
{
  gboolean res;
  GError *error = NULL;

  g_return_val_if_fail(conf, FALSE);

  res = g_key_file_get_boolean(conf->keyfile, "general", "readme_shown", &error);
  if (error) {
    g_warning("Unable to get whether the README.md file has been shown: %s",
      error->message);
    g_error_free(error); error = NULL;
  }

  return res;
}

void markdown_conf_set_readme_shown(MarkdownConf *conf, gboolean shown)
{
  g_return_if_fail(conf);
  g_key_file_set_boolean(conf->keyfile, "general", "readme_shown", shown);
}

MarkdownViewPosition markdown_conf_get_view_position(MarkdownConf *conf)
{
  gint conf_val = (gint) MARKDOWN_VIEW_POS_SIDEBAR;
  GError *error = NULL;

  g_return_val_if_fail(conf, MARKDOWN_VIEW_POS_SIDEBAR);

  conf_val = g_key_file_get_integer(conf->keyfile, "general", "position", &error);
  if (error) {
    g_warning("Unable to get the WebKit view position: %s", error->message);
    g_error_free(error); error = NULL;
  }

  return (MarkdownViewPosition) conf_val;
}

void markdown_conf_set_view_position(MarkdownConf *conf, MarkdownViewPosition pos)
{
  g_return_if_fail(conf);
  g_key_file_set_integer(conf->keyfile, "general", "position", (gint) pos);
}
