/*
 *	  geanylatex.c - Plugin to let Geany better work together with LaTeX
 *
 *	  Copyright 2007-2011 Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
 *	  Copyright 2005-2009 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *	  Copyright 2006-2009 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
 *
 *	  For long list of friendly supporters please have a look at THANKS.
 *
 *	  This program is free software; you can redistribute it and/or modify
 *	  it under the terms of the GNU General Public License as published by
 *	  the Free Software Foundation; either version 2 of the License, or
 *	  (at your option) any later version.
 *
 *	  This program is distributed in the hope that it will be useful,
 *	  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	  GNU General Public License for more details.
 *
 *	  You should have received a copy of the GNU General Public License
 *	  along with this program; if not, write to the Free Software
 *	  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/* LaTeX plugin */
/* This plugin improves the work with LaTeX and Geany.*/


#ifdef HAVE_CONFIG_H
	#include "config.h" /* for the gettext domain */
#endif

#include "geanylatex.h"
#include "ctype.h"

PLUGIN_VERSION_CHECK(199)

PLUGIN_SET_TRANSLATABLE_INFO(
	LOCALEDIR,
	GETTEXT_PACKAGE,
	_("GeanyLaTeX"),
	_("Plugin to provide better LaTeX support"),
	"0.7git",
	"Frank Lanitz <frank@frank.uvena.de>")

GeanyPlugin	 *geany_plugin;
GeanyData	   *geany_data;
GeanyFunctions  *geany_functions;

/* Widgets for plugin */
static GtkWidget *menu_latex = NULL;
static GtkWidget *menu_bibtex = NULL;
static GtkWidget *menu_bibtex_menu = NULL;
static GtkWidget *menu_latex_menu = NULL;
static GtkWidget *menu_latex_wizard = NULL;
static GtkWidget *menu_latex_menu_special_char = NULL;
static GtkWidget *menu_latex_menu_special_char_submenu = NULL;
static GtkWidget *menu_latex_ref = NULL;
static GtkWidget *menu_latex_label = NULL;
static GtkWidget *menu_latex_bibtex = NULL;
static GtkWidget *menu_latex_bibtex_submenu = NULL;
static GtkWidget *menu_latex_insert_bibtex_cite = NULL;
static GtkWidget *menu_latex_format_insert = NULL;
static GtkWidget *menu_latex_format_insert_submenu = NULL;
static GtkWidget *menu_latex_fontsize = NULL;
static GtkWidget *menu_latex_fontsize_submenu = NULL;
static GtkWidget *menu_latex_insert_environment = NULL;
static GtkWidget *menu_latex_insert_usepackage = NULL;
static GtkWidget *menu_latex_insert_command = NULL;
static GtkWidget *menu_latex_replacement = NULL;
static GtkWidget *menu_latex_replacement_submenu = NULL;
static GtkWidget *menu_latex_replace_selection = NULL;
static GtkWidget *menu_latex_replace_toggle = NULL;
static GtkWidget *menu_latex_toolbar_wizard = NULL;

/* Options for plugin */
static gboolean glatex_set_koma_active = FALSE;
static gboolean glatex_deactivate_toolbaritems_with_non_latex = TRUE;
static gboolean glatex_deactivate_menubarentry_with_non_latex = TRUE;
static gboolean glatex_add_menu_on_startup;
static gchar *glatex_ref_chapter_string = NULL;
static gchar *glatex_ref_page_string = NULL;
static gchar *glatex_ref_all_string = NULL;
static gboolean glatex_set_toolbar_active = FALSE;
static gboolean glatex_capitalize_sentence_starts = FALSE;
static gboolean glatex_wizard_to_generic_toolbar = FALSE;

/* We want to keep this deactivated by default as the
 * user needs to know what he is doing here.... */
static gboolean glatex_autocompletion_active = FALSE;
/* Value how many line should be search for autocompletion of \end{}
 * and \endgroup{}. */
static gint glatex_autocompletion_context_size;
static gboolean glatex_autocompletion_only_for_latex;
gboolean glatex_autobraces_active = TRUE;

/* Function will be deactivated, when only loaded */
static gboolean toggle_active = FALSE;

static GtkUIManager *uim;
static GtkActionGroup *group;
static GtkWidget *glatex_toolbar = NULL;
static GtkWidget *box = NULL;

static GtkToolItem *glatex_wizard_generic_toolbar_item = NULL;

/* Configuration file */
static gchar *config_file = NULL;

LaTeXWizard glatex_wizard;

const GtkActionEntry format_icons[] =
{
	{ "Wizard", GTK_STOCK_NEW, NULL, NULL, N_("Runs LaTeX wizard"), G_CALLBACK(glatex_kbwizard) },
	{ "Italic", GTK_STOCK_ITALIC, NULL, NULL, N_("Marks selected text as italic"), G_CALLBACK(glatex_kb_format_italic) },
	{ "Bold", GTK_STOCK_BOLD, NULL, NULL, N_("Marks selected text as bold"), G_CALLBACK(glatex_kb_format_bold) },
	{ "Underline", GTK_STOCK_UNDERLINE, NULL, NULL, N_("Underlines selected text"), G_CALLBACK(glatex_kb_format_typewriter) },
	{ "Centered", GTK_STOCK_JUSTIFY_CENTER, NULL, NULL, N_("Centered"), G_CALLBACK(glatex_kb_format_centering) },
	{ "Left", GTK_STOCK_JUSTIFY_LEFT, NULL, NULL, N_("Left side oriented"), G_CALLBACK(glatex_kb_format_left) },
	{ "Right", GTK_STOCK_JUSTIFY_RIGHT, NULL, NULL, N_("Right side oriented"), G_CALLBACK(glatex_kb_format_right) },
};

const guint ui_entries_n = G_N_ELEMENTS(format_icons);


/* fallback UI definition */
const gchar *toolbar_markup =
"<ui>"
	"<toolbar name='glatex_format_toolbar'>"
		"<toolitem action='Wizard'/>"
		"<separator/>"
		"<toolitem action='Italic'/>"
		"<toolitem action='Bold'/>"
		"<toolitem action='Underline'/>"
		"<separator/>"
		"<toolitem action='Centered' />"
		"<toolitem action='Left' />"
		"<toolitem action='Right'/>"
	"</toolbar>"
"</ui>";

static struct
{
	GtkWidget *koma_active;
	GtkWidget *toolbar_active;
	GtkWidget *glatex_autocompletion_active;
	GtkWidget *glatex_capitalize_sentence;
	GtkWidget *wizard_to_generic_toolbar;
}
config_widgets;

/* Some functions*/
static void toggle_toolbar_items_by_file_type(gint id);
static void add_menu_to_menubar(void);
static void remove_menu_from_menubar(void);
static void add_wizard_to_generic_toolbar(void);
static void remove_wizard_from_generic_toolbar(void);


static GtkWidget *init_toolbar()
{
	GtkWidget *toolbar = NULL;

	box = ui_lookup_widget(geany->main_widgets->window, "vbox1");
	uim = gtk_ui_manager_new();
	group = gtk_action_group_new("glatex_format_toolbar");
	gtk_action_group_set_translation_domain(group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions(group, format_icons, ui_entries_n, NULL);
	gtk_ui_manager_insert_action_group(uim, group, 0);
	if (gtk_ui_manager_add_ui_from_string(uim, toolbar_markup, -1, NULL) > 0)
	{
		toolbar = gtk_ui_manager_get_widget(uim, "/ui/glatex_format_toolbar");
		gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(toolbar), FALSE, TRUE, 0);
		gtk_box_reorder_child(GTK_BOX(box), toolbar, 2);
	}
	/* TODO maybe more error handling */

	return toolbar;
}


static void
on_configure_response(G_GNUC_UNUSED GtkDialog *dialog, gint response,
					  G_GNUC_UNUSED gpointer user_data)
{
	if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY)
	{
		GKeyFile *config = g_key_file_new();
		gchar *data;
		gchar *config_dir = g_path_get_dirname(config_file);
		gint glatex_autocompletion_active_response;

		config_file = g_strconcat(geany->app->configdir,
			G_DIR_SEPARATOR_S, "plugins", G_DIR_SEPARATOR_S,
			"geanyLaTeX", G_DIR_SEPARATOR_S, "general.conf", NULL);
		glatex_set_koma_active =
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(config_widgets.koma_active));
		glatex_set_toolbar_active =
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(config_widgets.toolbar_active));
		glatex_capitalize_sentence_starts =
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(config_widgets.glatex_capitalize_sentence));
		glatex_wizard_to_generic_toolbar =
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(config_widgets.wizard_to_generic_toolbar));

		/* Check the response code for geanyLaTeX's autocompletion functions.
		 * Due compatibility with oder Geany versions cass 0 will be treated
		 * as FALSE, which means autocompletion is deactivated. */
		glatex_autocompletion_active_response = gtk_combo_box_get_active(
			GTK_COMBO_BOX(config_widgets.glatex_autocompletion_active));
		if (glatex_autocompletion_active_response == 0)
			glatex_autocompletion_active = FALSE;
		else
			glatex_autocompletion_active = TRUE;

		/* write stuff to file */
		g_key_file_load_from_file(config, config_file, G_KEY_FILE_NONE, NULL);

		g_key_file_set_boolean(config, "general", "glatex_set_koma_active",
			glatex_set_koma_active);
		g_key_file_set_boolean(config, "general", "glatex_set_toolbar_active",
			glatex_set_toolbar_active);
		g_key_file_set_boolean(config, "general", "glatex_set_autocompletion",
			glatex_autocompletion_active);
		g_key_file_set_boolean(config, "autocompletion",
			"glatex_capitalize_sentence_starts", glatex_capitalize_sentence_starts);
		g_key_file_set_boolean(config, "toolbar", "glatex_wizard_to_generic_toolbar",
			glatex_wizard_to_generic_toolbar);


		if (!g_file_test(config_dir, G_FILE_TEST_IS_DIR)
			&& utils_mkdir(config_dir, TRUE) != 0)
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
		}

		g_free(config_dir);
		g_key_file_free(config);

		/* Apply changes to Geany */
		/* Add toolbar if requested */
		if (glatex_set_toolbar_active == TRUE)
		{
			if (glatex_toolbar == NULL)
			{
				glatex_toolbar = init_toolbar();
			}
			else
			{
				gtk_widget_show(glatex_toolbar);
			}
		}
		/* Hide toolbar */
		else if (glatex_set_toolbar_active == FALSE && glatex_toolbar != NULL)
		{
			gtk_widget_hide(glatex_toolbar);
		}

		/* Add wizard to generic toolbar if requested */
		if (glatex_wizard_to_generic_toolbar == TRUE &&
			glatex_wizard_generic_toolbar_item == NULL)
		{
			add_wizard_to_generic_toolbar();
		}
		else if (glatex_wizard_to_generic_toolbar == FALSE &&
				 glatex_wizard_generic_toolbar_item != NULL)
		{
			remove_wizard_from_generic_toolbar();
		}
	}
}

GtkWidget *
plugin_configure(GtkDialog * dialog)
{
	GtkWidget   *vbox;
	GtkWidget   *hbox_autocompletion;
	GtkWidget   *label_autocompletion = NULL;
	gint		tmp;

	vbox = gtk_vbox_new(FALSE, 6);

	config_widgets.koma_active = gtk_check_button_new_with_label(
		_("Use KOMA script by default"));
	config_widgets.toolbar_active = gtk_check_button_new_with_label(
		_("Show extra plugin toolbar"));
	config_widgets.glatex_capitalize_sentence = gtk_check_button_new_with_label(
		_("Capitalize sentence on typing"));
	config_widgets.wizard_to_generic_toolbar = gtk_check_button_new_with_label(
		_("Add a wizard icon to Geany's main toolbar"));

	config_widgets.glatex_autocompletion_active = gtk_combo_box_new_text();
	gtk_combo_box_insert_text(GTK_COMBO_BOX(config_widgets.glatex_autocompletion_active), 0,
		_("Don't care about this inside plugin"));
	gtk_combo_box_insert_text(GTK_COMBO_BOX(config_widgets.glatex_autocompletion_active), 1,
		_("Always perform autocompletion on LaTeX"));

	/* Configuration for auto completion feature */
	hbox_autocompletion = gtk_hbox_new(FALSE, 3);

	/* Dirty workarround for transferring boolean into a valid interger value */
	if (glatex_autocompletion_active == TRUE)
		tmp = 1;
	else
		tmp = 0;
	gtk_combo_box_set_active(GTK_COMBO_BOX(config_widgets.glatex_autocompletion_active), tmp);

	label_autocompletion = gtk_label_new(_("Modus of autocompletion"));

	gtk_box_pack_start(GTK_BOX(hbox_autocompletion), label_autocompletion, FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(hbox_autocompletion), config_widgets.glatex_autocompletion_active, TRUE, TRUE, 3);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_widgets.koma_active),
		glatex_set_koma_active);
	gtk_box_pack_start(GTK_BOX(vbox), config_widgets.koma_active, FALSE, FALSE, 2);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_widgets.toolbar_active),
		glatex_set_toolbar_active);
	gtk_box_pack_start(GTK_BOX(vbox), config_widgets.toolbar_active, FALSE, FALSE, 2);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_widgets.glatex_capitalize_sentence),
		glatex_capitalize_sentence_starts);
	gtk_box_pack_start(GTK_BOX(vbox), config_widgets.glatex_capitalize_sentence, FALSE, FALSE, 2);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_widgets.wizard_to_generic_toolbar),
		glatex_wizard_to_generic_toolbar);
	gtk_box_pack_start(GTK_BOX(vbox), config_widgets.wizard_to_generic_toolbar, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(vbox), hbox_autocompletion, FALSE, FALSE, 2);

	gtk_widget_show_all(vbox);
	g_signal_connect(dialog, "response", G_CALLBACK(on_configure_response), NULL);
	return vbox;
}

/* Functions to toggle the status of plugin */
static void glatex_set_latextoggle_status(gboolean new_status)
{
	/* No more function at the moment.*/
	if (toggle_active != new_status)
		toggle_active = new_status;
}

static void glatex_toggle_status(G_GNUC_UNUSED GtkMenuItem * menuitem)
{
	if (toggle_active == TRUE)
		glatex_set_latextoggle_status(FALSE);
	else
		glatex_set_latextoggle_status(TRUE);
}

static void toggle_toolbar_item(const gchar *path, gboolean new_status)
{
	if (gtk_action_get_sensitive(gtk_ui_manager_get_action(uim, path)) != new_status)
	{
		gtk_action_set_sensitive(gtk_ui_manager_get_action(uim, path), new_status);
	}
}

static void
check_for_menu(gint ft_id)
{
	/* We don't check whether the menu has been added here, as this
	 * will is checked by add_menu_to_menubar() anyway */
	if (ft_id == GEANY_FILETYPES_LATEX)
	{
		add_menu_to_menubar();
	}
	if (ft_id != GEANY_FILETYPES_LATEX)
	{
		if (glatex_deactivate_menubarentry_with_non_latex == TRUE)
		{
			remove_menu_from_menubar();
		}
	}
}


static void activate_toolbar_items()
{
	if (uim == NULL)
	{
		return;
	}

	toggle_toolbar_item("/ui/glatex_format_toolbar/Bold", TRUE);
	toggle_toolbar_item("/ui/glatex_format_toolbar/Underline", TRUE);
	toggle_toolbar_item("/ui/glatex_format_toolbar/Centered", TRUE);
	toggle_toolbar_item("/ui/glatex_format_toolbar/Italic", TRUE);
	toggle_toolbar_item("/ui/glatex_format_toolbar/Left", TRUE);
	toggle_toolbar_item("/ui/glatex_format_toolbar/Right", TRUE);
	gtk_ui_manager_ensure_update(uim);
}

static void deactivate_toolbar_items()
{
	if (uim == NULL)
	{
		return;
	}

	toggle_toolbar_item("/ui/glatex_format_toolbar/Bold", FALSE);
	toggle_toolbar_item("/ui/glatex_format_toolbar/Underline", FALSE);
	toggle_toolbar_item("/ui/glatex_format_toolbar/Centered", FALSE);
	toggle_toolbar_item("/ui/glatex_format_toolbar/Italic", FALSE);
	toggle_toolbar_item("/ui/glatex_format_toolbar/Left", FALSE);
	toggle_toolbar_item("/ui/glatex_format_toolbar/Right", FALSE);
	gtk_ui_manager_ensure_update(uim);
}


static void toggle_toolbar_items_by_file_type(gint id)
{
	if (glatex_set_toolbar_active == TRUE)
	{
		if (id == GEANY_FILETYPES_LATEX ||
			glatex_deactivate_toolbaritems_with_non_latex == FALSE)
		{
			activate_toolbar_items();
		}
		else
		{
			deactivate_toolbar_items();
		}
	}
}


static void on_document_activate(G_GNUC_UNUSED GObject *object,
								 GeanyDocument *doc, G_GNUC_UNUSED gpointer data)
{
	g_return_if_fail(doc != NULL);

	if (main_is_realized() == TRUE)
	{
		toggle_toolbar_items_by_file_type(doc->file_type->id);
		check_for_menu(doc->file_type->id);
	}
}


static void on_document_new(G_GNUC_UNUSED GObject *object, GeanyDocument *doc,
							G_GNUC_UNUSED gpointer data)
{
	g_return_if_fail(doc != NULL);

	if (main_is_realized() == TRUE)
	{
		toggle_toolbar_items_by_file_type(doc->file_type->id);
	}
}


static void
on_geany_startup_complete(G_GNUC_UNUSED GObject *obj,
						  G_GNUC_UNUSED gpointer user_data)
{
	GeanyDocument *doc = NULL;

	doc = document_get_current();
	if (doc != NULL)
	{
		toggle_toolbar_items_by_file_type(doc->file_type->id);
		if (glatex_add_menu_on_startup == TRUE||
			doc->file_type->id == GEANY_FILETYPES_LATEX)
		{
			add_menu_to_menubar();
		}
	}

}


static void on_document_filetype_set(G_GNUC_UNUSED GObject *obj, GeanyDocument *doc,
									 G_GNUC_UNUSED GeanyFiletype *filetype_old,
									 G_GNUC_UNUSED gpointer user_data)
{
	g_return_if_fail(doc != NULL);

	if (main_is_realized() == TRUE)
	{
		toggle_toolbar_items_by_file_type(doc->file_type->id);
		check_for_menu(doc->file_type->id);
	}
}


static gboolean on_editor_notify(G_GNUC_UNUSED GObject *object, GeanyEditor *editor,
									SCNotification *nt, G_GNUC_UNUSED gpointer data)
{
	ScintillaObject* sci;
	gint pos;

	g_return_val_if_fail(editor != NULL, FALSE);
	sci = editor->sci;
	/* Autocompletion for LaTeX specific stuff:
	 * Introducing \end{} or \endgroup{} after a \begin{}

	 * Function is based on function which was inside
	 * Geany's core under terms of GPLv2+
	 * EXtended for GeanyLaTeX with some more autocompletion features
	 * for e.g. _{} and ^{}.*/

	if (glatex_autocompletion_active == TRUE &&
		!(glatex_autocompletion_only_for_latex == TRUE &&
		editor->document->file_type->id != GEANY_FILETYPES_LATEX))
	{
		pos = sci_get_current_position(sci);

		if (nt->nmhdr.code == SCN_CHARADDED)
		{
			switch (nt->ch)
			{
				case '\n':
				case '\r':
				{
					if (sci_get_char_at(sci, pos - (editor_get_eol_char_len (editor) + 1)) == '}'||
						sci_get_char_at(sci, pos - (editor_get_eol_char_len (editor) + 1)) == ']')
					{
						gchar *buf, *construct;
						/* TODO: Make possible to have longer than a 50 chars environment */
						gchar env[50];
						gint line = sci_get_line_from_position(sci, pos - (editor_get_eol_char_len (editor) + 1));
						gint line_len = sci_get_line_length(sci, line);
						gint i, start;
						gint indent;

						/* get the line */
						buf = sci_get_line(sci, line);

						/* get to the first non-blank char (some kind of ltrim()) */
						start = 0;
						while (isspace(buf[start]) && buf[start] != '\0')
							start++;

						/* check for begin for autocompletion of \end{} and \endgr*/
						if (strncmp(buf + start, "\\begin", 6) == 0)
						{
							gchar full_cmd[15];
							guint j = 0;

							/* take also "\begingroup" (or whatever there can be) and
							 * append "\endgroup" and so on. */
							i = start + 6;
							while (i < line_len && buf[i] != '{' && j < (sizeof(full_cmd) - 1))
							{
								/* copy all between "\begin" and "{" to full_cmd */
								full_cmd[j] = buf[i];
								i++;
								j++;
							}
							full_cmd[j] = '\0';

							/* go through the line and get the environment */
							for (i = start + j; i < line_len; i++)
							{
								if (buf[i] == '{')
								{
									j = 0;
									i++;
									while (buf[i] != '}' && j < (sizeof(env) - 1))
									{   /* this could be done in a shorter way,
									 	 * but so it remains readable ;-) */
										env[j] = buf[i];
										j++;
										i++;
									}
									env[j] = '\0';
									break;
								}
							}
							/* Search whether the environment is closed within the next
							 * lines. We assume, no \end is needed in such cases */
							/* TODO using sci_find_text() should be way faster than getting
							 *	  the line buffer and performing string comparisons */
							for (i = 1; i < glatex_autocompletion_context_size; i++)
							{
								gchar *tmp;
								gchar *end_construct;
								tmp = sci_get_line(sci, line + i);

								end_construct = g_strdup_printf("\\end%s{%s}", full_cmd, env);
								if (strstr(tmp, end_construct) != NULL)
								{
									/* Clean up everything and quit as nothing
									 * needs to be done */
									g_free(tmp);
									g_free(buf);
									g_free(end_construct);
									return FALSE;
								}
								g_free(tmp);
								g_free(end_construct);
							}

							/* After we have this, we need to ensure basic
							 * indent is getting applied on closing command */
							indent = sci_get_line_indentation(sci, line);

							/* Now we build up closing string and insert
							 * it into document */
							construct = g_strdup_printf("\t\n\\end%s{%s}", full_cmd, env);
							editor_insert_text_block(editor, construct, pos,
								1, -1, TRUE);
							/* ... and setting the indention */
							sci_set_line_indentation(sci, sci_get_current_line(sci) + 1,
								indent);
							g_free(construct);
						}
						g_free(buf);
					}

					/* Now we are handling the case, a new line has been inserted
					* but no closing braces */
					else if (glatex_autobraces_active == TRUE)
					{
						gint line;

						gint line_len;
						gint i;
						gchar *buf;

						line = sci_get_line_from_position(sci, pos -
									(editor_get_eol_char_len (editor) + 1));
						line_len = sci_get_line_length(sci, line) -
									editor_get_eol_char_len (editor);


						/* Catching current line which has been 'finished'*/
						buf = sci_get_line(sci, line);

						/* Searching for either \ or " ", {, } from end of
						 * line back to start */
						for (i = line_len; i >= 0 ; i--)
						{
							if (buf[i] == '\\')
							{
								if ((i > 0 && buf[i-1] != '\\') ||
									(i == 0))
								{
									sci_insert_text(sci,
										pos - (editor_get_eol_char_len (editor)), "{}");
								}
								/* We will stop here in any case */
								break;
							}
							/* Else we want to stop once we found a space,
							 * some closing braces somewhere before as we
							 * are assuming, manipulating something here
							 * would cause a bigger mass. */
							else if (buf[i] == ' ' ||
									 buf[i] == '}' ||
									 buf[i] == '{' ||
									 buf[i] == '"')
							{
								break;
							}
						}
						g_free(buf);
					}
					break;
				} /* Closing case \r or \n */
				case '_':
				case '^':
				{
					if (glatex_autobraces_active == TRUE)
					{
						sci_insert_text(sci, -1, "{}");
						sci_set_current_position(sci, pos + 1, TRUE);
					}
					break;
				}
				default:
				{
					if (glatex_capitalize_sentence_starts == TRUE)
					{
						if (sci_get_char_at(sci, pos -2) == ' ' &&
							(sci_get_char_at(sci, pos -3) == '.' ||
							 sci_get_char_at(sci, pos -3) == '!' ||
							 sci_get_char_at(sci, pos -3) == '?' ))
						{
							gchar *upperLtr = NULL;
							gchar *selection = NULL;

							sci_set_selection_start(sci, pos - 1);
							sci_set_selection_end(sci, pos);

							selection = sci_get_selection_contents(sci);
							upperLtr = g_utf8_strup(selection, -1);
							sci_replace_sel(sci, upperLtr);

							g_free(upperLtr);
							g_free(selection);
						}
						break;
					}
				}
			} /* Closing switch  */
			/* later there could be some else ifs for other keywords */
		}
	} /* End of latex autocpletion */

	/* Toggle special characters on input */
	if (editor->document->file_type->id == GEANY_FILETYPES_LATEX &&
		toggle_active == TRUE)
	{
		if (nt->nmhdr.code == SCN_CHARADDED)
		{
			gchar buf[7];
			gint len;
			sci = editor->sci;

			len = g_unichar_to_utf8(nt->ch, buf);
			if (len > 0)
			{
				const gchar *entity;

				buf[len] = '\0';
				entity = glatex_get_entity(buf);

				if (entity != NULL)
				{
					pos = sci_get_current_position(sci);

					sci_set_selection_start(editor->sci, pos - len);
					sci_set_selection_end(editor->sci, pos);

					sci_replace_sel(editor->sci, entity);
				}
			}
		}
	}
	return FALSE;
}


static void on_document_close(G_GNUC_UNUSED GObject *obj, GeanyDocument *doc,
					   G_GNUC_UNUSED gpointer user_data)
{
	g_return_if_fail(doc != NULL);

	if (doc->index < 2)
		deactivate_toolbar_items();
	if (doc->index < 1 &&
		glatex_deactivate_menubarentry_with_non_latex == TRUE)
		remove_menu_from_menubar();
}


/* Called when keys were pressed */
static void glatex_kblatex_toggle(G_GNUC_UNUSED guint key_id)
{
	if (toggle_active == TRUE)
		glatex_set_latextoggle_status(FALSE);
	else
		glatex_set_latextoggle_status(TRUE);
}


PluginCallback plugin_callbacks[] =
{
	{ "editor-notify", (GCallback) &on_editor_notify, FALSE, NULL },
	{ "document-activate", (GCallback) &on_document_activate, FALSE, NULL },
	{ "document-filetype-set", (GCallback) &on_document_filetype_set, FALSE, NULL },
	{ "document-new", (GCallback) &on_document_new, FALSE, NULL},
	{ "geany-startup-complete", (GCallback) &on_geany_startup_complete, FALSE, NULL },
	{ "document-close", (GCallback) &on_document_close, FALSE, NULL},
	{ NULL, NULL, FALSE, NULL }
};


static inline const gchar*
get_latex_command(gint tab_index)
{
	return glatex_char_array[tab_index].latex;
}


static void
char_insert_activated(G_GNUC_UNUSED GtkMenuItem * menuitem,
					  G_GNUC_UNUSED gpointer gdata)
{
	glatex_insert_string(get_latex_command(GPOINTER_TO_INT(gdata)), TRUE);
}


void
glatex_insert_label_activated(G_GNUC_UNUSED GtkMenuItem * menuitem,
					   G_GNUC_UNUSED gpointer gdata)
{
	gchar *input = NULL;

	input = dialogs_show_input(_("Insert Label"),
								GTK_WINDOW(geany->main_widgets->window),
								_("Label name:"),
								NULL);

	if (input)
	{
		gchar *label_str = NULL;

		label_str = g_strconcat("\\label{",input , "}", NULL);
		glatex_insert_string(label_str, TRUE);
		g_free(input);
		g_free(label_str);
	}
}


void
glatex_insert_command_activated(G_GNUC_UNUSED GtkMenuItem * menuitem,
					 G_GNUC_UNUSED gpointer gdata)
{

	gchar *input = NULL;

	input = dialogs_show_input(_("Insert Command"),
								GTK_WINDOW(geany->main_widgets->window),
								_("Command name:"),
								NULL);

	if (input)
	{
		gchar *cmd_str = NULL;

		GeanyDocument *doc = document_get_current();

		sci_start_undo_action(doc->editor->sci);

		cmd_str = g_strdup_printf("\\%s{", input);
		glatex_insert_string(cmd_str, TRUE);
		/* This shall put the cursor inside the braces */
		glatex_insert_string("}", FALSE);

		sci_end_undo_action(doc->editor->sci);
		g_free(input);
		g_free(cmd_str);
	}
}


void
glatex_insert_ref_activated(G_GNUC_UNUSED GtkMenuItem * menuitem,
					 G_GNUC_UNUSED gpointer gdata)
{
	GtkWidget *dialog;
	GtkWidget *vbox = NULL;
	GtkWidget *label_ref = NULL;
	GtkWidget *textbox_ref = NULL;
	GtkWidget *table = NULL;
	GtkWidget *radio1 = NULL;
	GtkWidget *radio2 = NULL;
	GtkWidget *radio3 = NULL;
	GtkWidget *tmp_entry = NULL;
	GtkTreeModel *model = NULL;
	GeanyDocument *doc = NULL;
	GSList *file_list = NULL;
	gchar *dir;

	doc = document_get_current();

	dialog = gtk_dialog_new_with_buttons(_("Insert Reference"),
						 GTK_WINDOW(geany->main_widgets->window),
						 GTK_DIALOG_DESTROY_WITH_PARENT, GTK_STOCK_CANCEL,
						 GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
						 NULL);
	vbox = ui_dialog_vbox_new(GTK_DIALOG(dialog));
	gtk_widget_set_name(dialog, "GeanyDialog");
	gtk_box_set_spacing(GTK_BOX(vbox), 10);

	table = gtk_table_new(1, 2, FALSE);
	gtk_table_set_col_spacings(GTK_TABLE(table), 6);
	gtk_table_set_row_spacings(GTK_TABLE(table), 6);

	label_ref = gtk_label_new(_("Reference name:"));
	textbox_ref = gtk_combo_box_entry_new_text();

	if (doc->real_path != NULL)
	{
		dir = g_path_get_dirname(doc->real_path);
		file_list = utils_get_file_list_full(dir, TRUE, TRUE, NULL);
		glatex_add_Labels(textbox_ref, file_list);
		model = gtk_combo_box_get_model(GTK_COMBO_BOX(textbox_ref));
		gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model),
			0, GTK_SORT_ASCENDING);
		g_slist_foreach(file_list, (GFunc) g_free, NULL);
		g_slist_free(file_list);
		if (dir != NULL)
			g_free(dir);
	}


	gtk_misc_set_alignment(GTK_MISC(label_ref), 0, 0.5);

	gtk_table_attach_defaults(GTK_TABLE(table), label_ref, 0, 1, 0, 1);
	gtk_table_attach_defaults(GTK_TABLE(table), textbox_ref, 1, 2, 0, 1);
	gtk_container_add(GTK_CONTAINER(vbox), table);

	radio1 = gtk_radio_button_new_with_mnemonic(NULL,
		_("_Standard Reference"));
	gtk_button_set_focus_on_click(GTK_BUTTON(radio1), FALSE);
	gtk_container_add(GTK_CONTAINER(vbox), radio1);

	radio2 = gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(radio1),
		_("_Page Reference"));
	gtk_button_set_focus_on_click(GTK_BUTTON(radio2), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio2), FALSE);
	gtk_container_add(GTK_CONTAINER(vbox), radio2);

	radio3 = gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(radio1), _("_Add both"));
	gtk_button_set_focus_on_click(GTK_BUTTON(radio3), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio3), FALSE);
	gtk_container_add(GTK_CONTAINER(vbox), radio3);

	tmp_entry =  gtk_bin_get_child(GTK_BIN(textbox_ref));
	g_signal_connect(G_OBJECT(tmp_entry), "activate",
		G_CALLBACK(glatex_enter_key_pressed_in_entry), dialog);


	gtk_widget_show_all(vbox);


	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	{
		gchar *ref_string = NULL;
		GString *template_string = NULL;

		ref_string = g_strdup(gtk_combo_box_get_active_text(
			GTK_COMBO_BOX(textbox_ref)));

		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio1)) == TRUE)
		{
			template_string = g_string_new(glatex_ref_chapter_string);
		}
		else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio2))== TRUE)
		{
			template_string = g_string_new(glatex_ref_page_string);
		}
		else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio3))== TRUE)
		{
			template_string = g_string_new(glatex_ref_all_string);
		}

		if (ref_string != NULL && template_string != NULL)
		{
			gchar *tmp;
			utils_string_replace_all(template_string, "{{reference}}", ref_string);
			tmp = g_string_free(template_string, FALSE);
			glatex_insert_string(tmp, TRUE);
			g_free(ref_string);
			g_free(tmp);
		}
		else
		{
			if (ref_string != NULL)
				g_free(ref_string);
			if (template_string != NULL)
				g_free(template_string);
		}
	}

	gtk_widget_destroy(dialog);
}


static void glatex_character_create_menu_item(GtkWidget *menu, const gchar *label,
									   gint letter, MenuCallback callback)
{
	GtkWidget *tmp;

	tmp = gtk_menu_item_new_with_label(label);
	gtk_widget_show(tmp);
	gtk_container_add(GTK_CONTAINER(menu), tmp);
	g_signal_connect(tmp, "activate",
		G_CALLBACK(callback), GINT_TO_POINTER(letter));
}


/* returns -1, if there are more than gint can work with or any other error
 * returns 0, if categorie is empty
 * if gint categorie is -1, function will count every element.
 * Useful, if there is no need for a categorie check.*/
gint
glatex_count_menu_entries(SubMenuTemplate *tmp, gint categorie)
{
	/* TODO: Reset max value to stop before it's too late */
	gint i;
	gint count = 0;

	if (categorie == -1)
	{
		for (i =1; tmp[i].label != NULL; i++)
		{
			count = count + 1;
		}
	}
	else
	{
		for (i = 1; tmp[i].label != NULL; i++)
		{
			if (tmp[i].cat == categorie)
			{
				count = count + 1;
			}
			if (i >= 256)
			{
				count = -1;
				break;
			}
		}
	}
	return count + 1;
}

static gint
count_menu_cat_entries(CategoryName *tmp)
{
	gint i;

	for (i = 0; tmp[i].label != NULL; i++);
	return i;
}


static void glatex_sub_menu_init(GtkWidget *base_menu, SubMenuTemplate *menu_template,
				   CategoryName *category_name,
				   MenuCallback callback_function)
{
	gint i;
	gint j;
	gint categories = count_menu_cat_entries(category_name);
	GtkWidget *sub_menu = NULL;
	GtkWidget *sub_menu_cat[categories][2];
	GtkWidget *active_submenu = NULL;

	/* Creates sub menus based on information from letter.h */
	for (i = 0; i < categories; i++)
	{
		if (glatex_count_menu_entries(menu_template, i) > 0)
		{
			create_sub_menu(base_menu, sub_menu_cat[i][0],
			 sub_menu_cat[i][1], category_name[i].label);
		}
	}

	/* Searching for all categories */
	for (i = 0; i < categories; i++)
	{
		gboolean split = FALSE;
		gboolean last_sub_menu = FALSE;
		gboolean sorted = category_name[i].sorted;
		/* To check whether we need to build up a new sub sub menu. */
		gint local_count = 0;
		gint item_count = glatex_count_menu_entries(menu_template, i);

		if (item_count < 1)
			continue;

		/*  Default is, not to split anything to make menu not */
		/*  deeper than realy needed.  */
		if (item_count > MAX_MENU_ENTRIES)
		{
			split = TRUE;
		}

		/*  Setting active sub menu to sub menu of category */
		sub_menu = sub_menu_cat[i][0];
		active_submenu = sub_menu;
		/*  Finding entries for each category */

		for (j = 0; menu_template[j].latex != NULL; j++)
		{
			if (menu_template[j].cat == i)
			{
				/*  Creates a new sub sub menu if needed */
				if (split == TRUE && (local_count % MAX_MENU_ENTRIES) == 0)
				{
					gint next_split_point = 0;
					GtkWidget *tmp = NULL;
					GtkWidget *tmp_item = NULL;

					sub_menu = active_submenu;

					for (next_split_point = 0;
						next_split_point < MAX_MENU_ENTRIES ; next_split_point ++)
					{
						if (menu_template[j+next_split_point].cat != i)
						{
							last_sub_menu = TRUE;
							break;
						}

					}

					if (sorted == TRUE)
					{
						create_sub_menu(sub_menu_cat[i][0], tmp, tmp_item,
						g_strconcat(menu_template[j].label, " ... ",
						menu_template[j + next_split_point-1].label, NULL));

						sub_menu = tmp;
					}
					else if (sorted == FALSE)
					{
						if (last_sub_menu == FALSE)
						{
							create_sub_menu(sub_menu, tmp, tmp_item, _("More"));
							sub_menu = active_submenu;
							active_submenu = tmp;
						}
					}
				}

				/*  Sets the counter to keep in track if a new ,,
				 *  submenu needs to be build up */
				local_count = local_count + 1;
				glatex_character_create_menu_item(sub_menu, g_strconcat(
					menu_template[j].label, "\t", menu_template[j].latex,
					NULL), j, callback_function);
			}
		}
	}
}

static int
find_latex_enc(gint l_geany_enc)
{
	guint i;
	for (i = 0; i < LATEX_ENCODINGS_MAX; i++)
	{
		if (latex_encodings[i].geany_enc == l_geany_enc)
			return i;
	}
	return LATEX_ENCODING_NONE;
}


static void
show_output(const gchar * output, const gchar * name, const gint local_enc)
{
	GeanyDocument *doc = NULL;
	GeanyFiletype *ft = filetypes_lookup_by_name("LaTeX");

	if (output)
	{
		doc = document_new_file(name, ft, output);
		document_set_encoding(doc, encodings_get_charset_from_index
			(latex_encodings[local_enc].geany_enc));
	}
}

void glatex_insert_usepackage_dialog(G_GNUC_UNUSED GtkMenuItem * menuitem,
									 G_GNUC_UNUSED gpointer gdata)
{
	GtkWidget *dialog = NULL;
	GtkWidget *vbox = NULL;
	GtkWidget *label = NULL;
	GtkWidget *textbox = NULL;
	GtkWidget *table = NULL;
	GtkWidget *label_options = NULL;
	GtkWidget *textbox_options = NULL;

	dialog = gtk_dialog_new_with_buttons(_("Add additional package"),
						 GTK_WINDOW(geany->main_widgets->window),
						 GTK_DIALOG_DESTROY_WITH_PARENT, GTK_STOCK_CANCEL,
						 GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
						 NULL);
	vbox = ui_dialog_vbox_new(GTK_DIALOG(dialog));
	gtk_widget_set_name(dialog, "GeanyDialog");
	gtk_box_set_spacing(GTK_BOX(vbox), 10);

	table = gtk_table_new(1, 2, FALSE);
	gtk_table_set_col_spacings(GTK_TABLE(table), 6);
	gtk_table_set_row_spacings(GTK_TABLE(table), 6);

	label = gtk_label_new(_("Package name:"));
	textbox = gtk_entry_new();

	label_options = gtk_label_new(_("Package options:"));
	textbox_options = gtk_entry_new();

	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_misc_set_alignment(GTK_MISC(label_options), 0, 0.5);

	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 0, 1);
	gtk_table_attach_defaults(GTK_TABLE(table), textbox, 1, 2, 0, 1);
	gtk_table_attach_defaults(GTK_TABLE(table), label_options, 0, 1, 1, 2);
	gtk_table_attach_defaults(GTK_TABLE(table), textbox_options, 1, 2, 1, 2);

	gtk_container_add(GTK_CONTAINER(vbox), table);

	g_signal_connect(G_OBJECT(textbox), "activate",
		G_CALLBACK(glatex_enter_key_pressed_in_entry), dialog);
	g_signal_connect(G_OBJECT(textbox_options), "activate",
		G_CALLBACK(glatex_enter_key_pressed_in_entry), dialog);

	gtk_widget_show_all(vbox);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	{
		const gchar *pkg = NULL;
		const gchar *option = NULL;
		pkg = gtk_entry_get_text(GTK_ENTRY(textbox));
		option = gtk_entry_get_text(GTK_ENTRY(textbox_options));
		glatex_usepackage(pkg, option);
	}

	gtk_widget_destroy(dialog);

}

void
on_insert_bibtex_dialog_activate(G_GNUC_UNUSED GtkMenuItem *menuitem,
								 G_GNUC_UNUSED gpointer gdata)
{
	GtkWidget *dialog;
	GtkWidget *vbox = NULL;
	GtkWidget *label= NULL;
	GtkWidget *textbox = NULL;
	GtkWidget *table = NULL;
	GtkWidget *tmp_entry = NULL;
	GtkTreeModel *model = NULL;
	GeanyDocument *doc = NULL;

	doc = document_get_current();

	dialog = gtk_dialog_new_with_buttons(_("Insert BibTeX Reference"),
						 GTK_WINDOW(geany->main_widgets->window),
						 GTK_DIALOG_DESTROY_WITH_PARENT, GTK_STOCK_CANCEL,
						 GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
						 NULL);
	vbox = ui_dialog_vbox_new(GTK_DIALOG(dialog));
	gtk_widget_set_name(dialog, "GeanyDialog");
	gtk_box_set_spacing(GTK_BOX(vbox), 10);

	table = gtk_table_new(1, 2, FALSE);
	gtk_table_set_col_spacings(GTK_TABLE(table), 6);
	gtk_table_set_row_spacings(GTK_TABLE(table), 6);

	label = gtk_label_new(_("BibTeX reference name:"));
	textbox = gtk_combo_box_entry_new_text();

	if (doc->real_path != NULL)
	{
		GDir *dir;
		gchar *tmp_dir;
		const gchar *filename;

		tmp_dir = g_path_get_dirname(doc->real_path);
		dir = g_dir_open(tmp_dir, 0, NULL);

		g_return_if_fail(dir != NULL);

		foreach_dir(filename, dir)
		{
			gchar *fullpath = NULL;
			fullpath = g_build_path(G_DIR_SEPARATOR_S, tmp_dir, filename, NULL);

			glatex_parse_bib_file(fullpath, textbox);
			g_free(fullpath);
		}
		g_dir_close(dir);
		model = gtk_combo_box_get_model(GTK_COMBO_BOX(textbox));
		gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model),
			0, GTK_SORT_ASCENDING);
	}


	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 0, 1);
	gtk_table_attach_defaults(GTK_TABLE(table), textbox, 1, 2, 0, 1);
	gtk_container_add(GTK_CONTAINER(vbox), table);

	tmp_entry = gtk_bin_get_child(GTK_BIN(textbox));
	g_signal_connect(G_OBJECT(tmp_entry), "activate",
		G_CALLBACK(glatex_enter_key_pressed_in_entry), dialog);

	gtk_widget_show_all(vbox);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	{
		gchar *ref_string = NULL;
		GString *template_string = NULL;

		ref_string = g_strdup(gtk_combo_box_get_active_text(
			GTK_COMBO_BOX(textbox)));

		if (ref_string != NULL)
		{
			glatex_bibtex_insert_cite(ref_string, NULL);
			g_free(ref_string);
		}
		else
		{
			if (ref_string != NULL)
				g_free(ref_string);
			if (template_string != NULL)
				g_free(template_string);
		}
	}

	gtk_widget_destroy(dialog);
}


static void
on_wizard_response(G_GNUC_UNUSED GtkDialog *dialog, gint response,
				   G_GNUC_UNUSED gpointer user_data)
{
	GString *code = NULL;
	gchar *author = NULL;
	gchar *date = NULL;
	gchar *output = NULL;
	gchar *classoptions = NULL;
	gchar *title = NULL;
	gchar *enc_latex_char = NULL;
	gchar *documentclass_str = NULL;
	gchar *papersize = NULL;
	gchar *draft = NULL;
	gchar *fontsize = NULL;
	gchar *orientation_string = NULL;
	gboolean KOMA_active = FALSE;
	gboolean draft_active = FALSE;
	guint template_int;
	gint documentclass_int;
	gint encoding_int;
	gint papersize_int;
	gint paperorientation_int;

	if (response == GTK_RESPONSE_OK ||
		response == GTK_RESPONSE_APPLY ||
		response == GTK_RESPONSE_ACCEPT)
	{
		KOMA_active = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(glatex_wizard.checkbox_KOMA));
		draft_active = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(glatex_wizard.checkbox_draft));
		documentclass_int = gtk_combo_box_get_active(
			GTK_COMBO_BOX(glatex_wizard.documentclass_combobox));
		template_int = gtk_combo_box_get_active(
			GTK_COMBO_BOX(glatex_wizard.template_combobox));
		encoding_int = gtk_combo_box_get_active(
			GTK_COMBO_BOX(glatex_wizard.encoding_combobox));
		/* We don't want to set an encoding, if there is none choosen */
		if (encoding_int != LATEX_ENCODINGS_MAX)
		{
			enc_latex_char = g_strconcat("\\usepackage[",
				latex_encodings[encoding_int].latex,"]{inputenc}\n", NULL);
		}
		fontsize = gtk_combo_box_get_active_text(
			GTK_COMBO_BOX(glatex_wizard.fontsize_combobox));
		author = g_strdup(gtk_entry_get_text(GTK_ENTRY(glatex_wizard.author_textbox)));
		date = g_strdup(gtk_entry_get_text(GTK_ENTRY(glatex_wizard.date_textbox)));
		title = g_strdup(gtk_entry_get_text(GTK_ENTRY(glatex_wizard.title_textbox)));
		papersize_int = gtk_combo_box_get_active(
			GTK_COMBO_BOX(glatex_wizard.papersize_combobox));
		paperorientation_int = gtk_combo_box_get_active(
			GTK_COMBO_BOX(glatex_wizard.orientation_combobox));

		if (KOMA_active == TRUE)
		{
			switch (papersize_int)
			{
				case 0:
				{
					papersize = g_utf8_casefold("paper=a4", -1);
					break;
				}
				case 1:
				{
					papersize = g_utf8_casefold("paper=a5", -1);
					break;
				}
				case 2:
				{
					papersize = g_utf8_casefold("paper=a6", -1);
					break;
				}
			}

		}
		else
		{
			switch (papersize_int)
			{
				case 0:
				{
					papersize = g_utf8_casefold("a4paper", -1);
					break;
				}
				case 1:
				{
					papersize = g_utf8_casefold("a5paper", -1);
					break;
				}
				case 2:
				{
					papersize = g_utf8_casefold("a6paper", -1);
					break;
				}
			}
		}

		if (papersize != NULL)
		{
			classoptions = g_strdup(papersize);
			g_free(papersize);
		}

		switch (paperorientation_int)
		{
			case 2:
			{
				if (KOMA_active == TRUE)
				{
					orientation_string = g_utf8_casefold("paper=landscape", -1);
				}
				else
				{
					orientation_string = g_utf8_casefold("landscape", -1);
				}
				break;
			}
			/* 1 and default currently not handled differently, but
			 * already inside for future use. Dear future me, sorry */
			case 1:
			default:
			{
				orientation_string = g_utf8_casefold("", -1);
				break;
			}
		}
		if (classoptions != NULL && NZV(orientation_string))
		{
			classoptions = g_strconcat(classoptions, ",", orientation_string, NULL);
			g_free(orientation_string);
		}

		if (classoptions != NULL && draft_active == TRUE)
		{
			draft = g_utf8_casefold("draft", -1);
			classoptions = g_strconcat(classoptions,",", draft, NULL);
			g_free(draft);
		}
		else if (classoptions == NULL && draft_active == TRUE)
		{
			draft = g_utf8_casefold("draft", -1);
			classoptions = g_strconcat(draft, NULL);
			g_free(draft);
		}
		if (classoptions != NULL && NZV(fontsize))
		{
			classoptions = g_strconcat(classoptions, ",", fontsize, NULL);
		}
		else if (classoptions == NULL && NZV(fontsize))
		{
			classoptions = g_strdup(fontsize);
		}
		g_free(fontsize);


		switch (documentclass_int)
		{
			case 0:
			{
				documentclass_str = g_utf8_casefold("book", -1);
				break;
			}
			case 1:
			{
				documentclass_str = g_utf8_casefold("article", -1);
				break;
			}
			case 2:
			{
				documentclass_str = g_utf8_casefold("report", -1);
				break;
			}
			case 3:
			{
				documentclass_str = g_utf8_casefold("letter", -1);
				break;
			}
			case 4:
			{
				documentclass_str = g_utf8_casefold("beamer", -1);
			}
		}

		if (KOMA_active)
		{
			switch (documentclass_int)
			{
				case 0:
				{
					documentclass_str = g_utf8_casefold("scrbook", -1);
					break;
				}
				case 1:
				{
					documentclass_str = g_utf8_casefold("scrartcl", -1);
					break;
				}
				case 2:
				{
					documentclass_str = g_utf8_casefold("scrreprt", -1);
					break;
				}
			}

		}

		/*  Get the correct template */
		/*  First check whether its a build in one or a custom one than
		 *  assign the wished template */
		if (template_int < LATEX_WIZARD_TEMPLATE_END ||
			glatex_wizard.template_list == NULL)
		{
			if (template_int == LATEX_WIZARD_TEMPLATE_DEFAULT &&
				documentclass_int == 3 && documentclass_int == 4)
				code = g_string_new(TEMPLATE_LATEX);
			else if (documentclass_int == 3)
				code = g_string_new(TEMPLATE_LATEX_LETTER);
			else if (documentclass_int == 4)
				code = g_string_new(TEMPLATE_LATEX_BEAMER);
			else
				code = g_string_new(TEMPLATE_LATEX);
		}
		else
		{
			TemplateEntry *tmp = NULL;

			/* Return if the value choose is for some uknown reason to high */
			if (template_int > (glatex_wizard.template_list->len + LATEX_WIZARD_TEMPLATE_END))
				return;

			tmp = g_ptr_array_index(glatex_wizard.template_list, template_int - LATEX_WIZARD_TEMPLATE_END);
			code = glatex_get_template_from_file(tmp->filepath);

			/* Cleaning up template array as there is no usage for anymore */
			g_ptr_array_foreach (glatex_wizard.template_list, (GFunc)glatex_free_template_entry, NULL);
			g_ptr_array_free(glatex_wizard.template_list, TRUE);
		}

		if (code != NULL)
		{
			if (classoptions != NULL)
			{
				utils_string_replace_all(code, "{CLASSOPTION}", classoptions);
				g_free(classoptions);
			}
			if (documentclass_str != NULL)
			{
				utils_string_replace_all(code, "{DOCUMENTCLASS}", documentclass_str);
				g_free(documentclass_str);
			}
			if (enc_latex_char != NULL)
			{
				utils_string_replace_all(code, "{ENCODING}", enc_latex_char);
				g_free(enc_latex_char);
			}
			else
			/* If there is no encoding proberly set but {ENCODING} is set inside
			 * the template, replace it with nothing */
			{
				utils_string_replace_all(code, "{ENCODING}", "");
			}
			switch (paperorientation_int){
				case 2:
				{
					utils_string_replace_all(code, "{GEOMETRY}",
						"\\usepackage[landscape]{geometry}\n");
					break;
				}
				default:
				{
					utils_string_replace_all(code, "{GEOMETRY}", "");
					break;
				}
			}
			if (NZV(author))
			{
				gchar* author_string = NULL;
				if (documentclass_int == 3)
				{
					author_string = g_strconcat("\\signature{", author, "}\n", NULL);
				}
				else
				{
					author_string = g_strconcat("\\author{", author, "}\n", NULL);
				}
				utils_string_replace_all(code, "{AUTHOR}", author_string);
				g_free(author);
				g_free(author_string);
			}
			else
			{
				gchar* author_string = NULL;
				if (documentclass_int == 3)
				{
					utils_string_replace_all(code, "{AUTHOR}", "\% \\signature{}\n");
				}
				else
				{
					utils_string_replace_all(code, "{AUTHOR}", "\% \\author{}\n");
				}
				utils_string_replace_all(code, "{AUTHOR}", author_string);
				if (author != NULL)
				{
					g_free(author);
				}
				g_free(author_string);
			}

			if (NZV(date))
			{
				gchar *date_string = NULL;
				date_string = g_strconcat("\\date{", date, "}\n", NULL);
				utils_string_replace_all(code, "{DATE}", date_string);
				g_free(date);
				g_free(date_string);
			}
			else
			{
				utils_string_replace_all(code, "{DATE}", "\% \\date{}\n");
				if (date != NULL)
				{
					g_free(date);
				}
			}

			if (title != NULL)
			{
				gchar *title_string = NULL;
				if (documentclass_int == 3)
				{
					title_string = g_strconcat("\\subject{", title, "}\n", NULL);
				}
				else
				{
					title_string = g_strconcat("\\title{", title, "}\n", NULL);
				}

				utils_string_replace_all(code, "{TITLE}", title_string);
				g_free(title);
				g_free(title_string);
			}
			else
			{
				if (documentclass_int == 3)
				{
					utils_string_replace_all(code, "{TITLE}", "\% \\subject{} \n");
				}
				else
				{
					utils_string_replace_all(code, "{TITLE}", "\% \\title{} \n");
				}
				if (title != NULL)
				{
					g_free(title);
				}
			}
			utils_string_replace_all(code, "{OPENING}", _("Dear Sir or Madame"));
			utils_string_replace_all(code, "{CLOSING}", _("With kind regards"));

			output = g_string_free(code, FALSE);
			show_output(output, NULL, encoding_int);
			g_free(output);
		}
		else
		{
			g_warning(_("No template assigned. Aborting"));
		}
	}
	gtk_widget_destroy(GTK_WIDGET(dialog));
}

void
glatex_wizard_activated(G_GNUC_UNUSED GtkMenuItem * menuitem,
				 G_GNUC_UNUSED gpointer gdata)
{
	gint i;
	gchar *author;
	GtkWidget *dialog = NULL;
	GtkWidget *vbox = NULL;
	GtkWidget *label_documentclass = NULL;
	GtkWidget *label_encoding = NULL;
	GtkWidget *label_fontsize = NULL;
	GtkWidget *table = NULL;
	GtkWidget *label_author = NULL;
	GtkWidget *label_date = NULL;
	GtkWidget *label_title = NULL;
	GtkWidget *label_papersize = NULL;
	GtkWidget *label_template = NULL;
	GtkWidget *label_orientation = NULL;
	GtkWidget *fontsize_entry = NULL;

	/*  Building the wizard-dialog and showing it */
	dialog = gtk_dialog_new_with_buttons(_("LaTeX-Wizard"),
				GTK_WINDOW(geany->main_widgets->window),
				GTK_DIALOG_DESTROY_WITH_PARENT, GTK_STOCK_CANCEL,
				GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
				NULL);
	gtk_widget_set_name(dialog, "GeanyDialog");

	/*  Creating and formatting table */
	table = gtk_table_new(2, 6, FALSE);
	gtk_table_set_col_spacings(GTK_TABLE(table), 6);
	gtk_table_set_row_spacings(GTK_TABLE(table), 6);

	/*  Templates
	 *  Adds custom templates if there are any. If there are none just
	 *  adds default one */
	label_template = gtk_label_new(_("Template:"));

	glatex_wizard.template_combobox = gtk_combo_box_new_text();
	ui_widget_set_tooltip_text(glatex_wizard.template_combobox,
		_("Set the template which should be used for creating the new document"));
	gtk_misc_set_alignment(GTK_MISC(label_template), 0, 0.5);

	gtk_table_attach_defaults(GTK_TABLE(table), label_template, 0, 1, 0, 1);
	gtk_table_attach_defaults(GTK_TABLE(table), glatex_wizard.template_combobox, 1, 2, 0, 1);

	/*  Adding default/build in templates to pull down and set the generic
	 * 	one as default */
	gtk_combo_box_insert_text(GTK_COMBO_BOX(glatex_wizard.template_combobox),
		LATEX_WIZARD_TEMPLATE_DEFAULT, _("Default"));
	gtk_combo_box_set_active(GTK_COMBO_BOX(glatex_wizard.template_combobox),
		LATEX_WIZARD_TEMPLATE_DEFAULT);

	/*  Checking whether some custom template are available and adding
	 *  if so.
	 *  Also init array with templates available. */
	glatex_wizard.template_list = glatex_init_custom_templates();
	glatex_add_templates_to_combobox(glatex_wizard.template_list, glatex_wizard.template_combobox);

	/*  Documentclass */
	label_documentclass = gtk_label_new(_("Documentclass:"));
	glatex_wizard.documentclass_combobox = gtk_combo_box_new_text();
	ui_widget_set_tooltip_text(glatex_wizard.documentclass_combobox,
		_("Choose the kind of document you want to write"));
	gtk_combo_box_insert_text(GTK_COMBO_BOX(glatex_wizard.documentclass_combobox), 0,
		_("Book"));
	gtk_combo_box_insert_text(GTK_COMBO_BOX(glatex_wizard.documentclass_combobox), 1,
		_("Article"));
	gtk_combo_box_insert_text(GTK_COMBO_BOX(glatex_wizard.documentclass_combobox), 2,
		_("Report"));
	gtk_combo_box_insert_text(GTK_COMBO_BOX(glatex_wizard.documentclass_combobox), 3,
		_("Letter"));
	gtk_combo_box_insert_text(GTK_COMBO_BOX(glatex_wizard.documentclass_combobox), 4,
		_("Presentation"));

	gtk_combo_box_set_active(GTK_COMBO_BOX(glatex_wizard.documentclass_combobox), 0);

	gtk_misc_set_alignment(GTK_MISC(label_documentclass), 0, 0.5);

	gtk_table_attach_defaults(GTK_TABLE(table), label_documentclass, 0, 1, 1, 2);
	gtk_table_attach_defaults(GTK_TABLE(table), glatex_wizard.documentclass_combobox, 1, 2, 1, 2);

	/*  Encoding */
	label_encoding = gtk_label_new(_("Encoding:"));

	glatex_wizard.encoding_combobox = gtk_combo_box_new_text();
	ui_widget_set_tooltip_text(glatex_wizard.encoding_combobox,
		_("Set the encoding for your new document"));
	for (i = 0; i < LATEX_ENCODINGS_MAX; i++)
	{
		gtk_combo_box_insert_text(GTK_COMBO_BOX(glatex_wizard.encoding_combobox), i,
					  latex_encodings[i].name);
	}

	gtk_combo_box_set_active(GTK_COMBO_BOX(glatex_wizard.encoding_combobox),
		find_latex_enc(geany_data->file_prefs->default_new_encoding));

	gtk_misc_set_alignment(GTK_MISC(label_encoding), 0, 0.5);

	gtk_table_attach_defaults(GTK_TABLE(table), label_encoding, 0, 1, 2, 3);
	gtk_table_attach_defaults(GTK_TABLE(table), glatex_wizard.encoding_combobox, 1, 2, 2, 3);

	/*  fontsize */
	label_fontsize = gtk_label_new(_("Font size"));
	glatex_wizard.fontsize_combobox = gtk_combo_box_entry_new_text();
	gtk_combo_box_append_text(GTK_COMBO_BOX(glatex_wizard.fontsize_combobox),"10pt");
	gtk_combo_box_append_text(GTK_COMBO_BOX(glatex_wizard.fontsize_combobox),"11pt");
	gtk_combo_box_append_text(GTK_COMBO_BOX(glatex_wizard.fontsize_combobox),"12pt");
	ui_widget_set_tooltip_text(glatex_wizard.fontsize_combobox,
		_("Set the default font size of your new document"));

	gtk_misc_set_alignment(GTK_MISC(label_fontsize), 0, 0.5);

	gtk_table_attach_defaults(GTK_TABLE(table), label_fontsize, 0, 1, 3, 4);
	gtk_table_attach_defaults(GTK_TABLE(table), glatex_wizard.fontsize_combobox, 1, 2, 3, 4);

	fontsize_entry =  gtk_bin_get_child(GTK_BIN(glatex_wizard.fontsize_combobox));
	g_signal_connect(G_OBJECT(fontsize_entry), "activate",
		G_CALLBACK(glatex_enter_key_pressed_in_entry), dialog);

	/*  Author */
	label_author = gtk_label_new(_("Author:"));
	glatex_wizard.author_textbox = gtk_entry_new();
	ui_widget_set_tooltip_text(glatex_wizard.author_textbox,
		_("Sets the value of the \\author command. In most cases this should be your name"));
	if (geany_data->template_prefs->developer != NULL)
	{
		author = geany_data->template_prefs->developer;
		gtk_entry_set_text(GTK_ENTRY(glatex_wizard.author_textbox), author);
	}
	gtk_misc_set_alignment(GTK_MISC(label_author), 0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label_author, 0, 1, 4, 5);
	gtk_table_attach_defaults(GTK_TABLE(table), glatex_wizard.author_textbox, 1, 2, 4, 5);

	g_signal_connect(G_OBJECT(glatex_wizard.author_textbox), "activate",
		G_CALLBACK(glatex_enter_key_pressed_in_entry), dialog);

	/*  Date */
	label_date = gtk_label_new(_("Date:"));
	glatex_wizard.date_textbox = gtk_entry_new();
	ui_widget_set_tooltip_text(glatex_wizard.date_textbox,
		_("Sets the value of the \\date command inside header of your "
		 "new created LaTeX-document. Keeping it at \\today is a good "
		 "decision if you don't need any fixed date."));
	gtk_entry_set_text(GTK_ENTRY(glatex_wizard.date_textbox), "\\today");
	gtk_misc_set_alignment(GTK_MISC(label_date), 0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label_date, 0, 1, 5, 6);
	gtk_table_attach_defaults(GTK_TABLE(table), glatex_wizard.date_textbox, 1, 2, 5, 6);

	g_signal_connect(G_OBJECT(glatex_wizard.date_textbox), "activate",
		G_CALLBACK(glatex_enter_key_pressed_in_entry), dialog);

	/*  Title of the new document */
	label_title = gtk_label_new(_("Title:"));
	glatex_wizard.title_textbox = gtk_entry_new();
	ui_widget_set_tooltip_text(glatex_wizard.title_textbox,
		_("Sets the title of your new document."));
	gtk_misc_set_alignment(GTK_MISC(label_title), 0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label_title, 0, 1, 6, 7);
	gtk_table_attach_defaults(GTK_TABLE(table), glatex_wizard.title_textbox, 1, 2, 6, 7);

	g_signal_connect(G_OBJECT(glatex_wizard.title_textbox), "activate",
		G_CALLBACK(glatex_enter_key_pressed_in_entry), dialog);

	/*  Papersize */
	label_papersize = gtk_label_new(_("Paper size:"));
	glatex_wizard.papersize_combobox = gtk_combo_box_new_text();
	ui_widget_set_tooltip_text(glatex_wizard.papersize_combobox,
		_("Choose the paper format for the newly created document"));
	gtk_combo_box_insert_text(GTK_COMBO_BOX(glatex_wizard.papersize_combobox), 0, "A4");
	gtk_combo_box_insert_text(GTK_COMBO_BOX(glatex_wizard.papersize_combobox), 1, "A5");
	gtk_combo_box_insert_text(GTK_COMBO_BOX(glatex_wizard.papersize_combobox), 2, "A6");

	gtk_combo_box_set_active(GTK_COMBO_BOX(glatex_wizard.papersize_combobox), 0);

	gtk_misc_set_alignment(GTK_MISC(label_papersize), 0, 0.5);

	gtk_table_attach_defaults(GTK_TABLE(table), label_papersize, 0, 1, 7, 8);
	gtk_table_attach_defaults(GTK_TABLE(table), glatex_wizard.papersize_combobox, 1, 2, 7, 8);

	/* Paper direction */
	label_orientation = gtk_label_new(_("Paper Orientation:"));
	glatex_wizard.orientation_combobox = gtk_combo_box_new_text();
	ui_widget_set_tooltip_text(glatex_wizard.orientation_combobox,
		_("Choose the paper orientation for the newly created document"));
	gtk_combo_box_insert_text(GTK_COMBO_BOX(glatex_wizard.orientation_combobox), 0, "Default");
	gtk_combo_box_insert_text(GTK_COMBO_BOX(glatex_wizard.orientation_combobox), 1, "Portrait");
	gtk_combo_box_insert_text(GTK_COMBO_BOX(glatex_wizard.orientation_combobox), 2, "Landscape");

	gtk_combo_box_set_active(GTK_COMBO_BOX(glatex_wizard.orientation_combobox), 0);

	gtk_misc_set_alignment(GTK_MISC(label_orientation), 0, 0.5);

	gtk_table_attach_defaults(GTK_TABLE(table), label_orientation, 0, 1, 8, 9);
	gtk_table_attach_defaults(GTK_TABLE(table), glatex_wizard.orientation_combobox, 1, 2, 8, 9);


	/* Doing the rest .... */
	gtk_widget_show_all(table);

	vbox = ui_dialog_vbox_new(GTK_DIALOG(dialog));
	gtk_box_set_spacing(GTK_BOX(vbox), 10);
	gtk_container_add(GTK_CONTAINER(vbox), table);

	glatex_wizard.checkbox_KOMA = gtk_check_button_new_with_label(
		_("Use KOMA-script classes if possible"));
	ui_widget_set_tooltip_text(glatex_wizard.checkbox_KOMA,
		_("Uses the KOMA-script classes by Markus Kohm.\n"
		"Keep in mind: To compile your document these classes "
		"have to be installed before."));
	gtk_button_set_focus_on_click(GTK_BUTTON(glatex_wizard.checkbox_KOMA), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(glatex_wizard.checkbox_KOMA), glatex_set_koma_active);
	gtk_box_pack_start(GTK_BOX(vbox), glatex_wizard.checkbox_KOMA, FALSE, FALSE, 5);

	glatex_wizard.checkbox_draft = gtk_check_button_new_with_label(_("Use draft mode"));
	ui_widget_set_tooltip_text(glatex_wizard.checkbox_draft,
		_("Set the draft flag inside new created documents to get "
		"documents with a number of debugging helpers"));
	gtk_button_set_focus_on_click(GTK_BUTTON(glatex_wizard.checkbox_draft), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(glatex_wizard.checkbox_draft), FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), glatex_wizard.checkbox_draft, FALSE, FALSE, 5);
	g_signal_connect(dialog, "response", G_CALLBACK(on_wizard_response), NULL);

	gtk_widget_show_all(dialog);
}

static void init_keybindings()
{
	GeanyKeyGroup *key_group;

	/* init keybindings */
	key_group = plugin_set_key_group(geany_plugin, "geanylatex", COUNT_KB, NULL);
	keybindings_set_item(key_group, KB_LATEX_WIZARD, glatex_kbwizard,
		0, 0, "run_latex_wizard", _("Run LaTeX-Wizard"), menu_latex_wizard);
	keybindings_set_item(key_group, KB_LATEX_INSERT_LABEL, glatex_kblabel_insert,
		0, 0, "insert_latex_label", _("Insert \\label"), menu_latex_label);
	keybindings_set_item(key_group, KB_LATEX_INSERT_REF, glatex_kbref_insert,
		0, 0, "insert_latex_ref", _("Insert \\ref"), menu_latex_ref);
	keybindings_set_item(key_group, KB_LATEX_INSERT_NEWLINE, glatex_kb_insert_newline,
		0, 0, "insert_new_line", _("Insert linebreak \\\\ "), NULL);
	keybindings_set_item(key_group, KB_LATEX_INSERT_COMMAND,
		glatex_kb_insert_command_dialog, 0, 0, "latex_insert_command",
		_("Insert command"), menu_latex_insert_command);
	keybindings_set_item(key_group, KB_LATEX_TOGGLE_ACTIVE, glatex_kblatex_toggle,
		0, 0, "latex_toggle_status", _("Turn input replacement on/off"),
		menu_latex_replace_toggle);
	keybindings_set_item(key_group, KB_LATEX_REPLACE_SPECIAL_CHARS,
		glatex_kb_replace_special_chars, 0, 0, "latex_replace_chars",
		_("Replace special characters"), NULL);
	keybindings_set_item(key_group, KB_LATEX_ENVIRONMENT_INSERT,
		glatex_kbref_insert_environment, 0, 0, "latex_insert_environment",
		_("Run insert environment dialog"), menu_latex_insert_environment);
	keybindings_set_item(key_group, KB_LATEX_INSERT_NEWITEM,
		glatex_kb_insert_newitem, 0, 0, "latex_insert_item", _("Insert \\item"), NULL);
	keybindings_set_item(key_group, KB_LATEX_FORMAT_BOLD, glatex_kb_format_bold,
		0, 0, "format_bold", _("Format selection in bold font face"), NULL);
	keybindings_set_item(key_group, KB_LATEX_FORMAT_ITALIC, glatex_kb_format_italic,
		0, 0, "format_italic", _("Format selection in italic font face"), NULL);
	keybindings_set_item(key_group, KB_LATEX_FORMAT_TYPEWRITER, glatex_kb_format_typewriter,
		0, 0, "format_typewriter", _("Format selection in typewriter font face"), NULL);
	keybindings_set_item(key_group, KB_LATEX_FORMAT_CENTER, glatex_kb_format_centering,
		0, 0, "format_center", _("Format selection centered"), NULL);
	keybindings_set_item(key_group, KB_LATEX_FORMAT_LEFT, glatex_kb_format_left,
		0, 0, "format_left", _("Format selection left-aligned"), NULL);
	keybindings_set_item(key_group, KB_LATEX_FORMAT_RIGHT, glatex_kb_format_right,
		0, 0, "format_right", _("Format selection right-aligned"), NULL);
	keybindings_set_item(key_group, KB_LATEX_ENVIRONMENT_INSERT_DESCRIPTION,
		glatex_kb_insert_description_list, 0, 0, "insert_description_list",
		_("Insert description list"), NULL);
	keybindings_set_item(key_group, KB_LATEX_ENVIRONMENT_INSERT_ITEMIZE,
		glatex_kb_insert_itemize_list, 0, 0, "insert_itemize_list",
		_("Insert itemize list"), NULL);
	keybindings_set_item(key_group, KB_LATEX_ENVIRONMENT_INSERT_ENUMERATE,
		glatex_kb_insert_enumerate_list, 0, 0, "insert_enumerate_list",
		_("Insert enumerate list"), NULL);
	keybindings_set_item(key_group, KB_LATEX_STRUCTURE_LVLUP,
		glatex_kb_structure_lvlup, 0, 0, "structure_lvl_up",
		_("Set selection one level up"), NULL);
	keybindings_set_item(key_group, KB_LATEX_STRUCTURE_LVLDOWN,
		glatex_kb_structure_lvldown, 0, 0, "structure_lvl_down",
		_("Set selection one level down"), NULL);
	keybindings_set_item(key_group, KB_LATEX_USEPACKAGE_DIALOG,
		glatex_kb_usepackage_dialog, 0, 0, "usepackage_dialog",
		_("Insert \\usepackage{}"), menu_latex_insert_usepackage);
	keybindings_set_item(key_group, KB_LATEX_INSERT_CITE,
		glatex_kb_insert_bibtex_cite, 0, 0, "insert_cite_dialog",
		_("Insert BibTeX reference dialog"), menu_latex_insert_bibtex_cite);
}


void plugin_help()
{
	dialogs_show_msgbox(GTK_MESSAGE_INFO,
		_("GeanyLaTeX is a plugin to improve support for LaTeX in Geany."
		"\n\nPlease report all bugs or feature requests to one of the "
		"authors."));
}


static void glatex_init_configuration()
{
	GKeyFile *config = g_key_file_new();

	/* loading configurations from file ...*/
	config_file = g_strconcat(geany->app->configdir, G_DIR_SEPARATOR_S,
	"plugins", G_DIR_SEPARATOR_S,
	"geanyLaTeX", G_DIR_SEPARATOR_S, "general.conf", NULL);

	/* ... and Initialising options from config file */
	g_key_file_load_from_file(config, config_file, G_KEY_FILE_NONE, NULL);

	glatex_set_koma_active = utils_get_setting_boolean(config, "general",
		"glatex_set_koma_active", FALSE);
	glatex_set_toolbar_active = utils_get_setting_boolean(config, "general",
		"glatex_set_toolbar_active", FALSE);
	glatex_autocompletion_active = utils_get_setting_boolean(config, "general",
		"glatex_set_autocompletion", TRUE);
	glatex_autobraces_active = utils_get_setting_boolean(config, "autocompletion",
		"glatex_set_autobraces", TRUE);

	/* Hidden preferences. Can be set directly via configuration file*/
	glatex_autocompletion_context_size = utils_get_setting_integer(config, "autocompletion",
		"glatex_set_autocompletion_contextsize", 5);

	/* Doing some input validation */
	if (glatex_autocompletion_active == TRUE &&
		glatex_autocompletion_context_size <= 0)
	{
		glatex_autocompletion_context_size = 5;
		g_warning(_("glatex_set_autocompletion_contextsize has been "
					"initialized with an invalid value. Default value taken. "
					"Please check your configuration file"));
	}
	/* Increase value by an offset as we add a new line so 2 really means 2 */
	glatex_autocompletion_context_size = glatex_autocompletion_context_size + 2;

	glatex_autocompletion_only_for_latex = utils_get_setting_boolean(config, "autocompletion",
		"glatex_autocompletion_only_for_latex", TRUE);
	glatex_capitalize_sentence_starts = utils_get_setting_boolean(config, "autocompletion",
		"glatex_capitalize_sentence_starts", FALSE);

	glatex_deactivate_toolbaritems_with_non_latex = utils_get_setting_boolean(config, "toolbar",
		"glatex_deactivate_toolbaritems_with_non_latex", TRUE);
	glatex_wizard_to_generic_toolbar = utils_get_setting_boolean(config, "toolbar",
		"glatex_wizard_to_generic_toolbar", TRUE);
	glatex_deactivate_menubarentry_with_non_latex = utils_get_setting_boolean(config, "menu",
		"glatex_deactivate_menubarentry_with_non_latex", TRUE);
	glatex_add_menu_on_startup = utils_get_setting_boolean(config, "menu",
		"glatex_add_menu_on_startup", FALSE);

	glatex_ref_page_string = utils_get_setting_string(config, "reference",
		"glatex_reference_page", _("page \\pageref{{{reference}}}"));
	glatex_ref_chapter_string = utils_get_setting_string(config, "reference",
		"glatex_reference_chapter", "\\ref{{{reference}}}");
	glatex_ref_all_string = utils_get_setting_string(config, "reference",
		"glatex_reference_all", _("\\ref{{{reference}}}, page \\pageref{{{reference}}}"));

	glatex_ref_page_string = utils_get_setting_string(config, "reference",
		"glatex_reference_page", _("page \\pageref{{{reference}}}"));
	glatex_ref_chapter_string = utils_get_setting_string(config, "reference",
		"glatex_reference_chapter", "\\ref{{{reference}}}");
	glatex_ref_all_string = utils_get_setting_string(config, "reference",
		"glatex_reference_all", _("\\ref{{{reference}}}, page \\pageref{{{reference}}}"));

	g_key_file_free(config);
}


static void
add_wizard_to_generic_toolbar()
{
	if (glatex_wizard_generic_toolbar_item == NULL)
	{
		/* TODO: Find a better icon as this one is used way too often */
		glatex_wizard_generic_toolbar_item =
			gtk_tool_button_new_from_stock(GTK_STOCK_NEW);
		plugin_add_toolbar_item(geany_plugin, glatex_wizard_generic_toolbar_item);
		gtk_widget_show_all(GTK_WIDGET(glatex_wizard_generic_toolbar_item));
		g_signal_connect(glatex_wizard_generic_toolbar_item, "clicked",
				G_CALLBACK(glatex_wizard_activated), NULL);
	}
}


static void
remove_wizard_from_generic_toolbar()
{
	if (glatex_wizard_generic_toolbar_item != NULL)
	{
		gtk_widget_destroy(GTK_WIDGET(glatex_wizard_generic_toolbar_item));
		glatex_wizard_generic_toolbar_item = NULL;
	}
}


static void
add_menu_to_menubar()
{
	GtkWidget *tmp = NULL;
	gint i;
	GtkMenuShell *menubar;

	/* First we check for the menubar where to add the LaTeX menu */
	menubar = GTK_MENU_SHELL(
				ui_lookup_widget(geany->main_widgets->window, "menubar1"));

	/* First we check whether the LaTeX menu entry already exists */
	if (menu_latex == NULL)
	{
		/* Build up menu for menubar */
		menu_latex = gtk_menu_item_new_with_mnemonic(_("_LaTeX"));
		gtk_menu_shell_insert(
			menubar, menu_latex, g_list_length(menubar->children)-1);

		menu_latex_menu = gtk_menu_new();
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_latex), menu_latex_menu);

		/* Filling up menubar menus */
		/* LaTeX menu */
		menu_latex_wizard = ui_image_menu_item_new(GTK_STOCK_NEW, _("LaTeX-_Wizard"));
		gtk_container_add(GTK_CONTAINER(menu_latex_menu), menu_latex_wizard);
		ui_widget_set_tooltip_text(menu_latex_wizard,
					 _("Starts a Wizard to easily create LaTeX-documents"));

		g_signal_connect(menu_latex_wizard, "activate",
				 G_CALLBACK(glatex_wizard_activated), NULL);

		menu_latex_menu_special_char = gtk_menu_item_new_with_mnemonic(_("I_nsert Special Character"));
		ui_widget_set_tooltip_text(menu_latex_menu_special_char,
					 _("Helps to use some not very common letters and signs"));
		gtk_container_add(GTK_CONTAINER(menu_latex_menu),
			menu_latex_menu_special_char);

		menu_latex_menu_special_char_submenu = gtk_menu_new();
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_latex_menu_special_char),
			menu_latex_menu_special_char_submenu);
		glatex_sub_menu_init(menu_latex_menu_special_char_submenu,
			glatex_char_array, glatex_cat_names, char_insert_activated);

		menu_latex_ref = gtk_menu_item_new_with_mnemonic(_("Insert _Reference"));
		ui_widget_set_tooltip_text(menu_latex_ref,
			_("Inserting references to the document"));
		gtk_container_add(GTK_CONTAINER(menu_latex_menu), menu_latex_ref);
		g_signal_connect(menu_latex_ref, "activate",
			G_CALLBACK(glatex_insert_ref_activated), NULL);

		menu_latex_label = gtk_menu_item_new_with_mnemonic(_("Insert _Label"));
		ui_widget_set_tooltip_text(menu_latex_label,
			_("Helps at inserting labels to a document"));
		gtk_container_add(GTK_CONTAINER(menu_latex_menu), menu_latex_label);
		g_signal_connect(menu_latex_label, "activate",
			G_CALLBACK(glatex_insert_label_activated), NULL);

		menu_latex_insert_environment = gtk_menu_item_new_with_mnemonic(
			_("Insert _Environment"));
		ui_widget_set_tooltip_text(menu_latex_insert_environment,
			 _("Helps at inserting an environment a document"));
		gtk_container_add(GTK_CONTAINER(menu_latex_menu), menu_latex_insert_environment);
		g_signal_connect(menu_latex_insert_environment, "activate",
			G_CALLBACK(glatex_insert_environment_dialog), NULL);

		menu_latex_insert_usepackage = gtk_menu_item_new_with_mnemonic(
			_("Insert P_ackage"));
		ui_widget_set_tooltip_text(menu_latex_insert_usepackage,
			 _("A small dialog to insert \\usepackage{} into header of current file"));
		gtk_container_add(GTK_CONTAINER(menu_latex_menu), menu_latex_insert_usepackage);
		g_signal_connect(menu_latex_insert_usepackage, "activate",
			G_CALLBACK(glatex_insert_usepackage_dialog), NULL);

		menu_latex_format_insert = gtk_menu_item_new_with_mnemonic(_("_Format"));
		gtk_container_add(GTK_CONTAINER(menu_latex_menu), menu_latex_format_insert);

		menu_latex_format_insert_submenu = gtk_menu_new();
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_latex_format_insert),
			menu_latex_format_insert_submenu);

		for (i = 0; i < LATEX_STYLES_END; i++)
		{
			tmp = NULL;
			tmp = gtk_menu_item_new_with_mnemonic(_(glatex_format_labels[i]));
			gtk_container_add(GTK_CONTAINER(menu_latex_format_insert_submenu), tmp);
			g_signal_connect(tmp, "activate",
				G_CALLBACK(glatex_insert_latex_format), GINT_TO_POINTER(i));
		}

		/* Add font size menu */
		menu_latex_fontsize = gtk_menu_item_new_with_mnemonic(_("F_ont size"));
		gtk_container_add(GTK_CONTAINER(menu_latex_menu), menu_latex_fontsize);

		menu_latex_fontsize_submenu = gtk_menu_new();
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_latex_fontsize),
			menu_latex_fontsize_submenu);

		for (i = 0; i < LATEX_FONTSIZE_END; i++)
		{
			tmp = NULL;
			tmp = gtk_menu_item_new_with_mnemonic(_(glatex_fontsize_labels[i]));
			gtk_container_add(GTK_CONTAINER(menu_latex_fontsize_submenu), tmp);
			g_signal_connect(tmp, "activate",
				G_CALLBACK(glatex_insert_latex_fontsize), GINT_TO_POINTER(i));
		}

		/* Add menuitem for LaTeX replacement functions*/
		menu_latex_replacement = gtk_menu_item_new_with_mnemonic(
			_("_Special Character Replacement"));
		menu_latex_replacement_submenu = gtk_menu_new();
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_latex_replacement),
			menu_latex_replacement_submenu);
		gtk_container_add(GTK_CONTAINER(menu_latex_menu), menu_latex_replacement);

		/* Add menuitem for bulk replacment */
		menu_latex_replace_selection = gtk_menu_item_new_with_mnemonic(
			_("Bulk _Replace Special Characters"));
		ui_widget_set_tooltip_text(menu_latex_replace_selection,
			_("_Replace selected special characters with TeX substitutes"));
		gtk_container_add(GTK_CONTAINER(menu_latex_replacement_submenu),
			menu_latex_replace_selection);
		g_signal_connect(menu_latex_replace_selection, "activate",
			G_CALLBACK(glatex_replace_special_character), NULL);

		/* Add menu entry for toggling input replacment */
		menu_latex_replace_toggle = gtk_check_menu_item_new_with_mnemonic(
			_("Toggle _Special Character Replacement"));
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(menu_latex_replace_toggle),
										toggle_active);
		gtk_container_add(GTK_CONTAINER(menu_latex_replacement_submenu),
			menu_latex_replace_toggle);

		g_signal_connect(menu_latex_replace_toggle, "activate",
						 G_CALLBACK(glatex_toggle_status), NULL);

		/* Add menu entry for inserting a command */
		menu_latex_insert_command = gtk_menu_item_new_with_mnemonic(
			_("Insert _Command"));
		ui_widget_set_tooltip_text(menu_latex_ref,
			_("Inserting costumized command to document"));
		gtk_container_add(GTK_CONTAINER(menu_latex_menu),
			menu_latex_insert_command);
		g_signal_connect(menu_latex_insert_command, "activate",
			G_CALLBACK(glatex_insert_command_activated), NULL);

		/* Switch document sitiveness for menu entries */
		ui_add_document_sensitive(menu_latex_menu_special_char);
		ui_add_document_sensitive(menu_latex_ref);
		ui_add_document_sensitive(menu_latex_label);
		ui_add_document_sensitive(menu_latex_format_insert);
		ui_add_document_sensitive(menu_latex_insert_environment);
		ui_add_document_sensitive(menu_latex_insert_usepackage);
		ui_add_document_sensitive(menu_latex_insert_command);
		ui_add_document_sensitive(menu_latex_fontsize);
		ui_add_document_sensitive(menu_latex_replacement);


		gtk_widget_show_all(menu_latex);
	} /* only execute if menuitem "LaTeX" has not already been inserted
	     into menubar before */

	/* BibTeX menu */
	if (menu_bibtex == NULL)
	{
		menu_bibtex = gtk_menu_item_new_with_mnemonic(_("_BibTeX"));
		gtk_menu_shell_insert(
			menubar, menu_bibtex, g_list_length(menubar->children)-1);

		menu_bibtex_menu = gtk_menu_new();
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_bibtex), menu_bibtex_menu);

		menu_latex_insert_bibtex_cite =
			gtk_menu_item_new_with_mnemonic(_("Insert B_ibTeX reference"));
		ui_widget_set_tooltip_text(menu_latex_insert_bibtex_cite,
			_("Helps to insert a reference out of BibTeX files"));
		gtk_container_add(GTK_CONTAINER(menu_bibtex_menu), menu_latex_insert_bibtex_cite);
		g_signal_connect(menu_latex_insert_bibtex_cite, "activate",
			G_CALLBACK(on_insert_bibtex_dialog_activate), NULL);

		menu_latex_bibtex = gtk_menu_item_new_with_mnemonic(_("_BibTeX entries"));
		gtk_container_add(GTK_CONTAINER(menu_bibtex_menu), menu_latex_bibtex);

		menu_latex_bibtex_submenu = gtk_menu_new();
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_latex_bibtex),
			menu_latex_bibtex_submenu);

		for (i = 0; i < GLATEX_BIBTEX_N_TYPES; i++)
		{
			tmp = NULL;
			tmp = gtk_menu_item_new_with_mnemonic(_(glatex_bibtex_types[i].label));
			gtk_container_add(GTK_CONTAINER(menu_latex_bibtex_submenu), tmp);
			g_signal_connect(tmp, "activate",
				G_CALLBACK(glatex_insert_bibtex_entry), GINT_TO_POINTER(i));
		}

		/* Switch document sensitivness */
		ui_add_document_sensitive(menu_latex_bibtex);

		gtk_widget_show_all(menu_bibtex);
	} /* Only insert BibTeX menu if not already done. */
}

/* Removes the menubar menus from menubar if requested and available */
static void remove_menu_from_menubar()
{
	if (menu_latex != NULL)
	{
		gtk_widget_destroy(menu_latex);
		menu_latex = NULL;
	}
	if (menu_bibtex != NULL)
	{
		gtk_widget_destroy(menu_bibtex);
		menu_bibtex = NULL;
	}
}


static void
remove_menu_from_tools_menu()
{
	if (menu_latex_toolbar_wizard != NULL)
	{
		gtk_widget_destroy(menu_latex_toolbar_wizard);
		menu_latex_toolbar_wizard = NULL;
	}
}


static void
add_wizard_to_tools_menu()
{
	if (menu_latex_toolbar_wizard == NULL)
	{
		menu_latex_toolbar_wizard = ui_image_menu_item_new(GTK_STOCK_NEW,
			_("LaTeX-_Wizard"));
		gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), menu_latex_toolbar_wizard);
		ui_widget_set_tooltip_text(menu_latex_toolbar_wizard,
					 _("Starts a Wizard to easily create LaTeX-documents"));
		gtk_widget_show_all(menu_latex_toolbar_wizard);
		g_signal_connect(menu_latex_toolbar_wizard, "activate",
				 G_CALLBACK(glatex_wizard_activated), NULL);
	}
}


void
plugin_init(G_GNUC_UNUSED GeanyData * data)
{
	GeanyDocument *doc = NULL;

	doc = document_get_current();

	main_locale_init(LOCALEDIR, GETTEXT_PACKAGE);

	glatex_init_configuration();
	glatex_init_encodings_latex();

	add_wizard_to_tools_menu();

	init_keybindings();

	/* Check whether the toolbar should be shown or not and do so*/
	if (glatex_set_toolbar_active == TRUE)
	{
		glatex_toolbar = init_toolbar();
	}
	else
	{
		glatex_toolbar = NULL;
	}

	if (glatex_wizard_to_generic_toolbar == TRUE)
	{
		add_wizard_to_generic_toolbar();
	}
	else
	{
		glatex_wizard_generic_toolbar_item = NULL;
	}

	if (doc != NULL)
	{
		if (glatex_add_menu_on_startup == TRUE||
			doc->file_type->id == GEANY_FILETYPES_LATEX)
		{
			add_menu_to_menubar();
		}
	}

}

void
plugin_cleanup()
{
	if (glatex_toolbar != NULL)
		gtk_widget_destroy(glatex_toolbar);
	remove_menu_from_menubar();
	remove_menu_from_tools_menu();
	remove_wizard_from_generic_toolbar();
	g_free(config_file);
	g_free(glatex_ref_chapter_string);
	g_free(glatex_ref_page_string);
	g_free(glatex_ref_all_string);
}
