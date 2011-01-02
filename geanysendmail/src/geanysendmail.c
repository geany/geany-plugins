/*
 *      geanysendmail.c
 *
 *      Copyright 2007-2011 Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
 *      Copyright 2007 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2007, 2008 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
 *      Copyright 2008, 2009 Timothy Boronczyk <tboronczyk(at)gmail(dot)com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/* A little plugin to send a document as attachment using the preferred mail client */

#include "geanyplugin.h"
#include "icon.h"


#ifdef HAVE_LOCALE_H
# include <locale.h>
#endif

GeanyPlugin		*geany_plugin;
GeanyData		*geany_data;
GeanyFunctions	*geany_functions;

PLUGIN_VERSION_CHECK(199)

PLUGIN_SET_TRANSLATABLE_INFO(
	LOCALEDIR,
	GETTEXT_PACKAGE,
	_("GeanySendMail"),
	_("A little plugin to send the current "
	  "file as attachment by user's favorite mailer"),
	VERSION,
	"Frank Lanitz <frank@frank.uvena.de>")

/* Keybinding(s) */
enum
{
	SENDMAIL_KB,
	COUNT_KB
};

static gchar *config_file = NULL;
static gchar *mailer = NULL;
static gchar *address = NULL;
gboolean icon_in_toolbar = FALSE;
gboolean use_address_dialog = FALSE;
/* Needed global to remove from toolbar again */
GtkWidget *mailbutton = NULL;
static GtkWidget *main_menu_item = NULL;


/* Callback for sending file as attachment */
static void
send_as_attachment(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer gdata)
{
	GeanyDocument *doc;
	gchar	*locale_filename = NULL;
	gchar	*command = NULL;
	GError	*error = NULL;
	GString	*cmd_str = NULL;
	GKeyFile 	*config = g_key_file_new();
	gchar 		*config_dir = g_path_get_dirname(config_file);
	gchar 		*data;

	doc = document_get_current();

	if (doc->file_name == NULL)
	{
		dialogs_show_save_as();
	}
	else
	{
		document_save_file(doc, FALSE);
	}

    if (doc->file_name != NULL)
	{
		if (mailer)
		{
			locale_filename = utils_get_locale_from_utf8(doc->file_name);
			cmd_str = g_string_new(mailer);

			if ((use_address_dialog == TRUE) && (g_strrstr(mailer, "%r") != NULL))
			{
 				gchar *input = dialogs_show_input(_("Recipient's Address"),
										GTK_WINDOW(geany->main_widgets->window),
										_("Enter the recipient's e-mail address:"),
										address);

				if (input)
				{
					g_key_file_load_from_file(config, config_file, G_KEY_FILE_NONE, NULL);

					g_free(address);
 					address = input;

 					g_key_file_set_string(config, "tools", "address", address);
 				}
 				else
 				{
					return;
				}

				if (! g_file_test(config_dir, G_FILE_TEST_IS_DIR) &&
				      utils_mkdir(config_dir, TRUE) != 0)
 				{
 					dialogs_show_msgbox(GTK_MESSAGE_ERROR,
 						_("Plugin configuration directory could not be created."));
 				}
 				else
 				{
 					/* write config to file */
 					data = g_key_file_to_data(config, NULL, NULL);
 					utils_write_file(config_file, data);
					g_free(data);
					g_key_file_free(config);
 					g_free(config_dir);
 				}
 			}

			if (! utils_string_replace_all(cmd_str, "%f", locale_filename))
				ui_set_statusbar(FALSE,
				_("Filename placeholder not found. The executed command might have failed."));

			if (use_address_dialog == TRUE && address != NULL)
			{
				if (! utils_string_replace_all(cmd_str, "%r", address))
 					ui_set_statusbar(FALSE,
					_("Recipient address placeholder not found. The executed command might have failed."));
					g_free(address);
			}
			else
			{
				/* Removes %r if option was not activ but was included into command */
				utils_string_replace_all(cmd_str, "%r", NULL);
				g_free(address);
			}

			utils_string_replace_all(cmd_str, "%b", g_path_get_basename(locale_filename));

			command = g_string_free(cmd_str, FALSE);
			g_spawn_command_line_async(command, &error);
			if (error != NULL)
			{
				ui_set_statusbar(FALSE, _("Could not execute mailer. Please check your configuration."));
				g_error_free(error);
			}

			g_free(locale_filename);
			g_free(command);
		}
		else
		{
			ui_set_statusbar(FALSE, _("Please define a mail client first."));
		}
	}
	else
	{
		ui_set_statusbar(FALSE, _("File has to be saved before sending."));
	}
}

static void key_send_as_attachment(G_GNUC_UNUSED guint key_id)
{
	send_as_attachment(NULL, NULL);
}

#define GEANYSENDMAIL_STOCK_MAIL "geanysendmail-mail"

static void add_stock_item(void)
{
	GtkIconSet *icon_set;
	GtkIconFactory *factory = gtk_icon_factory_new();
	GtkIconTheme *theme = gtk_icon_theme_get_default();
	GtkStockItem item = { GEANYSENDMAIL_STOCK_MAIL, _("Mail"), 0, 0, GETTEXT_PACKAGE };

	if (gtk_icon_theme_has_icon(theme, "mail-message-new"))
	{
		GtkIconSource *icon_source = gtk_icon_source_new();
		icon_set = gtk_icon_set_new();
		gtk_icon_source_set_icon_name(icon_source, "mail-message-new");
		gtk_icon_set_add_source(icon_set, icon_source);
		gtk_icon_source_free(icon_source);
	}
	else
	{
		GdkPixbuf *pb = gdk_pixbuf_new_from_inline(-1, mail_pixbuf, FALSE, NULL);
		icon_set = gtk_icon_set_new_from_pixbuf(pb);
		g_object_unref(pb);
	}
	gtk_icon_factory_add(factory, item.stock_id, icon_set);
	gtk_stock_add(&item, 1);
	gtk_icon_factory_add_default(factory);

	g_object_unref(factory);
	gtk_icon_set_unref(icon_set);
}


static void show_icon()
{
	mailbutton = GTK_WIDGET(gtk_tool_button_new_from_stock(GEANYSENDMAIL_STOCK_MAIL));
	plugin_add_toolbar_item(geany_plugin, GTK_TOOL_ITEM(mailbutton));
	ui_add_document_sensitive(mailbutton);
#if GTK_CHECK_VERSION(2, 12, 0)
	gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(mailbutton), _("Send by mail"));
#endif
	g_signal_connect (G_OBJECT(mailbutton), "clicked", G_CALLBACK(send_as_attachment), NULL);
	gtk_widget_show_all (mailbutton);
}

static void cleanup_icon()
{
	if (mailbutton != NULL)
	{
		gtk_container_remove(GTK_CONTAINER (geany->main_widgets->toolbar), mailbutton);
	}
}


static struct
{
	GtkWidget *entry;
	GtkWidget *checkbox_icon_to_toolbar;
	GtkWidget *checkbox_use_addressdialog;
}
pref_widgets;

static void
on_configure_response(G_GNUC_UNUSED GtkDialog *dialog, gint response, G_GNUC_UNUSED  gpointer user_data)
{
	if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY)
	{
		GKeyFile 	*config = g_key_file_new();
		gchar 		*config_dir = g_path_get_dirname(config_file);

		g_free(mailer);
		mailer = g_strdup(gtk_entry_get_text(GTK_ENTRY(pref_widgets.entry)));

		if (icon_in_toolbar == FALSE &&
		    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pref_widgets.checkbox_icon_to_toolbar)) == TRUE)
		{
			icon_in_toolbar = TRUE;
			show_icon();
		}
		else if (icon_in_toolbar == TRUE &&
		    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pref_widgets.checkbox_icon_to_toolbar)) == FALSE)
		{
			cleanup_icon();
			icon_in_toolbar = FALSE;
		}
		else
		{
			icon_in_toolbar = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pref_widgets.checkbox_icon_to_toolbar));
		}

		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pref_widgets.checkbox_use_addressdialog)) == TRUE)
			use_address_dialog = TRUE;
		else
			use_address_dialog = FALSE;

		g_key_file_load_from_file(config, config_file, G_KEY_FILE_NONE, NULL);
		g_key_file_set_string(config, "tools", "mailer", mailer);
		g_key_file_set_boolean(config, "tools", "address_usage", use_address_dialog);
		g_key_file_set_boolean(config, "icon", "show_icon", icon_in_toolbar);

		if (! g_file_test(config_dir, G_FILE_TEST_IS_DIR) && utils_mkdir(config_dir, TRUE) != 0)
		{
			dialogs_show_msgbox(GTK_MESSAGE_ERROR,
				_("Plugin configuration directory could not be created."));
		}
		else
		{
			/* write config to file */
			gchar *data = g_key_file_to_data(config, NULL, NULL);
			utils_write_file(config_file, data);
			g_free(data);
		}
		g_key_file_free(config);
		g_free(config_dir);
	}
}

GtkWidget *plugin_configure(GtkDialog *dialog)
{
	GtkWidget	*label1, *label2, *vbox;

	vbox = gtk_vbox_new(FALSE, 6);

	/* add a label and a text entry to the dialog */
	label1 = gtk_label_new(_("Path and options for the mail client:"));
	gtk_widget_show(label1);
	gtk_misc_set_alignment(GTK_MISC(label1), 0, 0.5);
	pref_widgets.entry = gtk_entry_new();
	gtk_widget_show(pref_widgets.entry);
	if (mailer != NULL)
		gtk_entry_set_text(GTK_ENTRY(pref_widgets.entry), mailer);

	label2 = gtk_label_new(_("Note: \n\t\%f will be replaced by your file."\
		"\n\t\%r will be replaced by recipient's email address."\
		"\n\t\%b will be replaced by basename of a file"\
		"\n\tExamples:"\
		"\n\tsylpheed --attach \"\%f\" --compose \"\%r\""\
		"\n\tmutt -s \"Sending \'\%b\'\" -a \"\%f\" \"\%r\""));
	gtk_label_set_selectable(GTK_LABEL(label2), TRUE);
	gtk_widget_show(label2);
	gtk_misc_set_alignment(GTK_MISC(label2), 0, 0.5);

	pref_widgets.checkbox_icon_to_toolbar = gtk_check_button_new_with_label(_("Show toolbar icon"));
	ui_widget_set_tooltip_text(pref_widgets.checkbox_icon_to_toolbar,
		_("Shows a icon in the toolbar to send file more easy."));
	gtk_button_set_focus_on_click(GTK_BUTTON(pref_widgets.checkbox_icon_to_toolbar), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pref_widgets.checkbox_icon_to_toolbar), icon_in_toolbar);
	gtk_widget_show(pref_widgets.checkbox_icon_to_toolbar);

	pref_widgets.checkbox_use_addressdialog = gtk_check_button_new_with_label(_
		("Use dialog for entering email address of recipients"));

	gtk_button_set_focus_on_click(GTK_BUTTON(pref_widgets.checkbox_use_addressdialog), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pref_widgets.checkbox_use_addressdialog), use_address_dialog);
	gtk_widget_show(pref_widgets.checkbox_use_addressdialog);

	gtk_box_pack_start(GTK_BOX(vbox), label1, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), pref_widgets.entry, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), label2, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), pref_widgets.checkbox_icon_to_toolbar, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), pref_widgets.checkbox_use_addressdialog, FALSE, FALSE, 0);

	gtk_widget_show(vbox);

	g_signal_connect(dialog, "response", G_CALLBACK(on_configure_response), NULL);
	return vbox;
}

/* Called by Geany to initialize the plugin */
void plugin_init(GeanyData G_GNUC_UNUSED *data)
{
	GtkTooltips *tooltips = NULL;
	GKeyFile *config = g_key_file_new();
	gchar *kb_label = _("Send file by mail");
	GtkWidget *menu_mail = NULL;
	GeanyKeyGroup *key_group;

	main_locale_init(LOCALEDIR, GETTEXT_PACKAGE);

	config_file = g_strconcat(geany->app->configdir, G_DIR_SEPARATOR_S, "plugins", G_DIR_SEPARATOR_S,
		"geanysendmail", G_DIR_SEPARATOR_S, "mail.conf", NULL);

	/* Initialising options from config file */
	g_key_file_load_from_file(config, config_file, G_KEY_FILE_NONE, NULL);
	mailer = g_key_file_get_string(config, "tools", "mailer", NULL);
	address = g_key_file_get_string(config, "tools", "address", NULL);
	use_address_dialog = g_key_file_get_boolean(config, "tools", "address_usage", NULL);
	icon_in_toolbar = g_key_file_get_boolean(config, "icon", "show_icon", NULL);

	g_key_file_free(config);

	tooltips = gtk_tooltips_new();

	add_stock_item();
	if (icon_in_toolbar == TRUE)
	{
		show_icon();
	}

	/* Build up menu entry */
	menu_mail = gtk_menu_item_new_with_mnemonic(_("_Mail document"));
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), menu_mail);
	ui_widget_set_tooltip_text(menu_mail,
		_("Sends the opened file as unzipped attachment by any mailer from your $PATH"));
	g_signal_connect(G_OBJECT(menu_mail), "activate", G_CALLBACK(send_as_attachment), NULL);

	/* setup keybindings */
	key_group = plugin_set_key_group(geany_plugin, "sendmail", COUNT_KB, NULL);
	keybindings_set_item(key_group, SENDMAIL_KB, key_send_as_attachment,
		0, 0, "send_file_as_attachment", kb_label, menu_mail);

	gtk_widget_show_all(menu_mail);
	ui_add_document_sensitive(menu_mail);
	main_menu_item = menu_mail;
}


void plugin_cleanup()
{
	gtk_widget_destroy(main_menu_item);
	cleanup_icon();
	g_free(mailer);
	g_free(address);
	g_free(config_file);
}
