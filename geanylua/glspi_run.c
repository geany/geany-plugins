
/*
 * glspi_run.c - This file is part of the Lua scripting plugin for the Geany IDE
 * See the file "geanylua.c" for copyright information.
 */

#define NEED_FAIL_ARG_TYPE
#include "glspi.h"


static KeyfileAssignFunc glspi_kfile_assign=NULL;


/*
	If a script gets caught in a tight loop and the timeout expires,
	and the user confirms they want to keep waiting, for some reason
	the normal methods for repainting the window don't work in the
	editor window, which makes it appear as if the dialog is still
	active. So we need to tell scintilla to paint over the spot
	where the dialog was.
*/
static void repaint_scintilla(void)
{
	GeanyDocument* doc=document_get_current();
	if ( doc && doc->is_valid ) {
		gdk_window_invalidate_rect(gtk_widget_get_window(GTK_WIDGET(doc->editor->sci)), NULL, TRUE);
		gdk_window_process_updates(gtk_widget_get_window(GTK_WIDGET(doc->editor->sci)), TRUE);
	}
}



/* Internal yes-or-no question box (not used by scripts) */
static gboolean glspi_show_question(const gchar*title, const gchar*question, gboolean default_result)
{
	GtkWidget *dialog, *yes_btn, *no_btn;
	GtkResponseType dv, rv;
	dv=default_result?GTK_RESPONSE_YES:GTK_RESPONSE_NO;
	dialog=gtk_message_dialog_new(GTK_WINDOW(main_widgets->window),
		GTK_DIALOG_DESTROY_WITH_PARENT|GTK_DIALOG_MODAL,
			GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE, "%s", title);
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", question);
	yes_btn=gtk_dialog_add_button(GTK_DIALOG(dialog),
		GTK_STOCK_YES, GTK_RESPONSE_YES);
	no_btn=gtk_dialog_add_button(GTK_DIALOG(dialog),
		GTK_STOCK_NO, GTK_RESPONSE_NO);
	gtk_widget_grab_default(dv==GTK_RESPONSE_YES?yes_btn:no_btn);
	gtk_window_set_title(GTK_WINDOW(dialog), DEFAULT_BANNER);
	rv=gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	if ((rv!=GTK_RESPONSE_YES)&&(rv!=GTK_RESPONSE_NO)) {rv=dv;}
	repaint_scintilla();
	return GTK_RESPONSE_YES==rv;
}


static gboolean glspi_goto_error(const gchar *fn, gint line)
{
	GeanyDocument *doc=document_open_file(fn, FALSE, NULL, NULL);
	if (doc) {
		if (doc->is_valid) {
			ScintillaObject*sci=doc->editor->sci;
			if (sci) {
				gint pos=sci_get_position_from_line(sci,line-1);
				sci_set_current_position(sci,pos,TRUE);
				return TRUE;
			}
		}
	}
	return FALSE;
}



/*
	Display a message box showing any script error...
	Depending on the type of error, Lua will sometimes prepend the filename
	to the message. If need_name is TRUE then we assume that Lua didn't add
	the filename, so we prepend it ourself. If need_name is FALSE, then the
	error message likely contains a filename *and* a line number, so we
	give the user an option to automatically open the file and scroll to
	the offending line.
*/
static void glspi_script_error(const gchar *script_file, const gchar *msg, gboolean need_name, gint line)
{
	GtkWidget *dialog;
	if (need_name) {
		dialog=gtk_message_dialog_new(GTK_WINDOW(main_widgets->window),
			GTK_DIALOG_DESTROY_WITH_PARENT|GTK_DIALOG_MODAL,
			GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, _("Lua script error:"));
		gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
				"%s:\n%s", script_file, msg);
	} else {
		GtkWidget *open_btn;
		dialog=gtk_message_dialog_new(GTK_WINDOW(main_widgets->window),
			GTK_DIALOG_DESTROY_WITH_PARENT|GTK_DIALOG_MODAL,
			GTK_MESSAGE_ERROR, GTK_BUTTONS_NONE, _("Lua script error:"));
		gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", msg);
		gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
		open_btn=gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT);
		gtk_widget_grab_default(open_btn);
	}
	gtk_window_set_title(GTK_WINDOW(dialog), DEFAULT_BANNER);
	if ( (gtk_dialog_run(GTK_DIALOG(dialog))==GTK_RESPONSE_ACCEPT) && !need_name) {
		glspi_goto_error(script_file, line);
	}
	gtk_widget_destroy(dialog);
}





typedef struct _StateInfo {
	lua_State *state;
	GString *source;
	gint line;
	GTimer*timer;
	gint counter;
	gdouble remaining;
	gdouble max;
	gboolean optimized;
} StateInfo;

static GSList *state_list=NULL;


static StateInfo*find_state(lua_State *L)
{
	GSList*p=state_list;
	for (p=state_list; p; p=p->next) {
		if ( p->data && ((StateInfo*)p->data)->state==L ) { return p->data; }
	}
	return NULL;
}


static gchar *glspi_get_error_info(lua_State* L, gint *line)
{
	StateInfo*si=find_state(L);
	if (si) {
		*line=si->line;
		if (si->source->str && *(si->source->str) )	{
			return g_strdup(si->source->str);
		}
	} else { *line=-1; }
	return NULL;
}



static gint glspi_timeout(lua_State* L)
{
	if (( lua_gettop(L) > 0 ) && lua_isnumber(L,1)) {
		gint n=lua_tonumber(L,1);
		if (n>=0) {
			StateInfo*si=find_state(L);
			if (si) {
				si->max=n;
				si->remaining=n;
			}
		} else { return FAIL_UNSIGNED_ARG(1); }
	} else { return FAIL_NUMERIC_ARG(1); }
	return 0;
}



static gint glspi_yield(lua_State* L)
{
	while (gtk_events_pending()) { gtk_main_iteration(); }
	return 0;
}


static gint glspi_optimize(lua_State* L)
{
	StateInfo*si=find_state(L);
	if (si) { si->optimized=TRUE; }
	return 0;
}


/* Lua debug hook callback */
static void debug_hook(lua_State *L, lua_Debug *ar)
{
	StateInfo*si=find_state(L);
	if (si && !si->optimized) {
		if (lua_getinfo(L,"Sl",ar)) {
			if (ar->source && (ar->source[0]=='@') && strcmp(si->source->str, ar->source+1)) {
				g_string_assign(si->source, ar->source+1);
			}
			si->line=ar->currentline;
		}
		if (si->timer) {
			if (si->timer && si->max && (g_timer_elapsed(si->timer,NULL)>si->remaining)) {
				if ( glspi_show_question(_("Script timeout"), _(
					"A Lua script seems to be taking excessive time to complete.\n"
					"Do you want to continue waiting?"
				), FALSE) )
				{
					si->remaining=si->max;
					g_timer_start(si->timer);
				} else
				{
					lua_pushstring(L, _("Script timeout exceeded."));
					lua_error(L);
				}
			}
		}
		if (si->counter > 100000) {
			gdk_window_invalidate_rect(gtk_widget_get_window(main_widgets->window), NULL, TRUE);
			gdk_window_process_updates(gtk_widget_get_window(main_widgets->window), TRUE);
			si->counter=0;
		} else si->counter++;
	}
}



/*
	Pause the run timer, while dialogs are displayed. Note that we
	purposely add 1/10 of a second to our elapsed time here.
	That should not even be noticeable for most scripts, but
	it helps us time-out faster for dialogs caught in a loop.
*/

static void glspi_pause_timer(gboolean pause, gpointer user_data)
{
	StateInfo*si=find_state((lua_State*)user_data);
	if (si && si->timer) {
		if (pause) {
			si->remaining -= g_timer_elapsed(si->timer,NULL) + 0.10;
			if ( si->remaining < 0 ) si->remaining = 0;
			g_timer_stop(si->timer);
		} else {
			g_timer_start(si->timer);
		}
	}
}




static lua_State *glspi_state_new(void)
{
	lua_State *L = luaL_newstate();
	StateInfo*si=g_new0(StateInfo,1);
	luaL_openlibs(L);
	si->state=L;
	si->timer=g_timer_new();
	si->max=DEFAULT_MAX_EXEC_TIME;
	si->remaining=DEFAULT_MAX_EXEC_TIME;
	si->source=g_string_new("");
	si->line=-1;
	si->counter=0;
	state_list=g_slist_append(state_list,si);
	lua_sethook(L,debug_hook,LUA_MASKLINE,1);
	return L;
}


static void glspi_state_done(lua_State *L)
{
	StateInfo*si=find_state(L);
	if (si) {
		if (si->timer) {
			g_timer_destroy(si->timer);
			si->timer=NULL;
		}
		if (si->source) {
			g_string_free(si->source, TRUE);
		}
		state_list=g_slist_remove(state_list,si);
		g_free(si);
	}
	lua_close(L);
}



static const struct luaL_Reg glspi_timer_funcs[] = {
	{"timeout",  glspi_timeout},
	{"yield",    glspi_yield},
	{"optimize", glspi_optimize},
	{NULL,NULL}
};





/* Catch and report script errors */
static gint glspi_traceback(lua_State *L)
{
	lua_getfield(L, LUA_GLOBALSINDEX, "debug");
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		return 1;
	}
	lua_getfield(L, -1, "traceback");
	if (!lua_isfunction(L, -1)) {
		lua_pop(L, 2);
		return 1;
	}
	lua_pushvalue(L, 1);
	lua_pushinteger(L, 2);
	lua_call(L, 2, 1);

	return 1;
}

/*
	The set_*_token functions assign default values for module-level variables
*/

static void set_string_token(lua_State *L, const gchar*name, const gchar*value)
{
	lua_getglobal(L, LUA_MODULE_NAME);
	if (lua_istable(L, -1)) {
		lua_pushstring(L,name);
		lua_pushstring(L,value);
		lua_settable(L, -3);
	} else {
		g_printerr("*** %s: Failed to set value for %s\n", PLUGIN_NAME, name);
	}
}



static void set_numeric_token(lua_State *L, const gchar*name, gint value)
{
	lua_getglobal(L, LUA_MODULE_NAME);
	if (lua_istable(L, -1)) {
		lua_pushstring(L,name);
		push_number(L,value);
		lua_settable(L, -3);
	} else {
		g_printerr("*** %s: Failed to set value for %s\n", PLUGIN_NAME, name);
	}
}



static void set_boolean_token(lua_State *L, const gchar*name, gboolean value)
{
	lua_getglobal(L, LUA_MODULE_NAME);
	if (lua_istable(L, -1)) {
		lua_pushstring(L,name);
		lua_pushboolean(L,value);
		lua_settable(L, -3);
	} else {
		g_printerr("*** %s: Failed to set value for %s\n", PLUGIN_NAME, name);
	}
}



static void set_keyfile_token(lua_State *L, const gchar*name, GKeyFile* value)
{
	if (!value) {return;}
	lua_getglobal(L, LUA_MODULE_NAME);
	if (lua_istable(L, -1)) {
		lua_pushstring(L,name);
		glspi_kfile_assign(L, value);
		lua_settable(L, -3);
	} else {
		g_printerr("*** %s: Failed to set value for %s\n", PLUGIN_NAME, name);
	}
}



static void show_error(lua_State *L, const gchar *script_file)
{
	gint line=-1;
	gchar *fn = glspi_get_error_info(L, &line);
	if (!lua_isnil(L, -1)) {
		const gchar *msg;
		msg = lua_tostring(L, -1);
		if (msg == NULL) {
			msg = _("(error object is not a string)");
		}
		glspi_script_error(fn?fn:script_file, msg, FALSE, line);
		lua_pop(L, 1);
	} else {
		glspi_script_error(fn?fn:script_file, _("Unknown Error inside script."), FALSE, line);
	}
	if (fn) g_free(fn);
}



static gint glspi_init_module(lua_State *L, const gchar *script_file, gint caller, GKeyFile*proj, const gchar*script_dir)
{
	luaL_register(L, LUA_MODULE_NAME, glspi_timer_funcs);
	glspi_init_sci_funcs(L);
	glspi_init_doc_funcs(L);
	glspi_init_mnu_funcs(L);
	glspi_init_dlg_funcs(L, glspi_pause_timer);
	glspi_init_app_funcs(L,script_dir);
	set_string_token(L,tokenWordChars,GEANY_WORDCHARS);
	set_string_token(L,tokenBanner,DEFAULT_BANNER);
	set_string_token(L,tokenDirSep, G_DIR_SEPARATOR_S);
	set_boolean_token(L,tokenRectSel,FALSE);
	set_numeric_token(L,tokenCaller, caller);
	glspi_init_gsdlg_module(L,glspi_pause_timer, geany_data?GTK_WINDOW(main_widgets->window):NULL);
	glspi_init_kfile_module(L,&glspi_kfile_assign);
	set_keyfile_token(L,tokenProject, proj);
	set_string_token(L,tokenScript,script_file);
	return 0;
}



/*
	Function to load this module into the standalone lua interpreter.
	The only reason you would ever want to do this is to re-generate
	the "keywords.list" file from the command line.
	See the file "util/keywords.lua" for more info.
*/
PLUGIN_EXPORT
gint luaopen_libgeanylua(lua_State *L)
{
	return glspi_init_module(L, "", 0, NULL, NULL);
}



/* Load and run the script */
void glspi_run_script(const gchar *script_file, gint caller, GKeyFile*proj, const gchar *script_dir)
{
	gint status;
	lua_State *L = glspi_state_new();
	glspi_init_module(L, script_file, caller,proj,script_dir);
#if 0
	while (gtk_events_pending()) { gtk_main_iteration(); }
#endif
	status = luaL_loadfile(L, script_file);
	switch (status) {
	case 0: {
		gint base = lua_gettop(L); /* function index */
		lua_pushcfunction(L, glspi_traceback);	/* push traceback function */
		lua_insert(L, base); /* put it under chunk and args */
		status = lua_pcall(L, 0, 0, base);
		lua_remove(L, base); /* remove traceback function */
		if (0 == status) {
			status = lua_pcall(L, 0, 0, 0);
		} else {
			lua_gc(L, LUA_GCCOLLECT, 0); /* force garbage collection if error */
			show_error(L, script_file);
		}
		break;
	}
	case LUA_ERRSYNTAX:
		show_error(L, script_file);
		break;
	case LUA_ERRMEM:
		glspi_script_error(script_file, _("Out of memory."), TRUE, -1);
		break;
	case LUA_ERRFILE:
		glspi_script_error(script_file, _("Failed to open script file."), TRUE, -1);
		break;
	default:
		glspi_script_error(script_file, _("Unknown error while loading script file."), TRUE, -1);
	}
	glspi_state_done(L);
}

