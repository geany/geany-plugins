/*
 * Copyright 2024 Jiri Techet <techet@gmail.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <jsonrpc-glib.h>

#include "lsp-workspace-folders.h"
#include "lsp-utils.h"
#include "lsp-rpc.h"


extern GeanyData *geany_data;


void lsp_workspace_folders_init(LspServer *srv)
{
	if (!srv->wks_folder_table)
		srv->wks_folder_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	g_hash_table_remove_all(srv->wks_folder_table);
}


void lsp_workspace_folders_free(LspServer *srv)
{
	if (srv->wks_folder_table)
		g_hash_table_destroy(srv->wks_folder_table);
	srv->wks_folder_table = NULL;
}


static void notify_root_change(LspServer *srv, const gchar *root, gboolean added)
{
	gchar *root_uri = g_filename_to_uri(root, NULL, NULL);
	GVariant *node;

	if (added)
	{
		node = JSONRPC_MESSAGE_NEW (
			"event", "{",
				"added", "[", "{",
					"uri", JSONRPC_MESSAGE_PUT_STRING(root_uri),
					"name", JSONRPC_MESSAGE_PUT_STRING(root),
				"}", "]",
				"removed", "[",
				"]",
			"}"
		);
	}
	else
	{
		node = JSONRPC_MESSAGE_NEW (
			"event", "{",
				"added", "[",
				"]",
				"removed", "[", "{",
					"uri", JSONRPC_MESSAGE_PUT_STRING(root_uri),
					"name", JSONRPC_MESSAGE_PUT_STRING(root),
				"}", "]",
			"}"
		);
	}

	//printf("%s\n\n\n", lsp_utils_json_pretty_print(node));

	lsp_rpc_notify(srv, "workspace/didChangeWorkspaceFolders", node, NULL, NULL);

	g_free(root_uri);
	g_variant_unref(node);
}


void lsp_workspace_folders_add_project_root(LspServer *srv)
{
	gchar *base_path;

	if (!srv || !srv->use_workspace_folders)
		return;

	base_path = lsp_utils_get_project_base_path();
	if (base_path)
		notify_root_change(srv, base_path, TRUE);
	g_free(base_path);
}


void lsp_workspace_folders_doc_open(GeanyDocument *doc)
{
	LspServer *srv = lsp_server_get_if_running(doc);
	gchar *project_root;
	gchar *project_base;

	if (!doc->real_path || !srv || !srv->use_workspace_folders)
		return;

	project_base = lsp_utils_get_project_base_path();
	if (project_base)
	{
		SETPTR(project_base, g_strconcat(project_base, G_DIR_SEPARATOR_S, NULL));
		if (g_str_has_prefix(doc->real_path, project_base))  // already added during initialize
		{
			g_free(project_base);
			return;
		}
		g_free(project_base);
	}

	project_root = lsp_utils_find_project_root(doc, &srv->config);
	if (!project_root)
		return;

	if (!g_hash_table_contains(srv->wks_folder_table, project_root))
	{
		g_hash_table_insert(srv->wks_folder_table, project_root, GINT_TO_POINTER(0));

		notify_root_change(srv, project_root, TRUE);
	}
	else
		g_free(project_root);
}


void lsp_workspace_folders_doc_closed(GeanyDocument *doc)
{
	LspServer *srv = lsp_server_get_if_running(doc);
	GList *roots, *root;

	if (!srv || !srv->use_workspace_folders)
		return;

	roots = g_hash_table_get_keys(srv->wks_folder_table);

	foreach_list(root, roots)
	{
		gboolean root_used = FALSE;
		guint i;

		foreach_document(i)
		{
			GeanyDocument *document = documents[i];

			if (doc == document)
				continue;

			if (g_str_has_prefix(document->real_path, root->data))
			{
				root_used = TRUE;
				break;
			}
		}

		if (!root_used)
		{
			notify_root_change(srv, root->data, FALSE);

			g_hash_table_remove(srv->wks_folder_table, root->data);
		}
	}

	g_list_free(roots);
}


GPtrArray *lsp_workspace_folders_get(LspServer *srv)
{
	GPtrArray *arr = g_ptr_array_new_full(1, g_free);
	gchar *project_base;
	GList *node, *lst;

	if (!srv->wks_folder_table)
		lsp_workspace_folders_init(srv);

	project_base = lsp_utils_get_project_base_path();
	if (project_base)
		g_ptr_array_add(arr, project_base);
	g_free(project_base);

	lst = g_hash_table_get_keys(srv->wks_folder_table);
	foreach_list(node, lst)
		g_ptr_array_add(arr, g_strdup(node->data));
	g_list_free(lst);

	return arr;
}
