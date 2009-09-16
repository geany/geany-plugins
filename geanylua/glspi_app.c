
/*
 * glspi_app.c - This file is part of the Lua scripting plugin for the Geany IDE
 * See the file "geanylua.c" for copyright information.
 */

#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>


#define NEED_FAIL_ARG_TYPE
#include "glspi.h"


static gint glspi_pluginver(lua_State* L)
{
	lua_pushfstring(L, _(
"%s %s: %s\n"
"Copyright (c) 2007-2008 "PLUGIN_AUTHOR", et al.\n"
"Compiled on "__DATE__" at "__TIME__" for Geany API version %d\n"
"Released under version 2 of the GNU General Public License.\n"
	),
			PLUGIN_NAME, PLUGIN_VER, PLUGIN_DESC, MY_GEANY_API_VER);
	return 1;
}


static gint glspi_tools(lua_State* L)
{
	lua_newtable(L);
	SetTableStr("browser", geany_data->tool_prefs->browser_cmd);
	SetTableStr("term",    geany_data->tool_prefs->term_cmd);
	SetTableStr("grep",    geany_data->tool_prefs->grep_cmd);
	SetTableStr("action",  geany_data->tool_prefs->context_action_cmd);
	return 1;
}


static gint glspi_template(lua_State* L)
{
	lua_newtable(L);
	SetTableStr("developer", geany_data->template_prefs->developer);
	SetTableStr("company",   geany_data->template_prefs->company);
	SetTableStr("mail",      geany_data->template_prefs->mail);
	SetTableStr("initial",   geany_data->template_prefs->initials);
	SetTableStr("version",   geany_data->template_prefs->version);
	return 1;
}



static gint glspi_project(lua_State* L)
{
	GeanyProject *project = geany->app->project;

	if (project) {
		lua_newtable(L);
		SetTableStr("name", project->name);
		SetTableStr("desc", project->description);
		SetTableStr("file", project->file_name);
		SetTableStr("base", project->base_path);
		if (project->file_patterns && *project->file_patterns) {
			gchar *tmp=g_strjoinv(";", project->file_patterns);
			SetTableStr("mask", tmp);
			g_free(tmp);
		}
		return 1;
	} else {
		return 0;
	}
}

static gchar *glspi_script_dir = NULL;

static gint glspi_appinfo(lua_State* L)
{
	GeanyApp *app = geany->app;

	lua_newtable(L);
	SetTableBool("debug", app->debug_mode);
	SetTableStr("configdir", app->configdir);
	SetTableStr("datadir", app->datadir);
	SetTableStr("docdir", app->docdir);
	SetTableStr("scriptdir", glspi_script_dir);
	lua_pushstring(L,"template");
	glspi_template(L);
	lua_rawset(L,1);

	lua_pushstring(L,"tools");
	glspi_tools(L);
	lua_rawset(L,1);

	if (app->project) {
		lua_pushstring(L,"project");
		glspi_project(L);
		lua_rawset(L,1);
	}
	return 1;
}



#ifndef G_OS_WIN32

#define CLIPBOARD gtk_clipboard_get(GDK_SELECTION_PRIMARY)

static gint glspi_xsel(lua_State* L)
{
	if (lua_gettop(L)>0) {
		if (lua_isstring(L,1)) {
			guint len;
			const gchar*txt=lua_tolstring(L,1,&len);
			gtk_clipboard_set_text(CLIPBOARD,txt,len);
		} else {
			FAIL_STRING_ARG(1);
		}
		return 0;
	} else {
		gchar *txt=gtk_clipboard_wait_for_text(CLIPBOARD);
		if (txt) {
			lua_pushstring(L,txt);
			g_free(txt);
		} else {
			lua_pushstring(L,"");
		}
		return 1;
	}
}

#else
static gint glspi_xsel(lua_State* L) { return 0; }
#endif




static gint glspi_signal(lua_State* L) {
	const gchar*widname,*signame;
	GtkWidget*w;
	GType typeid;
	guint sigid;
	if ((lua_gettop(L)<2)||!lua_isstring(L,2) ) {	return FAIL_STRING_ARG(2); }
	if (!lua_isstring(L,1) ) {	return FAIL_STRING_ARG(1); }
	widname=lua_tostring(L,1);
	signame=lua_tostring(L,2);
	w=ui_lookup_widget(main_widgets->window, widname);
	if (!w) {
		lua_pushfstring(L, _("Error in module \"%s\" at function %s():\n"
			"widget \"%s\"  not found for argument #1.\n "),
			LUA_MODULE_NAME, &__FUNCTION__[6], widname);
			lua_error(L);
		return 0;
	}
	typeid=G_OBJECT_TYPE(w);
	sigid=g_signal_lookup(signame,typeid);
	if (!sigid) {
		lua_pushfstring(L, _("Error in module \"%s\" at function %s() argument #2:\n"
			"widget \"%s\" has no signal named \"%s\".\n "),
			LUA_MODULE_NAME, &__FUNCTION__[6], widname, signame);
			lua_error(L);
		return 0;
	}

	g_signal_emit(w, sigid, 0);
	return 0;
}




#ifdef G_OS_WIN32
#define lstat stat
#define realpath(src,dst) _fullpath((dst),(src),_MAX_PATH)
#include <io.h>
#else
#include <unistd.h>
#endif

typedef int (*statfunc) (const char *fn, struct stat *st);

static gint glspi_stat(lua_State* L)
{
	statfunc sf=stat;
	const gchar*fn=NULL;
	struct stat st;
	if (lua_gettop(L)<1) { return FAIL_STRING_ARG(1); }
	if (lua_gettop(L)>=2) {
		if (!lua_isboolean(L,2)) { return FAIL_BOOL_ARG(2); }
		sf=lua_toboolean(L,2)?lstat:stat;
	}
	if (!lua_isstring(L,1)) { return FAIL_STRING_ARG(1); }
	fn=lua_tostring(L,1);
	if (sf(fn,&st)==0) {
		gchar *ft=NULL;
		switch ( st.st_mode & S_IFMT) {
			case S_IFBLK:ft="b"; break;
			case S_IFCHR:ft="c"; break;
			case S_IFDIR:ft="d"; break;
			case S_IFIFO:ft="f"; break;
			case S_IFREG:ft="r"; break;
#ifndef G_OS_WIN32
			case S_IFLNK:ft="l"; break;
			case S_IFSOCK:ft="s"; break;
#endif
		}
		lua_newtable(L);
		SetTableNum("size",st.st_size);
		SetTableNum("time",st.st_mtime);
		SetTableStr("type",ft);
		SetTableBool("read", (access(fn,R_OK)==0));
		SetTableBool("write", (access(fn,W_OK)==0));
		SetTableBool("exec", (access(fn,X_OK)==0));
		return 1;
	}
	lua_pushnil(L);
	lua_pushstring(L, strerror(errno));
	return 2;
}


static gint glspi_basename(lua_State* L)
{
	if (lua_gettop(L)>=1) {
		gchar *bn=NULL;
		const gchar *fn=NULL;
		if (!lua_isstring(L,1)) { return FAIL_STRING_ARG(1); }
		fn=lua_tostring(L,1);
		bn=g_path_get_basename(fn);
		lua_pushstring(L,bn);
		g_free(bn);
		return 1;
	}
	return 0;
}


static gint glspi_dirname(lua_State* L)
{
	if (lua_gettop(L)>=1) {
		gchar *dn=NULL;
		const gchar *fn=NULL;
		if (!lua_isstring(L,1)) { return FAIL_STRING_ARG(1); }
		fn=lua_tostring(L,1);
		dn=g_path_get_dirname(fn);
		lua_pushstring(L,dn);
		g_free(dn);
		return 1;
	}
	return 0;
}




static gint glspi_fullpath(lua_State* L)
{
	if (lua_gettop(L)>=1) {
		gchar *rp=NULL;
		const gchar *fn=NULL;
		if (!lua_isstring(L,1)) { return FAIL_STRING_ARG(1); }
		fn=lua_tostring(L,1);
		rp=realpath(fn,NULL);
		if (rp) {
			lua_pushstring(L,rp);
			free(rp);
			return 1;
		}
	}
	return 0;
}



static gint dirlist_closure(lua_State *L)
{
	GDir*dir=lua_touserdata(L,lua_upvalueindex(1));
	const gchar*entry=g_dir_read_name(dir);
	if (entry) {
		lua_pushstring(L,entry);
		return 1;
	} else {
		g_dir_close(dir);
		return 0;
	}
}



static gint glspi_dirlist(lua_State* L)
{
	GDir*dir=NULL;
	const gchar*dn=".";
	GError*err=NULL;
	if (lua_gettop(L)>=1) {
		if (!lua_isstring(L,1)) { return FAIL_STRING_ARG(1); }
		dn=lua_tostring(L,1);
	}
	dir=g_dir_open(dn,0,&err);
	if (dir) {
		lua_pushlightuserdata(L,dir);
		lua_pushcclosure(L,&dirlist_closure,1);
		return 1;
	} else {
		lua_pushfstring(L, "Error in module \"%s\" at function %s() argument #2\n%s",
		LUA_MODULE_NAME, &__FUNCTION__[6],err?err->message:"Error reading directory."
		);
		if (err) {g_error_free(err);}
		lua_error(L);
		return 0;
	}
	return 0;
}



static gint glspi_wkdir(lua_State* L)
{
	if (lua_gettop(L)== 0 ) {
		gchar*wd=getcwd(NULL,0);
		if (wd) {
			lua_pushstring(L,wd);
			free(wd);
			return 1;
		} else { return 0; }
	} else {
		if (!lua_isstring(L,1)) { return FAIL_STRING_ARG(1); }
		if ( chdir(lua_tostring(L,1)) == 0 ) {
			lua_pushboolean(L,TRUE);
			return 1;
		} else {
			lua_pushboolean(L,FALSE);
			lua_pushstring(L, strerror(errno));
			return 2;
		}
	}
}


#include "glspi_keycmd.h"

static GHashTable*key_cmd_hash=NULL;

static void glspi_init_key_cmd_hash(void)
{
	gint i;
	key_cmd_hash=g_hash_table_new(g_str_hash,g_str_equal);
	for (i=0;key_cmd_hash_entries[i].name; i++) {
		g_hash_table_insert(
			key_cmd_hash,key_cmd_hash_entries[i].name,&key_cmd_hash_entries[i]);
	}
}


static void glspi_free_key_cmd_hash(void) {
	if (key_cmd_hash) {
		g_hash_table_destroy(key_cmd_hash);
		key_cmd_hash=NULL;
	}
}

void glspi_set_key_cmd_hash(gboolean create) {
	if (create) {
		glspi_init_key_cmd_hash ();
	} else {
		glspi_free_key_cmd_hash();
	}
}



#define lookup_key_cmd_str(cmd) g_hash_table_lookup(key_cmd_hash,cmd);


static gint glspi_keycmd(lua_State* L)
{
	KeyCmdHashEntry*he=NULL;
	if (lua_gettop(L)<1) {return FAIL_STRING_ARG(1); }
	if (lua_isstring(L,1)) {
		gchar cmdbuf[64];
		gchar *cmdname;
		gint i;
		memset(cmdbuf,'\0', sizeof(cmdbuf));
		strncpy(cmdbuf,lua_tostring(L,1),sizeof(cmdbuf)-1);
		for (i=0;cmdbuf[i];i++) {cmdbuf[i]=g_ascii_toupper(cmdbuf[i]);}
		cmdname=cmdbuf;
		if (strncmp(cmdname,"GEANY_",6)==0) {
			cmdname+=6;
			if (strncmp(cmdname,"KEYS_",5)==0) {
				cmdname+=5;
			}
		}
		he=lookup_key_cmd_str(cmdname);
	} else { return FAIL_STRING_ARG(1); }
	if ( !he ) {
		lua_pushfstring(
			L, _( "Error in module \"%s\" at function %s():\n"
				"unknown command \"%s\" given for argument #1.\n"),
			LUA_MODULE_NAME, &__FUNCTION__[6], lua_tostring(L,1));
		lua_error(L);
		return 0;
	}
	keybindings_send_command(he->group, he->key_id);
	return 0;
}


static gint glspi_launch(lua_State* L)
{
	gint argc=lua_gettop(L);
	gint i;
	gchar **argv=NULL;
	gboolean rv;
	GError *err=NULL;
	if (argc==0) { return FAIL_STRING_ARG(1); }
	for (i=1;i<=argc;i++) {
		if (!lua_isstring(L,i)) { return FAIL_STRING_ARG(i); }
	}
	argv=g_malloc0(sizeof(gchar *)*argc+1);
	for (i=0;i<argc;i++) {
		argv[i]=(g_strdup(lua_tostring(L,i+1)));
	}
	rv=g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &err);
	g_strfreev(argv);
	lua_pushboolean(L,rv);
	if (rv) { return 1; }
	lua_pushstring(L,err->message);
	g_error_free(err);
	return 2;
}


static guint My_Shift_L=0;
static guint My_Shift_R=0;
static guint My_Control_L=0;
static guint My_Control_R=0;
static guint My_Alt_L=0;
static guint My_Alt_R=0;


#ifndef G_OS_WIN32

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <sys/time.h>


#define IsShift ( (My_Shift_L == ev->xkey.keycode) || (My_Shift_R == ev->xkey.keycode) )

#define IsCtrl ( (My_Control_L == ev->xkey.keycode) || (My_Control_R == ev->xkey.keycode) )
#define IsAlt ( (My_Alt_L == ev->xkey.keycode) || (My_Alt_R == ev->xkey.keycode) )

#define IsCtrlAlt ( IsCtrl || IsAlt )

typedef struct _KeyGrabData {
gchar *prompt;
GdkKeymapKey km;
} _KeyGrabData;


static GdkFilterReturn keygrab_cb(GdkXEvent *xevent, GdkEvent *event, gpointer data)
{
	XEvent*ev = (XEvent*) xevent;
	GdkKeymapKey *km = (GdkKeymapKey*) data;
	switch (ev->type) {
		case KeyPress:{
			if (IsShift) {
				km->level=1;
			} else {
				if (!IsCtrlAlt) km->group=1; /* Flag to know we have keydown before keyup */
			}
			return GDK_FILTER_REMOVE;
		}
		case KeyRelease:{
			if (IsShift) {
				km->level=0;
			} else {
				if ((km->group==1)&&(!IsCtrlAlt)) { /* OK, we already got our keydown  */
					km->group=2;
					km->level=(ev->xkey.state & ShiftMask)?1:0;
					km->keycode=ev->xkey.keycode;
				}
			}
			return GDK_FILTER_REMOVE;
		}
		default:{}
	}
	return GDK_FILTER_CONTINUE;
}

static void dosleep(void)
{
	struct timespec req, rem;
	req.tv_sec=0;
	req.tv_nsec=1000;
	nanosleep(&req, &rem);
}

#else
#include <windows.h>
#define dosleep() Sleep(1)

#define IsShift ( (My_Shift_L == msg->wParam) || (My_Shift_R == msg->wParam) )

#define IsCtrl ( (My_Control_L == msg->wParam) || (My_Control_R == msg->wParam) )
#define IsAlt ( (My_Alt_L == msg->wParam) || (My_Alt_R == msg->wParam) )

#define IsCtrlAlt ( IsCtrl || IsAlt )


static GdkFilterReturn keygrab_cb(GdkXEvent *xevent, GdkEvent *event, gpointer data)
{
	MSG*msg = (MSG*) xevent;
	GdkKeymapKey *km = (GdkKeymapKey*) data;
	switch (msg->message) {
		case WM_KEYDOWN:{
			if (IsShift) {
				km->level=1;
			} else {
				if (!IsCtrlAlt) km->group=1; /* Flag to know we have keydown before keyup */
			}
			return GDK_FILTER_REMOVE;
		}
		case WM_KEYUP:{
			if (IsShift) {
				km->level=0;
			} else {
				if ((km->group==1)&&(!IsCtrlAlt)) { /* OK, we already got our keydown  */
					km->group=2;
					km->level=HIBYTE(GetKeyState(VK_SHIFT))?1:0;
					km->keycode=msg->wParam;
				}
			}
			return GDK_FILTER_REMOVE;
		}
		default:{}
	}
	return GDK_FILTER_CONTINUE;
}


#endif


#include <gdk/gdkkeysyms.h>
static gint init_key(guint keyval){
	GdkKeymapKey *kmk=NULL;
	gint n_keys=0;
	gint rv=0;
	if (gdk_keymap_get_entries_for_keyval(NULL,keyval,&kmk,&n_keys)) {
		rv=kmk[0].keycode;
		g_free(kmk);
	}
	return rv;
}

#define InitKey(code,value) if (!code) { code=init_key(value); }

static gint glspi_keygrab(lua_State* L)
{
	GeanyDocument*doc=NULL;
	const gchar*prompt=NULL;
	GdkKeymapKey km={0,0,0};
	km.keycode=0;
	km.group=0; /* Note: we hijack this field to use as a flag for first keydown. */
	km.level=0;
	InitKey(My_Shift_L, GDK_Shift_L);
	InitKey(My_Shift_R, GDK_Shift_R);
	InitKey(My_Control_L, GDK_Control_L);
	InitKey(My_Control_R, GDK_Control_R);
	InitKey(My_Alt_L, GDK_Alt_L);
	InitKey(My_Alt_R, GDK_Alt_R);
	if (lua_gettop(L)>0) {
		if (!lua_isstring(L,1)) {return FAIL_STRING_ARG(1); }
		prompt=lua_tostring(L,1);
		doc=document_get_current();
	}

	if (prompt && doc && doc->is_valid ) {
		gint fvl=scintilla_send_message(doc->editor->sci,SCI_GETFIRSTVISIBLELINE, 0,0);
		gint pos=sci_get_position_from_line(doc->editor->sci, fvl+1);
		scintilla_send_message(doc->editor->sci,SCI_CALLTIPSHOW,pos+3, (gint)prompt);
	}
	gdk_window_add_filter(main_widgets->window->window, keygrab_cb, &km);
	do {
		while (gtk_events_pending()) {
			if (km.group==2) { break; }
			gtk_main_iteration();
		}
		if (km.group==2) { break; }
		dosleep();
	} while (km.group!=2);

	gdk_window_remove_filter(main_widgets->window->window, keygrab_cb, &km);
	if (prompt && doc && doc->is_valid) {
	sci_send_command(doc->editor->sci, SCI_CALLTIPCANCEL);
	}
	km.group=0; /* reset the hijacked flag before passing to GDK */
	lua_pushstring(L, gdk_keyval_name(gdk_keymap_lookup_key(NULL, &km)));

	return 1;
}



static const struct luaL_reg glspi_app_funcs[] = {
	{"pluginver", glspi_pluginver},
	{"appinfo",   glspi_appinfo},
	{"xsel",      glspi_xsel},
	{"signal",    glspi_signal},
	{"stat",      glspi_stat},
	{"basename",  glspi_basename},
	{"dirname",   glspi_dirname},
	{"fullpath",  glspi_fullpath},
	{"dirlist",   glspi_dirlist},
	{"wkdir",     glspi_wkdir},
	{"keycmd",    glspi_keycmd},
	{"launch",    glspi_launch},
	{"keygrab",   glspi_keygrab},
	{NULL,NULL}
};

void glspi_init_app_funcs(lua_State *L, gchar*script_dir) {
	glspi_script_dir = script_dir;
	luaL_register(L, NULL,glspi_app_funcs);
}

