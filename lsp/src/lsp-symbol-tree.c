/*
 * Copyright 2023 Jiri Techet <techet@gmail.com>
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

// This file contains mostly stolen code from Geany's symbol tree implementation
// modified to work for LSP

// for gtk_widget_override_font() and GTK_STOCK_*
#define GDK_DISABLE_DEPRECATION_WARNINGS

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <geanyplugin.h>

#include "lsp-symbol.h"
#include "lsp-symbols.h"
#include "lsp-symbol-tree.h"
#include "lsp-goto.h"
#include "lsp-utils.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>


#define SYM_TREE_KEY "lsp_symbol_tree"
#define SYM_STORE_KEY "lsp_symbol_store"
#define SYM_FILTER_KEY "lsp_symbol_filter"


enum
{
	SYMBOLS_COLUMN_ICON,
	SYMBOLS_COLUMN_NAME,
	SYMBOLS_COLUMN_SYMBOL,
	SYMBOLS_COLUMN_TOOLTIP,
	SYMBOLS_N_COLUMNS
};


typedef struct
{
	gint found_line; /* return: the nearest line found */
	gint line;       /* input: the line to look for */
	gboolean lower   /* input: search only for lines with lower number than @line */;
} TreeSearchData;


extern GeanyData *geany_data;
extern GeanyPlugin *geany_plugin;

static struct
{
	GtkWidget *expand_all;
	GtkWidget *collapse_all;
	GtkWidget *find_refs;
	GtkWidget *find_impls;
	GtkWidget *goto_type;
	GtkWidget *goto_decl;
}
s_symbol_menu;

static GtkWidget *s_sym_view_vbox = NULL;
static GtkWidget *s_search_entry = NULL;
static GtkWidget *s_sym_window;	/* scrolled window that holds the symbol list GtkTreeView */
static GtkWidget *s_default_sym_tree;
static GtkWidget *s_popup_sym_list;


/* sort by line, then scope */
static gint compare_symbol_lines(gconstpointer a, gconstpointer b)
{
	const LspSymbol *sym_a = a;
	const LspSymbol *sym_b = b;
	gint ret;

	if (a == NULL || b == NULL)
		return 0;

	ret = lsp_symbol_get_line(sym_a) - lsp_symbol_get_line(sym_b);
	if (ret == 0)
	{
		if (lsp_symbol_get_scope(sym_a) == NULL)
			return -(lsp_symbol_get_scope(sym_a) != lsp_symbol_get_scope(sym_b));
		if (lsp_symbol_get_scope(sym_b) == NULL)
			return lsp_symbol_get_scope(sym_a) != lsp_symbol_get_scope(sym_b);
		else
			return strcmp(lsp_symbol_get_scope(sym_a), lsp_symbol_get_scope(sym_b));
	}
	return ret;
}


static gboolean is_symbol_within_parent(const LspSymbol *sym, const LspSymbol *parent)
{
	const gchar *scope_sep = LSP_SCOPE_SEPARATOR;
	const gchar *sym_scope = lsp_symbol_get_scope(sym);
	const gchar *parent_scope = lsp_symbol_get_scope(parent);
	const gchar *parent_name = lsp_symbol_get_name(parent);

	guint scope_len = 0;

	if (EMPTY(sym_scope))
		return FALSE;

	if (!EMPTY(parent_scope))
	{
		if (!g_str_has_prefix(sym_scope, parent_scope))
			return FALSE;
		scope_len = strlen(parent_scope);

		if (!g_str_has_prefix(sym_scope + scope_len, scope_sep))
			return FALSE;
		scope_len += strlen(scope_sep);
	}

	if (!g_str_has_prefix(sym_scope + scope_len, parent_name))
		return FALSE;
	scope_len += strlen(parent_name);

	if (sym_scope[scope_len] == '\0')
		return TRUE;

	if (!g_str_has_prefix(sym_scope + scope_len, scope_sep))
		return FALSE;

	return TRUE;
}


/* for symbol tree construction make sure that symbol parents appear before
 * their children in the list */
static gint compare_symbol_parent_first(gconstpointer a, gconstpointer b)
{
	const LspSymbol *sym_a = a;
	const LspSymbol *sym_b = b;

	if (is_symbol_within_parent(sym_a, sym_b))
		return 1;

	if (is_symbol_within_parent(sym_b, sym_a))
		return -1;

	return compare_symbol_lines(a, b);
}


static GList *get_symbol_list(GeanyDocument *doc, GPtrArray *lsp_symbols)
{
	GList *symbols = NULL;
	guint i;
	gchar **tf_strv;

	g_return_val_if_fail(doc, NULL);

	const gchar *tag_filter = plugin_get_document_data(geany_plugin, doc, SYM_FILTER_KEY);
	if (!tag_filter)
		tag_filter = "";
	tf_strv = g_strsplit_set(tag_filter, " ", -1);

	for (i = 0; i < lsp_symbols->len; ++i)
	{
		LspSymbol *sym = lsp_symbols->pdata[i];
		gboolean filtered = FALSE;
		gchar **val;
		gchar *full_tagname = lsp_symbol_get_symtree_name(sym, TRUE);
		gchar *normalized_tagname = g_utf8_normalize(full_tagname, -1, G_NORMALIZE_ALL);

		foreach_strv(val, tf_strv)
		{
			gchar *normalized_val = g_utf8_normalize(*val, -1, G_NORMALIZE_ALL);

			if (normalized_tagname != NULL && normalized_val != NULL)
			{
				gchar *case_normalized_tagname = g_utf8_casefold(normalized_tagname, -1);
				gchar *case_normalized_val = g_utf8_casefold(normalized_val, -1);

				filtered = strstr(case_normalized_tagname, case_normalized_val) == NULL;
				g_free(case_normalized_tagname);
				g_free(case_normalized_val);
			}
			g_free(normalized_val);

			if (filtered)
				break;
		}

		if (!filtered)
			symbols = g_list_prepend(symbols, sym);

		g_free(normalized_tagname);
		g_free(full_tagname);
	}
	symbols = g_list_sort(symbols, compare_symbol_parent_first);

	g_strfreev(tf_strv);

	return symbols;
}


static const gchar *get_parent_name(const LspSymbol *sym)
{
	return !EMPTY(lsp_symbol_get_scope(sym)) ? lsp_symbol_get_scope(sym) : NULL;
}


static gboolean symbols_table_equal(gconstpointer v1, gconstpointer v2)
{
	const LspSymbol *s1 = v1;
	const LspSymbol *s2 = v2;

	return (lsp_symbol_get_kind(s1) == lsp_symbol_get_kind(s2) &&
			strcmp(lsp_symbol_get_name(s1), lsp_symbol_get_name(s2)) == 0 &&
			utils_str_equal(lsp_symbol_get_scope(s1), lsp_symbol_get_scope(s2)) &&
			/* include arglist in match to support e.g. C++ overloading */
			utils_str_equal(lsp_symbol_get_detail(s1), lsp_symbol_get_detail(s2)));
}


/* inspired by g_str_hash() */
static guint symbols_table_hash(gconstpointer v)
{
	const LspSymbol *sym = v;
	const gchar *p;
	guint32 h = 5381;

	h = (h << 5) + h + lsp_symbol_get_kind(sym);
	for (p = lsp_symbol_get_name(sym); *p != '\0'; p++)
		h = (h << 5) + h + *p;
	if (lsp_symbol_get_scope(sym))
	{
		for (p = lsp_symbol_get_scope(sym); *p != '\0'; p++)
			h = (h << 5) + h + *p;
	}
	/* for e.g. C++ overloading */
	if (lsp_symbol_get_detail(sym))
	{
		for (p = lsp_symbol_get_detail(sym); *p != '\0'; p++)
			h = (h << 5) + h + *p;
	}

	return h;
}


/* like gtk_tree_view_expand_to_path() but with an iter */
static void tree_view_expand_to_iter(GtkTreeView *view, GtkTreeIter *iter)
{
	GtkTreeModel *model = gtk_tree_view_get_model(view);
	GtkTreePath *path = gtk_tree_model_get_path(model, iter);

	gtk_tree_view_expand_to_path(view, path);
	gtk_tree_path_free(path);
}


static gboolean ui_tree_model_iter_any_next(GtkTreeModel *model, GtkTreeIter *iter, gboolean down)
{
	GtkTreeIter guess;
	GtkTreeIter copy = *iter;

	/* go down if the item has children */
	if (down && gtk_tree_model_iter_children(model, &guess, iter))
		*iter = guess;
	/* or to the next item at the same level */
	else if (gtk_tree_model_iter_next(model, &copy))
		*iter = copy;
	/* or to the next item at a parent level */
	else if (gtk_tree_model_iter_parent(model, &guess, iter))
	{
		copy = guess;
		while (TRUE)
		{
			if (gtk_tree_model_iter_next(model, &copy))
			{
				*iter = copy;
				return TRUE;
			}
			else if (gtk_tree_model_iter_parent(model, &copy, &guess))
				guess = copy;
			else
				return FALSE;
		}
	}
	else
		return FALSE;

	return TRUE;
}


/* like gtk_tree_store_remove() but finds the next iter at any level */
static gboolean tree_store_remove_row(GtkTreeStore *store, GtkTreeIter *iter)
{
	GtkTreeIter parent;
	gboolean has_parent;
	gboolean cont;

	has_parent = gtk_tree_model_iter_parent(GTK_TREE_MODEL(store), &parent, iter);
	cont = gtk_tree_store_remove(store, iter);
	/* if there is no next at this level but there is a parent iter, continue from it */
	if (! cont && has_parent)
	{
		*iter = parent;
		cont = ui_tree_model_iter_any_next(GTK_TREE_MODEL(store), iter, FALSE);
	}

	return cont;
}


static gint tree_search_func(gconstpointer key, gpointer user_data)
{
	TreeSearchData *data = user_data;
	gint parent_line = GPOINTER_TO_INT(key);
	gboolean new_nearest;

	if (data->found_line == -1)
		data->found_line = parent_line; /* initial value */

	new_nearest = ABS(data->line - parent_line) < ABS(data->line - data->found_line);

	if (parent_line > data->line)
	{
		if (new_nearest && !data->lower)
			data->found_line = parent_line;
		return -1;
	}

	if (new_nearest)
		data->found_line = parent_line;

	if (parent_line < data->line)
		return 1;

	return 0;
}


static gint tree_cmp(gconstpointer a, gconstpointer b, gpointer user_data)
{
	return GPOINTER_TO_INT(a) - GPOINTER_TO_INT(b);
}


static void parents_table_tree_value_free(gpointer data)
{
	g_slice_free(GtkTreeIter, data);
}


/* adds a new element in the parent table if its key is known. */
static void update_parents_table(GHashTable *table, const LspSymbol *sym, const GtkTreeIter *iter)
{
	gchar *name = lsp_symbol_get_name_with_scope(sym);
	GTree *tree;

	if (name && g_hash_table_lookup_extended(table, name, NULL, (gpointer *) &tree))
	{
		if (!tree)
		{
			tree = g_tree_new_full(tree_cmp, NULL, NULL, parents_table_tree_value_free);
			g_hash_table_insert(table, name, tree);
			name = NULL;
		}

		g_tree_insert(tree, GINT_TO_POINTER(lsp_symbol_get_line(sym)), g_slice_dup(GtkTreeIter, iter));
	}

	g_free(name);
}


static GtkTreeIter *parents_table_lookup(GHashTable *table, const gchar *name, guint line)
{
	GtkTreeIter *parent_search = NULL;
	GTree *tree;

	tree = g_hash_table_lookup(table, name);
	if (tree)
	{
		TreeSearchData user_data = {-1, line, TRUE};

		/* search parent candidates for the one with the nearest
		 * line number which is lower than the symbol's line number */
		g_tree_search(tree, (GCompareFunc)tree_search_func, &user_data);
		parent_search = g_tree_lookup(tree, GINT_TO_POINTER(user_data.found_line));
	}

	return parent_search;
}


static void parents_table_value_free(gpointer data)
{
	GTree *tree = data;
	if (tree)
		g_tree_destroy(tree);
}


/* inserts a @data in @table on key @sym.
 * previous data is not overwritten if the key is duplicated, but rather the
 * two values are kept in a list
 *
 * table is: GHashTable<LspSymbol, GTree<line_num, GList<GList<LspSymbol>>>> */
static void symbols_table_insert(GHashTable *table, LspSymbol *sym, GList *data)
{
	GTree *tree = g_hash_table_lookup(table, sym);
	GList *list;

	if (!tree)
	{
		tree = g_tree_new_full(tree_cmp, NULL, NULL, NULL);
		g_hash_table_insert(table, sym, tree);
	}
	list = g_tree_lookup(tree, GINT_TO_POINTER(lsp_symbol_get_line(sym)));
	list = g_list_prepend(list, data);
	g_tree_insert(tree, GINT_TO_POINTER(lsp_symbol_get_line(sym)), list);
}


/* looks up the entry in @table that best matches @sym.
 * if there is more than one candidate, the one that has closest line position to @sym is chosen */
static GList *symbols_table_lookup(GHashTable *table, LspSymbol *sym)
{
	TreeSearchData user_data = {-1, lsp_symbol_get_line(sym), FALSE};
	GTree *tree = g_hash_table_lookup(table, sym);

	if (tree)
	{
		GList *list;

		g_tree_search(tree, (GCompareFunc)tree_search_func, &user_data);
		list = g_tree_lookup(tree, GINT_TO_POINTER(user_data.found_line));
		/* return the first value in the list - we don't care which of the
		 * symbols with identical names defined on the same line we get */
		if (list)
			return list->data;
	}
	return NULL;
}


/* removes the element at @sym from @table.
 * @sym must be the exact pointer used at insertion time */
static void symbols_table_remove(GHashTable *table, LspSymbol *sym)
{
	GTree *tree = g_hash_table_lookup(table, sym);
	if (tree)
	{
		GList *list = g_tree_lookup(tree, GINT_TO_POINTER(lsp_symbol_get_line(sym)));
		if (list)
		{
			GList *node;
			/* should always be the first element as we returned the first one in
			 * symbols_table_lookup() */
			foreach_list(node, list)
			{
				if (((GList *) node->data)->data == sym)
					break;
			}
			list = g_list_delete_link(list, node);
			if (!list)
				g_tree_remove(tree, GINT_TO_POINTER(lsp_symbol_get_line(sym)));
			else
				g_tree_insert(tree, GINT_TO_POINTER(lsp_symbol_get_line(sym)), list);
		}
	}
}


static gboolean symbols_table_tree_value_free(gpointer key, gpointer value, gpointer data)
{
	GList *list = value;
	g_list_free(list);
	return FALSE;
}


static void symbols_table_value_free(gpointer data)
{
	GTree *tree = data;
	if (tree)
	{
		/* free any leftover elements.  note that we can't register a value_free_func when
		 * creating the tree because we only want to free it when destroying the tree,
		 * not when inserting a duplicate (we handle this manually) */
		g_tree_foreach(tree, symbols_table_tree_value_free, NULL);
		g_tree_destroy(tree);
	}
}


/*
 * Updates the symbol tree for a document with the symbols in *list.
 * @param doc a document
 * @param symbols a pointer to a GList* holding the symbols to add/update.  This
 *             list may be updated, removing updated elements.
 *
 * The update is done in two passes:
 * 1) walking the current tree, update symbols that still exist and remove the
 *    obsolescent ones;
 * 2) walking the remaining (non updated) symbols, adds them in the list.
 *
 * For better performances, we use 2 hash tables:
 * - one containing all the symbols for lookup in the first pass (actually stores a
 *   reference in the symbols list for removing it efficiently), avoiding list search
 *   on each symbol;
 * - the other holding "symbol-name":row references for symbols having children, used to
 *   lookup for a parent in both passes, avoiding tree traversal.
 */
static void update_symbols(GeanyDocument *doc, GList **symbols)
{
	GtkTreeStore *store = plugin_get_document_data(geany_plugin, doc, SYM_STORE_KEY);
	GtkWidget *sym_tree = plugin_get_document_data(geany_plugin, doc, SYM_TREE_KEY);
	GtkTreeModel *model = GTK_TREE_MODEL(store);
	GHashTable *parents_table;
	GHashTable *symbols_table;
	GtkTreeIter iter;
	gboolean cont;
	GList *item;

	/* Build hash tables holding symbols and parents */
	/* parent table is GHashTable<symbol_name, GTree<line_num, GtkTreeIter>>
	 * where symbol_name might be a fully qualified name (with scope) if the language
	 * parser reports scope properly (see tm_parser_has_full_scope()). */
	parents_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, parents_table_value_free);
	/* symbols table is another representation of the @symbols list,
	 * GHashTable<GeanySymbol, GTree<line_num, GList<GList<GeanySymbol>>>> */
	symbols_table = g_hash_table_new_full(symbols_table_hash, symbols_table_equal,
		NULL, symbols_table_value_free);
	foreach_list(item, *symbols)
	{
		LspSymbol *symbol = item->data;
		const gchar *parent_name;

		symbols_table_insert(symbols_table, symbol, item);

		parent_name = get_parent_name(symbol);
		if (parent_name)
			g_hash_table_insert(parents_table, g_strdup(parent_name), NULL);
	}

	/* First pass, update existing rows or delete them.
	 * It is OK to delete them since we walk top down so we would remove
	 * parents before checking for their children, thus never implicitly
	 * deleting an updated child */
	cont = gtk_tree_model_get_iter_first(model, &iter);
	while (cont)
	{
		LspSymbol *symbol;

		gtk_tree_model_get(model, &iter, SYMBOLS_COLUMN_SYMBOL, &symbol, -1);
		if (! symbol) /* most probably a toplevel, skip it */
			cont = ui_tree_model_iter_any_next(model, &iter, TRUE);
		else
		{
			GList *found_item;

			found_item = symbols_table_lookup(symbols_table, symbol);
			if (! found_item) /* symbol doesn't exist, remove it */
				cont = tree_store_remove_row(store, &iter);
			else /* symbol still exist, update it */
			{
				const gchar *parent_name;
				LspSymbol *found = found_item->data;

				parent_name = get_parent_name(found);
				/* if parent is unknown, ignore it */
				if (parent_name && ! g_hash_table_lookup(parents_table, parent_name))
					parent_name = NULL;

				if (!lsp_symbol_equal(symbol, found))
				{
					gchar *name, *tooltip;

					/* only update fields that (can) have changed (name that holds line
					 * number, tooltip, and the symbol itself) */
					name = lsp_symbol_get_symtree_name(found, parent_name == NULL);
					tooltip = lsp_symbol_get_symtree_tooltip(found, doc->encoding);
					gtk_tree_store_set(store, &iter,
							SYMBOLS_COLUMN_NAME, name,
							SYMBOLS_COLUMN_TOOLTIP, tooltip,
							SYMBOLS_COLUMN_SYMBOL, found,
							-1);
					g_free(tooltip);
					g_free(name);
				}

				update_parents_table(parents_table, found, &iter);

				/* remove the updated symbol from the table and list */
				symbols_table_remove(symbols_table, found);
				*symbols = g_list_delete_link(*symbols, found_item);

				cont = ui_tree_model_iter_any_next(model, &iter, TRUE);
			}

			lsp_symbol_unref(symbol);
		}
	}

	/* Second pass, now we have a tree cleaned up from invalid rows,
	 * we simply add new ones */
	foreach_list (item, *symbols)
	{
		LspSymbol *symbol = item->data;
		GtkTreeIter *parent = NULL;
		gboolean expand = FALSE;
		const gchar *parent_name;
		gchar *tooltip, *name;
		GdkPixbuf *icon = symbols_get_icon_pixbuf(lsp_symbol_get_icon(symbol));

		parent_name = get_parent_name(symbol);
		if (parent_name)
		{
			GtkTreeIter *parent_search = parents_table_lookup(parents_table, parent_name,
				lsp_symbol_get_line(symbol));

			if (parent_search)
				parent = parent_search;
			else
				parent_name = NULL;
		}

		/* only expand to the iter if the parent was empty, otherwise we let the
		 * folding as it was before (already expanded, or closed by the user) */
		if (parent)
			expand = ! gtk_tree_model_iter_has_child(model, parent);

		/* insert the new element */
		name = lsp_symbol_get_symtree_name(symbol, parent_name == NULL);
		tooltip = lsp_symbol_get_symtree_tooltip(symbol, doc->encoding);
		gtk_tree_store_insert_with_values(store, &iter, parent, 0,
				SYMBOLS_COLUMN_NAME, name,
				SYMBOLS_COLUMN_TOOLTIP, tooltip,
				SYMBOLS_COLUMN_ICON, icon,
				SYMBOLS_COLUMN_SYMBOL, symbol,
				-1);
		g_free(tooltip);
		g_free(name);

		update_parents_table(parents_table, symbol, &iter);

		if (expand)
			tree_view_expand_to_iter(GTK_TREE_VIEW(sym_tree), &iter);
	}

	g_hash_table_destroy(parents_table);
	g_hash_table_destroy(symbols_table);
}


static gboolean symbol_has_missing_parent(const LspSymbol *symbol, GtkTreeStore *store,
		GtkTreeIter *iter)
{
	/* if the symbol has a parent symbol, it should be at depth >= 2 */
	return !EMPTY(lsp_symbol_get_scope(symbol)) &&
		gtk_tree_store_iter_depth(store, iter) == 1;
}


static gint tree_sort_func(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b,
		gpointer user_data)
{
	LspSymbol *sym_a, *sym_b;
	gint cmp;

	gtk_tree_model_get(model, a, SYMBOLS_COLUMN_SYMBOL, &sym_a, -1);
	gtk_tree_model_get(model, b, SYMBOLS_COLUMN_SYMBOL, &sym_b, -1);

	/* Check if the iters can be sorted based on symbol name and line, not tree item name.
	 * Sort by tree name if the scope was prepended, e.g. 'ScopeNameWithNoSymbol::SymbolName'. */
	if (sym_a && !symbol_has_missing_parent(sym_a, GTK_TREE_STORE(model), a) &&
		sym_b && !symbol_has_missing_parent(sym_b, GTK_TREE_STORE(model), b))
	{
		cmp = compare_symbol_lines(sym_a, sym_b);
	}
	else
	{
		gchar *astr, *bstr;

		gtk_tree_model_get(model, a, SYMBOLS_COLUMN_NAME, &astr, -1);
		gtk_tree_model_get(model, b, SYMBOLS_COLUMN_NAME, &bstr, -1);

		{
			/* this is what g_strcmp0() does */
			if (! astr)
				cmp = -(astr != bstr);
			else if (! bstr)
				cmp = astr != bstr;
			else
			{
				cmp = strcmp(astr, bstr);

				/* sort duplicate 'ScopeName::OverloadedSymbolName' items by line as well */
				if (sym_a && sym_b)
					cmp = compare_symbol_lines(sym_a, sym_b);
			}
		}
		g_free(astr);
		g_free(bstr);
	}
	lsp_symbol_unref(sym_a);
	lsp_symbol_unref(sym_b);

	return cmp;
}


static void sort_tree(GtkTreeStore *store)
{
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(store), SYMBOLS_COLUMN_NAME, tree_sort_func,
		NULL, NULL);

	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store), SYMBOLS_COLUMN_NAME, GTK_SORT_ASCENDING);
}


static void symbols_recreate_symbol_list(GeanyDocument *doc)
{
	GList *symbols;
	GPtrArray *lsp_symbols;
	GtkTreeStore *sym_store;

	g_return_if_fail(DOC_VALID(doc));

	sym_store = plugin_get_document_data(geany_plugin, doc, SYM_STORE_KEY);
	if (!sym_store)
		return;

	lsp_symbols = lsp_symbols_doc_get_cached(doc);
	symbols = get_symbol_list(doc, lsp_symbols);
	if (symbols == NULL)
		return;

	/* disable sorting during update because the code doesn't support correctly
	 * models that are currently being built */
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(sym_store), GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID, 0);

	update_symbols(doc, &symbols);
	g_list_free(symbols);

	sort_tree(sym_store);
}


static void on_symbol_tree_menu_show(GtkWidget *widget,
		gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	LspServer *srv = lsp_server_get_if_running(doc);
	LspServerConfig *cfg = srv ? &srv->config : NULL;

	gtk_widget_set_sensitive(s_symbol_menu.expand_all, cfg != NULL);
	gtk_widget_set_sensitive(s_symbol_menu.collapse_all, cfg != NULL);
	gtk_widget_set_sensitive(s_symbol_menu.find_refs, cfg != NULL && cfg->goto_references_enable);
	gtk_widget_set_sensitive(s_symbol_menu.find_impls, cfg != NULL && cfg->goto_implementation_enable);
	gtk_widget_set_sensitive(s_symbol_menu.goto_type, cfg != NULL && cfg->goto_type_definition_enable);
	gtk_widget_set_sensitive(s_symbol_menu.goto_decl, cfg != NULL && cfg->goto_declaration_enable);
}


static void on_expand_collapse(GtkWidget *widget, gpointer user_data)
{
	gboolean expand = GPOINTER_TO_INT(user_data);
	GeanyDocument *doc = document_get_current();
	GtkWidget *sym_tree;

	if (! doc)
		return;

	sym_tree = plugin_get_document_data(geany_plugin, doc, SYM_TREE_KEY);

	g_return_if_fail(sym_tree);

	if (expand)
		gtk_tree_view_expand_all(GTK_TREE_VIEW(sym_tree));
	else
		gtk_tree_view_collapse_all(GTK_TREE_VIEW(sym_tree));
}


static void hide_sidebar(gpointer data)
{
	keybindings_send_command(GEANY_KEY_GROUP_VIEW, GEANY_KEYS_VIEW_SIDEBAR);
}


static void change_focus_to_editor(GeanyDocument *doc)
{
	if (DOC_VALID(doc))
	{
		GtkWidget *sci = GTK_WIDGET(doc->editor->sci);
		gtk_widget_grab_focus(sci);
	}
}


static gboolean symlist_go_to_selection(GtkTreeSelection *selection, guint keyval, guint state)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	gint line = 0;
	gboolean handled = TRUE;

	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		LspSymbol *sym;

		gtk_tree_model_get(model, &iter, SYMBOLS_COLUMN_SYMBOL, &sym, -1);
		if (! sym)
			return FALSE;

		line = lsp_symbol_get_line(sym);
		if (line > 0)
		{
			GeanyDocument *doc = document_get_current();

			if (doc != NULL)
			{
				navqueue_goto_line(doc, doc, line);
				state = keybindings_get_modifiers(state);
				if (keyval != GDK_KEY_space && ! (state & GEANY_PRIMARY_MOD_MASK))
					change_focus_to_editor(doc);
				else
					handled = FALSE;
			}
		}
		lsp_symbol_unref(sym);
	}
	return handled;
}


static gboolean sidebar_button_press_cb(GtkWidget *widget, GdkEventButton *event,
		gpointer user_data)
{
	GtkTreeSelection *selection;
	GtkWidgetClass *widget_class = GTK_WIDGET_GET_CLASS(widget);
	gboolean handled = FALSE;

	/* force the TreeView handler to run before us for it to do its job (selection & stuff).
	 * doing so will prevent further handlers to be run in most cases, but the only one is our own,
	 * so guess it's fine. */
	if (widget_class->button_press_event)
		handled = widget_class->button_press_event(widget, event);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));

	if (event->type == GDK_2BUTTON_PRESS)
	{	/* double click on parent node(section) expands/collapses it */
		GtkTreeModel *model;
		GtkTreeIter iter;

		if (gtk_tree_selection_get_selected(selection, &model, &iter))
		{
			if (gtk_tree_model_iter_has_child(model, &iter))
			{
				GtkTreePath *path = gtk_tree_model_get_path(model, &iter);

				if (gtk_tree_view_row_expanded(GTK_TREE_VIEW(widget), path))
					gtk_tree_view_collapse_row(GTK_TREE_VIEW(widget), path);
				else
					gtk_tree_view_expand_row(GTK_TREE_VIEW(widget), path, FALSE);

				gtk_tree_path_free(path);
				return TRUE;
			}
		}
	}
	else if (event->button == 1)
	{	
		handled = symlist_go_to_selection(selection, 0, event->state);
	}
	else if (event->button == 3)
	{
		gtk_menu_popup_at_pointer(GTK_MENU(s_popup_sym_list), (GdkEvent *) event);
		handled = TRUE;
	}
	return handled;
}


static gboolean sidebar_key_press_cb(GtkWidget *widget, GdkEventKey *event,
											 gpointer user_data)
{
	if (ui_is_keyval_enter_or_return(event->keyval) || event->keyval == GDK_KEY_space)
	{
		GtkWidgetClass *widget_class = GTK_WIDGET_GET_CLASS(widget);
		GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));

		/* force the TreeView handler to run before us for it to do its job (selection & stuff).
		 * doing so will prevent further handlers to be run in most cases, but the only one is our
		 * own, so guess it's fine. */
		if (widget_class->key_press_event)
			widget_class->key_press_event(widget, event);

		symlist_go_to_selection(selection, event->keyval, event->state);

		return TRUE;
	}
	return FALSE;
}


/* the prepare_* functions are document-related, but I think they fit better here than in document.c */
static void prepare_symlist(GtkWidget *tree, GtkTreeStore *store)
{
	GtkCellRenderer *text_renderer, *icon_renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;
	PangoFontDescription *pfd;

	pfd = pango_font_description_from_string(geany_data->interface_prefs->tagbar_font);
	gtk_widget_override_font(tree, pfd);
	pango_font_description_free(pfd);

	text_renderer = gtk_cell_renderer_text_new();
	icon_renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new();

	gtk_tree_view_column_pack_start(column, icon_renderer, FALSE);
  	gtk_tree_view_column_set_attributes(column, icon_renderer, "pixbuf", SYMBOLS_COLUMN_ICON, NULL);
  	g_object_set(icon_renderer, "xalign", 0.0, NULL);

  	gtk_tree_view_column_pack_start(column, text_renderer, TRUE);
  	gtk_tree_view_column_set_attributes(column, text_renderer, "text", SYMBOLS_COLUMN_NAME, NULL);
  	g_object_set(text_renderer, "yalign", 0.5, NULL);
  	gtk_tree_view_column_set_title(column, _("Symbols"));

	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree), FALSE);

	ui_widget_modify_font_from_string(tree, geany_data->interface_prefs->tagbar_font);

	gtk_tree_view_set_model(GTK_TREE_VIEW(tree), GTK_TREE_MODEL(store));
	g_object_unref(store);

	g_signal_connect(tree, "button-press-event",
		G_CALLBACK(sidebar_button_press_cb), NULL);
	g_signal_connect(tree, "key-press-event",
		G_CALLBACK(sidebar_key_press_cb), NULL);

	gtk_tree_view_set_show_expanders(GTK_TREE_VIEW(tree), geany_data->interface_prefs->show_symbol_list_expanders);
	if (! geany_data->interface_prefs->show_symbol_list_expanders)
		gtk_tree_view_set_level_indentation(GTK_TREE_VIEW(tree), 10);
	/* Tooltips */
	ui_tree_view_set_tooltip_text_column(GTK_TREE_VIEW(tree), SYMBOLS_COLUMN_TOOLTIP);

	/* selection handling */
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
	/* callback for changed selection not necessary, will be handled by button-press-event */
}


static gboolean on_default_sym_tree_button_press_event(GtkWidget *widget, GdkEventButton *event,
		gpointer user_data)
{
	if (event->button == 3)
	{
		gtk_menu_popup_at_pointer(GTK_MENU(s_popup_sym_list), (GdkEvent *) event);
		return TRUE;
	}
	return FALSE;
}


static gint find_symbol_tab(void)
{
	GtkNotebook *notebook = GTK_NOTEBOOK(geany_data->main_widgets->sidebar_notebook);
	gint pagenum = gtk_notebook_get_n_pages(notebook);
	gint i;

	for (i = 0; i < pagenum; i++)
	{
		GtkWidget *page = gtk_notebook_get_nth_page(notebook, i);
		if (page == s_sym_view_vbox)
			return i;
	}

	return -1;
}


void lsp_symbol_tree_refresh(void)
{
	GeanyDocument *doc = document_get_current();
	GtkWidget *child;
	GPtrArray *symbols;
	const gchar *filter;
	const gchar *entry_text;
	GtkTreeStore *sym_store;
	GtkWidget *sym_tree;

	if (!doc || !s_sym_window)
		return;

	if (gtk_notebook_get_current_page(GTK_NOTEBOOK(geany_data->main_widgets->sidebar_notebook)) != find_symbol_tab())
		return; /* don't bother updating symbol tree if we don't see it */

	child = gtk_bin_get_child(GTK_BIN(s_sym_window));
	symbols = lsp_symbols_doc_get_cached(doc);
	filter = plugin_get_document_data(geany_plugin, doc, SYM_FILTER_KEY);
	filter = filter ? filter : "";
	entry_text = gtk_entry_get_text(GTK_ENTRY(s_search_entry));
	if (g_strcmp0(filter, entry_text) != 0)
	{
		gtk_entry_set_text(GTK_ENTRY(s_search_entry), filter);
		/* on_entry_tagfilter_changed() will call lsp_symbol_tree_refresh() again */
		return;
	}

	/* changes the tree view to the given one, trying not to do useless changes */
	#define CHANGE_TREE(new_child) \
		G_STMT_START { \
			/* only change the symbol tree if it's actually not the same (to avoid flickering) and if \
			 * it's the one of the current document (to avoid problems when e.g. reloading \
			 * configuration files */ \
			if (child != new_child) \
			{ \
				if (child) \
					gtk_container_remove(GTK_CONTAINER(s_sym_window), child); \
				gtk_container_add(GTK_CONTAINER(s_sym_window), new_child); \
			} \
		} G_STMT_END

	/* show default empty symbol tree if there are no symbols */
	if (symbols == NULL || symbols->len == 0)
	{
		CHANGE_TREE(s_default_sym_tree);
		return;
	}

	sym_store = plugin_get_document_data(geany_plugin, doc, SYM_STORE_KEY);
	sym_tree = plugin_get_document_data(geany_plugin, doc, SYM_TREE_KEY);

	if (sym_tree == NULL)
	{
		sym_store = gtk_tree_store_new(
			SYMBOLS_N_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING, LSP_TYPE_SYMBOL, G_TYPE_STRING);
		sym_tree = gtk_tree_view_new();
		prepare_symlist(sym_tree, sym_store);
		gtk_widget_show(sym_tree);
		g_object_ref(sym_tree);	/* to hold it after removing */

		plugin_set_document_data_full(geany_plugin, doc, SYM_STORE_KEY, g_object_ref(sym_store), g_object_unref);
		plugin_set_document_data_full(geany_plugin, doc, SYM_TREE_KEY, g_object_ref(sym_tree), g_object_unref);
	}

	symbols_recreate_symbol_list(doc);

	CHANGE_TREE(sym_tree);

	#undef CHANGE_TREE
}


static void on_sidebar_switch_page(GtkNotebook *notebook,
	gpointer page, guint page_num, gpointer user_data)
{
	if (page_num == find_symbol_tab())
		lsp_symbol_tree_refresh();
}


static void on_entry_tagfilter_changed(GtkAction *action, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	GtkTreeStore *store;

	if (!doc)
		return;

	plugin_set_document_data_full(geany_plugin, doc, SYM_FILTER_KEY,
		g_strdup(gtk_entry_get_text(GTK_ENTRY(s_search_entry))), g_free);

	/* make sure the tree is fully re-created so it appears correctly
	 * after applying filter */
	store = plugin_get_document_data(geany_plugin, doc, SYM_STORE_KEY);
	if (store)
		gtk_tree_store_clear(store);

	lsp_symbol_tree_refresh();
}


static void on_entry_tagfilter_activate(GtkEntry *entry, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	GtkWidget *sym_tree;

	if (!doc)
		return;

	sym_tree = plugin_get_document_data(geany_plugin, doc, SYM_TREE_KEY);

	gtk_widget_grab_focus(sym_tree);
}


static void on_symtree_goto(GtkWidget *widget, G_GNUC_UNUSED gpointer unused)
{
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GeanyDocument *doc;
	LspSymbol *sym = NULL;
	GtkWidget *sym_tree;

	doc = document_get_current();
	if (!doc)
		return;

	sym_tree = plugin_get_document_data(geany_plugin, doc, SYM_TREE_KEY);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(sym_tree));
	if (gtk_tree_selection_get_selected(selection, &model, &iter))
		gtk_tree_model_get(model, &iter, SYMBOLS_COLUMN_SYMBOL, &sym, -1);

	if (sym)
	{
		LspPosition lsp_pos = {lsp_symbol_get_line(sym) - 1, lsp_symbol_get_pos(sym)};
		gint sci_pos = lsp_utils_lsp_pos_to_scintilla(doc->editor->sci, lsp_pos);

		if (widget == s_symbol_menu.find_refs)
			lsp_goto_references(sci_pos);
		else if (widget == s_symbol_menu.find_impls)
			lsp_goto_implementations(sci_pos);
		else if (widget == s_symbol_menu.goto_type)
			lsp_goto_type_definition(sci_pos);
		else if (widget == s_symbol_menu.goto_decl)
			lsp_goto_declaration(sci_pos);

		lsp_symbol_unref(sym);
	}
}


static void create_symlist_popup_menu(void)
{
	GtkWidget *item, *menu;

	s_popup_sym_list = menu = gtk_menu_new();

	s_symbol_menu.expand_all = item = ui_image_menu_item_new(GTK_STOCK_ADD, _("_Expand All"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_expand_collapse), GINT_TO_POINTER(TRUE));

	s_symbol_menu.collapse_all = item = ui_image_menu_item_new(GTK_STOCK_REMOVE, _("_Collapse All"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_expand_collapse), GINT_TO_POINTER(FALSE));

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	s_symbol_menu.find_refs = item = ui_image_menu_item_new(GTK_STOCK_FIND, _("Find _References"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_symtree_goto), s_symbol_menu.find_refs);

	s_symbol_menu.find_impls = item = ui_image_menu_item_new(GTK_STOCK_FIND, _("Find _Implementations"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_symtree_goto), s_symbol_menu.find_refs);

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	s_symbol_menu.goto_decl = item = gtk_menu_item_new_with_mnemonic(_("Go to _Declaration"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_symtree_goto), NULL);

	s_symbol_menu.goto_type = item = gtk_menu_item_new_with_mnemonic(_("Go to _Type"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_symtree_goto), NULL);

	g_signal_connect(menu, "show", G_CALLBACK(on_symbol_tree_menu_show), NULL);

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	item = ui_image_menu_item_new(GTK_STOCK_CLOSE, _("H_ide Sidebar"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(hide_sidebar), NULL);
}


static void create_default_sym_tree(void)
{
	GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW(s_sym_window);

	/* default_sym_tree is a GtkViewPort with a GtkLabel inside it */
	s_default_sym_tree = gtk_viewport_new(
		gtk_scrolled_window_get_hadjustment(scrolled_window),
		gtk_scrolled_window_get_vadjustment(scrolled_window));
	gtk_viewport_set_shadow_type(GTK_VIEWPORT(s_default_sym_tree), GTK_SHADOW_NONE);
	gtk_widget_show_all(s_default_sym_tree);
	g_signal_connect(s_default_sym_tree, "button-press-event",
		G_CALLBACK(on_default_sym_tree_button_press_event), NULL);
	g_object_ref((gpointer)s_default_sym_tree);	/* to hold it after removing */
}


void lsp_symbol_tree_destroy(void)
{
	guint i;

	if (!s_default_sym_tree)
		return;

	gtk_widget_destroy(s_default_sym_tree); /* make GTK release its references, if any... */
	g_object_unref(s_default_sym_tree); /* ...and release our own */
	s_default_sym_tree = NULL;

	gtk_widget_destroy(s_popup_sym_list);
	gtk_widget_destroy(s_sym_view_vbox);

	foreach_document(i)
	{
		GeanyDocument *doc = documents[i];

		plugin_set_document_data(geany_plugin, doc, SYM_TREE_KEY, NULL);
		plugin_set_document_data(geany_plugin, doc, SYM_STORE_KEY, NULL);
		plugin_set_document_data(geany_plugin, doc, SYM_FILTER_KEY, NULL);
	}
}


void lsp_symbol_tree_init(void)
{
	LspServerConfig *cfg = lsp_server_get_all_section_config();
	const gchar *tab_label = cfg->document_symbols_tab_label;
	const gchar *old_tab_label = NULL;
	gboolean show_symtree_tab = !EMPTY(tab_label);

	if (s_default_sym_tree)
		old_tab_label = gtk_notebook_get_tab_label_text(GTK_NOTEBOOK(geany_data->main_widgets->sidebar_notebook), s_sym_view_vbox);

	if (old_tab_label && g_strcmp0(old_tab_label, tab_label) != 0)
		lsp_symbol_tree_destroy();

	if ((show_symtree_tab && s_default_sym_tree) || (!show_symtree_tab && !s_default_sym_tree))
		return;

	if (!show_symtree_tab && s_default_sym_tree)
	{
		lsp_symbol_tree_destroy();
		return;
	}

	s_sym_view_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

	s_search_entry = gtk_entry_new();
	g_signal_connect(s_search_entry, "activate",
		G_CALLBACK(on_entry_tagfilter_activate), NULL);
	g_signal_connect(s_search_entry, "changed",
		G_CALLBACK(on_entry_tagfilter_changed), NULL);
	ui_entry_add_clear_icon(GTK_ENTRY(s_search_entry));
	g_object_set(s_search_entry, "primary-icon-stock", GTK_STOCK_FIND, NULL);

	gtk_box_pack_start(GTK_BOX(s_sym_view_vbox), s_search_entry, FALSE, FALSE, 0);

	s_sym_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(s_sym_window),
					   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	gtk_box_pack_start(GTK_BOX(s_sym_view_vbox), s_sym_window, TRUE, TRUE, 0);

	gtk_widget_show_all(s_sym_view_vbox);

	create_symlist_popup_menu();
	create_default_sym_tree();

	gtk_notebook_append_page(GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook),
				 s_sym_view_vbox, gtk_label_new(tab_label));
	g_signal_connect_after(geany_data->main_widgets->sidebar_notebook, "switch-page",
		G_CALLBACK(on_sidebar_switch_page), NULL);
}
