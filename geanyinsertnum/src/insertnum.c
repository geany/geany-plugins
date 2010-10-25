/*
 *  insertnum.c
 *
 *  Copyright 2010 Dimitar Toshkov Zhekov <dimitar.zhekov@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "geanyplugin.h"

GeanyPlugin	*geany_plugin;
GeanyData	*geany_data;
GeanyFunctions	*geany_functions;

PLUGIN_VERSION_CHECK(150)

PLUGIN_SET_INFO(_("Insert Numbers"), _("Insert/Fill columns with numbers."),
		"0.1", "Dimitar Toshkov Zhekov <dimitar.zhekov@gmail.com>");

/* Keybinding(s) */
enum
{
	INSERT_NUMBERS_KB,
	COUNT_KB
};

PLUGIN_KEY_GROUP(insert_numbers, COUNT_KB)

/* when altering the RANGE_ or MAX_LINES, make sure that RANGE_ * MAX_LINES
   fit in gint64, and that RANGE_LEN is enough for RANGE_ digits and '-' */
#define RANGE_MIN (-2147483647 - 1)
#define RANGE_MAX 2147483647
#define RANGE_LEN 11
#define RANGE_TOOLTIP "-2147483648..2147483647"
#define MAX_LINES 500000

typedef struct _InsertNumbersDialog
{
	GtkWidget *dialog;
	GtkWidget *start, *step;
	GtkWidget *base, *lower;
	GtkWidget *prefix, *zero;
} InsertNumbersDialog;

typedef gboolean (*entry_valid)(const gchar *text);

static GObject *tools1_menu = NULL;
static GtkWidget *main_menu_item = NULL;
static gint start_pos, start_line;
static gint end_pos, end_line;
/* dialog data */
static gchar *start_text = NULL;
static gchar *step_text = NULL;
static gchar *base_text = NULL;
static gboolean lower_case = 0;
static gboolean base_prefix = 0;
static gboolean pad_zeros = 0;

static void plugin_beep(void)
{
	if (geany_data->prefs->beep_on_errors)
		gdk_beep();
}

static void free_insert_dialog_data(void)
{
	g_free(start_text);
	g_free(step_text);
	g_free(base_text);
}

static gboolean can_insert_numbers(void)
{
	GeanyDocument *doc = document_get_current();

	if (doc && !doc->readonly)
	{
		ScintillaObject *sci = doc->editor->sci;

		if (sci_has_selection(sci) && sci_get_selection_mode(sci) == SC_SEL_RECTANGLE)
		{
			start_pos = sci_get_selection_start(sci);
			start_line = sci_get_line_from_position(sci, start_pos);
			end_pos = sci_get_selection_end(sci);
			end_line = sci_get_line_from_position(sci, end_pos);

			return end_line - start_line < MAX_LINES;
		}
	}

	return FALSE;
}

static void update_display(void)
{
	while (gtk_events_pending())
        	gtk_main_iteration();
}

/* not included in 0.18 */
static int sci_point_x_from_position(ScintillaObject *sci, gint position)
{
	return scintilla_send_message(sci, SCI_POINTXFROMPOSITION, 0, position);
}

/* not #defined in 0.18 */
#define sci_get_pos_at_line_sel_start(sci, line) \
	scintilla_send_message((sci), SCI_GETLINESELSTARTPOSITION, (line), 0)
#define sci_goto_pos(sci, position) \
	scintilla_send_message((sci), SCI_GOTOPOS, (position), 0)

static void insert_numbers(gboolean *cancel)
{
	/* editor */
	ScintillaObject *sci = document_get_current()->editor->sci;
	gint xinsert = sci_point_x_from_position(sci, start_pos);
	gint xend = sci_point_x_from_position(sci, end_pos);
	gint *line_pos = g_new(gint, end_line - start_line + 1);
	gint line, i;
	/* generator */
	gint64 start = *start_text ? atol(start_text) : 1;
	gint64 value;
	unsigned base = *base_text ? atoi(base_text) : 10;
	unsigned count = 0;
	gint64 step = *step_text ? atol(step_text) : 1;
	size_t prefix_len = 0;
	int plus = 0, minus;
	size_t length, lend;
	char pad, aax;
	gchar *buffer;

	if (xend < xinsert)
		xinsert = xend;

	ui_progress_bar_start(_("Counting..."));
	/* lines shorter than the current selection are skipped */
	for (line = start_line, i = 0; line <= end_line; line++, i++)
	{
          	if (sci_point_x_from_position(sci,
			scintilla_send_message(sci, SCI_GETLINEENDPOSITION, line, 0)) >= xinsert)
		{
			line_pos[i] = sci_get_pos_at_line_sel_start(sci, line) -
				sci_get_position_from_line(sci, line);
			count++;
		}
		else
			line_pos[i] = -1;

		if (cancel && i % 2500 == 0)
		{
			update_display();
			if (*cancel)
			{
				ui_progress_bar_stop();
				return;
			}
		}
	}

	switch (base * base_prefix)
	{
		case 8 : prefix_len = 1; break;
		case 16 : prefix_len = 2; break;
		case 10 : plus++;
	}

	value = start + (count - 1) * step;
	minus = start < 0 || value < 0;
	lend = plus || (pad_zeros ? minus : value < 0);
	while (value /= base) lend++;
	value = start;
	length = plus || (pad_zeros ? minus : value < 0);
	while (value /= base) length++;
	length = prefix_len + (length > lend ? length : lend) + 1;

	buffer = g_new(gchar, length + 1);
	buffer[length] = '\0';
	pad = pad_zeros ? '0' : ' ';
	aax = (lower_case ? 'a' : 'A') - 10;

	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(geany->main_widgets->progressbar),
		_("Preparing..."));
	update_display();
	sci_start_undo_action(sci);
	sci_replace_sel(sci, "");

	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(geany->main_widgets->progressbar),
		_("Inserting..."));
	for (line = start_line, i = 0; line <= end_line; line++, i++)
	{
		gchar *beg, *end;
		gint insert_pos;

		if (line_pos[i] < 0)
			continue;

		beg = buffer;
		end = buffer + length;
		value = ABS(start);

		do
		{
			unsigned digit = value % base;
			*--end = digit + (digit < 10 ? '0' : aax);
		} while (value /= base);

		if (pad_zeros)
		{
			if (start < 0) *beg++ = '-';
			else if (plus) *beg++ = '+';
			else if (minus) *beg++ = ' ';
			memcpy(beg, "0x", prefix_len);
			beg += prefix_len;
		}
		else
		{
			if (start < 0) *--end = '-';
			else if (plus) *--end = '+';
			end -= prefix_len;
			memcpy(end, "0x", prefix_len);
		}

		memset(beg, pad, end - beg);
		insert_pos = sci_get_position_from_line(sci, line) + line_pos[i];
		sci_insert_text(sci, insert_pos, buffer);
		start += step;

		if (cancel && i % 1000 == 0)
		{
			update_display();
			if (*cancel)
			{
				sci_goto_pos(sci, insert_pos + length);
				break;
			}
		}
	}

	sci_end_undo_action(sci);
	g_free(buffer);
	ui_progress_bar_stop();
}

/* interface */
static gboolean range_valid(const gchar *text)
{
	gint64 value = atoll(text);
	return value >= RANGE_MIN && value <= RANGE_MAX;
}

static gboolean base_valid(const gchar *text)
{
	return atoi(text) <= 36;
}

static void on_entry_insert_text(GtkEntry *entry, const gchar *text, gint length, gint *position,
	gpointer data)
{
	GtkEditable *editable = GTK_EDITABLE(entry);
  	gint i, start = *position, pos = *position;
	gchar *result;

	if (length == -1)
		length = strlen(text);
	result = g_new(gchar, strlen(gtk_entry_get_text(entry)) + length + 1);
	strcpy(result, gtk_entry_get_text(entry));

	for (i = 0; i < length; i++)
	{
		if (isdigit(text[i]) || (data == range_valid && pos == 0 && text[i] == '-'))
		{
			memmove(result + pos + 1, result + pos, strlen(result + pos) + 1);
			result[pos++] = text[i];
			if (!((entry_valid) data)(result))
			{
				pos = start;
				plugin_beep();
				break;
			}
		}
	}

	if (pos > start)
	{
		g_signal_handlers_block_by_func(G_OBJECT(editable),
			G_CALLBACK(on_entry_insert_text), data);
		gtk_editable_insert_text(editable, result + start, pos - start, position);
		g_signal_handlers_unblock_by_func(G_OBJECT(editable),
			G_CALLBACK(on_entry_insert_text), data);
	}

	g_signal_stop_emission_by_name(G_OBJECT(editable), "insert_text");
	g_free(result);
}

static void on_insert_numbers_response(G_GNUC_UNUSED GtkDialog *dialog,
	G_GNUC_UNUSED gint response_id, G_GNUC_UNUSED gpointer user_data)
{
	*(gboolean *) user_data = TRUE;
}

static void on_insert_numbers_ok_clicked(G_GNUC_UNUSED GtkButton *button, gpointer user_data)
{
	InsertNumbersDialog *d = (InsertNumbersDialog *) user_data;
	GtkWidget *bad_entry = NULL;

	free_insert_dialog_data();
	start_text = g_strdup(gtk_entry_get_text(GTK_ENTRY(d->start)));
	step_text = g_strdup(gtk_entry_get_text(GTK_ENTRY(d->step)));
	base_text = g_strdup(gtk_entry_get_text(GTK_ENTRY(d->base)));
	lower_case = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(d->lower));
	base_prefix = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(d->prefix));
	pad_zeros = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(d->zero));

	if (!strcmp(step_text, "-"))
		bad_entry = d->step;
	else if (*base_text && atoi(base_text) < 2)
		bad_entry = d->base;

	if (bad_entry)
	{
		plugin_beep();
		gtk_widget_grab_focus(bad_entry);
		return;
	}

	gtk_dialog_response(GTK_DIALOG(d->dialog), GTK_RESPONSE_ACCEPT);
}

static void set_entry(GtkWidget *entry, gint maxlen, GtkWidget *label, entry_valid valid,
	const gchar *tooltip)
{
	gtk_entry_set_max_length(GTK_ENTRY(entry), maxlen);
	gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
	g_signal_connect(entry, "insert-text", G_CALLBACK(on_entry_insert_text), valid);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
	ui_widget_set_tooltip_text(entry, tooltip);
}

static void on_insert_numbers_activate(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED
	gpointer gdata)
{
	InsertNumbersDialog d;
	GtkWidget *vbox, *label, *upper, *space, *button;
	GtkTable *table;
	GtkComboBox *combo;
	const char *case_tip = _("For base 11 and above");
	gint result;

	if (!can_insert_numbers())
	{
		if (!tools1_menu)
			plugin_beep();	/* no visual feedback, so beep */
		return;
	}

	d.dialog = gtk_dialog_new_with_buttons(_("Insert Numbers"),
		GTK_WINDOW(geany->main_widgets->window),
		GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL);
	vbox = ui_dialog_vbox_new(GTK_DIALOG(d.dialog));
	gtk_box_set_spacing(GTK_BOX(vbox), 9);

	table = GTK_TABLE(gtk_table_new(3, 6, FALSE));
	gtk_table_set_row_spacings(table, 6);
	gtk_table_set_col_spacings(table, 6);
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(table), TRUE, TRUE, 0);

	label = gtk_label_new_with_mnemonic(_("_Start:"));
	gtk_table_attach_defaults(table, label, 0, 1, 0, 1);
	d.start = gtk_entry_new();
	set_entry(d.start, RANGE_LEN, label, range_valid, RANGE_TOOLTIP);
	gtk_table_attach_defaults(table, d.start, 1, 3, 0, 1);
	label = gtk_label_new_with_mnemonic(_("S_tep:"));
	gtk_table_attach_defaults(table, label, 3, 4, 0, 1);
	d.step = gtk_entry_new();
	set_entry(d.step, RANGE_LEN, label, range_valid, RANGE_TOOLTIP);
	gtk_table_attach_defaults(table, d.step, 4, 6, 0, 1);

	label = gtk_label_new_with_mnemonic(_("_Base:"));
	gtk_table_attach_defaults(table, label, 0, 1, 1, 2);
	combo = GTK_COMBO_BOX(gtk_combo_box_entry_new_text());
	d.base = gtk_bin_get_child(GTK_BIN(combo));
	set_entry(d.base, 2, label, base_valid, "2..36");
	gtk_combo_box_append_text(combo, "2");
	gtk_combo_box_append_text(combo, "8");
	gtk_combo_box_append_text(combo, "10");
	gtk_combo_box_append_text(combo, "16");
	gtk_table_attach(table, GTK_WIDGET(combo), 1, 3, 1, 2, GTK_EXPAND | GTK_FILL, 0, 0, 0);
	label = gtk_label_new(_("Letters:"));
	ui_widget_set_tooltip_text(label, case_tip);
	gtk_table_attach_defaults(table, label, 3, 4, 1, 2);
	upper = gtk_radio_button_new_with_mnemonic(NULL, _("_Upper"));
	ui_widget_set_tooltip_text(upper, case_tip);
	gtk_table_attach_defaults(table, upper, 4, 5, 1, 2);
	d.lower = gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(upper));
	ui_widget_set_tooltip_text(label, case_tip);
	label = gtk_label_new_with_mnemonic(_("_Lower"));
	gtk_container_add(GTK_CONTAINER(d.lower), label);
	gtk_table_attach_defaults(table, d.lower, 5, 6, 1, 2);

	d.prefix = gtk_check_button_new_with_mnemonic(_("Base _prefix"));
	ui_widget_set_tooltip_text(d.prefix,
		_("0 for octal, 0x for hex, + for positive decimal"));
	gtk_table_attach_defaults(table, d.prefix, 1, 3, 2, 3);
	label = gtk_label_new(_("Padding:"));
	gtk_table_attach_defaults(table, label, 3, 4, 2, 3);
	space = gtk_radio_button_new_with_mnemonic(NULL, _("Sp_ace"));
	gtk_table_attach_defaults(table, space, 4, 5, 2, 3);
	d.zero = gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(space));
	label = gtk_label_new_with_mnemonic(_("_Zero"));
	gtk_container_add(GTK_CONTAINER(d.zero), label);
	gtk_table_attach_defaults(table, d.zero, 5, 6, 2, 3);

	button = gtk_button_new_from_stock(GTK_STOCK_OK);
	g_signal_connect(button, "clicked", G_CALLBACK(on_insert_numbers_ok_clicked), &d);
	gtk_box_pack_end(GTK_BOX(GTK_DIALOG(d.dialog)->action_area), button, TRUE, TRUE, 0);
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_widget_grab_default(button);

	gtk_entry_set_text(GTK_ENTRY(d.start), start_text);
	gtk_entry_set_text(GTK_ENTRY(d.step), step_text);
	gtk_entry_set_text(GTK_ENTRY(d.base), base_text);
	gtk_button_clicked(GTK_BUTTON(lower_case ? d.lower : upper));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(d.prefix), base_prefix);
	gtk_button_clicked(GTK_BUTTON(pad_zeros ? d.zero : space));

	gtk_widget_show_all(d.dialog);
	result = gtk_dialog_run(GTK_DIALOG(d.dialog));

	if (result == GTK_RESPONSE_ACCEPT)
	{
		if (can_insert_numbers())
		{
			if (end_line - start_line < 1000)
			{
				/* quick version */
				gtk_widget_hide(d.dialog);
				insert_numbers(NULL);
			}
			else
			{
				gboolean cancel = FALSE;

				gtk_widget_set_sensitive(GTK_WIDGET(table), FALSE);
				gtk_widget_set_sensitive(button, FALSE);
				update_display();
				g_signal_connect(d.dialog, "response",
					G_CALLBACK(on_insert_numbers_response), &cancel);
				insert_numbers(&cancel);
			}
		}
		else
			plugin_beep();	/* reloaded or something */
	}

	gtk_widget_destroy(d.dialog);
}

static void on_insert_numbers_key(G_GNUC_UNUSED guint key_id)
{
	on_insert_numbers_activate(NULL, NULL);
}

static void on_tools1_activate(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer gdata)
{
	gtk_widget_set_sensitive(main_menu_item, can_insert_numbers());
}

void plugin_init(G_GNUC_UNUSED GeanyData *data)
{
	main_locale_init(LOCALEDIR, GETTEXT_PACKAGE);

	start_text = g_strdup("1");
	step_text = g_strdup("1");
	base_text = g_strdup("10");

	main_menu_item = gtk_menu_item_new_with_mnemonic(_("Insert _Numbers"));
	gtk_widget_show(main_menu_item);
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), main_menu_item);
	g_signal_connect(main_menu_item, "activate", G_CALLBACK(on_insert_numbers_activate),
		NULL);

	keybindings_set_item(plugin_key_group, INSERT_NUMBERS_KB, on_insert_numbers_key,
		0, 0, "insert_numbers", _("Insert Numbers"), main_menu_item);

	/* an "update-tools-menu" or something would have been nice */
	tools1_menu = G_OBJECT(g_object_get_data((gpointer) geany->main_widgets->window,
		"tools1"));
	if (tools1_menu)
	{
		plugin_signal_connect(geany_plugin, tools1_menu, "activate", FALSE,
			(GCallback) on_tools1_activate, NULL);
	}
}

void plugin_cleanup(void)
{
	gtk_widget_destroy(main_menu_item);
	free_insert_dialog_data();
}
