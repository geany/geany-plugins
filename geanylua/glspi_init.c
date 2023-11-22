/*
 * geanylua.c - This file is part of the Lua scripting plugin for the Geany IDE
 * See the file "geanylua.c" for copyright information.
 */

#include "glspi.h"


#define DIR_SEP  G_DIR_SEPARATOR_S

#define USER_SCRIPT_FOLDER  DIR_SEP  "plugins"  DIR_SEP  "geanylua"

#define EVENTS_FOLDER  USER_SCRIPT_FOLDER DIR_SEP  "events"  DIR_SEP

#define ON_SAVED_SCRIPT      EVENTS_FOLDER  "saved.lua"
#define ON_OPENED_SCRIPT     EVENTS_FOLDER  "opened.lua"
#define ON_CREATED_SCRIPT    EVENTS_FOLDER  "created.lua"
#define ON_ACTIVATED_SCRIPT  EVENTS_FOLDER  "activated.lua"

#define ON_PROJ_OPENED_SCRIPT  EVENTS_FOLDER  "proj-opened.lua"
#define ON_PROJ_SAVED_SCRIPT   EVENTS_FOLDER  "proj-saved.lua"
#define ON_PROJ_CLOSED_SCRIPT  EVENTS_FOLDER  "proj-closed.lua"

#define ON_INIT_SCRIPT       EVENTS_FOLDER  "init.lua"
#define ON_CLEANUP_SCRIPT    EVENTS_FOLDER  "cleanup.lua"
#define ON_CONFIGURE_SCRIPT  EVENTS_FOLDER  "configure.lua"

#define HOTKEYS_CFG DIR_SEP "hotkeys.cfg"
#define MAX_HOT_KEYS 100

PLUGIN_EXPORT
const gchar* glspi_version = VERSION;
PLUGIN_EXPORT
const guint glspi_abi = GEANY_ABI_VERSION;

GeanyData *glspi_geany_data=NULL;
GeanyPlugin *glspi_geany_plugin=NULL;

static struct {
	GtkWidget *menu_item;
	gchar *script_dir;
	gchar *on_saved_script;
	gchar *on_created_script;
	gchar *on_opened_script;
	gchar *on_activated_script;
	gchar *on_init_script;
	gchar *on_cleanup_script;
	gchar *on_configure_script;
	gchar *on_proj_opened_script;
	gchar *on_proj_saved_script;
	gchar *on_proj_closed_script;
	GSList *script_list;
	GtkAccelGroup *acc_grp;
	GeanyKeyGroup *keybind_grp;
	gchar **keybind_scripts;
} local_data;

#define SD  local_data.script_dir
#define KG local_data.keybind_grp
#define KS local_data.keybind_scripts


/* Called by Geany, run a script associated with a keybinding. */
static void kb_activate(guint key_id)
{
	if ((key_id<MAX_HOT_KEYS) && KS[key_id]) {
		glspi_run_script(KS[key_id],0,NULL,SD);
	}
}

/* Convert a script filename into a "pretty-printed" menu label. */
static gchar* fixup_label(gchar*label)
{
	gint i;

	if (isdigit(label[0])&&isdigit(label[1])&&('.'==label[2])&&(label[3])) {
		memmove(label,label+3,strlen(label)-2);
	}
	/* Capitalize first char of menu label */
	if (('_'==label[0])&&(label[1])) { /* Skip leading underscore */
		label[1]=g_ascii_toupper(label[1]);
	} else {
		label[0]=g_ascii_toupper(label[0]);
	}
	/* Convert hyphens in filename to spaces for menu label */
	for (i=0; label[i]; i++) { if ('-' == label[i]) {label[i]=' ';} }
	return label;
}


/* Free all hotkey data */
static void hotkey_cleanup(void)
{
	if (KS) { g_strfreev(KS); }
}


#define KEYFILE_FAIL(msg) \
if (geany->app->debug_mode) { \
	g_printerr("%s: %s\n", PLUGIN_NAME, msg); \
} \
g_error_free(err); \
g_key_file_free(kf); \
kf=NULL;


/* Initialize the interface to Geany's keybindings API */
static void hotkey_init(void)
{
	gchar *hotkeys_cfg=g_strconcat(SD,HOTKEYS_CFG,NULL);
	hotkey_cleanup(); /* Make sure we are in initial state. */
	if (g_file_test(hotkeys_cfg,G_FILE_TEST_IS_REGULAR)) {
		GError *err=NULL;
		gchar*all=NULL;
		gsize len;
		if (g_file_get_contents(hotkeys_cfg,&all,&len,&err)) {
			gchar**lines=g_strsplit(all, "\n", 0);
			gint i;
			gint n=0;
			g_free(all);
			for (i=0; lines[i]; i++) {
				g_strstrip(lines[i]);
				if ((lines[i][0]!='#')&&(lines[i][0]!='\0')) {
					n++;
					if (n==MAX_HOT_KEYS) { break; }
				}
			}
			KS=g_new0(gchar*, n+1);
			n=0;
			for (i=0; lines[i]; i++) {
				if ((lines[i][0]!='#')&&(lines[i][0]!='\0')) {
					if (g_path_is_absolute(lines[i])) {
						KS[n]=g_strdup(lines[i]);
					} else {
						KS[n]=g_build_filename(SD, lines[i], NULL);
					}
					n++;
					if (n==MAX_HOT_KEYS) { break; }
				}
			}
			g_strfreev(lines);
			KG=plugin_set_key_group(glspi_geany_plugin, "lua_scripts", n, NULL);
			for (i=0; i<n; i++) {
				gchar *label=NULL;
				gchar *name=NULL;
				if (KS[i]) {
					gchar*p=NULL;
					label=g_path_get_basename(KS[i]);
					fixup_label(label);
					p=strchr(label,'_');
					if (p) { *p=' ';}
					p=strrchr(label, '.');
					if (p && (g_ascii_strcasecmp(p, ".lua")==0)) {
						*p='\0';
					}
					name=g_strdup_printf("lua_script_%d", i+1);
				}
				/* no default keycombos, just overridden by user settings */
				keybindings_set_item(KG, i, kb_activate, 0, 0, name, label, NULL);
				g_free(label);
				g_free(name);
			}
		} else {
			if (geany->app->debug_mode) {
				g_printerr("%s: %s\n", PLUGIN_NAME, err->message);
			}
			g_error_free(err);
		}
	} else {
		if (geany->app->debug_mode) {
			g_printerr("%s:  File not found %s\n", PLUGIN_NAME, hotkeys_cfg);
		}
	}
	g_free(hotkeys_cfg);
}




static void on_doc_new(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
	gint idx = doc->index;
	if (g_file_test(local_data.on_created_script,G_FILE_TEST_IS_REGULAR)) {
		glspi_run_script(local_data.on_created_script,idx+1, NULL, SD);
	}
}


static void on_doc_save(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
	gint idx = doc->index;
	if (g_file_test(local_data.on_saved_script,G_FILE_TEST_IS_REGULAR)) {
		glspi_run_script(local_data.on_saved_script,idx+1, NULL, SD);
	}
}



static void on_doc_open(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
	gint idx = doc->index;
	if (g_file_test(local_data.on_opened_script,G_FILE_TEST_IS_REGULAR)) {
		glspi_run_script(local_data.on_opened_script,idx+1, NULL, SD);
	}
}



static void on_doc_activate(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
	gint idx = doc->index;
	if (g_file_test(local_data.on_activated_script,G_FILE_TEST_IS_REGULAR)) {
		glspi_run_script(local_data.on_activated_script,idx+1, NULL, SD);
	}
}



static void on_proj_open(GObject *obj, GKeyFile *config, gpointer user_data)
{
	if (g_file_test(local_data.on_proj_opened_script,G_FILE_TEST_IS_REGULAR)) {
		glspi_run_script(local_data.on_proj_opened_script,0,config, SD);
	}
}



static void on_proj_save(GObject *obj, GKeyFile *config, gpointer user_data)
{
	if (g_file_test(local_data.on_proj_saved_script,G_FILE_TEST_IS_REGULAR)) {
		glspi_run_script(local_data.on_proj_saved_script,0,config, SD);
	}
}



static void on_proj_close(GObject *obj, gpointer user_data)
{
	if (g_file_test(local_data.on_proj_closed_script,G_FILE_TEST_IS_REGULAR)) {
		glspi_run_script(local_data.on_proj_closed_script,0, NULL, SD);
	}
}


PLUGIN_EXPORT
PluginCallback	glspi_geany_callbacks[] = {
	{"document-new", (GCallback) &on_doc_new, TRUE, NULL},
	{"document-open", (GCallback) &on_doc_open, TRUE, NULL},
	{"document-save", (GCallback) &on_doc_save, TRUE, NULL},
	{"document-activate", (GCallback) &on_doc_activate, TRUE, NULL},
	{"project-open", (GCallback) &on_proj_open, TRUE, NULL},
	{"project-save", (GCallback) &on_proj_save, TRUE, NULL},
	{"project-close", (GCallback) &on_proj_close, TRUE, NULL},
	{NULL, NULL, FALSE, NULL}
};



/* Callback when the menu item is clicked */
static void menu_item_activate(GtkMenuItem * menuitem, gpointer gdata)
{
	glspi_run_script(gdata, 0,NULL, SD);
}


#define is_blank(c) ( (c==32) || (c==9) )


/*
	Check if the script file begins with a special comment in the form:
	-- @ACCEL@ <Modifiers>key
	If we find one, parse it, and bind that key combo to its menu item.
	See gtk_accelerator_parse() doc for more info on accel syntax...
*/
static void assign_accel(GtkWidget*w, char*fn)
{
	FILE*f=fopen(fn,"r");
	gchar buf[512];
	gint len;
	if (!f) { return; }
	len=fread(buf,1,sizeof(buf)-1,f);
	if (len>0) {
		gchar*p1=buf;
		buf[len]='\0';
		while (*p1 && is_blank(*p1)) p1++;
		if ( strncmp(p1,"--", 2) == 0 ) {
			p1+=2;
			while (*p1 && is_blank(*p1)) p1++;
			if ( strncmp(p1,"@ACCEL@", 7) == 0 ) {
				guint key=0;
				GdkModifierType mods=0;
				p1+=7;
				while (*p1 && is_blank(*p1)) p1++;
				if (*p1) {
					gchar*p2=p1;
					while ( (*p2) && (!isspace(*p2)) ) { p2++; }
					*p2='\0';
					gtk_accelerator_parse(p1, &key, &mods);
					if ( key && mods ) {
						if (!local_data.acc_grp) {
							local_data.acc_grp=gtk_accel_group_new();
						}
						gtk_widget_add_accelerator(w,
								"activate",local_data.acc_grp,key,mods,GTK_ACCEL_VISIBLE);
					}
				}
			}
		}
	}
	fclose(f);
}




static GtkWidget* new_menu(GtkWidget *parent, const gchar* script_dir, const gchar*title);

/* GSList "for each" callback to create a menu item for each found script */
static void init_menu(gpointer data, gpointer user_data)
{
	g_return_if_fail(data && user_data);
	if (g_file_test(data,G_FILE_TEST_IS_REGULAR)) {
		gchar *dot = strrchr(data, '.');
		if ( dot && (((gpointer)dot)>data) && (g_ascii_strcasecmp(dot, ".lua")==0) ) {
			GtkWidget *item;
			gchar*label=strrchr(data,DIR_SEP[0]);
			gchar *tmp=NULL;
			if (label) { label++; } else { label=data; }
			tmp=g_malloc0(strlen(label));
			strncpy(tmp, label, dot-label);
			label=tmp;
			label=fixup_label(label);
			if ('_'==*(dot-1)) { strcpy(strchr(label, '\0')-1, "..."); }
			item = gtk_menu_item_new_with_mnemonic(label);
			g_free(label);
			gtk_container_add(GTK_CONTAINER(user_data), item);
			g_signal_connect(G_OBJECT(item), "activate",
				G_CALLBACK(menu_item_activate), data);
			assign_accel(item, data);
		}
	} else {
		if (g_file_test(data,G_FILE_TEST_IS_DIR)) {
			gchar*label=strrchr(data,DIR_SEP[0]);
			if (label) { label++; } else { label=data; }
			if ((g_ascii_strcasecmp(label,"events")!=0)&&(g_ascii_strcasecmp(label,"support")!=0)) {
				label=g_strdup(label);
				fixup_label(label);
				new_menu(user_data, data, label); /* Recursive */
				g_free(label);
			}
		}
	}
}



static GtkWidget* new_menu(GtkWidget *parent, const gchar* script_dir, const gchar*title)
{
	GSList *script_names=utils_get_file_list_full(script_dir, TRUE, TRUE, NULL);
	if (script_names) {
		GtkWidget *menu = gtk_menu_new();
		GtkWidget *menu_item = gtk_menu_item_new_with_mnemonic(title);
		g_slist_foreach(script_names, init_menu, menu);
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), menu);
		gtk_container_add(GTK_CONTAINER(parent), menu_item);
		gtk_widget_show_all(menu_item);
		local_data.script_list=g_slist_concat(local_data.script_list,script_names);
		return menu_item;
	}
	g_printerr("%s: No scripts found in %s\n", PLUGIN_NAME, script_dir);
	return NULL;
}



static void build_menu(void)
{
	local_data.script_list = NULL;
	local_data.acc_grp=NULL;
	local_data.menu_item=new_menu(main_widgets->tools_menu,
		local_data.script_dir, _("_Lua Scripts"));
	if (local_data.acc_grp) {
		gtk_window_add_accel_group(GTK_WINDOW(main_widgets->window), local_data.acc_grp);
	}
}


static gchar *get_data_dir(void)
{
#ifdef G_OS_WIN32
	gchar *install_dir, *result;
# if GLIB_CHECK_VERSION(2, 16, 0)
	install_dir = g_win32_get_package_installation_directory_of_module(NULL);
# else
	install_dir = g_win32_get_package_installation_directory(NULL, NULL);
# endif
	result = g_strconcat(install_dir, "\\share", NULL);
	g_free(install_dir);
	return result;
#else
	return g_strdup(GEANYPLUGINS_DATADIR);
#endif
}



/* Called by Geany to initialize the plugin */
PLUGIN_EXPORT
void glspi_init (GeanyData *data, GeanyPlugin *plugin)
{
	glspi_geany_data = data;
	glspi_geany_plugin = plugin;

	local_data.script_dir =
		g_strconcat(geany->app->configdir, USER_SCRIPT_FOLDER, NULL);

	if (!g_file_test(local_data.script_dir, G_FILE_TEST_IS_DIR)) {
		gchar *datadir = get_data_dir();
		g_free(local_data.script_dir);
		local_data.script_dir =
			g_build_path(G_DIR_SEPARATOR_S, datadir, "geany-plugins", "geanylua", NULL);
		g_free(datadir);
	}
	if (geany->app->debug_mode) {
		g_printerr(_("     ==>> %s: Building menu from '%s'\n"),
			PLUGIN_NAME, local_data.script_dir);
	}
	local_data.on_saved_script =
		g_strconcat(geany->app->configdir, ON_SAVED_SCRIPT, NULL);
	local_data.on_opened_script =
		g_strconcat(geany->app->configdir, ON_OPENED_SCRIPT, NULL);
	local_data.on_created_script =
		g_strconcat(geany->app->configdir, ON_CREATED_SCRIPT, NULL);
	local_data.on_activated_script =
		g_strconcat(geany->app->configdir, ON_ACTIVATED_SCRIPT, NULL);
	local_data.on_init_script =
		g_strconcat(geany->app->configdir, ON_INIT_SCRIPT, NULL);
	local_data.on_cleanup_script =
		g_strconcat(geany->app->configdir, ON_CLEANUP_SCRIPT, NULL);
	local_data.on_configure_script =
		g_strconcat(geany->app->configdir, ON_CONFIGURE_SCRIPT, NULL);
	local_data.on_proj_opened_script =
		g_strconcat(geany->app->configdir, ON_PROJ_OPENED_SCRIPT, NULL);
	local_data.on_proj_saved_script =
		g_strconcat(geany->app->configdir, ON_PROJ_SAVED_SCRIPT, NULL);
	local_data.on_proj_closed_script =
		g_strconcat(geany->app->configdir, ON_PROJ_CLOSED_SCRIPT, NULL);

	glspi_set_sci_cmd_hash(TRUE);
	glspi_set_key_cmd_hash(TRUE);
	build_menu();
	hotkey_init();
	if (g_file_test(local_data.on_init_script,G_FILE_TEST_IS_REGULAR)) {
		glspi_run_script(local_data.on_init_script,0,NULL, SD);
	}
}



/* GSList "for each" callback to free our script list items */
static void free_script_names(gpointer data, gpointer user_data)
{
	if (data) { g_free(data); }
}


static void remove_menu(void)
{
	if (local_data.acc_grp) { g_object_unref(local_data.acc_grp); }
	if (local_data.menu_item) { gtk_widget_destroy(local_data.menu_item);	}
}


#define done(f) if (local_data.f) g_free(local_data.f)

/* Called by Geany when it is time to free the plugin's resources */
PLUGIN_EXPORT
void glspi_cleanup(void)
{

	if (g_file_test(local_data.on_cleanup_script,G_FILE_TEST_IS_REGULAR)) {
		glspi_run_script(local_data.on_cleanup_script,0,NULL, SD);
	}
	remove_menu();
	hotkey_cleanup();
	done(script_dir);
	done(on_saved_script);
	done(on_created_script);
	done(on_opened_script);
	done(on_activated_script);
	done(on_init_script);
	done(on_cleanup_script);
	done(on_configure_script);
	done(on_proj_opened_script);
	done(on_proj_saved_script);
	done(on_proj_closed_script);

	if (local_data.script_list) {
		g_slist_foreach(local_data.script_list, free_script_names, NULL);
		g_slist_free(local_data.script_list);
	}
	glspi_set_sci_cmd_hash(FALSE);
	glspi_set_key_cmd_hash(FALSE);

}





/*
 Called by geany when user clicks preferences button
 in plugin manager dialog.
*/
PLUGIN_EXPORT
void glspi_configure(GtkWidget *parent)
{
	if (g_file_test(local_data.on_configure_script,G_FILE_TEST_IS_REGULAR)) {
		glspi_run_script(local_data.on_configure_script,0,NULL, SD);
	} else {
		gint flags=GTK_DIALOG_DESTROY_WITH_PARENT|GTK_DIALOG_MODAL;
		gint type=GTK_MESSAGE_INFO;
		GtkWidget *dlg=gtk_message_dialog_new(
			GTK_WINDOW(parent),flags,type,GTK_BUTTONS_OK,_("Nothing to configure!"));
			gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dlg),
				_("You can create the script:\n\n\"%s\"\n\n"
				"to add your own custom configuration dialog."),
		local_data.on_configure_script);
		gtk_window_set_title(GTK_WINDOW(dlg),PLUGIN_NAME);
		gtk_dialog_run(GTK_DIALOG(dlg));
		gtk_widget_destroy(dlg);
	}
}

static gint glspi_rescan(lua_State* L) {
	remove_menu();
	build_menu();
	hotkey_init();
	return 0;
}

static const struct luaL_Reg glspi_mnu_funcs[] = {
	{"rescan",    glspi_rescan},
	{NULL,NULL}
};


void glspi_init_mnu_funcs(lua_State *L) {
	luaL_register(L, NULL,glspi_mnu_funcs);
}
