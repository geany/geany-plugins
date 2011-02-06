/*
 * gdb-ui-main.c - A GTK-based user interface for the GNU debugger.
 * Copyright 2008 Jeff Pohlmeyer <yetanothergeek(at)gmail(dot)com>
 * Copyright 2010 Radu Stefan <radu124@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA. *
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <gtk/gtk.h>

#include "geanyplugin.h"

#include "gdb-io.h"
#include "gdb-ui.h"

#ifdef HAVE_ELF_H
# include <elf.h>
#elif defined(HAVE_ELF_ABI_H)
# include <elf_abi.h>
#else
# error Missing Elf header
#endif


GdbUiSetup gdbui_setup;

static GtkWidget *main_vbox;
static GtkWidget *action_area;
static GtkWidget *stat_lab;

static GtkWidget *term_chk;
static GtkWidget *pipe_chk;

#define DEBUGACTIONS \
DAENTRY(load,	GTK_STOCK_OPEN,			"_Load",	"Load target program into debugger.")\
DAENTRY(unload,	GTK_STOCK_QUIT,			"_Unload",	"Kill the target program AND the debugger.")\
DAENTRY(run, 	GTK_STOCK_EXECUTE,		"_Run",		"Execute target program in debugger.")\
DAENTRY(kill,	GTK_STOCK_STOP,			"_Kill",	"Kill the target program with SIGKILL.")\
DAENTRY(pause,	GTK_STOCK_MEDIA_PAUSE,	"_Pause",	"Pause the target program with SIGINT.")\
DAENTRY(cont,	GTK_STOCK_MEDIA_PLAY,	"_Continue","Continue executing target program.")\
DAENTRY(step,	GTK_STOCK_GO_FORWARD,	"_Step",	"Step to the next line or function call.")\
DAENTRY(stepi,	GTK_STOCK_GOTO_LAST,	"Step _Into","Execute the next machine instruction or function call.")\
DAENTRY(next,	GTK_STOCK_MEDIA_FORWARD,"_Next",	"Step to the next line.")\
DAENTRY(nexti,	GTK_STOCK_MEDIA_NEXT,	"Ne_xt in",	"Execute the next machine instruction.")\
DAENTRY(until,	GTK_STOCK_JUMP_TO,		"Run _To",	"Run to specified source line.")\
DAENTRY(stack,	GTK_STOCK_DND_MULTIPLE,	"Sta_ck",	"Display a backtrace of the current call stack.")\
DAENTRY(break,	GTK_STOCK_INDEX,		"_Break",	"Add or remove breakpoints.")\
DAENTRY(watch,	GTK_STOCK_FIND,			"_Watches",	"Add or remove watches.")\
DAENTRY(finish,	GTK_STOCK_GOTO_BOTTOM,	"_Finish",	"Complete the currently executing function.")\
DAENTRY(return,	GTK_STOCK_UNDO,			"_Return",	"Return immediately from the current function.")\
DAENTRY(env,	GTK_STOCK_PROPERTIES,	"En_vironment" ,"Set target environment and command line options.")\
DAENTRY(prefs,	GTK_STOCK_PREFERENCES,	"_Options",	"Set user interface options.")

struct SdebugMenu
{
	GtkWidget *menu;
#define DAENTRY(A,B,C,D) GtkWidget *_##A;
	DEBUGACTIONS
#undef DAENTRY
} sdm;

#define DAENTRY(A,B,C,D) GtkWidget *A##_btn;
	DEBUGACTIONS
#undef DAENTRY


static GtkWidget *con_lab;
static GtkWidget *con_cmd;

static GtkWidget *last_used = NULL;

static gboolean pause_clicked = FALSE;


#define we(w) gtk_widget_set_sensitive(w,TRUE);
#define wd(w) gtk_widget_set_sensitive(w,FALSE);

#define bwe(w) do { gtk_widget_set_sensitive(w##_btn,TRUE); \
	gtk_widget_set_sensitive(sdm._##w,TRUE); \
	} while(0)
#define bwd(w) do { gtk_widget_set_sensitive(w##_btn,FALSE); \
	gtk_widget_set_sensitive(sdm._##w,FALSE); \
	} while(0)


#define pipe_chk_active() gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pipe_chk))

#define black "#000000"
#define white "#FFFFFF"
#define red   "#EE0000"
#define green  "#00DD00"
#define blue   "#0000FF"
#define yellow "#FFFF00"

/*
 * targetpath is to remember the path of the executable. Will be set in
 * load_click()
 */
gchar* targetpath = NULL;


void
gdbui_enable(gboolean enabled)
{
	gtk_widget_set_sensitive(main_vbox, enabled);
}


static void
status(const gchar * msg, const gchar * bg, const gchar * fg)
{
	gchar *esc = g_markup_escape_text(msg, -1);
	gchar *mu =
		g_strdup_printf
		("<span weight=\"bold\" foreground=\"%s\"  background=\"%s\">  %s  </span>", fg, bg,
		 esc);
	gtk_label_set_markup(GTK_LABEL(stat_lab), mu);
	g_free(mu);
	g_free(esc);
}

static GdbStatus curr_status = GdbDead;

static void
status_func(GdbStatus st)
{
	bwd(load);
	bwd(run);
	bwd(pause);
	bwd(cont);
	bwd(step);
	bwd(stepi);
	bwd(next);
	bwd(nexti);
	bwd(until);
	bwd(finish);
	bwd(return);
	bwd(stack);

	bwd(kill);
	wd(pipe_chk);
	bwd(break);
	bwd(watch);
	wd(con_lab);
	wd(con_cmd);
	wd(term_chk);
	bwd(env);
	bwe(unload);
	switch (st)
	{
		case GdbDead:
			{
				bwe(load);
				we(pipe_chk);
				we(term_chk);
				bwd(unload);
				status(_("(no program)"), black, white);
				break;
			}
		case GdbLoaded:
			{
				bwe(load);
				bwe(run);
				we(pipe_chk);
				bwe(break);
				bwe(watch);
				we(term_chk);
				we(con_lab);
				we(con_cmd);
				bwe(env);
				status(_("loaded"), white, black);
				break;
			}
		case GdbStartup:
			{
				status(_("starting"), yellow, red);
				break;
			}
		case GdbRunning:
			{
				bwe(pause);
				bwe(kill);
				status(_("running"), green, white);
				break;
			}
		case GdbStopped:
			{
				bwe(cont);
				bwe(step);
				bwe(stepi);
				bwe(next);
				bwe(nexti);
				bwe(until);
				bwe(finish);
				bwe(return);
				bwe(stack);
				bwe(kill);
				bwe(break);
				bwe(watch);
				we(con_lab);
				we(con_cmd);
				we(pipe_chk);
				bwe(env);
				status(_("stopped"), red, yellow);
				break;
			}
		case GdbFinished:
			{
				bwe(load);
				bwe(run);
				we(con_lab);
				we(con_cmd);
				we(pipe_chk);
				we(term_chk);
				bwe(break);
				bwe(watch);
				bwe(env);
				status(_("terminated"), white, black);
				break;
			}
	}
	if (!(last_used->state & GTK_STATE_INSENSITIVE))
	{
		gtk_widget_grab_focus(last_used);
	}
	curr_status = st;
}



static void
show_line(const gchar * filename, const gchar * line, const gchar * reason)
{
	if (gdbui_setup.line_func)
	{
		gdbui_setup.line_func(filename, line, reason);
	}
	else
	{
		g_printerr("%s:%s (%s)\n", filename, line, reason);
	}
}



static void
signal_func(const GdbSignalInfo * si)
{
	gchar *msg = g_strdup_printf("%s (%s)\nat %s in function %s()\nat %s:%s%s%s",
					 si->signal_name, si->signal_meaning, si->addr,
					 si->func, si->file, si->line, si->from ? "\nfrom " : "",
					 si->from ? si->from : "");
	if (pause_clicked)
	{
		status(_("paused"), yellow, red);
		pause_clicked = FALSE;
	}
	else
	{
		GtkWidget *dlg = gtk_message_dialog_new(GTK_WINDOW(gdbui_setup.main_window),
							GTK_DIALOG_MODAL |
							GTK_DIALOG_DESTROY_WITH_PARENT,
							GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
							_("Program received signal:"));

		gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dlg), "%s", msg);
		gtk_window_set_title(GTK_WINDOW(dlg), "debugger");
		gtk_window_set_keep_above(GTK_WINDOW(dlg), TRUE);
		gtk_dialog_run(GTK_DIALOG(dlg));
		gtk_widget_destroy(dlg);
		status(si->signal_name, yellow, red);
	}
	if (gdbui_setup.warn_func)
	{
		gchar *p;
		for (p = msg; *p; p++)
		{
			if (*p == '\n')
			{
				*p = ' ';
			}
		}
		gdbui_setup.warn_func(msg);
	}
	g_free(msg);
	show_line(si->fullname, si->line, NULL);
}



static void
step_func(const gchar * filename, const gchar * line, const gchar * reason)
{
	status(reason, yellow, red);
	show_line(filename, line, reason);
}



static void
info_func(const gchar * msg)
{
	if (gdbui_setup.info_func)
	{
		gdbui_setup.info_func(msg);
	}
	else
	{
		g_printerr("%s", msg);
	}
}

static void
stack_dlg(const GSList * frame_list)
{
	while (gtk_events_pending())
	{
		gtk_main_iteration();
	}
	gdbui_stack_dlg(frame_list);
	we(stack_btn);
	gtk_widget_grab_focus(stack_btn);
}



static void
err_func(const gchar * msg)
{
	GtkWidget *dlg = NULL;
	dlg = gtk_message_dialog_new(GTK_WINDOW(gdbui_setup.main_window),
					 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, _("Error:"));
	gtk_window_set_keep_above(GTK_WINDOW(dlg), TRUE);
	gtk_window_set_title(GTK_WINDOW(dlg), "debugger");
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dlg), "%s", msg);
	gtk_window_set_keep_above(GTK_WINDOW(dlg), TRUE);
	gtk_dialog_run(GTK_DIALOG(dlg));
	gtk_widget_destroy(dlg);
	if (gdbui_setup.warn_func)
	{
		gdbui_setup.warn_func(msg);
	}
}



static void
pipe_click(GtkWidget * w, gpointer user_data)
{
	gdbio_send_cmd("-interpreter-exec console \"handle SIGPIPE %s\"\n",
			   pipe_chk_active()? "nostop" : "stop");
}




void
gdbui_set_tip(GtkWidget * w, gchar * tip)
{
	if (gdbui_setup.options.show_tooltips)
	{
		if (w && tip)
		{
			ui_widget_set_tooltip_text(w, tip);
		}
	}
}


static const gboolean disable_mnemonics = TRUE;

static GtkWidget *
make_btn(const gchar *text, GtkCallback cb, gchar *img, gchar *tip)
{
	GtkWidget *button;

	if (disable_mnemonics)
	{
		gchar *p;
		gchar *buffer = g_strdup(text);

		for (p = buffer; *p; p++)
		{
			if (*p == '_')
			{
				memmove(p, p + 1, strlen(p));
			}
		}

		button = gtk_button_new_with_label(buffer);
		g_free(buffer);
	}

	else
	{
		gtk_button_new_with_mnemonic(text);
	}

	if (cb)
	{
		g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(cb), NULL);
	}


	if (tip)
	{
		gdbui_set_tip(button, tip);
	}


	if (img && gdbui_setup.options.show_icons)
	{
		gtk_button_set_image(GTK_BUTTON(button),
					 gtk_image_new_from_stock(img, GTK_ICON_SIZE_BUTTON));
	}


	return button;
}


#define used(w) last_used=w;



/* Check for ELF type */
typedef enum
{ ELF_RELOC, ELF_EXEC, ELF_SHARED, ELF_CORE, ELF_UNKNOWN } ElfType;

static ElfType
get_elf_type(const char * filename)
{
	/* There are seperate 32 bit and 64 bit elf headers on linux. The
	 * bits we are interested in should be in the same location for
	 * both. */
	union
	{
		Elf32_Ehdr ehdr32;
		Elf64_Ehdr ehdr64;
	} e_header;

	int read_ehdr;
	int is_32bit_elf;
	int is_little_endian;
	const char* e_type_buf;
	unsigned int e_type;
	FILE* file = fopen(filename, "r");

	if (!file)
		return ELF_UNKNOWN;

	read_ehdr = fread(&e_header, sizeof(e_header), 1, file);
	fclose(file);

	if (read_ehdr != 1)
		return ELF_UNKNOWN;

	/* Determine how to read the rest of the header.  */
	switch (e_header.ehdr32.e_ident[EI_DATA])
	{
		default: /* fall through */
		case ELFDATANONE: /* fall through */
		case ELFDATA2LSB:
			is_little_endian = 1;
			break;
		case ELFDATA2MSB:
			is_little_endian = 0;
		break;
	}

	/* For now we only support 32 bit and 64 bit ELF files.  */
	is_32bit_elf = (e_header.ehdr32.e_ident[EI_CLASS] != ELFCLASS64);

	/* The type field offset and size should be the same between 32 and 64 bit platforms! */
	if (!is_32bit_elf &&
		((char*) &e_header.ehdr32.e_type != (char*) &e_header.ehdr64.e_type ||
		(char*) &e_header.ehdr32.e_machine != (char*) &e_header.ehdr64.e_machine))
		return ELF_UNKNOWN;

	/* get the type taking endian into account */
	e_type_buf = (char*) &e_header.ehdr32.e_type;
	if (is_little_endian)
		e_type = ((unsigned int) (e_type_buf[0])) | (((unsigned int) (e_type_buf[1])) << 8);
	else
		e_type = ((unsigned int) (e_type_buf[1])) | (((unsigned int) (e_type_buf[0])) << 8);

	switch (e_type)
	{
		case ET_REL:
			return ELF_RELOC;
		case ET_EXEC:
			return ELF_EXEC;
		case ET_DYN:
			return ELF_SHARED;
		case ET_CORE:
			return ELF_CORE;
		default:
			return ELF_UNKNOWN;
	}
}



static void
load_click(GtkWidget * btn, gpointer user_data)
{
	gchar *errmsg = NULL;
	GtkWidget *dlg = gtk_file_chooser_dialog_new(_("Select executable to debug"),
							 GTK_WINDOW(gdbui_setup.main_window),
							 GTK_FILE_CHOOSER_ACTION_OPEN,
							 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
							 GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);

	if (targetpath != NULL)
	{
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER(dlg), targetpath);
	}

	do
	{
		if (errmsg)
		{
			err_func(errmsg);
			errmsg = NULL;
		}

		if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT)
		{
			gchar *fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
			targetpath = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(dlg));

			if (fn)
			{
				if (access(fn, R_OK) == 0)
				{
					switch (get_elf_type(fn))
					{
						case ELF_RELOC:
						{
							errmsg = _("Debugging of object files is not supported.");
							break;
						}
						case ELF_EXEC:
						{
							if (access(fn, X_OK) == 0)
							{
								gchar *base_path;
								gchar *ldd = g_strdup_printf("ldd \"%s\"", fn);
								FILE *ph = popen(ldd, "r");
								if (ph)
								{
									ssize_t r = 0;
									char *buf = NULL;
									size_t len = 0;
									gboolean have_x = FALSE;
									while (r >= 0)
									{
										r = getline(&buf, &len, ph);
										if (len && buf
												&& strstr(buf, "libX11.so"))
										{
											have_x = TRUE;
										}
									}
									pclose(ph);
									gtk_toggle_button_set_active
											(GTK_TOGGLE_BUTTON(term_chk),
											!have_x);
								}
								gdbio_load(fn);
								if (pipe_chk_active())
									pipe_click(pipe_chk, NULL);

								/* set working dir after loading */
								base_path = g_path_get_dirname(fn);
								gdbio_send_cmd("-environment-cd %s\n", base_path);
								g_free(base_path);
								g_free(ldd);
							}
							else
							{
								errmsg = _("You don't have permission to execute this file.");
							}
							break;
						}
						case ELF_SHARED:
						{
							errmsg = _("Debugging of shared libraries is not supported.");
							break;
						}
						case ELF_CORE:
						{
							errmsg = _("Debugging of core files id not supported.");
							break;
						}
						default:
						{
							errmsg = _("Target file must be ELF 32-bit x86 executable.");
						}
					}
				}
				else
				{
					errmsg = _("You don't have permission to read this file.");
				}
				g_free(fn);
			}
		}
		else
		{
			break;
		}
	}
	while (errmsg);
	gtk_widget_destroy(dlg);
	used(btn);
}



static void
pause_click(GtkWidget * btn, gpointer user_data)
{
	pause_clicked = TRUE;
	gdbio_pause_target();
	used(btn);
}



static void
kill_click(GtkWidget * btn, gpointer user_data)
{
	gboolean have_cmd_line = !(con_cmd->state & GTK_STATE_INSENSITIVE);
	gdbio_kill_target(!have_cmd_line);
	used(btn);
}



static void
run_click(GtkWidget * btn, gpointer user_data)
{
	gdbio_exec_target(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(term_chk))
			  && gdbui_setup.options.term_cmd
			  && *gdbui_setup.options.term_cmd ? gdbui_setup.options.term_cmd : NULL);
	used(btn);
}


static void
cont_click(GtkWidget * btn, gpointer user_data)
{
	gdbio_continue();
	used(btn);
}



static void
stack_click(GtkWidget * btn, gpointer user_data)
{
	wd(stack_btn);
	gdbio_show_stack(stack_dlg);
	used(btn);
}



static void
break_click(GtkWidget * btn, gpointer user_data)
{
	gdbui_break_dlg(FALSE);
	used(btn);
}


static void
watch_click(GtkWidget * btn, gpointer user_data)
{
	gdbui_break_dlg(TRUE);
	used(btn);
}



static void
step_click(GtkWidget * btn, gpointer user_data)
{
	gdbio_send_cmd("-exec-step\n");
	used(btn);
}

static void
stepi_click(GtkWidget * btn, gpointer user_data)
{
	gdbio_send_cmd("-exec-step-instruction\n");
	used(btn);
}


static void
next_click(GtkWidget * btn, gpointer user_data)
{
	gdbio_send_cmd("-exec-next\n");
	used(btn);
}

static void
nexti_click(GtkWidget * btn, gpointer user_data)
{
	gdbio_send_cmd("-exec-next-instruction\n");
	used(btn);
}



static void
until_click(GtkWidget * btn, gpointer user_data)
{
	LocationInfo *li = gdbui_location_dlg(_("Run to location"), FALSE);
	if (li)
	{
		if (li->filename && *(li->filename))
		{
			gdbio_send_cmd("-exec-until %s:%s\n", li->filename, li->line_num);
		}
		else
		{
			gdbio_send_cmd("-exec-until %s:%s\n", li->line_num);
		}
		gdbui_free_location_info(li);
	}
	used(btn);
}


static void
finish_click(GtkWidget * btn, gpointer user_data)
{
	gdbio_finish();
	used(btn);
}



static void
return_click(GtkWidget * btn, gpointer user_data)
{
	gdbio_return();
	used(btn);
}



static void
env_click(GtkWidget * btn, gpointer user_data)
{
	gdbio_get_env(gdbui_env_dlg);
	used(btn);
}



static void
unload_click(GtkWidget * btn, gpointer user_data)
{
	gdbio_exit();
	used(btn);
}



static void
prefs_click(GtkWidget * btn, gpointer user_data)
{
	gdbui_opts_dlg();
#ifdef STANDALONE
	gtk_window_set_keep_above(GTK_WINDOW(gdbui_setup.main_window), gdbui_opts.stay_on_top);
#endif
}



static void
entry_activate(GtkWidget * w, gpointer user_data)
{
	const gchar *txt = gtk_entry_get_text(GTK_ENTRY(w));
	if (txt && *txt)
	{
		gdbio_send_cmd("%s\n", txt);
		gtk_entry_set_text(GTK_ENTRY(w), "");
	}
	used(w);
}

#define new_row \
  w=gtk_hbox_new(TRUE,0); \
  gtk_box_pack_start(GTK_BOX(action_area),w, TRUE, TRUE, 0); \
  gtk_widget_show(w);



#define split1 split(action_area)
#define split2 split(main_vbox)


//#define splitw gtk_widget_new(GTK_TYPE_BIN,NULL)
//#define splitw gtk_hseparator_new()
#define splitw gtk_hbox_new(FALSE,0)


#define split(vb) \
gtk_box_pack_start(GTK_BOX(vb),splitw,FALSE,FALSE,3);


#define BtnGrow TRUE
#define BtnFill TRUE
#define BtnPad 1

#define MENUENTRY(A,QI,Q2,D) \
		sdm._##A = gtk_image_menu_item_new_with_mnemonic (_(Q2)); \
	gtk_widget_show (sdm._##A); \
		gtk_container_add (GTK_CONTAINER (sdm.menu), sdm._##A); \
	if (QI[0]) {\
		img=gtk_image_new_from_stock (QI, GTK_ICON_SIZE_MENU); \
		gtk_widget_show (img); \
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (sdm._##A), img); \
	} \
	ui_widget_set_tooltip_text(sdm._##A,_(D));\
	g_signal_connect(GTK_OBJECT(sdm._##A), "activate", G_CALLBACK(A##_click), NULL); \
	keybindings_set_item(plugin_key_group, KB_DEBUG_##A, kb_activate, \
		0, 0, ""#Q2, Q2 ":  " D, sdm._##A);

#define DBKEYACT(A,B,C) \
	case KB_DEBUG_##A: \
		A##_click(NULL,NULL); \
		break;

#define DBBUTTON(A,B,C,D) A##_btn = make_btn(_(C), A##_click, B, _(D));

enum
{
#define DAENTRY(A,B,C,D) KB_DEBUG_##A,
	DEBUGACTIONS
#undef DAENTRY
	KB_DEBUG_COUNT
};

PLUGIN_KEY_GROUP(debug, KB_DEBUG_COUNT)

static void kb_activate(guint key_id)
{
	//gtk_notebook_set_current_page(GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook), page_number);
	//gtk_widget_grab_focus(file_view);
	switch (key_id)
	{
#define DAENTRY(A,B,C,D) DBKEYACT(A,B,C)
	DEBUGACTIONS
#undef DAENTRY
	}
}


void gdbui_create_menu(GtkWidget * parent)
{
	GtkWidget* img;
		sdm.menu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (parent), sdm.menu);
#define DAENTRY(A,B,C,D) MENUENTRY(A,B,C,D);
	DEBUGACTIONS
#undef DAENTRY
//	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (sdm._load), GTK_STOCK_OPEN);
}

void gdbui_create_dview(GtkWidget * parent)
{

}

GtkWidget *
gdbui_create_widgets(GtkWidget * parent)
{
	GtkWidget *w = NULL;
	GtkWidget *scrollable;

	scrollable = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollable), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_show(scrollable);

	if (parent)
	{
		gtk_container_add(GTK_CONTAINER(parent), scrollable);
	}

	main_vbox = gtk_vbox_new(FALSE, 0);

	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrollable), main_vbox);

	split2 stat_lab = gtk_label_new(_("no program"));
	gtk_box_pack_start(GTK_BOX(main_vbox), stat_lab, FALSE, FALSE, 4);

	action_area = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(main_vbox), action_area, FALSE, FALSE, 0);
	gtk_widget_show(action_area);

#define DAENTRY DBBUTTON
	DEBUGACTIONS
#undef DAENTRY

	split1;
	new_row;
	gtk_box_pack_start(GTK_BOX(w), load_btn, BtnGrow, BtnFill, BtnPad);
	gtk_box_pack_start(GTK_BOX(w), unload_btn, BtnGrow, BtnFill, BtnPad);

	new_row;
	gtk_box_pack_start(GTK_BOX(w), run_btn, BtnGrow, BtnFill, BtnPad);
	gtk_box_pack_start(GTK_BOX(w), kill_btn, BtnGrow, BtnFill, BtnPad);

	split1;

	new_row;
	gtk_box_pack_start(GTK_BOX(w), pause_btn, BtnGrow, BtnFill, BtnPad);
	gtk_box_pack_start(GTK_BOX(w), cont_btn, BtnGrow, BtnFill, BtnPad);

	new_row;
	gtk_box_pack_start(GTK_BOX(w), next_btn, BtnGrow, BtnFill, BtnPad);
	gtk_box_pack_start(GTK_BOX(w), nexti_btn, BtnGrow, BtnFill, BtnPad);

	new_row;
	gtk_box_pack_start(GTK_BOX(w), step_btn, BtnGrow, BtnFill, BtnPad);
	gtk_box_pack_start(GTK_BOX(w), stepi_btn, BtnGrow, BtnFill, BtnPad);

	new_row;
	gtk_box_pack_start(GTK_BOX(w), finish_btn, BtnGrow, BtnFill, BtnPad);
	gtk_box_pack_start(GTK_BOX(w), return_btn, BtnGrow, BtnFill, BtnPad);

	new_row;
	gtk_box_pack_start(GTK_BOX(w), until_btn, BtnGrow, BtnFill, BtnPad);
	gtk_box_pack_start(GTK_BOX(w), stack_btn, BtnGrow, BtnFill, BtnPad);

	split1;

	new_row;
	gtk_box_pack_start(GTK_BOX(w), watch_btn, BtnGrow, BtnFill, BtnPad);
	gtk_box_pack_start(GTK_BOX(w), break_btn, BtnGrow, BtnFill, BtnPad);

	new_row;
	gtk_box_pack_start(GTK_BOX(w), env_btn, BtnGrow, BtnFill, BtnPad);
	gtk_box_pack_start(GTK_BOX(w), prefs_btn, BtnGrow, BtnFill, BtnPad);

	split1;

	w = main_vbox;
	term_chk = gtk_check_button_new_with_label(_("Run in terminal"));
	gtk_box_pack_start(GTK_BOX(w), term_chk, FALSE, FALSE, 0);
	gdbui_set_tip(term_chk, _("Execute target program inside a terminal window."));


	pipe_chk = gtk_check_button_new_with_label(_("Ignore SIGPIPE"));
	gdbui_set_tip(pipe_chk,
			  _("Don't pause execution when target gets a SIGPIPE signal.\n"
			  "(Useful for certain networking applications.)"));
	gtk_box_pack_start(GTK_BOX(w), pipe_chk, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(pipe_chk), "clicked", G_CALLBACK(pipe_click), NULL);

	split2;

	con_lab = gtk_label_new(_("Console:"));
	gdbui_set_tip(con_lab, _("Send a GDB command directly to the debugger."));
	gtk_misc_set_alignment(GTK_MISC(con_lab), 0.0f, 0.0f);
	gtk_box_pack_start(GTK_BOX(main_vbox), con_lab, FALSE, FALSE, 0);

	con_cmd = gtk_entry_new();
	g_signal_connect(G_OBJECT(con_cmd), "activate", G_CALLBACK(entry_activate), NULL);
	gtk_box_pack_start(GTK_BOX(main_vbox), con_cmd, FALSE, FALSE, 0);

	gtk_widget_show_all(main_vbox);

	gdbio_setup.error_func = err_func;
	gdbio_setup.signal_func = signal_func;
	gdbio_setup.status_func = status_func;
	gdbio_setup.step_func = step_func;
	gdbio_setup.info_func = info_func;
	last_used = load_btn;
	status_func(curr_status);

	return main_vbox;
}





#ifdef STANDALONE

static void
quit()
{
	gdbio_exit();
	g_printerr("\n");
	gtk_main_quit();
}



static void
quit_click(GtkWidget * btn, gpointer user_data)
{
	quit();
}



gint
main(gint argc, gchar * argv[])
{
	GtkWidget *quit_btn;
	GtkWidget *vbox;
	gtk_init(&argc, &argv);
	gdbui_setup.main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(gdbui_setup.main_window), "debug");
	g_signal_connect(G_OBJECT(gdbui_setup.main_window), "destroy", quit, NULL);
	vbox = gdbui_create_widgets();
	gtk_box_pack_start(GTK_BOX(vbox), gtk_hseparator_new(), TRUE, TRUE, 4);
	quit_btn = make_btn(_("_Quit"), quit_click, _("Exit everything"));
	gtk_box_pack_start(GTK_BOX(vbox), quit_btn, TRUE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER(gdbui_setup.main_window), vbox);
	gtk_widget_show_all(gdbui_setup.main_window);
	gtk_main();
	gdbui_opts_done();
	return 0;
}

#endif /* STANDALONE */
