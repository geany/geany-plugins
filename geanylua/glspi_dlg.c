/*
 * glspi_dlg.c - This file is part of the Lua scripting plugin for the Geany IDE
 * See the file "geanylua.c" for copyright information.
 */


#include <gdk/gdkkeysyms.h>

#define NEED_FAIL_ARG_TYPE
#define NEED_FAIL_ELEM_TYPE

#include "glspi.h"

/* Use newer "generic" dialogs if available */
#if ! GTK_CHECK_VERSION(2, 10, 0)
#define GTK_MESSAGE_OTHER GTK_MESSAGE_INFO
#endif

#define DIALOG_FLAGS GTK_DIALOG_DESTROY_WITH_PARENT|GTK_DIALOG_MODAL


static GsDlgRunHook glspi_pause_timer = NULL;


static gint do_glspi_dialog_run(lua_State *L, GtkDialog *dialog)
{
	gint rv;
	glspi_pause_timer(TRUE, L);
	rv=gtk_dialog_run(dialog);
	glspi_pause_timer(FALSE, L);
	return rv;
}

#define glspi_dialog_run(d) do_glspi_dialog_run(L,d)

static void  set_dialog_title(lua_State *L, GtkWidget*dialog) {
	const gchar *banner=DEFAULT_BANNER;
	lua_getglobal(L, LUA_MODULE_NAME);
	if ( lua_istable(L, -1) ) {
		lua_pushstring(L,tokenBanner);
		lua_gettable(L, -2);
		if (lua_isstring(L, -1)) {
			banner=lua_tostring(L, -1);
		} else {
			banner=DEFAULT_BANNER;
			lua_getglobal(L, LUA_MODULE_NAME);
			lua_pushstring(L,tokenBanner);
			lua_pushstring(L,banner);
			lua_settable(L, -3);
		}
	}
	gtk_window_set_title(GTK_WINDOW(dialog), banner);
}


/*
	The GtkMessageDialog wants format strings, but we want literals.
	So we need to replace all '%' with "%%"
*/
static gchar*pct_esc(const gchar*s)
{
	gchar*rv=NULL;
	if (s && strchr(s, '%')) {
		gchar**a=g_strsplit(s,"%",-1);
		rv=g_strjoinv("%%",a);
		g_strfreev(a);
	}
	return rv;
}

static GtkWidget*new_dlg(gint msg_type, gint buttons, const gchar*msg1, const gchar*msg2)
{
	gchar *tmp=pct_esc(msg1);
	GtkWidget*rv=gtk_message_dialog_new(GTK_WINDOW(main_widgets->window),
										DIALOG_FLAGS, msg_type, buttons, "%s", tmp?tmp:msg1);

	if (tmp) {
		g_free(tmp);
		tmp=NULL;
	}
	if (msg2) {
		tmp=pct_esc(msg2);
		gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(rv), "%s", tmp?tmp:msg2);
		if (tmp) {g_free(tmp);}
	}
	return rv;
}


/* Close the list dialog with an OK response if user double-clicks an item */
/* FIXME: Make sure the click was really on an item! */
static gboolean on_tree_clicked(GtkWidget *w, GdkEventButton *e, gpointer p)
{
	if (w && p && e && (GDK_2BUTTON_PRESS==e->type) && (1==e->button)) {
		gtk_dialog_response(GTK_DIALOG(p), GTK_RESPONSE_OK);
	}
	return FALSE;
}



/* Close the list dialog with an OK response if user presses [Enter] in list */
static gboolean on_tree_key_release(GtkWidget *w, GdkEventKey *e, gpointer p)
{
	if (w && p && e && (GDK_Return==e->keyval) ) {
		gtk_dialog_response(GTK_DIALOG(p), GTK_RESPONSE_OK);
	}
	return FALSE;
}


static gint glspi_choose(lua_State* L)
{
	const gchar *arg1=NULL;
	gint i, n;
	GtkResponseType rv;
	GtkWidget*dialog, *ok_btn, *tree, *scroll;
	GtkListStore *store;
	GtkTreeIter iter;
	GtkTreeSelection *select;

	if ( (lua_gettop(L)!=2) || (!lua_istable(L,2)) ) {
		return FAIL_TABLE_ARG(2);
	}

	if (!lua_isnil(L, 1)) {
		if (!lua_isstring(L, 1))	{ return FAIL_STRING_ARG(1); }
			arg1=lua_tostring(L, 1);
	}

	n=lua_objlen(L,2);
	for (i=1;i<=n; i++) {
		lua_rawgeti(L,2,i);
		if (!lua_isstring(L, -1)) {
			return glspi_fail_elem_type(L, __FUNCTION__, 2, i, "string");
		}
		lua_pop(L, 1);
	}
	store=gtk_list_store_new(1, G_TYPE_STRING);
	for (i=1;i<=n; i++) {
		lua_rawgeti(L,2,i);
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, 0, lua_tostring(L, -1), -1);
		lua_pop(L, 1);
	}
	dialog = new_dlg(GTK_MESSAGE_OTHER, GTK_BUTTONS_NONE, arg1, NULL);
	ok_btn=gtk_dialog_add_button(GTK_DIALOG(dialog),
			GTK_STOCK_OK, GTK_RESPONSE_OK);
	gtk_dialog_add_button(GTK_DIALOG(dialog),
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	gtk_widget_grab_default(ok_btn);
	set_dialog_title(L,dialog);

	tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(tree), TRUE);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree), FALSE);
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree),
				-1, NULL, gtk_cell_renderer_text_new(), "text", 0, NULL);

	select = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree));
	gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);

	scroll=gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
		GTK_POLICY_AUTOMATIC,  GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW (scroll), GTK_SHADOW_IN);
	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),scroll);
	gtk_widget_set_vexpand(scroll, TRUE);
	gtk_container_add(GTK_CONTAINER(scroll),tree);

	gtk_widget_set_size_request(scroll, 320, 240);
	gtk_widget_show_all(dialog);
	gtk_window_set_resizable(GTK_WINDOW(dialog), TRUE);

	g_signal_connect(G_OBJECT(tree), "button-press-event",
		G_CALLBACK(on_tree_clicked), dialog);
	g_signal_connect(G_OBJECT(tree), "key-release-event",
		G_CALLBACK(on_tree_key_release), dialog);

	rv=glspi_dialog_run(GTK_DIALOG(dialog));

	if (GTK_RESPONSE_OK == rv ) {
		gchar *txt=NULL;
		GtkTreeModel *model;
		if (gtk_tree_selection_get_selected(select, &model, &iter)) {
			gtk_tree_model_get(model, &iter, 0, &txt, -1);
		}
		if (txt) {
			lua_pushstring(L, txt);
			g_free(txt);
		} else {
			lua_pushnil(L);
		}
	} else {
		lua_pushnil(L);
	}
	gtk_widget_destroy(dialog);
	return 1;
}



/* Display a simple message dialog */
static gint glspi_message(lua_State* L)
{
	const gchar *arg1=NULL;
	const gchar *arg2=NULL;
	GtkWidget *dialog;
	switch (lua_gettop(L)) {
	case 0: break;
	case 2:
		if (!lua_isstring(L, 2))	{ return FAIL_STRING_ARG(2); }
		arg2=lua_tostring(L, 2);
	default:
		if (!lua_isstring(L, 1))	{ return FAIL_STRING_ARG(1); }
		arg1=lua_tostring(L, 1);
	}
	dialog = new_dlg(GTK_MESSAGE_OTHER, GTK_BUTTONS_OK, arg1, arg2);
	set_dialog_title(L,dialog);
	glspi_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	return 0;
}




/* Display a yes-or-no dialog */
static gint glspi_confirm(lua_State* L)
{
	const gchar *arg1=NULL;
	const gchar *arg2=NULL;
	GtkWidget *dialog, *yes_btn, *no_btn;
	GtkResponseType dv, rv;

	if (lua_gettop(L)<3) {	return FAIL_BOOL_ARG(3);	}
	if (!lua_isboolean(L,3)) { return FAIL_BOOL_ARG(3); }
	dv=lua_toboolean(L,3)?GTK_RESPONSE_YES:GTK_RESPONSE_NO;
	if (!lua_isnil(L, 2)) {
		if (!lua_isstring(L, 2))	{ return FAIL_STRING_ARG(2); }
		arg2=lua_tostring(L, 2);
	}
	if (!lua_isnil(L, 1)) {
		if (!lua_isstring(L, 1))	{ return FAIL_STRING_ARG(1); }
		arg1=lua_tostring(L, 1);
	}
	dialog = new_dlg(GTK_MESSAGE_OTHER, GTK_BUTTONS_NONE, arg1,arg2);
	yes_btn=gtk_dialog_add_button(GTK_DIALOG(dialog),
		GTK_STOCK_YES, GTK_RESPONSE_YES);
	no_btn=gtk_dialog_add_button(GTK_DIALOG(dialog),
		GTK_STOCK_NO, GTK_RESPONSE_NO);
	gtk_widget_grab_default(dv==GTK_RESPONSE_YES?yes_btn:no_btn);
	/* Where I come from, we ask "yes-or-no?"
	 *  who the hell ever asks "no-or-yes?" ??? */
	/* It's probably better to use descriptive names for button text -ntrel */
	gtk_dialog_set_alternative_button_order(GTK_DIALOG(dialog),
		GTK_RESPONSE_YES,GTK_RESPONSE_NO );
	set_dialog_title(L,dialog);
	rv=glspi_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	if ((rv!=GTK_RESPONSE_YES)&&(rv!=GTK_RESPONSE_NO)) {rv=dv;}
	lua_pushboolean(L, (GTK_RESPONSE_YES==rv));
	return 1;
}



/* Display a dialog to prompt user for some text input */
static gint glspi_input(lua_State* L)
{
	const gchar *arg1=NULL;
	const gchar *arg2=NULL;
	GtkResponseType rv;
	GtkWidget*dialog, *entry, *ok_btn;

	switch (lua_gettop(L)) {
		case 0: break;
		case 2:
			if (!lua_isnil(L, 2)) {
				if (!lua_isstring(L, 2))	{ return FAIL_STRING_ARG(2); }
				arg2=lua_tostring(L, 2);
			}
		default:
			if (!lua_isnil(L, 1)) {
				if (!lua_isstring(L, 1))	{ return FAIL_STRING_ARG(1); }
				arg1=lua_tostring(L, 1);
			}
	}


	dialog = new_dlg(GTK_MESSAGE_OTHER, GTK_BUTTONS_NONE, arg1, NULL);
	ok_btn=gtk_dialog_add_button(GTK_DIALOG(dialog),
			GTK_STOCK_OK, GTK_RESPONSE_OK);
	gtk_dialog_add_button(GTK_DIALOG(dialog),
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	gtk_widget_grab_default(ok_btn);
	entry=gtk_entry_new();
	if (arg2) { gtk_entry_set_text(GTK_ENTRY(entry), arg2); }
	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), entry);
	gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
	set_dialog_title(L,dialog);
	gtk_widget_set_size_request(entry, 320, -1);
	gtk_widget_show_all(dialog);
	gtk_window_set_resizable(GTK_WINDOW(dialog), TRUE);

	rv=glspi_dialog_run(GTK_DIALOG(dialog));

	if (GTK_RESPONSE_OK == rv ) {
		const gchar *s=gtk_entry_get_text(GTK_ENTRY(entry));
		if (s) {
			lua_pushstring(L, s);
		} else {
			lua_pushnil(L);
		}
	} else {
		lua_pushnil(L);
	}
	gtk_widget_destroy(dialog);
	return 1;
}



#define NEED_OVERWRITE_PROMPT !GTK_CHECK_VERSION(2, 8, 0)


#if NEED_OVERWRITE_PROMPT
static void on_file_dlg_response(GtkWidget*w, gint resp, gpointer ud)
{
	gboolean *accepted=ud;
	*accepted=TRUE;
	if ( resp == GTK_RESPONSE_ACCEPT) {
		gchar*fn=gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(w));
		if (fn) {
			if (g_file_test(fn, G_FILE_TEST_EXISTS)){
				GtkWidget*dlg;
				dlg=new_dlg(GTK_MESSAGE_QUESTION,GTK_BUTTONS_YES_NO,
										_("File exists"),_("Do you want to overwrite it?"));
				gtk_window_set_title(GTK_WINDOW(dlg), _("confirm"));
				if (gtk_dialog_run(GTK_DIALOG(dlg)) != GTK_RESPONSE_YES) {
					*accepted=FALSE;
				}
				gtk_widget_destroy(dlg);
			}
			g_free(fn);
		}
	}
}
#endif


static gboolean create_file_filter(lua_State* L, GtkFileChooser*dlg, const gchar*mask)
{
	gchar **patterns=NULL;
	if (mask && *mask) {
		patterns=g_strsplit(mask, "|", 64);
		if (patterns) {
			gchar**pattern=patterns;
			if (g_strv_length(patterns)%2) {
				g_strfreev(patterns);
				return FALSE;
			}
			while (*pattern) {
				if (!**pattern) {
					g_strfreev(patterns);
					return FALSE;
				}
				pattern++;
			}
			pattern=patterns;
			while (*pattern) {
				gchar*maskname,*wildcard;
				maskname=*pattern;
				pattern++;
				wildcard=*pattern;
				if (maskname && wildcard) {
					GtkFileFilter*filter=gtk_file_filter_new();
					gchar*wc=wildcard;
					gtk_file_filter_set_name(filter, maskname);
					do {
						gchar*sem=strchr(wc,';');
						if (sem) {
							*sem='\0';
							gtk_file_filter_add_pattern(filter,wc);
							wc++;
						} else {
							gtk_file_filter_add_pattern(filter,wc);
							break;
						}
					} while (1);
					gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dlg),filter);
				}
				pattern++;
			}
		}
	}
	if (patterns) { g_strfreev(patterns); }
	return TRUE;
}


static gchar *file_dlg(lua_State* L, gboolean save, const gchar *path,	const gchar *mask, const gchar *name)
{
	gchar *rv=NULL;
	gchar *fullname = NULL;
	GtkWidget*dlg=NULL;
#if NEED_OVERWRITE_PROMPT
	gboolean accepted=FALSE;
#endif
	gint resp=GTK_RESPONSE_CANCEL;
	if (save) {
		dlg=gtk_file_chooser_dialog_new(_("Save file"),
					GTK_WINDOW(main_widgets->window), GTK_FILE_CHOOSER_ACTION_SAVE,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,	NULL);
#if NEED_OVERWRITE_PROMPT
		g_signal_connect(G_OBJECT(dlg),"response",G_CALLBACK(on_file_dlg_response),&accepted);
#else
		gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dlg), TRUE);
#endif
	} else {
		dlg=gtk_file_chooser_dialog_new(_("Open file"),
					GTK_WINDOW(main_widgets->window),	GTK_FILE_CHOOSER_ACTION_OPEN,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,	NULL);
	}
	if (name && *name) {
		if (g_path_is_absolute(name)) {
			fullname=g_strdup(name);
		} else if (path) {
			fullname=g_build_filename(path,name,NULL);
		}
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dlg), fullname);
		if (save) { gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dlg), name); }
	}
	if (path && *path) {
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), path);
	}
	if (!create_file_filter(L, GTK_FILE_CHOOSER(dlg), mask)) {
		lua_pushfstring(L, _("Error in module \"%s\" at function pickfile():\n"
			"failed to parse filter string at argument #3.\n"),
			LUA_MODULE_NAME);
		lua_error(L);
		g_free(fullname);
		return NULL;
	}

#if NEED_OVERWRITE_PROMPT
	glspi_pause_timer(TRUE,L);
	while (!accepted) {
		resp=gtk_dialog_run(GTK_DIALOG(dlg));
		if (!save) { accepted=TRUE; }
	}
	glspi_pause_timer(FALSE,L);
#else
		resp=glspi_dialog_run(GTK_DIALOG(dlg));
#endif
	if (resp == GTK_RESPONSE_ACCEPT) {
		rv=gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
	}
	gtk_widget_destroy(dlg);
	g_free(fullname);
	return rv;
}



/*
mode (open or save?)
path
filter

*/

static gint glspi_pickfile(lua_State* L)
{
	gboolean save = FALSE;
	gchar *path = NULL;
	const gchar *mask = NULL;
	gchar *name = NULL;
	gchar*result;
	gint argc=lua_gettop(L);

	if (argc >= 1) {
		if  (lua_isstring(L,1))	{
			const gchar*tmp=lua_tostring(L,1);
			if (g_ascii_strcasecmp(tmp,"save")==0) {
				save=TRUE;
			} else
			if ( (*tmp != '\0') && (g_ascii_strcasecmp(tmp,"open")!=0) ) {
				lua_pushfstring(L, _("Error in module \"%s\" at function %s():\n"
							"expected string \"open\" or \"save\" for argument #1.\n"),
							LUA_MODULE_NAME, &__FUNCTION__[6]);
				lua_error(L);
				return 0;
			}
		} else if (!lua_isnil(L,1)) {
			FAIL_STRING_ARG(1);
			return 0;
		}
	}
	if (argc >= 2) {
		if (lua_isstring(L,2)) {
			path=g_strdup(lua_tostring(L,2));
		} else if (!lua_isnil(L,2)) {
			FAIL_STRING_ARG(2);
			return 0;
		}
	}
	if (argc >= 3 ) {
		if (lua_isstring(L,3)) {
			mask=lua_tostring(L,3);
		} else if (!lua_isnil(L,3)) {
			FAIL_STRING_ARG(3);
			return 0;
		}
	}

	if (path && *path && !g_file_test(path,G_FILE_TEST_IS_DIR) ){
		gchar*sep=strrchr(path, G_DIR_SEPARATOR);
		if (sep) {
			name=sep+1;
			*sep='\0';
		} else {
			name=path;
			path=NULL;
		}
	}

	result=file_dlg(L,save,path,mask,name);
	if (path) { g_free(path); } else if ( name ) { g_free(name); };
	if (result) {
		lua_pushstring(L,result);
		g_free(result);
	} else {
		lua_pushnil(L);
	}
	return 1;
}





static const struct luaL_Reg glspi_dlg_funcs[] = {
	{"choose",   glspi_choose},
	{"confirm",  glspi_confirm},
	{"input",    glspi_input},
	{"message",  glspi_message},
	{"pickfile", glspi_pickfile},
	{NULL,NULL}
};




void glspi_init_dlg_funcs(lua_State *L, GsDlgRunHook hook) {
	glspi_pause_timer = hook;
	luaL_register(L, NULL,glspi_dlg_funcs);
}
