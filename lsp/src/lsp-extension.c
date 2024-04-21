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

#include "lsp-extension.h"
#include "lsp-utils.h"
#include "lsp-rpc.h"
#include "lsp-server.h"


static void goto_cb(GVariant *return_value, GError *error, gpointer user_data)
{
	if (!error)
	{
		const gchar *str = g_variant_get_string(return_value, NULL);

		if (str && strlen(str) > 0)
		{
			gchar *fname = lsp_utils_get_real_path_from_uri_locale(str);

			if (fname)
				document_open_file(fname, FALSE, NULL, NULL);
			g_free(fname);
		}
	}
}


void lsp_extension_clangd_switch_source_header(void)
{
	GeanyDocument *doc = document_get_current();
	LspServer *srv = lsp_server_get(doc);
	GVariant *node;
	gchar *doc_uri;

	if (!doc || !srv)
		return;

	doc_uri = lsp_utils_get_doc_uri(doc);

	node = g_variant_new("{sv}", "uri", g_variant_new_string(doc_uri));
	g_variant_ref_sink(node);

	lsp_rpc_call(srv, "textDocument/switchSourceHeader", node, goto_cb, doc);

	g_free(doc_uri);
	g_variant_unref(node);
}
