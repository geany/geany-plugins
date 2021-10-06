/*
 * geanylua.c - This file is part of the Lua scripting plugin for the Geany IDE
 * See the file "geanylua.c" for copyright information.
 */

#include "glspi.h"

// This macro sets the script filename if it isn't already set.  Since the
// filename is set only once, setting it to NULL afterwards disables the script.
#define SET_SCRIPT_FILENAME(_sc)                                               \
	static gboolean script_checked = FALSE;                                     \
	static gchar *script_fn = NULL;                                             \
	if (!script_checked && !script_fn) {                                        \
		script_fn = g_strconcat(geany->app->configdir, (_sc), NULL);             \
		script_checked = TRUE;                                                   \
	}

// This macro disables a script if the file doesn't exist.
#define DISABLE_SCRIPT_IF_NOT_EXIST()                                          \
	do {                                                                        \
		if (script_fn && !g_file_test(script_fn, G_FILE_TEST_IS_REGULAR)) {      \
			g_free(script_fn);                                                    \
			script_fn = NULL;                                                     \
		}                                                                        \
	} while (0)

// This macro runs scripts without associating them with documents.
#define RUN_SCRIPT()                                                           \
	do {                                                                        \
		if (script_fn && g_file_test(script_fn, G_FILE_TEST_IS_REGULAR)) {       \
			glspi_run_script(script_fn, 0, NULL, SD, NULL);                       \
		}                                                                        \
	} while (0)

// This macro runs a script associated with a doc index.
#define RUN_SCRIPT_IDX()                                                       \
	do {                                                                        \
		gint idx = doc->index;                                                   \
		if (script_fn && g_file_test(script_fn, G_FILE_TEST_IS_REGULAR)) {       \
			glspi_run_script(script_fn, idx + 1, NULL, SD, NULL);                 \
		}                                                                        \
	} while (0)

// This macro caches and runs a script associated with a doc index.
// Its purpose is to improve performance on high-latency network devices.
#define CACHE_RUN_SCRIPT_IDX()                                                 \
	do {                                                                        \
		static gchar *script_cache = NULL;                                       \
		if (script_fn && !script_cache) {                                        \
			gsize length = 0;                                                     \
			g_file_get_contents(script_fn, &script_cache, &length, NULL);         \
			if (script_cache && length != strlen(script_cache)) {                 \
				g_free(script_cache);                                              \
				script_cache = NULL;                                               \
				g_free(script_fn);                                                 \
				script_fn = NULL;                                                  \
			}                                                                     \
		}                                                                        \
		gint idx = doc->index;                                                   \
		if (script_fn && script_cache) {                                         \
			glspi_run_script(script_fn, idx + 1, NULL, SD, script_cache);         \
		}                                                                        \
	} while (0)

// This macro caches and runs scripts without associating with documents.
#define CACHE_RUN_SCRIPT()                                                     \
	do {                                                                        \
		static gchar *script_cache = NULL;                                       \
		if (script_fn && !script_cache) {                                        \
			gsize length = 0;                                                     \
			g_file_get_contents(script_fn, &script_cache, &length, NULL);         \
			if (script_cache && length != strlen(script_cache)) {                 \
				g_free(script_cache);                                              \
				script_cache = NULL;                                               \
			}                                                                     \
		}                                                                        \
		if (script_cache) {                                                      \
			glspi_run_script(script_fn, 0, NULL, SD, script_cache);               \
		}                                                                        \
	} while (0)

#define DIR_SEP G_DIR_SEPARATOR_S
#define USER_SCRIPT_FOLDER DIR_SEP "plugins" DIR_SEP "geanylua"
#define EVENTS_FOLDER USER_SCRIPT_FOLDER DIR_SEP "events" DIR_SEP

#define ALLOW_DEPRECATED_SCRIPTS 1
#ifdef ALLOW_DEPRECATED_SCRIPTS
// (Oct 2021): Use the old script if it exists and new one does not.
// TODO: Someday, show only the warning message.
// TODO: Eventually, stop checking for the old scripts entirely.
#define FIND_DEPRECATED_SCRIPT(_ns)                                            \
	do {                                                                        \
		gchar *tmp_script = NULL;                                                \
		tmp_script = g_strconcat(geany->app->configdir, (_ns##_OLD), NULL);      \
		if (!script_fn) {                                                        \
			script_fn = g_strconcat(geany->app->configdir, (_ns), NULL);          \
		}                                                                        \
		if (g_file_test(tmp_script, G_FILE_TEST_IS_REGULAR) &&                   \
			 !g_file_test(script_fn, G_FILE_TEST_IS_REGULAR)) {                   \
			g_free(script_fn);                                                    \
			script_fn = tmp_script;                                               \
			msgwin_status_add("GeanyLua Warning: Found deprecated script "        \
									"'%s%s'.  Please switch to '%s%s'.",                \
									geany->app->configdir, (_ns##_OLD),                 \
									geany->app->configdir, (_ns));                      \
		} else {                                                                 \
			g_free(tmp_script);                                                   \
		}                                                                        \
	} while (0)

#define ON_DOCUMENT_SAVE_SCRIPT_OLD EVENTS_FOLDER "saved.lua"
#define ON_DOCUMENT_OPEN_SCRIPT_OLD EVENTS_FOLDER "opened.lua"
#define ON_DOCUMENT_NEW_SCRIPT_OLD EVENTS_FOLDER "created.lua"
#define ON_DOCUMENT_ACTIVATE_SCRIPT_OLD EVENTS_FOLDER "activated.lua"
#define ON_PROJECT_OPEN_SCRIPT_OLD EVENTS_FOLDER "proj-opened.lua"
#define ON_PROJECT_SAVE_SCRIPT_OLD EVENTS_FOLDER "proj-saved.lua"
#define ON_PROJECT_CLOSE_SCRIPT_OLD EVENTS_FOLDER "proj-closed.lua"
#else
#define FIND_DEPRECATED_SCRIPT(_sv, _ns)
#endif // ALLOW_DEPRECATED_SCRIPTS

// Scripts to respond to Geany signals
#define ON_DOCUMENT_ACTIVATE_SCRIPT EVENTS_FOLDER "document-activate.lua"
#define ON_DOCUMENT_BEFORE_SAVE_SCRIPT EVENTS_FOLDER "document-before-save.lua"
#define ON_DOCUMENT_CLOSE_SCRIPT EVENTS_FOLDER "document-close.lua"
#define ON_DOCUMENT_FILETYPE_SET_SCRIPT                                        \
	EVENTS_FOLDER "document-filetype-set.lua"
#define ON_DOCUMENT_NEW_SCRIPT EVENTS_FOLDER "document-new.lua"
#define ON_DOCUMENT_OPEN_SCRIPT EVENTS_FOLDER "document-open.lua"
#define ON_DOCUMENT_RELOAD_SCRIPT EVENTS_FOLDER "document-reload.lua"
#define ON_DOCUMENT_SAVE_SCRIPT EVENTS_FOLDER "document-save.lua"

#define ON_PROJECT_BEFORE_CLOSE_SCRIPT EVENTS_FOLDER "project-before-close.lua"
#define ON_PROJECT_CLOSE_SCRIPT EVENTS_FOLDER "project-close.lua"
#define ON_PROJECT_DIALOG_CLOSE_SCRIPT EVENTS_FOLDER "project-dialog-close.lua"
#define ON_PROJECT_DIALOG_CONFIRMED_SCRIPT                                     \
	EVENTS_FOLDER "project-dialog-confirmed.lua"
#define ON_PROJECT_DIALOG_OPEN_SCRIPT EVENTS_FOLDER "project-dialog-open.lua"
#define ON_PROJECT_OPEN_SCRIPT EVENTS_FOLDER "project-open.lua"
#define ON_PROJECT_SAVE_SCRIPT EVENTS_FOLDER "project-save.lua"

#define ON_BUILD_START_SCRIPT EVENTS_FOLDER "build-start.lua"
#define ON_EDITOR_NOTIFY_SCRIPT EVENTS_FOLDER "editor-notify.lua"
#define ON_GEANY_STARTUP_COMPLETE_SCRIPT                                       \
	EVENTS_FOLDER "geany-startup-complete.lua"
#define ON_KEY_PRESS_SCRIPT EVENTS_FOLDER "key-press.lua"
#define ON_UPDATE_EDITOR_MENU_SCRIPT EVENTS_FOLDER "update-editor-menu.lua"

#define ON_INIT_SCRIPT EVENTS_FOLDER "init.lua"
#define ON_CLEANUP_SCRIPT EVENTS_FOLDER "cleanup.lua"
#define ON_CONFIGURE_SCRIPT EVENTS_FOLDER "configure.lua"

#define HOTKEYS_CFG DIR_SEP "hotkeys.cfg"
#define MAX_HOT_KEYS 100

PLUGIN_EXPORT
const gchar *glspi_version = VERSION;
PLUGIN_EXPORT
const guint glspi_abi = GEANY_ABI_VERSION;

GeanyData *glspi_geany_data = NULL;
GeanyPlugin *glspi_geany_plugin = NULL;

static struct {
	GtkWidget *menu_item;
	gchar *script_dir;

	GSList *script_list;
	GtkAccelGroup *acc_grp;
	GeanyKeyGroup *keybind_grp;
	gchar **keybind_scripts;
} local_data;

#define SD local_data.script_dir
#define KG local_data.keybind_grp
#define KS local_data.keybind_scripts

/* Called by Geany, run a script associated with a keybinding. */
static void kb_activate(guint key_id) {
	if ((key_id < MAX_HOT_KEYS) && KS[key_id]) {
		glspi_run_script(KS[key_id], 0, NULL, SD, NULL);
	}
}

/* Convert a script filename into a "pretty-printed" menu label. */
static gchar *fixup_label(gchar *label) {
	gint i;

	if (isdigit(label[0]) && isdigit(label[1]) && ('.' == label[2]) &&
		 (label[3])) {
		memmove(label, label + 3, strlen(label) - 2);
	}
	/* Capitalize first char of menu label */
	if (('_' == label[0]) && (label[1])) { /* Skip leading underscore */
		label[1] = g_ascii_toupper(label[1]);
	} else {
		label[0] = g_ascii_toupper(label[0]);
	}
	/* Convert hyphens in filename to spaces for menu label */
	for (i = 0; label[i]; i++) {
		if ('-' == label[i]) {
			label[i] = ' ';
		}
	}
	return label;
}

/* Free all hotkey data */
static void hotkey_cleanup(void) {
	if (KS) {
		g_strfreev(KS);
	}
}

#define KEYFILE_FAIL(msg)                                                      \
	if (geany->app->debug_mode) {                                               \
		g_printerr("%s: %s\n", PLUGIN_NAME, msg);                                \
	}                                                                           \
	g_error_free(err);                                                          \
	g_key_file_free(kf);                                                        \
	kf = NULL;

/* Initialize the interface to Geany's keybindings API */
static void hotkey_init(void) {
	gchar *hotkeys_cfg = g_strconcat(SD, HOTKEYS_CFG, NULL);
	hotkey_cleanup(); /* Make sure we are in initial state. */
	if (g_file_test(hotkeys_cfg, G_FILE_TEST_IS_REGULAR)) {
		GError *err = NULL;
		gchar *all = NULL;
		gsize len;
		if (g_file_get_contents(hotkeys_cfg, &all, &len, &err)) {
			gchar **lines = g_strsplit(all, "\n", 0);
			gint i;
			gint n = 0;
			g_free(all);
			for (i = 0; lines[i]; i++) {
				g_strstrip(lines[i]);
				if ((lines[i][0] != '#') && (lines[i][0] != '\0')) {
					n++;
					if (n == MAX_HOT_KEYS) {
						break;
					}
				}
			}
			KS = g_new0(gchar *, n + 1);
			n = 0;
			for (i = 0; lines[i]; i++) {
				if ((lines[i][0] != '#') && (lines[i][0] != '\0')) {
					if (g_path_is_absolute(lines[i])) {
						KS[n] = g_strdup(lines[i]);
					} else {
						KS[n] = g_build_filename(SD, lines[i], NULL);
					}
					n++;
					if (n == MAX_HOT_KEYS) {
						break;
					}
				}
			}
			g_strfreev(lines);
			KG = plugin_set_key_group(glspi_geany_plugin, "lua_scripts", n, NULL);
			for (i = 0; i < n; i++) {
				gchar *label = NULL;
				gchar *name = NULL;
				if (KS[i]) {
					gchar *p = NULL;
					label = g_path_get_basename(KS[i]);
					fixup_label(label);
					p = strchr(label, '_');
					if (p) {
						*p = ' ';
					}
					p = strrchr(label, '.');
					if (p && (g_ascii_strcasecmp(p, ".lua") == 0)) {
						*p = '\0';
					}
					name = g_strdup_printf("lua_script_%d", i + 1);
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

/*
 * Callbacks to run scripts
 */
static void on_build_start(GObject *obj, GeanyDocument *doc,
									gpointer user_data) {
	SET_SCRIPT_FILENAME(ON_BUILD_START_SCRIPT);
	DISABLE_SCRIPT_IF_NOT_EXIST();
	CACHE_RUN_SCRIPT_IDX();
}

static void on_editor_notify(GObject *obj, GeanyDocument *doc,
									  gpointer user_data) {
	SET_SCRIPT_FILENAME(ON_EDITOR_NOTIFY_SCRIPT);
	DISABLE_SCRIPT_IF_NOT_EXIST();
	CACHE_RUN_SCRIPT_IDX();
}

static void on_geany_startup_complete(GObject *obj, GeanyDocument *doc,
												  gpointer user_data) {
	SET_SCRIPT_FILENAME(ON_GEANY_STARTUP_COMPLETE_SCRIPT);
	CACHE_RUN_SCRIPT();
}

static void on_key_press(GObject *obj, GeanyDocument *doc, gpointer user_data) {
	SET_SCRIPT_FILENAME(ON_KEY_PRESS_SCRIPT);
	DISABLE_SCRIPT_IF_NOT_EXIST();
	CACHE_RUN_SCRIPT_IDX();
}

static void on_update_editor_menu(GObject *obj, GeanyDocument *doc,
											 gpointer user_data) {
	SET_SCRIPT_FILENAME(ON_UPDATE_EDITOR_MENU_SCRIPT);
	DISABLE_SCRIPT_IF_NOT_EXIST();
	CACHE_RUN_SCRIPT_IDX();
}

static void on_document_new(GObject *obj, GeanyDocument *doc,
									 gpointer user_data) {
	SET_SCRIPT_FILENAME(ON_DOCUMENT_NEW_SCRIPT);
	FIND_DEPRECATED_SCRIPT(ON_DOCUMENT_NEW_SCRIPT);
	CACHE_RUN_SCRIPT_IDX();
}

static void on_document_open(GObject *obj, GeanyDocument *doc,
									  gpointer user_data) {
	SET_SCRIPT_FILENAME(ON_DOCUMENT_OPEN_SCRIPT);
	FIND_DEPRECATED_SCRIPT(ON_DOCUMENT_OPEN_SCRIPT);
	DISABLE_SCRIPT_IF_NOT_EXIST();
	CACHE_RUN_SCRIPT_IDX();
}

static void on_document_save(GObject *obj, GeanyDocument *doc,
									  gpointer user_data) {
	SET_SCRIPT_FILENAME(ON_DOCUMENT_SAVE_SCRIPT);
	FIND_DEPRECATED_SCRIPT(ON_DOCUMENT_SAVE_SCRIPT);
	DISABLE_SCRIPT_IF_NOT_EXIST();
	CACHE_RUN_SCRIPT_IDX();
}

static void on_document_activate(GObject *obj, GeanyDocument *doc,
											gpointer user_data) {
	SET_SCRIPT_FILENAME(ON_DOCUMENT_ACTIVATE_SCRIPT);
	FIND_DEPRECATED_SCRIPT(ON_DOCUMENT_ACTIVATE_SCRIPT);
	DISABLE_SCRIPT_IF_NOT_EXIST();
	CACHE_RUN_SCRIPT_IDX();
}

static void on_document_close(GObject *obj, GeanyDocument *doc,
										gpointer user_data) {
	SET_SCRIPT_FILENAME(ON_DOCUMENT_CLOSE_SCRIPT);
	DISABLE_SCRIPT_IF_NOT_EXIST();
	CACHE_RUN_SCRIPT_IDX();
}

static void on_document_before_save(GObject *obj, GeanyDocument *doc,
												gpointer user_data) {
	SET_SCRIPT_FILENAME(ON_DOCUMENT_BEFORE_SAVE_SCRIPT);
	DISABLE_SCRIPT_IF_NOT_EXIST();
	CACHE_RUN_SCRIPT_IDX();
}

static void on_document_filetype_set(GObject *obj, GeanyDocument *doc,
												 gpointer user_data) {
	SET_SCRIPT_FILENAME(ON_DOCUMENT_FILETYPE_SET_SCRIPT);
	DISABLE_SCRIPT_IF_NOT_EXIST();
	CACHE_RUN_SCRIPT_IDX();
}

static void on_document_reload(GObject *obj, GeanyDocument *doc,
										 gpointer user_data) {
	SET_SCRIPT_FILENAME(ON_DOCUMENT_RELOAD_SCRIPT);
	DISABLE_SCRIPT_IF_NOT_EXIST();
	CACHE_RUN_SCRIPT_IDX();
}

static void on_project_open(GObject *obj, GKeyFile *config,
									 gpointer user_data) {
	SET_SCRIPT_FILENAME(ON_PROJECT_OPEN_SCRIPT);
	FIND_DEPRECATED_SCRIPT(ON_PROJECT_OPEN_SCRIPT);
	DISABLE_SCRIPT_IF_NOT_EXIST();
	CACHE_RUN_SCRIPT();
}

static void on_project_close(GObject *obj, GKeyFile *config,
									  gpointer user_data) {
	SET_SCRIPT_FILENAME(ON_PROJECT_CLOSE_SCRIPT);
	FIND_DEPRECATED_SCRIPT(ON_PROJECT_CLOSE_SCRIPT);
	DISABLE_SCRIPT_IF_NOT_EXIST();
	CACHE_RUN_SCRIPT();
}

static void on_project_save(GObject *obj, GKeyFile *config,
									 gpointer user_data) {
	SET_SCRIPT_FILENAME(ON_PROJECT_SAVE_SCRIPT);
	FIND_DEPRECATED_SCRIPT(ON_PROJECT_SAVE_SCRIPT);
	DISABLE_SCRIPT_IF_NOT_EXIST();
	CACHE_RUN_SCRIPT();
}
static void on_project_before_close(GObject *obj, GKeyFile *config,
												gpointer user_data) {
	SET_SCRIPT_FILENAME(ON_PROJECT_BEFORE_CLOSE_SCRIPT);
	DISABLE_SCRIPT_IF_NOT_EXIST();
	CACHE_RUN_SCRIPT();
}

static void on_project_dialog_close(GObject *obj, GKeyFile *config,
												gpointer user_data) {
	SET_SCRIPT_FILENAME(ON_PROJECT_DIALOG_CLOSE_SCRIPT);
	DISABLE_SCRIPT_IF_NOT_EXIST();
	CACHE_RUN_SCRIPT();
}

static void on_project_dialog_confirmed(GObject *obj, GKeyFile *config,
													 gpointer user_data) {
	SET_SCRIPT_FILENAME(ON_PROJECT_DIALOG_CONFIRMED_SCRIPT);
	DISABLE_SCRIPT_IF_NOT_EXIST();
	CACHE_RUN_SCRIPT();
}

static void on_project_dialog_open(GObject *obj, GKeyFile *config,
											  gpointer user_data) {
	SET_SCRIPT_FILENAME(ON_PROJECT_DIALOG_OPEN_SCRIPT);
	DISABLE_SCRIPT_IF_NOT_EXIST();
	CACHE_RUN_SCRIPT();
}

PLUGIN_EXPORT
PluginCallback glspi_geany_callbacks[] = {
	 {"build_start", (GCallback)&on_build_start, TRUE, NULL},
	 {"editor_notify", (GCallback)&on_editor_notify, TRUE, NULL},
	 {"geany_startup_complete", (GCallback)&on_geany_startup_complete, TRUE,
	  NULL},
	 {"key_press", (GCallback)&on_key_press, TRUE, NULL},
	 {"update_editor_menu", (GCallback)&on_update_editor_menu, TRUE, NULL},
	 {"document_activate", (GCallback)&on_document_activate, TRUE, NULL},
	 {"document_before_save", (GCallback)&on_document_before_save, TRUE, NULL},
	 {"document_close", (GCallback)&on_document_close, TRUE, NULL},
	 {"document_filetype_set", (GCallback)&on_document_filetype_set, TRUE,
	  NULL},
	 {"document_new", (GCallback)&on_document_new, TRUE, NULL},
	 {"document_open", (GCallback)&on_document_open, TRUE, NULL},
	 {"document_reload", (GCallback)&on_document_reload, TRUE, NULL},
	 {"document_save", (GCallback)&on_document_save, TRUE, NULL},
	 {"project_before_close", (GCallback)&on_project_before_close, TRUE, NULL},
	 {"project_close", (GCallback)&on_project_close, TRUE, NULL},
	 {"project_dialog_close", (GCallback)&on_project_dialog_close, TRUE, NULL},
	 {"project_dialog_confirmed", (GCallback)&on_project_dialog_confirmed, FALSE,
	  NULL},
	 {"project_dialog_open", (GCallback)&on_project_dialog_open, TRUE, NULL},
	 {"project_open", (GCallback)&on_project_open, TRUE, NULL},
	 {"project_save", (GCallback)&on_project_save, TRUE, NULL},
	 {NULL, NULL, FALSE, NULL}};

/* Callback when the menu item is clicked */
static void menu_item_activate(GtkMenuItem *menuitem, gpointer gdata) {
	glspi_run_script(gdata, 0, NULL, SD, NULL);
}

#define is_blank(c) ((c == 32) || (c == 9))

/*
	Check if the script file begins with a special comment in the form:
	-- @ACCEL@ <Modifiers>key
	If we find one, parse it, and bind that key combo to its menu item.
	See gtk_accelerator_parse() doc for more info on accel syntax...
*/
static void assign_accel(GtkWidget *w, char *fn) {
	FILE *f = fopen(fn, "r");
	gchar buf[512];
	gint len;
	if (!f) {
		return;
	}
	len = fread(buf, 1, sizeof(buf) - 1, f);
	if (len > 0) {
		gchar *p1 = buf;
		buf[len] = '\0';
		while (*p1 && is_blank(*p1))
			p1++;
		if (strncmp(p1, "--", 2) == 0) {
			p1 += 2;
			while (*p1 && is_blank(*p1))
				p1++;
			if (strncmp(p1, "@ACCEL@", 7) == 0) {
				guint key = 0;
				GdkModifierType mods = 0;
				p1 += 7;
				while (*p1 && is_blank(*p1))
					p1++;
				if (*p1) {
					gchar *p2 = p1;
					while ((*p2) && (!isspace(*p2))) {
						p2++;
					}
					*p2 = '\0';
					gtk_accelerator_parse(p1, &key, &mods);
					if (key && mods) {
						if (!local_data.acc_grp) {
							local_data.acc_grp = gtk_accel_group_new();
						}
						gtk_widget_add_accelerator(w, "activate", local_data.acc_grp,
															key, mods, GTK_ACCEL_VISIBLE);
					}
				}
			}
		}
	}
	fclose(f);
}

static GtkWidget *new_menu(GtkWidget *parent, const gchar *script_dir,
									const gchar *title);

/* GSList "for each" callback to create a menu item for each found script */
static void init_menu(gpointer data, gpointer user_data) {
	g_return_if_fail(data && user_data);
	if (g_file_test(data, G_FILE_TEST_IS_REGULAR)) {
		gchar *dot = strrchr(data, '.');
		if (dot && (((gpointer)dot) > data) &&
			 (g_ascii_strcasecmp(dot, ".lua") == 0)) {
			GtkWidget *item;
			gchar *label = strrchr(data, DIR_SEP[0]);
			gchar *tmp = NULL;
			if (label) {
				label++;
			} else {
				label = data;
			}
			tmp = g_malloc0(strlen(label));
			strncpy(tmp, label, dot - label);
			label = tmp;
			label = fixup_label(label);
			if ('_' == *(dot - 1)) {
				strcpy(strchr(label, '\0') - 1, "...");
			}
			item = gtk_menu_item_new_with_mnemonic(label);
			g_free(label);
			gtk_container_add(GTK_CONTAINER(user_data), item);
			g_signal_connect(G_OBJECT(item), "activate",
								  G_CALLBACK(menu_item_activate), data);
			assign_accel(item, data);
		}
	} else {
		if (g_file_test(data, G_FILE_TEST_IS_DIR)) {
			gchar *label = strrchr(data, DIR_SEP[0]);
			if (label) {
				label++;
			} else {
				label = data;
			}
			if ((g_ascii_strcasecmp(label, "events") != 0) &&
				 (g_ascii_strcasecmp(label, "support") != 0)) {
				label = g_strdup(label);
				fixup_label(label);
				new_menu(user_data, data, label); /* Recursive */
				g_free(label);
			}
		}
	}
}

static GtkWidget *new_menu(GtkWidget *parent, const gchar *script_dir,
									const gchar *title) {
	GSList *script_names =
		 utils_get_file_list_full(script_dir, TRUE, TRUE, NULL);
	if (script_names) {
		GtkWidget *menu = gtk_menu_new();
		GtkWidget *menu_item = gtk_menu_item_new_with_mnemonic(title);
		g_slist_foreach(script_names, init_menu, menu);
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), menu);
		gtk_container_add(GTK_CONTAINER(parent), menu_item);
		gtk_widget_show_all(menu_item);
		local_data.script_list =
			 g_slist_concat(local_data.script_list, script_names);
		return menu_item;
	}
	g_printerr("%s: No scripts found in %s\n", PLUGIN_NAME, script_dir);
	return NULL;
}

static void build_menu(void) {
	local_data.script_list = NULL;
	local_data.acc_grp = NULL;
	local_data.menu_item = new_menu(main_widgets->tools_menu,
											  local_data.script_dir, _("_Lua Scripts"));
	if (local_data.acc_grp) {
		gtk_window_add_accel_group(GTK_WINDOW(main_widgets->window),
											local_data.acc_grp);
	}
}

static gchar *get_data_dir(void) {
#ifdef G_OS_WIN32
	gchar *install_dir, *result;
#if GLIB_CHECK_VERSION(2, 16, 0)
	install_dir = g_win32_get_package_installation_directory_of_module(NULL);
#else
	install_dir = g_win32_get_package_installation_directory(NULL, NULL);
#endif
	result = g_strconcat(install_dir, "\\share", NULL);
	g_free(install_dir);
	return result;
#else
	return g_strdup(GEANYPLUGINS_DATADIR);
#endif
}

/* Called by Geany to initialize the plugin */
PLUGIN_EXPORT
void glspi_init(GeanyData *data, GeanyPlugin *plugin) {
	glspi_geany_data = data;
	glspi_geany_plugin = plugin;

	local_data.script_dir =
		 g_strconcat(geany->app->configdir, USER_SCRIPT_FOLDER, NULL);

	if (!g_file_test(local_data.script_dir, G_FILE_TEST_IS_DIR)) {
		gchar *datadir = get_data_dir();
		g_free(local_data.script_dir);
		local_data.script_dir = g_build_path(G_DIR_SEPARATOR_S, datadir,
														 "geany-plugins", "geanylua", NULL);
		g_free(datadir);
	}
	if (geany->app->debug_mode) {
		g_printerr(_("     ==>> %s: Building menu from '%s'\n"), PLUGIN_NAME,
					  local_data.script_dir);
	}

	glspi_set_sci_cmd_hash(TRUE);
	glspi_set_key_cmd_hash(TRUE);
	build_menu();
	hotkey_init();

	SET_SCRIPT_FILENAME(ON_INIT_SCRIPT);
	RUN_SCRIPT();
}

/* GSList "for each" callback to free our script list items */
static void free_script_names(gpointer data, gpointer user_data) {
	if (data) {
		g_free(data);
	}
}

static void remove_menu(void) {
	if (local_data.acc_grp) {
		g_object_unref(local_data.acc_grp);
	}
	if (local_data.menu_item) {
		gtk_widget_destroy(local_data.menu_item);
	}
}

#define done(f)                                                                \
	if (local_data.f)                                                           \
	g_free(local_data.f)

/* Called by Geany when it is time to free the plugin's resources */
PLUGIN_EXPORT
void glspi_cleanup(void) {
	SET_SCRIPT_FILENAME(ON_CLEANUP_SCRIPT);
	RUN_SCRIPT();

	remove_menu();
	hotkey_cleanup();
	done(script_dir);

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
void glspi_configure(GtkWidget *parent) {
	SET_SCRIPT_FILENAME(ON_CONFIGURE_SCRIPT);

	if (g_file_test(script_fn, G_FILE_TEST_IS_REGULAR)) {
		glspi_run_script(script_fn, 0, NULL, SD, NULL);
	} else {
		gint flags = GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL;
		gint type = GTK_MESSAGE_INFO;
		GtkWidget *dlg =
			 gtk_message_dialog_new(GTK_WINDOW(parent), flags, type,
											GTK_BUTTONS_OK, _("Nothing to configure!"));
		gtk_message_dialog_format_secondary_text(
			 GTK_MESSAGE_DIALOG(dlg),
			 _("You can create the script:\n\n\"%s\"\n\n"
				"to add your own custom configuration dialog."),
			 script_fn);
		gtk_window_set_title(GTK_WINDOW(dlg), PLUGIN_NAME);
		gtk_dialog_run(GTK_DIALOG(dlg));
		gtk_widget_destroy(dlg);
	}
}

static gint glspi_rescan(lua_State *L) {
	remove_menu();
	build_menu();
	hotkey_init();
	return 0;
}

static const struct luaL_reg glspi_mnu_funcs[] = {{"rescan", glspi_rescan},
																  {NULL, NULL}};

void glspi_init_mnu_funcs(lua_State *L) {
	luaL_register(L, NULL, glspi_mnu_funcs);
}
