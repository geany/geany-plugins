/*
 *
 *  Copyright (C) 2024  Sylvain Cresto <scresto@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include <string.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <geanyplugin.h>

#define INCDEC_VERSION "1.0"

#define CONF_GROUP "Settings"
#define CONF_SHOW_IN_MENU "show_in_menu"

#define SSM(s, m, w, l) scintilla_send_message((s), (m), (w), (l))

GeanyPlugin	*geany_plugin;
GeanyData	*geany_data;

PLUGIN_VERSION_CHECK(224)

PLUGIN_SET_TRANSLATABLE_INFO(
	LOCALEDIR, GETTEXT_PACKAGE,
	_("IncDec"),
	_("Provides shortcuts to increment and decrement number at (or to the right of) the cursor"),
	INCDEC_VERSION,
	"Sylvain Cresto <scresto@gmail.com>"
)

/* Plugin */
enum {
	KB_INCREMENT_NUMBER,
	KB_DECREMENT_NUMBER,
	KB_INCREMENT_DECREMENT_NUMBER_X,
	KB_COUNT
};

struct {
	GtkWidget	*_dialog;
	GtkWidget	*_entry;
	GtkWidget	*_display_in_popup;
	GtkWidget	*_menu_item_sep;
	GtkWidget	*_menu_item_change_number;
	gboolean	show_in_menu;
} plugin_data = {
	NULL, NULL, NULL, NULL, NULL, TRUE
};

/* constants used when detecting hexadecimal and decimal numbers */
#define HEXA_NONE               -1      /* hexadecimal number not found */
#define HEXA_MAYBE              0       /* in progress, need scan more characters */
#define HEXA_REQUIRE_x          1       /* need character x/X */
#define HEXA_REQUIRE_0          2       /* need character 0 */
#define HEXA_REQUIRE_xnumber    3       /* need at least one number */
#define HEXA_ACCEPT_xnumber     4       /* there may be more numbers */
#define HEXA_FOUND              5       /* ok, hexadecimal number found */

#define DECIMAL_NONE            -1      /* decimal number not found */
#define DECIMAL_MAYBE           0       /* in progress, need scan more characters */
#define DECIMAL_REQUIRE_number  1       /* need at least one number */
#define DECIMAL_FOUND           5       /* ok, decimal number found */

#define HEXA_CASE_UNKNOWN       -1      /* hexadicmal case not yey known */
#define HEXA_CASE_UPPER         1       /* upper case detected */
#define HEXA_CASE_LOWER         0       /* lower case detected */


static gint sci_get_position_after(ScintillaObject *sci, gint start)
{
	return (gint) SSM(sci, SCI_POSITIONAFTER, (uptr_t) start, 0);
}


static gint sci_get_position_before(ScintillaObject *sci, gint start)
{
	return (gint) SSM(sci, SCI_POSITIONBEFORE, (uptr_t) start, 0);
}


static gboolean on_change_number(gint step)
{
	ScintillaObject *sci = document_get_current()->editor->sci;

	gint pos = sci_get_current_position(sci);
	gint line = sci_get_line_from_position(sci, pos);
	gint line_start = sci_get_position_from_line(sci, line);
	gint line_end = sci_get_line_end_position(sci, line);
	gchar ch = 0, first_ch = 0;
	gint64 guessed_number = 0;
	gchar *buf;

	/* position of number in decimal or hexadecimal format */
	gint decimal_start = -1, decimal_end = -1;
	gint hexadecimal_start = -1, hexadecimal_end = -1;

	gint i, prev_i;

	gint hexa_state = 0;
	gint decimal_sate = 0;
	gint hexaCase = HEXA_CASE_UNKNOWN;

	/* if we are at the end of the line we go to the previous character */
	if (pos == line_end && pos > line_start)
	{
		pos = sci_get_position_before(sci, pos);
	}

	/* first we search backward to check if the beginning of the number is not before the cursor */
	for (i=pos, prev_i = -1; i >= line_start && i != prev_i; prev_i = i, i = sci_get_position_before(sci, i))
	{
		ch = sci_get_char_at(sci, i);

		if (i == pos)
		{
			first_ch = ch;
			if (ch == '-')
			{
				decimal_start = i;
				decimal_sate = DECIMAL_REQUIRE_number;
				break;
			}
		}

		if (hexa_state == HEXA_REQUIRE_0)
		{
			if (ch != '0')
			{
				hexa_state = HEXA_NONE;
			}
			else
			{
				hexa_state = hexadecimal_start == -1 ? HEXA_REQUIRE_xnumber : HEXA_ACCEPT_xnumber;
				hexadecimal_start = i;
			}
			break;
		}

		if (decimal_sate == DECIMAL_MAYBE)
		{
			if (g_ascii_isdigit(ch) || ch == '-')
			{
				decimal_start = i;
				if (ch == '-')
					decimal_sate = DECIMAL_FOUND;
			}
			else if (decimal_start == -1)
			{
				decimal_sate = DECIMAL_NONE;
			}
			else
			{
				decimal_sate = DECIMAL_FOUND;
			}
		}
		if (hexa_state == HEXA_MAYBE)
		{
			if (!g_ascii_isxdigit(ch))
			{
				if (ch == 'x' || ch == 'X')
				{
					hexa_state = HEXA_REQUIRE_0;
				}
				else
				{
					hexa_state = HEXA_NONE;
				}
			}
			else
			{
				if (i == pos)
					hexadecimal_start = i;
				if (hexaCase == HEXA_CASE_UNKNOWN && g_ascii_isalpha(ch))
				{
					hexaCase = g_ascii_isupper(ch) ? HEXA_CASE_UPPER : HEXA_CASE_LOWER;
				}
			}
		}

		if ((hexa_state == HEXA_NONE || hexa_state == HEXA_REQUIRE_x) && (decimal_sate == DECIMAL_NONE || decimal_sate == DECIMAL_FOUND))
			break;
	}

	if (hexadecimal_start == pos && first_ch == '0')
	{
		hexa_state = HEXA_REQUIRE_x;
	}
	else if (hexa_state == HEXA_NONE || hexa_state == HEXA_MAYBE)
	{
		hexa_state = HEXA_REQUIRE_0;
	}


	if (decimal_sate == DECIMAL_FOUND || decimal_sate == DECIMAL_NONE)
		decimal_sate = DECIMAL_MAYBE;

	/* now we go ahead to detect the beginning if we have not found it yet and the end of the number */
	for (i=sci_get_position_after(sci, pos), prev_i = -1; i <= line_end && prev_i != i; prev_i = i, i = sci_get_position_after(sci, i))
	{
		ch = sci_get_char_at(sci, i);

		if (decimal_sate == DECIMAL_REQUIRE_number)
		{
			if (ch == '-')
			{
				decimal_start = i;
			}
			else if (g_ascii_isdigit(ch))
			{
				decimal_sate = DECIMAL_MAYBE;
			}
		}
		else if (decimal_sate == DECIMAL_MAYBE)
		{
			if (!g_ascii_isdigit(ch))
			{
				if (decimal_start != -1)
				{
					decimal_end = i;
					decimal_sate = DECIMAL_FOUND;
				}
				else if (ch == '-')
				{
					decimal_sate = DECIMAL_REQUIRE_number;
					decimal_start = i;
				}
			}
			else if (decimal_start == -1)
			{
				decimal_start = i;
			}
		}

		if (hexa_state == HEXA_REQUIRE_0)
		{
			if (ch == '0')
			{
				hexa_state = HEXA_REQUIRE_x;
				hexaCase = HEXA_CASE_UNKNOWN;
				hexadecimal_start = i;
			}
		}
		else if (hexa_state == HEXA_REQUIRE_x)
		{
			if (ch == 'x' || ch == 'X')
			{
				hexa_state = HEXA_REQUIRE_xnumber;
			}
			else if (ch != '0')
			{
				hexa_state = HEXA_REQUIRE_0;
				hexadecimal_start = -1;
			}
		}
		else if (hexa_state == HEXA_REQUIRE_xnumber)
		{
			if (!g_ascii_isxdigit(ch))
			{
				hexa_state = HEXA_REQUIRE_0;
				hexadecimal_start = -1;
			}
			else
			{
				hexa_state = HEXA_ACCEPT_xnumber;

				if (g_ascii_isalpha(ch))
					hexaCase = g_ascii_isupper(ch) ? HEXA_CASE_UPPER : HEXA_CASE_LOWER;
			}
		}
		else if (hexa_state == HEXA_ACCEPT_xnumber)
		{
			if (!g_ascii_isxdigit(ch))
			{
				hexadecimal_end = i;
				hexa_state = HEXA_FOUND;
			}
			else
			{
				if (g_ascii_isalpha(ch))
				{
					hexaCase = g_ascii_isupper(ch) ? HEXA_CASE_UPPER : HEXA_CASE_LOWER;
				}
			}
		}

		if (hexa_state == HEXA_FOUND && decimal_sate == DECIMAL_FOUND)
			break;
	}

	if (hexa_state == HEXA_ACCEPT_xnumber)
	{
		hexa_state = HEXA_FOUND;
		hexadecimal_end = i;
	}
	if (hexa_state == HEXA_FOUND)
	{
		hexadecimal_start += 2;
	}
	if (decimal_sate == DECIMAL_MAYBE)
	{
		if (decimal_start == -1)
		{
			decimal_sate = DECIMAL_NONE;
		}
		else
		{
			decimal_end = i;
			decimal_sate = DECIMAL_FOUND;
		}
	}

	if (hexa_state == HEXA_FOUND || decimal_sate == DECIMAL_FOUND)
	{
		/* ok so we found a number to process */
		gchar *txt;
		gchar format_buf[13];
		gint format_length;
		gboolean use_hexa = (decimal_sate != DECIMAL_FOUND || (hexa_state == HEXA_FOUND && hexadecimal_start <= decimal_start + 2)) ? TRUE : FALSE;
		gboolean positive = TRUE;
		gint digit_start = use_hexa ? hexadecimal_start : decimal_start;
		gint digit_end = use_hexa ? hexadecimal_end : decimal_end;

		txt = sci_get_contents_range(sci, digit_start, digit_end);
		if (txt == NULL)
			return TRUE;

		/* we convert the number from hexadecimal or decimal format */
		if (use_hexa)
		{
			guessed_number = g_ascii_strtoll(txt, NULL, 16);
		}
		else
		{
			guessed_number = atoi(txt);
		}
		g_free(txt);

		positive = (guessed_number < 0) ? FALSE : TRUE;
		guessed_number += step;

		/* when the number changes sign, the format is reset to avoid a display shift */
		if ((positive == FALSE && guessed_number >= 0) || (positive == TRUE && guessed_number < 0))
		{
			format_length = 0;
		}
		else
		{
			format_length = digit_end - digit_start;
			if (format_length > 12)
				format_length = 0;
		}

		g_snprintf(format_buf, sizeof(format_buf)-1, "%%0%d%c", format_length, use_hexa ? (hexaCase == HEXA_CASE_UPPER ? 'X' : 'x') : 'd');

		if ((buf = g_strdup_printf(format_buf, guessed_number)))
		{
			sci_set_target_start(sci, digit_start);
			sci_set_target_end(sci, digit_end);
			gint nLen = sci_replace_target(sci, buf, FALSE);
			sci_set_current_position(sci, digit_start + nLen - 1, TRUE);

			g_free(buf);
		}
	}

	return TRUE;
}

static void configure_response(GtkDialog *dialog, gint response, gpointer data)
{
	if (response != GTK_RESPONSE_OK || plugin_data._entry == NULL)
		return;

	gint increment = gtk_spin_button_get_value(GTK_SPIN_BUTTON(plugin_data._entry));

	if (increment)
		on_change_number(increment);
}

static gboolean on_change_number_x(G_GNUC_UNUSED GtkMenuItem * menuitem, G_GNUC_UNUSED gpointer gdata)
{
	if (plugin_data._dialog == NULL)
	{
		plugin_data._dialog = gtk_dialog_new_with_buttons(_("Increment or Decrement number by ..."),
						GTK_WINDOW(geany->main_widgets->window),
						GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, _("Cancel"),
						GTK_RESPONSE_CANCEL, _("OK"), GTK_RESPONSE_OK,
						NULL);

		gtk_dialog_set_default_response(GTK_DIALOG(plugin_data._dialog), GTK_RESPONSE_OK);

		GtkWidget *okButton = gtk_dialog_get_widget_for_response(GTK_DIALOG(plugin_data._dialog), GTK_RESPONSE_OK);
		gtk_widget_set_can_default(okButton, TRUE);

		GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(plugin_data._dialog));

		GtkWidget *label = gtk_label_new("Enter the number of positive or negative increments to apply.\n");

		plugin_data._entry = gtk_spin_button_new_with_range(-64000, 64000,1);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(plugin_data._entry), 1);
		gtk_widget_set_tooltip_text(plugin_data._entry, _("Enter the number of positive or negative increments to apply then press Enter key.\n"));
		gtk_entry_set_activates_default(GTK_ENTRY(plugin_data._entry), TRUE);

		GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
		GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
		gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 10);
		gtk_box_pack_start(GTK_BOX(vbox), plugin_data._entry, FALSE, FALSE, 10);
		gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 10);

		gtk_container_add(GTK_CONTAINER(content_area), hbox);

		g_signal_connect(plugin_data._dialog, "response", G_CALLBACK(configure_response), NULL);
	}

	gtk_widget_show_all(plugin_data._dialog);
	gtk_widget_grab_focus(plugin_data._entry);

	while (gtk_dialog_run(GTK_DIALOG(plugin_data._dialog)) == GTK_RESPONSE_ACCEPT);

	gtk_widget_set_visible(plugin_data._dialog, FALSE);

	return TRUE;
}


static void configuration_apply(void)
{
	if (plugin_data.show_in_menu)
	{
		gtk_widget_show_all(plugin_data._menu_item_sep);
		gtk_widget_show_all(plugin_data._menu_item_change_number);
	}
	else
	{
		gtk_widget_hide(plugin_data._menu_item_sep);
		gtk_widget_hide(plugin_data._menu_item_change_number);
	}
}


static gboolean on_change_number_callback(guint key_id)
{
	switch (key_id)
	{
		case KB_INCREMENT_NUMBER:
			return on_change_number(1);
		case KB_DECREMENT_NUMBER:
			return on_change_number(-1);
		case KB_INCREMENT_DECREMENT_NUMBER_X:
			return on_change_number_x(NULL, NULL);
	}
	return FALSE;
}


static gchar *get_config_filename(void)
{
	return g_build_filename(geany_data->app->configdir, "plugins", PLUGIN, PLUGIN".conf", NULL);
}


static void load_config(void)
{
	gchar *filename = get_config_filename();
	GKeyFile *kf = g_key_file_new();

	if (g_key_file_load_from_file(kf, filename, G_KEY_FILE_NONE, NULL))
	{
		plugin_data.show_in_menu = utils_get_setting_boolean(kf, CONF_GROUP, CONF_SHOW_IN_MENU, TRUE);
	}

	g_key_file_free(kf);
	g_free(filename);
}


static gboolean save_config(void)
{
	GKeyFile *kf = g_key_file_new();
	gchar *filename = get_config_filename();
	gchar *dirname = g_path_get_dirname(filename);
	gchar *data;
	gsize length;
	gboolean status;

	g_key_file_set_boolean(kf, CONF_GROUP, CONF_SHOW_IN_MENU, plugin_data.show_in_menu);

	utils_mkdir(dirname, TRUE);
	data = g_key_file_to_data(kf, &length, NULL);
	status = g_file_set_contents(filename, data, length, NULL);

	g_free(data);
	g_key_file_free(kf);
	g_free(filename);
	g_free(dirname);

	return status;
}


static void on_configure_response(GtkDialog *dialog, gint response, gpointer user_data)
{
	if (response != GTK_RESPONSE_OK && response != GTK_RESPONSE_APPLY)
		return;

	plugin_data.show_in_menu = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(plugin_data._display_in_popup));

	if (! save_config())
		dialogs_show_msgbox(GTK_MESSAGE_ERROR, _("Plugin configuration directory could not be saved."));

	configuration_apply();
}


GtkWidget *plugin_configure(GtkDialog *dialog)
{
	GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

	plugin_data._display_in_popup = gtk_check_button_new_with_label(_("Display in Popup editor menu"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(plugin_data._display_in_popup), plugin_data.show_in_menu);
	gtk_box_pack_start(GTK_BOX(vbox), plugin_data._display_in_popup, FALSE, FALSE, 10);

	gtk_widget_show_all(vbox);
	g_signal_connect(dialog, "response", G_CALLBACK(on_configure_response), NULL);

	return vbox;
}


void plugin_init (GeanyData *data)
{
	GeanyKeyGroup *key_group;

	load_config();

	key_group = plugin_set_key_group(geany_plugin, "incdec", KB_COUNT, on_change_number_callback);
	keybindings_set_item(key_group, KB_INCREMENT_NUMBER, NULL, GDK_KEY_A, GDK_SHIFT_MASK,
				"increment_number",
				_("Increment Number By 1"), NULL);
	keybindings_set_item(key_group, KB_DECREMENT_NUMBER, NULL, GDK_KEY_X, GDK_SHIFT_MASK,
				"decrement_number",
				_("Decrement Number By 1"), NULL);
	keybindings_set_item(key_group, KB_INCREMENT_DECREMENT_NUMBER_X, NULL, GDK_KEY_M, GDK_SHIFT_MASK,
				"increment_decrement_number_x",
				_("Increment or Decrement Number X times"), NULL);

	plugin_data._menu_item_sep = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->editor_menu), plugin_data._menu_item_sep);

	plugin_data._menu_item_change_number = gtk_menu_item_new_with_mnemonic(_("_Increment or Decrement number"));
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->editor_menu), plugin_data._menu_item_change_number);

	configuration_apply();

	g_signal_connect(plugin_data._menu_item_change_number, "activate", G_CALLBACK(on_change_number_x), NULL);
}


void plugin_cleanup (void)
{
	if (plugin_data._dialog)
	{
		gtk_widget_destroy(GTK_WIDGET(plugin_data._dialog));
		plugin_data._dialog = NULL;
		plugin_data._entry = NULL;
	}
}


void plugin_help (void)
{
	utils_open_browser(DOCDIR "/" PLUGIN "/README");
}
