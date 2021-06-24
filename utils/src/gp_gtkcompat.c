/*
 * Copyright 2019 LarsGit223
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

#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <../../utils/src/gp_gtkcompat.h>

#if GTK_CHECK_VERSION(3, 10, 0)

typedef struct
{
	gchar *icon_name;
	gchar *label;
	gboolean has_underscore;
}STOCK_ITEM_LOOKUP_RECORD;

static GHashTable *stock_items = NULL;

/* The function adds a record to the GHashTable. */
static void gp_gtkcompat_add_stock_item(GHashTable *table, const gchar *stock_id,
	const gchar *icon_name, const gchar *label, gboolean has_underscore)
{
	STOCK_ITEM_LOOKUP_RECORD *record;

	record = g_new0(STOCK_ITEM_LOOKUP_RECORD, 1);
	if (icon_name != NULL)
	{
		record->icon_name = g_strdup(icon_name);
	}
	if (label != NULL)
	{
		record->label = g_strdup(label);
	}
	record->has_underscore = has_underscore;

	g_hash_table_insert (table, (gpointer)stock_id, record);
}


/* The function builds the GHashTable for quick searching of the stock-ids. */
static GHashTable *gp_gtkcompat_build_stock_item_table(void)
{
	GHashTable *table;

	table = g_hash_table_new (g_str_hash, g_str_equal);

	gp_gtkcompat_add_stock_item(table, "gtk-about", "help-about", "_About", TRUE);
	gp_gtkcompat_add_stock_item(table, "gtk-add", "list-add", "_Add", TRUE);
	gp_gtkcompat_add_stock_item(table, "gtk-apply", NULL, "_Apply", TRUE);
	gp_gtkcompat_add_stock_item(table, "gtk-bold", "format-text-bold", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-cancel", NULL, "_Cancel", TRUE);
	gp_gtkcompat_add_stock_item(table, "gtk-caps-lock-warning", "dialog-warning-symbolic", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-cdrom", "media-optical", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-clear", "edit-clear", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-close", "window-close", "_Close", TRUE);
	gp_gtkcompat_add_stock_item(table, "gtk-color-picker", NULL, NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-convert", NULL, NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-connect", NULL, NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-copy", "edit-copy", "_Copy", TRUE);
	gp_gtkcompat_add_stock_item(table, "gtk-cut", "edit-cut", "Cu_t", TRUE);
	gp_gtkcompat_add_stock_item(table, "gtk-delete", "edit-delete", "_Delete", TRUE);
	gp_gtkcompat_add_stock_item(table, "gtk-dialog-authentication", "dialog-password", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-dialog-error", "dialog-error", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-dialog-info", "dialog-information", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-dialog-question", "dialog-question", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-dialog-warning", "dialog-warning", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-directory", "folder", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-discard", NULL, NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-disconnect", NULL, NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-dnd", NULL, NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-dnd-multiple", NULL, NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-edit", NULL, NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-execute", "system-run", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-file", "text-x-generic", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-find", "edit-find", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-find-and-replace", "edit-find-replace", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-floppy", NULL, NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-fullscreen", "view-fullscreen", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-goto-bottom", "go-bottom", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-goto-first", "go-first", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-goto-last", "go-last", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-goto-top", "go-top", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-go-back", "go-previous", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-go-down", "go-down", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-go-forward", "go-next", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-go-up", "go-up", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-harddisk", "drive-harddisk", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-help", "help-browser", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-home", "go-home", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-indent", "format-indent-more", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-index", NULL, NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-info", "dialog-information", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-italic", "format-text-italic", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-jump-to", "go-jump", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-justify-center", "format-justify-center", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-justify-fill", "format-justify-fill", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-justify-left", "format-justify-left", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-justify-right", "format-justify-right", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-leave-fullscreen", "view-restore", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-media-forward", "media-seek-forward", "_Forward", TRUE);
	gp_gtkcompat_add_stock_item(table, "gtk-media-next", "media-skip-forward", "_Next", TRUE);
	gp_gtkcompat_add_stock_item(table, "gtk-media-pause", "media-playback-pause", "P_ause", TRUE);
	gp_gtkcompat_add_stock_item(table, "gtk-media-play", "media-playback-start", "_Play", TRUE);
	gp_gtkcompat_add_stock_item(table, "gtk-media-previous", "media-skip-backward", "Pre_vious", TRUE);
	gp_gtkcompat_add_stock_item(table, "gtk-media-record", "media-record", "_Record", TRUE);
	gp_gtkcompat_add_stock_item(table, "gtk-media-rewind", "media-seek-backward", "R_ewind", TRUE);
	gp_gtkcompat_add_stock_item(table, "gtk-media-stop", "media-playback-stop", "_Stop", TRUE);
	gp_gtkcompat_add_stock_item(table, "gtk-missing-image", "image-missing", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-network", "network-workgroup", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-new", "document-new", "_New", TRUE);
	gp_gtkcompat_add_stock_item(table, "gtk-no", NULL, NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-ok", NULL, "_OK", TRUE);
	gp_gtkcompat_add_stock_item(table, "gtk-open", "document-open", "_Open", TRUE);
	gp_gtkcompat_add_stock_item(table, "gtk-orientation-landscape", NULL, NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-orientation-portrait", NULL, NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-orientation-reverse-landscape", NULL, NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-orientation-reverse-portrait", NULL, NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-page-setup", "document-page-setup", "Page Set_up", TRUE);
	gp_gtkcompat_add_stock_item(table, "gtk-paste", "edit-paste", "_Paste", TRUE);
	gp_gtkcompat_add_stock_item(table, "gtk-preferences", "preferences-system", "_Preferences", TRUE);
	gp_gtkcompat_add_stock_item(table, "gtk-print", "document-print", "_Print", TRUE);
	gp_gtkcompat_add_stock_item(table, "gtk-print-error", "printer-error", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-print-paused", NULL, NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-print-preview", NULL, "Pre_view", TRUE);
	gp_gtkcompat_add_stock_item(table, "gtk-print-report", NULL, NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-print-warning", NULL, NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-properties", "document-properties", "_Properties", TRUE);
	gp_gtkcompat_add_stock_item(table, "gtk-quit", "application-exit", "_Quit", TRUE);
	gp_gtkcompat_add_stock_item(table, "gtk-redo", "edit-redo", "_Redo", TRUE);
	gp_gtkcompat_add_stock_item(table, "gtk-refresh", "view-refresh", "_Refresh", TRUE);
	gp_gtkcompat_add_stock_item(table, "gtk-remove", "list-remove", "_Remove", TRUE);
	gp_gtkcompat_add_stock_item(table, "gtk-revert-to-saved", "document-revert", "_Revert", TRUE);
	gp_gtkcompat_add_stock_item(table, "gtk-save", "document-save", "_Save", TRUE);
	gp_gtkcompat_add_stock_item(table, "gtk-save-as", "document-save-as", "Save _As", TRUE);
	gp_gtkcompat_add_stock_item(table, "gtk-select-all", "edit-select-all", "Select _All", TRUE);
	gp_gtkcompat_add_stock_item(table, "gtk-select-color", NULL, NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-select-font", NULL, NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-sort-ascending", "view-sort-ascending", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-sort-descending", "view-sort-descending", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-spell-check", "tools-check-spelling", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-stop", "process-stop", "_Stop", TRUE);
	gp_gtkcompat_add_stock_item(table, "gtk-strikethrough", "format-text-strikethrough", "_Strikethrough", TRUE);
	gp_gtkcompat_add_stock_item(table, "gtk-undelete", NULL, NULL, TRUE);
	gp_gtkcompat_add_stock_item(table, "gtk-underline", "format-text-underline", "_Underline", TRUE);
	gp_gtkcompat_add_stock_item(table, "gtk-undo", "edit-undo", "_Undo", TRUE);
	gp_gtkcompat_add_stock_item(table, "gtk-unindent", "format-indent-less", NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-yes", NULL, NULL, FALSE);
	gp_gtkcompat_add_stock_item(table, "gtk-zoom-100", "zoom-original", "_Normal Size", TRUE);
	gp_gtkcompat_add_stock_item(table, "gtk-zoom-fit", "zoom-fit-best", "Best _Fit", TRUE);
	gp_gtkcompat_add_stock_item(table, "gtk-zoom-in", "zoom-in", "Zoom _In", TRUE);
	gp_gtkcompat_add_stock_item(table, "gtk-zoom-out", "zoom-out", "Zoom _Out", TRUE);

	return table;
}

/** Get the information for a stock-item
 *
 * Compatibility function to handle deprecated stock items. The caller
 * may pass NULL for icon_name, label or has_underscore if there is no
 * interrest for that value.
 *
 * @param stock-id			Stock item for which info is requested
 * @param icon_name			Will return NULL or the icon name of the stock item
 * @param label				Will return NULL or the label of the stock item
 * @param has_underscore	Will return FALSE or TRUE if the stock item has
 *							got a label and it includes the underscore.
 * @return TRUE if the stock_id is a known stock-item, FALSE otherwise
 **/
gboolean gp_gtkcompat_get_stock_item_info(const gchar *stock_id, gchar **icon_name,
										  gchar **label, gboolean *has_underscore)
{
	STOCK_ITEM_LOOKUP_RECORD	*record;

	if (stock_items == NULL)
	{
		stock_items = gp_gtkcompat_build_stock_item_table();
	}

	record = g_hash_table_lookup (stock_items, stock_id);
	if (record == NULL)
	{
		return FALSE;
	}

	if (icon_name != NULL)
	{
		*icon_name = record->icon_name;
	}
	if (label != NULL)
	{
		*label = record->label;
	}
	if (icon_name != NULL)
	{
		*has_underscore = record->has_underscore;
	}

	return TRUE;
}

#endif
