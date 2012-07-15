/*
 * tmplmgr.c - Part of the Geany Markdown plugin
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

#include <string.h>
#include <glib.h>
#include "tmpl.h"
#include "tmplmgr.h"

struct MarkdownTemplateManager {
  gchar *user_path;
  gchar *system_path;
  MarkdownTemplate *template;
};

MarkdownTemplateManager *markdown_template_manager_new(const gchar *user_path,
  const gchar *system_path)
{
  MarkdownTemplateManager *tmplmgr = g_slice_new0(MarkdownTemplateManager);
  if (tmplmgr) {
    tmplmgr->user_path = g_strdup(user_path);
    tmplmgr->system_path = g_strdup(system_path);
  }
  return tmplmgr;
}

void markdown_template_manager_free(MarkdownTemplateManager *mgr)
{
  g_return_if_fail(mgr);
  g_free(mgr->user_path);
  g_free(mgr->system_path);
  g_slice_free(MarkdownTemplateManager, mgr);
}

static GSList *markdown_template_manager_read_templates_from_dir(
  MarkdownTemplateManager *mgr, const gchar *dirpath)
{
  GError *error = NULL;
  GSList *list = NULL;
  GDir *dir;
  const gchar *ent;

  dir = g_dir_open(dirpath, 0, &error);

  if (!dir) {
    /*g_warning("Error opening directory '%s': %s", dirpath, error->message);*/
    g_error_free(error); error = NULL;
    return NULL;
  }

  while ((ent = g_dir_read_name(dir)) != NULL) {
    MarkdownTemplate *tmpl;
    gchar *path, *tmpl_name, *tmpl_file;
    if (g_strcmp0(ent, ".") == 0 || g_strcmp0(ent, "..") == 0)
      continue;
    path = g_build_filename(dirpath, ent, NULL);
    tmpl_file = g_build_filename(path, "index.html", NULL);
    tmpl_name = g_path_get_dirname(tmpl_file);
    gchar *last_slash = strrchr(tmpl_name, '/');
    if (last_slash) {
      last_slash++;
      g_memmove(tmpl_name, last_slash, strlen(last_slash) + 1);
    }
    if (g_file_test(tmpl_file, G_FILE_TEST_EXISTS)) {
      tmpl = markdown_template_new(tmpl_name, tmpl_file);
      list = g_slist_prepend(list, tmpl);
    }
    g_free(path);
    g_free(tmpl_name);
    g_free(tmpl_file);
  }

  return list;
}

static gboolean markdown_template_manager_list_has_template(
  MarkdownTemplateManager *mgr, GSList *list, const gchar *tmpl_name)
{
  GSList *iter;
  gboolean result = FALSE;

  for (iter = list; iter != NULL; iter = g_slist_next(iter)) {
    MarkdownTemplate *tmpl = (MarkdownTemplate*) iter->data;
    if (g_strcmp0(markdown_template_get_name(tmpl), tmpl_name) == 0) {
      result = TRUE;
      break;
    }
  }

  return result;
}

static GSList *markdown_template_manager_combine_lists(MarkdownTemplateManager *mgr,
  GSList *user_list, GSList *system_list)
{
  GSList *list = NULL, *iter;

  for (iter = user_list; iter != NULL; iter = g_slist_next(iter)) {
    MarkdownTemplate *tmpl = (MarkdownTemplate*) iter->data;
    if (!markdown_template_manager_list_has_template(mgr, list,
      markdown_template_get_name(tmpl))) {
      list = g_slist_prepend(list, tmpl);
    }
  }

  for (iter = system_list; iter != NULL; iter = g_slist_next(iter)) {
    MarkdownTemplate *tmpl = (MarkdownTemplate*) iter->data;
    if (!markdown_template_manager_list_has_template(mgr, list,
      markdown_template_get_name(tmpl))) {
      list = g_slist_prepend(list, tmpl);
    }
  }

  return list;
}

GSList *markdown_template_manager_list_templates(MarkdownTemplateManager *mgr)
{
  GSList *list, *user_list, *system_list;

  g_return_val_if_fail(mgr, NULL);

  user_list = markdown_template_manager_read_templates_from_dir(mgr, mgr->user_path);
  system_list = markdown_template_manager_read_templates_from_dir(mgr, mgr->system_path);

  list = markdown_template_manager_combine_lists(mgr, user_list, system_list);

  g_slist_free(user_list);
  g_slist_free(system_list);

  return list;
}
