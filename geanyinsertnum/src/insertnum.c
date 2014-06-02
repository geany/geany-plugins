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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "geanyplugin.h"

#ifndef GTK_COMPAT_H
#define GtkComboBoxText GtkComboBox
#define GTK_COMBO_BOX_TEXT GTK_COMBO_BOX
#define gtk_combo_box_text_new_with_entry gtk_combo_box_entry_new_text
#define gtk_combo_box_text_append_text gtk_combo_box_append_text
#endif

GeanyPlugin	*geany_plugin;
GeanyData	*geany_data;
GeanyFunctions	*geany_functions;

PLUGIN_VERSION_CHECK(189)

PLUGIN_SET_INFO(_("Insert Numbers"), _("Insert/Fill columns with numbers."),
	"0.2.2", "Dimitar Toshkov Zhekov <dimitar.zhekov@gmail.com>")

/* Keybinding(s) */
enum
{
	INSERT_NUMBERS_KB,
	COUNT_KB
};

/* when altering the RANGE_ or MAX_LINES, make sure that RANGE_ * MAX_LINES
   fit in gint64, and that RANGE_LEN is enough for RANGE_ digits and sign */
#define RANGE_MIN (-2147483647 - 1)
#define RANGE_MAX 2147483647
#define RANGE_LEN 11
#define RANGE_TOOLTIP "-2147483648..2147483647"
#define MAX_LINES 250000

typedef struct _InsertNumbersDialog
{
	GtkWidget *dialog;
	GtkWidget *start, *step;
	GtkWidget *base, *lower;
	GtkWidget *prefix, *zero;
} InsertNumbersDialog;

typedef gboolean (*entry_valid)(const gchar *text);

static GtkWidget *main_menu_item = NULL;
static gint start_pos, start_line;
static gint end_pos, end_line;
/* input data */
static gint64 start_value;
static gint64 step_value;
static gint base_value;
static gboolean lower_case = 0;
static gboolean base_prefix = 0;
static gboolean pad_zeros = 0;

static void plugin_beep(void)
{
	if (geany_data->prefs->beep_on_errors)
		gdk_beep();
}

static gboolean can_insert_numbers(void)
{
	GeanyDocument *doc = document_get_current();

	if (doc && !doc->readonly)
	{
		ScintillaObject *sci = doc->editor->sci;

		if (sci_has_selection(sci) && (sci_get_selection_mode(sci) == SC_SEL_RECTANGLE ||
			sci_get_selection_mode(sci) == SC_SEL_THIN))
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

#define sci_point_x_from_position(sci, position) \
	scintilla_send_message(sci, SCI_POINTXFROMPOSITION, 0, position)
#define sci_get_pos_at_line_sel_start(sci, line) \
	scintilla_send_message(sci, SCI_GETLINESELSTARTPOSITION, line, 0)

static void insert_numbers(gboolean *cancel)
{
	/* editor */
	ScintillaObject *sci = document_get_current()->editor->sci;
	gint xinsert = sci_point_x_from_position(sci, start_pos);
	gint xend = sci_point_x_from_position(sci, end_pos);
	gint *line_pos = g_new(gint, end_line - start_line + 1);
	gint line, i;
	/* generator */
	gint64 start = start_value;
	gint64 value;
	unsigned count = 0;
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
				g_free(line_pos);
				return;
			}
		}
	}

	switch (base_value * base_prefix)
	{
		case 8 : prefix_len = 1; break;
		case 16 : prefix_len = 2; break;
		case 10 : plus++;
	}

	value = start + (count - 1) * step_value;
	minus = start < 0 || value < 0;
	lend = plus || (pad_zeros ? minus : value < 0);
	while (value /= base_value) lend++;
	value = start;
	length = plus || (pad_zeros ? minus : value < 0);
	while (value /= base_value) length++;
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
			unsigned digit = value % base_value;
			*--end = digit + (digit < 10 ? '0' : aax);
		} while (value /= base_value);

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
		start += step_value;

		if (cancel && i % 1000 == 0)
		{
			update_display();
			if (*cancel)
			{
				scintilla_send_message(sci, SCI_GOTOPOS, insert_pos + length, 0);
				break;
			}
		}
	}

	sci_end_undo_action(sci);
	g_free(buffer);
	g_free(line_pos);
	ui_progress_bar_stop();
}

/* interface */
static void on_base_insert_text(G_GNUC_UNUSED GtkEntry *entry, const gchar *text, gint length,
	G_GNUC_UNUSED gint *position, G_GNUC_UNUSED gpointer data)
{
	gint i;

	if (length == -1)
		length = strlen(text);

	for (i = 0; i < length; i++)
	{
		if (!isdigit(text[i]))
		{
			g_signal_stop_emission_by_name(G_OBJECT(entry), "insert-text");
			break;
		}
	}
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

	start_value = gtk_spin_button_get_value(GTK_SPIN_BUTTON(d->start));
	step_value = gtk_spin_button_get_value(GTK_SPIN_BUTTON(d->step));
	base_value = atoi(gtk_entry_get_text(GTK_ENTRY(d->base)));
	lower_case = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(d->lower));
	base_prefix = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(d->prefix));
	pad_zeros = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(d->zero));

	if (!step_value)
		bad_entry = d->step;
	else if (base_value < 2 || base_value > 36)
		bad_entry = d->base;

	if (bad_entry)
	{
		plugin_beep();
		gtk_widget_grab_focus(bad_entry);
		return;
	}

	gtk_dialog_response(GTK_DIALOG(d->dialog), GTK_RESPONSE_ACCEPT);
}

static void set_entry(GtkWidget *entry, gint maxlen, GtkWidget *label, const gchar *tooltip)
{
	gtk_entry_set_max_length(GTK_ENTRY(entry), maxlen);
	gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
	ui_widget_set_tooltip_text(entry, tooltip);
}

#if !GTK_CHECK_VERSION(3, 0, 0)
#define GtkGrid GtkTable
#define GTK_GRID GTK_TABLE
#define gtk_grid_attach(grid, child, left, top, width, height) \
	gtk_table_attach_defaults(grid, child, left, left + width, top, top + height)
#define gtk_grid_set_row_spacing gtk_table_set_row_spacings
#define gtk_grid_set_column_spacing gtk_table_set_col_spacings
#endif

static void on_insert_numbers_activate(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED
	gpointer gdata)
{
	InsertNumbersDialog d;
	GtkWidget *vbox, *label, *upper, *space, *button;
	GtkGrid *grid;
	GtkComboBoxText *combo;
	const char *case_tip = _("For base 11 and above");
	gchar *base_text;
	gint result;

	d.dialog = gtk_dialog_new_with_buttons(_("Insert Numbers"),
		GTK_WINDOW(geany->main_widgets->window),
		GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL);
	vbox = ui_dialog_vbox_new(GTK_DIALOG(d.dialog));
	gtk_box_set_spacing(GTK_BOX(vbox), 9);

#if GTK_CHECK_VERSION(3, 0, 0)
	grid = GTK_GRID(gtk_grid_new());
#else
	grid = GTK_TABLE(gtk_table_new(3, 6, FALSE));
#endif
	gtk_grid_set_row_spacing(grid, 6);
	gtk_grid_set_column_spacing(grid, 6);
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(grid), TRUE, TRUE, 0);

	label = gtk_label_new_with_mnemonic(_("_Start:"));
	gtk_grid_attach(grid, label, 0, 0, 1, 1);
	d.start = gtk_spin_button_new_with_range(RANGE_MIN, RANGE_MAX, 1);
	set_entry(d.start, RANGE_LEN, label, RANGE_TOOLTIP);
	gtk_grid_attach(grid, d.start, 1, 0, 2, 1);
	label = gtk_label_new_with_mnemonic(_("S_tep:"));
	gtk_grid_attach(grid, label, 3, 0, 1, 1);
	d.step = gtk_spin_button_new_with_range(RANGE_MIN, RANGE_MAX, 1);
	set_entry(d.step, RANGE_LEN, label, RANGE_TOOLTIP);
	gtk_grid_attach(grid, d.step, 4, 0, 2, 1);

	label = gtk_label_new_with_mnemonic(_("_Base:"));
	gtk_grid_attach(grid, label, 0, 1, 1, 1),
	combo = GTK_COMBO_BOX_TEXT(gtk_combo_box_text_new_with_entry());
	d.base = gtk_bin_get_child(GTK_BIN(combo));
	set_entry(d.base, 2, label, "2..36");
	g_signal_connect(d.base, "insert-text", G_CALLBACK(on_base_insert_text), NULL);
	gtk_combo_box_text_append_text(combo, "2");
	gtk_combo_box_text_append_text(combo, "8");
	gtk_combo_box_text_append_text(combo, "10");
	gtk_combo_box_text_append_text(combo, "16");
#if GTK_CHECK_VERSION(3, 0, 0)
	gtk_grid_attach(grid, GTK_WIDGET(combo), 1, 1, 2, 1);
	gtk_widget_set_hexpand(GTK_WIDGET(combo), TRUE);
#else
	gtk_table_attach(grid, GTK_WIDGET(combo), 1, 3, 1, 2, GTK_EXPAND | GTK_FILL, 0, 0, 0);
#endif
	label = gtk_label_new(_("Letters:"));
	ui_widget_set_tooltip_text(label, case_tip);
	gtk_grid_attach(grid, label, 3, 1, 1, 1);
	upper = gtk_radio_button_new_with_mnemonic(NULL, _("_Upper"));
	ui_widget_set_tooltip_text(upper, case_tip);
	gtk_grid_attach(grid, upper, 4, 1, 1, 1);
	d.lower = gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(upper));
	ui_widget_set_tooltip_text(label, case_tip);
	label = gtk_label_new_with_mnemonic(_("_Lower"));
	ui_widget_set_tooltip_text(label, case_tip);
	gtk_container_add(GTK_CONTAINER(d.lower), label);
	gtk_grid_attach(grid, d.lower, 5, 1, 1, 1);

	d.prefix = gtk_check_button_new_with_mnemonic(_("Base _prefix"));
	ui_widget_set_tooltip_text(d.prefix,
		_("0 for octal, 0x for hex, + for positive decimal"));
	gtk_grid_attach(grid, d.prefix, 1, 2, 2, 1);
	label = gtk_label_new(_("Padding:"));
	gtk_grid_attach(grid, label, 3, 2, 1, 1);
	space = gtk_radio_button_new_with_mnemonic(NULL, _("Sp_ace"));
	gtk_grid_attach(grid, space, 4, 2, 1, 1);
	d.zero = gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(space));
	label = gtk_label_new_with_mnemonic(_("_Zero"));
	gtk_container_add(GTK_CONTAINER(d.zero), label);
	gtk_grid_attach(grid, d.zero, 5, 2, 1, 1);

	button = gtk_button_new_from_stock(GTK_STOCK_OK);
	g_signal_connect(button, "clicked", G_CALLBACK(on_insert_numbers_ok_clicked), &d);
	gtk_box_pack_end(GTK_BOX(gtk_dialog_get_action_area(GTK_DIALOG(d.dialog))), button,
		TRUE, TRUE, 0);
#if GTK_CHECK_VERSION(2, 18, 0)
	gtk_widget_set_can_default(button, TRUE);
#else
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
#endif
	gtk_widget_grab_default(button);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(d.start), start_value);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(d.step), step_value);
	base_text = g_strdup_printf("%d", base_value);
	gtk_entry_set_text(GTK_ENTRY(d.base), base_text);
	g_free(base_text);
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

				gtk_widget_set_sensitive(GTK_WIDGET(grid), FALSE);
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
	if (can_insert_numbers())
		on_insert_numbers_activate(NULL, NULL);
}

static void on_tools_show(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer gdata)
{
	gtk_widget_set_sensitive(main_menu_item, can_insert_numbers());
}

void plugin_init(G_GNUC_UNUSED GeanyData *data)
{
	GeanyKeyGroup *plugin_key_group;

	main_locale_init(LOCALEDIR, GETTEXT_PACKAGE);
	plugin_key_group = plugin_set_key_group(geany_plugin, "insert_numbers", COUNT_KB, NULL);

	start_value = 1;
	step_value = 1;
	base_value = 10;

	main_menu_item = gtk_menu_item_new_with_mnemonic(_("Insert _Numbers"));
	gtk_widget_show(main_menu_item);
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), main_menu_item);
	g_signal_connect(main_menu_item, "activate", G_CALLBACK(on_insert_numbers_activate),
		NULL);

	keybindings_set_item(plugin_key_group, INSERT_NUMBERS_KB, on_insert_numbers_key,
		0, 0, "insert_numbers", _("Insert Numbers"), main_menu_item);

	plugin_signal_connect(geany_plugin, G_OBJECT(geany->main_widgets->tools_menu), "show",
		FALSE, (GCallback) on_tools_show, NULL);
}

void plugin_cleanup(void)
{
	gtk_widget_destroy(main_menu_item);
}
