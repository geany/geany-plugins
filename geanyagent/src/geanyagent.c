/*
 * geanyagent.c — Geany plugin: AI agent terminal tab in the message window
 *
 * The configured command is run inside the user's login shell so that PATH
 * and profile scripts (nvm, pyenv, etc.) are honoured.  When the process
 * exits it is restarted automatically.
 *
 * Copyright 2025 teknopaul
 * License: GPL-2.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <termios.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <gio/gunixsocketaddress.h>
#include <gtk/gtk.h>
#include <vte/vte.h>

#include <geanyplugin.h>


/* ------------------------------------------------------------------ */

GeanyPlugin  *geany_plugin;
GeanyData    *geany_data;

static VteTerminal *agent_term  = NULL;
static GtkWidget   *agent_hbox  = NULL;
static GtkWidget   *agent_label = NULL;
static gint         tab_index   = -1;
static gboolean     running     = FALSE;  /* FALSE during cleanup to suppress restart */
static GPid         agent_pid   = 0;      /* child PID, 0 when not running */

static GSocketService *agent_sock_service = NULL;
static gchar          *agent_sock_path    = NULL;

static gchar *config_file  = NULL;
static gchar *search_url   = NULL;

typedef struct {
    gchar *key;      /* config group key */
    gchar *name;     /* display name */
    gchar *cmd;      /* shell command */
    gchar *work_dir; /* optional working directory template */
} AgentConfig;

static GPtrArray *agents       = NULL;
static gint       active_agent = 0;

#define DEFAULT_SEARCH_URL "https://www.qwant.com/?q=%s"

/* askpass: temp dir prepended to PATH containing a sudo wrapper + askpass script */
static gboolean use_askpass = FALSE;  /* opt-in; disabled by default */
static gchar   *askpass_dir  = NULL;
static gchar   *askpass_prog = NULL;

static gboolean focus_on_stop = TRUE;   /* persisted in config */

static GtkWidget *focus_event_box = NULL;
static GtkWidget *focus_label     = NULL;

#define DEFAULT_CMD "claude"

static const gchar SKILL_TEMPLATE[] =
    "---\n"
    "name: %s\n"
    "description: \n"
    "---\n"
    "\n"
    "<objective>\n"
    "\n"
    "</objective>\n"
    "\n"
    "<process>\n"
    "\n"
    "</process>\n";

#define AGENT_TOOLS_CONFIG "agenttools.conf"
#define AGENT_TOOLS_GROUP  "tools"

/* Forward declarations */
static void update_agent_label(void);
static void ga_save_config(void);
static void ga_switch_agent(gint index);

static GKeyFile  *agent_tools_config    = NULL;
static GtkWidget *agent_tools_menu_item = NULL;

static gulong editor_nb_handler_id = 0;


/* ------------------------------------------------------------------ */
/* Askpass setup                                                       */
/*
 * Creates a private temp dir (0700) prepended to PATH containing:
 *   sudo             — wrapper that forces -A so sudo never prompts on the
 *                      terminal; the AI agent cannot see or handle the password
 *   geanyagent-askpass — zenity dialog (or delegates to a system askpass)
 *
 * sudo reads SUDO_ASKPASS and calls the helper; the helper shows a GTK
 * password dialog and writes the password to stdout.  Nothing is logged.
 */

static void setup_askpass(void)
{
    /* Find a system askpass program */
    const gchar *candidates[] = {
        "/usr/lib/ssh/x11-ssh-askpass",
        "/usr/libexec/ssh-askpass",
        "/usr/lib/openssh/gnome-ssh-askpass",
        "/usr/lib/openssh/x11-ssh-askpass",
        "/usr/bin/ksshaskpass",
        "/usr/bin/seahorse-askpass",
        NULL
    };
    for (gint i = 0; candidates[i]; i++)
    {
        if (g_file_test(candidates[i], G_FILE_TEST_IS_EXECUTABLE))
        {
            askpass_prog = g_strdup(candidates[i]);
            break;
        }
    }

    /* No system askpass — try to build a zenity-based one */
    if (!askpass_prog)
    {
        gchar *zenity = g_find_program_in_path("zenity");
        if (!zenity)
            return; /* nothing we can do */
        g_free(zenity);
    }

    /* Private temp dir — mode 0700 so other users cannot replace scripts */
    gchar *tmpl = g_build_filename(g_get_tmp_dir(), "geanyagent-XXXXXX", NULL);
    askpass_dir = g_mkdtemp(tmpl);  /* modifies tmpl in-place on success */
    if (!askpass_dir)
    {
        g_free(tmpl);
        g_free(askpass_prog);
        askpass_prog = NULL;
        return;
    }

    /* Write zenity askpass helper if no system one was found */
    if (!askpass_prog)
    {
        askpass_prog = g_build_filename(askpass_dir, "geanyagent-askpass", NULL);
        const gchar *script =
            "#!/bin/sh\n"
            "zenity --password --title 'Agent needs sudo' -- \"${1:-Password:}\" 2>/dev/null\n";
        utils_write_file(askpass_prog, script);
        chmod(askpass_prog, S_IRWXU);
    }

    /* Write sudo wrapper: forces -A so sudo never prompts on the PTY.
     * The AI sees only the exit code, never the password. */
    gchar *sudo_path = g_build_filename(askpass_dir, "sudo", NULL);
    gchar *content   = g_strdup_printf(
        "#!/bin/sh\n"
        "SUDO_ASKPASS='%s' exec /usr/bin/sudo -A \"$@\"\n",
        askpass_prog);
    utils_write_file(sudo_path, content);
    g_free(content);
    chmod(sudo_path, S_IRWXU);
    g_free(sudo_path);
}

static void cleanup_askpass(void)
{
    if (askpass_dir)
    {
        gchar *p;
        p = g_build_filename(askpass_dir, "sudo", NULL);
        g_unlink(p); g_free(p);
        p = g_build_filename(askpass_dir, "geanyagent-askpass", NULL);
        g_unlink(p); g_free(p);
        g_rmdir(askpass_dir);
        g_free(askpass_dir);
        askpass_dir = NULL;
    }
    g_free(askpass_prog);
    askpass_prog = NULL;
}


static void agent_config_free(AgentConfig *ac)
{
    g_free(ac->key);
    g_free(ac->name);
    g_free(ac->cmd);
    g_free(ac->work_dir);
    g_free(ac);
}

static void agents_free(void)
{
    if (agents) {
        for (guint i = 0; i < agents->len; i++)
            agent_config_free((AgentConfig *)g_ptr_array_index(agents, i));
        g_ptr_array_free(agents, TRUE);
        agents = NULL;
    }
}

/* ------------------------------------------------------------------ */
/* Agent command dispatch                                              */

static gboolean grab_agent_focus_idle(G_GNUC_UNUSED gpointer data)
{
	if (agent_term)
		gtk_widget_grab_focus(GTK_WIDGET(agent_term));
	return G_SOURCE_REMOVE;
}


/* ------------------------------------------------------------------ */
/* Agent Unix socket — receives {\"event\":\"stop\"} from Claude hooks  */

static gboolean on_agent_sock_incoming(G_GNUC_UNUSED GSocketService *svc,
                                        GSocketConnection *conn,
                                        G_GNUC_UNUSED GObject *src,
                                        G_GNUC_UNUSED gpointer data)
{
    GInputStream *in = g_io_stream_get_input_stream(G_IO_STREAM(conn));
    gchar buf[512];
    gssize n = g_input_stream_read(in, buf, sizeof(buf) - 1, NULL, NULL);
    if (n <= 0)
        return TRUE;
    buf[n] = '\0';

    if (strstr(buf, "\"stop\"") && focus_on_stop && agent_term) {
        if (tab_index >= 0)
            gtk_notebook_set_current_page(
                GTK_NOTEBOOK(geany_data->main_widgets->message_window_notebook),
                tab_index);
        g_idle_add(grab_agent_focus_idle, NULL);
    }
    return TRUE;
}

static gboolean agent_socket_init(void)
{
    agent_sock_path = g_strdup_printf("/tmp/geany-agent-%d.sock", (int)getpid());
    GSocketAddress *addr = g_unix_socket_address_new(agent_sock_path);
    agent_sock_service = g_socket_service_new();

    GError *err = NULL;
    if (!g_socket_listener_add_address(G_SOCKET_LISTENER(agent_sock_service),
                                       addr, G_SOCKET_TYPE_STREAM,
                                       G_SOCKET_PROTOCOL_DEFAULT,
                                       NULL, NULL, &err)) {
        g_warning("geanyagent: socket bind failed: %s", err->message);
        g_error_free(err);
        g_object_unref(addr);
        g_object_unref(agent_sock_service);
        agent_sock_service = NULL;
        g_free(agent_sock_path);
        agent_sock_path = NULL;
        return FALSE;
    }
    g_object_unref(addr);

    g_signal_connect(agent_sock_service, "incoming",
                     G_CALLBACK(on_agent_sock_incoming), NULL);
    g_socket_service_start(agent_sock_service);
    g_setenv("GEANY_AGENT_SOCK", agent_sock_path, TRUE);
    return TRUE;
}

static void agent_socket_cleanup(void)
{
    if (agent_sock_service) {
        g_socket_service_stop(agent_sock_service);
        g_object_unref(agent_sock_service);
        agent_sock_service = NULL;
    }
    if (agent_sock_path) {
        unlink(agent_sock_path);
        g_unsetenv("GEANY_AGENT_SOCK");
        g_free(agent_sock_path);
        agent_sock_path = NULL;
    }
}


static void ga_send_command(const gchar *cmd)
{
	if (!agent_term || !cmd || !*cmd)
		return;
	if (tab_index >= 0)
	{
		gtk_notebook_set_current_page(
		    GTK_NOTEBOOK(geany_data->main_widgets->message_window_notebook),
		    tab_index);
	}
#if VTE_CHECK_VERSION(0, 70, 0)
	VtePty *pty = vte_terminal_get_pty(agent_term);
	if (pty) {
		int fd = vte_pty_get_fd(pty);
		write(fd, "\x15", 1);
		write(fd, cmd, strlen(cmd));
		write(fd, "\n", 1);
	}
#else
	vte_terminal_feed_child(agent_term, "\x15", 1);
	vte_terminal_feed_child(agent_term, cmd, (gssize)strlen(cmd));
	vte_terminal_feed_child(agent_term, "\n", 1);
#endif
	/* Grab focus after GTK has processed the page-switch events so the
	 * editor cannot reclaim it when the notebook switch settles. */
	g_idle_add(grab_agent_focus_idle, NULL);
}

/* Split expanded command on ';' and send each part to the agent. */
static void ga_run_agent_cmd(const gchar *expanded)
{
	gchar **parts = g_strsplit(expanded, ";", -1);
	for (gint i = 0; parts[i]; i++) {
		gchar *part = g_strstrip(parts[i]);
		if (*part)
			ga_send_command(part);
	}
	g_strfreev(parts);
}


/* ------------------------------------------------------------------ */
/* Agent tools config                                                  */

/* Expand %p %d %f %e %l %r %F in tmpl using filepath.
 * %p = absolute path, %d = directory, %f = filename, %e = stem (no ext),
 * %l = current line, %r = project root, %F = path relative to root.
 * Caller frees result. */
static gchar *agent_tools_format_cmd(const gchar *tmpl, const gchar *filepath)
{
	GString     *out  = g_string_sized_new(256);
	GeanyApp    *app  = geany->app;
	gchar       *root;

	if (app->project && app->project->base_path && *app->project->base_path)
		root = g_strdup(app->project->base_path);
	else
		root = g_strdup(g_get_home_dir());

	/* When no file is open, %d falls back to project root rather than "." */
	gchar *dir  = (*filepath) ? g_path_get_dirname(filepath) : g_strdup(root);
	gchar *base = g_path_get_basename(filepath);

	const gchar *first_dot = strchr(base, '.');
	gchar *stem = first_dot ? g_strndup(base, (gsize)(first_dot - base))
	                        : g_strdup(base);

	gint line = 1;
	GeanyDocument *doc = document_get_current();
	if (doc && doc->file_name && strcmp(doc->file_name, filepath) == 0)
		line = sci_get_current_line(doc->editor->sci) + 1;

	const gchar *rel = filepath;
	gsize root_len = strlen(root);
	if (g_str_has_prefix(filepath, root) && filepath[root_len] == '/')
		rel = filepath + root_len + 1;

	for (const gchar *p = tmpl; *p; p++) {
		if (*p != '%' || !*(p + 1)) {
			g_string_append_c(out, *p);
			continue;
		}
		p++;
		switch (*p) {
			case 'p': g_string_append(out, filepath);          break;
			case 'd': g_string_append(out, dir);               break;
			case 'f': g_string_append(out, base);              break;
			case 'e': g_string_append(out, stem);              break;
			case 'l': g_string_append_printf(out, "%d", line); break;
			case 'r': g_string_append(out, root);              break;
			case 'F': g_string_append(out, rel);               break;
			case '%': g_string_append_c(out, '%');             break;
			default:
				g_string_append_c(out, '%');
				g_string_append_c(out, *p);
		}
	}
	g_free(root);
	g_free(dir);
	g_free(base);
	g_free(stem);
	return g_string_free(out, FALSE);
}

static void on_agent_tool_item_activate(GtkMenuItem *item,
                                        G_GNUC_UNUSED gpointer data)
{
	const gchar   *tmpl = g_object_get_data(G_OBJECT(item), "agent-tmpl");
	GeanyDocument *doc  = document_get_current();
	if (!tmpl)
		return;
	/* Save the current file first so the agent reads the latest version. */
	if (doc && doc->file_name)
		document_save_file(doc, FALSE);
	const gchar *filepath = (doc && doc->file_name) ? doc->file_name : "";
	gchar *expanded = agent_tools_format_cmd(tmpl, filepath);
	ga_run_agent_cmd(expanded);
	g_free(expanded);
}

static void agent_tools_load(void)
{
	if (agent_tools_config)
		g_key_file_free(agent_tools_config);
	agent_tools_config = g_key_file_new();

	/* User-level: ~/.config/geany/agenttools.conf */
	gchar *user_path = g_build_filename(geany->app->configdir,
	                                    AGENT_TOOLS_CONFIG, NULL);
	g_key_file_load_from_file(agent_tools_config, user_path, G_KEY_FILE_NONE, NULL);
	g_free(user_path);

	/* Project-level: <project>/config/geany/agenttools.conf — overlays user config */
	GeanyProject *proj = geany->app->project;
	if (!proj || !proj->base_path)
		return;

	gchar *proj_path = g_build_filename(proj->base_path, "config", "geany",
	                                    AGENT_TOOLS_CONFIG, NULL);
	GKeyFile *pkf = g_key_file_new();
	if (g_key_file_load_from_file(pkf, proj_path, G_KEY_FILE_NONE, NULL)) {
		gsize   ng;
		gchar **groups = g_key_file_get_groups(pkf, &ng);
		for (gsize gi = 0; gi < ng; gi++) {
			gsize   nk;
			gchar **keys = g_key_file_get_keys(pkf, groups[gi], &nk, NULL);
			for (gsize ki = 0; ki < nk; ki++) {
				gchar *val = g_key_file_get_string(pkf, groups[gi], keys[ki], NULL);
				g_key_file_set_string(agent_tools_config, groups[gi], keys[ki], val);
				g_free(val);
			}
			g_strfreev(keys);
		}
		g_strfreev(groups);
	}
	g_key_file_free(pkf);
	g_free(proj_path);
}

static void agent_tools_menu_populate(const gchar *filepath)
{
	if (!agent_tools_menu_item)
		return;

	gtk_menu_item_set_submenu(GTK_MENU_ITEM(agent_tools_menu_item), NULL);

	if (!filepath || !agent_tools_config || !g_str_has_suffix(filepath, ".md") ||
	    !g_key_file_has_group(agent_tools_config, AGENT_TOOLS_GROUP)) {
		gtk_widget_set_sensitive(agent_tools_menu_item, FALSE);
		return;
	}

	GtkWidget *submenu = gtk_menu_new();
	gboolean   any     = FALSE;

	for (gint i = 0; ; i++) {
		gchar *agent_key = g_strdup_printf("agent_%d", i);
		gchar *tmpl      = g_key_file_get_string(agent_tools_config,
		                                          AGENT_TOOLS_GROUP, agent_key, NULL);
		g_free(agent_key);
		if (!tmpl)
			break;

		gchar *name_key = g_strdup_printf("name_%d", i);
		gchar *name     = g_key_file_get_string(agent_tools_config,
		                                         AGENT_TOOLS_GROUP, name_key, NULL);
		g_free(name_key);

		GtkWidget *mi = gtk_menu_item_new_with_label(name ? name : tmpl);
		g_object_set_data_full(G_OBJECT(mi), "agent-tmpl", tmpl, g_free);
		g_signal_connect(mi, "activate", G_CALLBACK(on_agent_tool_item_activate), NULL);
		gtk_menu_shell_append(GTK_MENU_SHELL(submenu), mi);
		gtk_widget_show(mi);

		g_free(name);
		any = TRUE;
	}

	if (any) {
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(agent_tools_menu_item), submenu);
		gtk_widget_set_sensitive(agent_tools_menu_item, TRUE);
		gtk_widget_show(submenu);
	} else {
		gtk_widget_destroy(submenu);
		gtk_widget_set_sensitive(agent_tools_menu_item, FALSE);
	}
}

static void on_agent_doc_activate(G_GNUC_UNUSED GObject *obj,
                                   GeanyDocument *doc,
                                   G_GNUC_UNUSED gpointer data)
{
	agent_tools_menu_populate(doc ? doc->file_name : NULL);
}

static void on_agent_project_open(G_GNUC_UNUSED GObject *obj,
                                   G_GNUC_UNUSED GKeyFile *config,
                                   G_GNUC_UNUSED gpointer data)
{
	agent_tools_load();
	GeanyDocument *doc = document_get_current();
	agent_tools_menu_populate(doc ? doc->file_name : NULL);
}

static void on_agent_project_close(G_GNUC_UNUSED GObject *obj,
                                    G_GNUC_UNUSED gpointer data)
{
	agent_tools_load();
	GeanyDocument *doc = document_get_current();
	agent_tools_menu_populate(doc ? doc->file_name : NULL);
}


/* ------------------------------------------------------------------ */
/* New Prompt / New Skill helpers (also called by signal handlers)     */

static void ga_create_prompt(const gchar *name)
{
	gchar *base_path;
	GeanyApp *app = geany->app;
	if (app->project && app->project->base_path && *app->project->base_path)
		base_path = g_strdup(app->project->base_path);
	else
		base_path = g_strdup(g_get_home_dir());

	gchar *slug = g_ascii_strdown(name, -1);
	for (gchar *c = slug; *c; c++) {
		if (*c == ' ')
			*c = '_';
	}

	gchar *prompts_dir = g_build_filename(base_path, "ai-prompts", NULL);
	gchar *filename    = g_strdup_printf("%s.prompt.md", slug);
	gchar *prompt_path = g_build_filename(prompts_dir, filename, NULL);

	g_mkdir_with_parents(prompts_dir, 0755);
	if (!g_file_test(prompt_path, G_FILE_TEST_EXISTS))
		utils_write_file(prompt_path, "");

	document_open_file(prompt_path, FALSE, NULL, NULL);
	keybindings_send_command(GEANY_KEY_GROUP_FOCUS, GEANY_KEYS_FOCUS_EDITOR);

	g_free(prompt_path);
	g_free(filename);
	g_free(slug);
	g_free(prompts_dir);
	g_free(base_path);
}


/* ------------------------------------------------------------------ */
/* New Prompt dialog                                                    */

static void on_new_prompt_activate(G_GNUC_UNUSED GtkMenuItem *item,
                                   G_GNUC_UNUSED gpointer data)
{
	GtkWidget *dialog = gtk_dialog_new_with_buttons(
	    _("New Prompt"),
	    GTK_WINDOW(geany_data->main_widgets->window),
	    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
	    _("_Cancel"), GTK_RESPONSE_CANCEL,
	    _("_OK"),     GTK_RESPONSE_OK,
	    NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

	GtkWidget *entry = gtk_entry_new();
	gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
	gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "My Prompt Name");

	GtkWidget *err_label = gtk_label_new("");

	GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	gtk_container_set_border_width(GTK_CONTAINER(content_area), 8);
	gtk_box_pack_start(GTK_BOX(content_area), gtk_label_new(_("Prompt name:")), FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(content_area), entry, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(content_area), err_label, FALSE, FALSE, 2);
	gtk_widget_show_all(dialog);

	while (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
	{
		const gchar *name = gtk_entry_get_text(GTK_ENTRY(entry));
		if (!name || !*name) {
			gtk_label_set_text(GTK_LABEL(err_label), _("Name cannot be empty."));
			continue;
		}

		ga_create_prompt(name);
		break;
	}
	gtk_widget_destroy(dialog);
}


/* ------------------------------------------------------------------ */
/* Spawn / restart                                                     */

static gboolean ga_spawn_idle(gpointer data);

static void on_spawn_done(G_GNUC_UNUSED VteTerminal *term,
                          GPid pid,
                          GError *error,
                          G_GNUC_UNUSED gpointer data)
{
	if (error)
		g_warning("geanyagent: spawn failed: %s", error->message);
	else
		agent_pid = pid;
}

static void on_child_exited(G_GNUC_UNUSED VteTerminal *term,
                             G_GNUC_UNUSED int status,
                             G_GNUC_UNUSED gpointer data)
{
	agent_pid = 0;
	if (running)
		g_idle_add(ga_spawn_idle, NULL);
}

static void ga_spawn(void)
{
	const gchar *shell = g_getenv("SHELL");
	if (!shell || !*shell)
		shell = "/bin/sh";

	gchar **shell_parts = g_strsplit(shell, " ", -1);
	guint   n           = g_strv_length(shell_parts);
	gchar **argv        = g_new(gchar *, n + 3);
	guint   i;

	for (i = 0; i < n; i++)
		argv[i] = shell_parts[i];
	argv[n]     = (gchar *)"-c";
	AgentConfig *ac = agents && active_agent < (gint)agents->len
	                ? (AgentConfig *)g_ptr_array_index(agents, active_agent)
	                : NULL;

	/* Resolve working directory: configured template > project base > home */
	GeanyDocument *doc_for_spawn = document_get_current();
	const gchar *filepath = (doc_for_spawn && doc_for_spawn->file_name) ? doc_for_spawn->file_name : "";
	const gchar *raw_cmd = (ac && ac->cmd) ? ac->cmd : DEFAULT_CMD;
	gchar *expanded_cmd = agent_tools_format_cmd(raw_cmd, filepath);
	argv[n + 1] = expanded_cmd;
	argv[n + 2] = NULL;

	gchar *work_dir = NULL;
	if (ac && ac->work_dir && *ac->work_dir) {
		work_dir = agent_tools_format_cmd(ac->work_dir, filepath);
		if (!g_file_test(work_dir, G_FILE_TEST_IS_DIR)) {
			g_free(work_dir);
			work_dir = NULL;
		}
	}
	if (!work_dir) {
		GeanyApp *app = geany->app;
		if (app->project && app->project->base_path && *app->project->base_path)
			work_dir = g_strdup(app->project->base_path);
		else
			work_dir = g_strdup(g_get_home_dir());
	}

	/* Build PATH: prepend askpass_dir so our sudo wrapper takes priority */
	const gchar *old_path = g_getenv("PATH");
	gchar *new_path = askpass_dir
	    ? g_strconcat(askpass_dir, ":", old_path ? old_path : "/usr/bin:/bin", NULL)
	    : g_strdup(old_path ? old_path : "/usr/bin:/bin");

	/* strip vars that confuse nested terminals; rebuild PATH explicitly */
	const gchar *exclude[] = { "COLUMNS", "LINES", "TERM", "TERM_PROGRAM", "PATH", NULL };
	gchar **env;
	if (askpass_prog)
		env = utils_copy_environment(exclude,
		                             "TERM",         "xterm-256color",
		                             "PATH",         new_path,
		                             "SUDO_ASKPASS", askpass_prog,
		                             NULL);
	else
		env = utils_copy_environment(exclude,
		                             "TERM", "xterm-256color",
		                             "PATH", new_path,
		                             NULL);
	g_free(new_path);

	vte_terminal_spawn_async(agent_term,
	                         VTE_PTY_DEFAULT,
	                         work_dir,
	                         argv,
	                         env,
	                         G_SPAWN_SEARCH_PATH,
	                         NULL, NULL, NULL,
	                         -1,
	                         NULL,
	                         on_spawn_done,
	                         NULL);

	g_strfreev(env);
	g_free(expanded_cmd);
	g_free(work_dir);
	g_free(argv);
	g_strfreev(shell_parts);
}

static gboolean ga_spawn_idle(G_GNUC_UNUSED gpointer data)
{
	ga_spawn();
	return G_SOURCE_REMOVE;
}


static void update_agent_label(void)
{
	if (!agent_label || !agents || active_agent >= (gint)agents->len)
		return;
	AgentConfig *ac = (AgentConfig *)g_ptr_array_index(agents, active_agent);
	gchar *text = g_strdup_printf("\xf0\x9f\xa4\x96 %s", ac->name);
	gtk_label_set_text(GTK_LABEL(agent_label), text);
	g_free(text);
}

static gboolean ga_unblock_child_exited_idle(G_GNUC_UNUSED gpointer data)
{
	g_signal_handlers_unblock_by_func(agent_term,
	                                  G_CALLBACK(on_child_exited), NULL);
	return G_SOURCE_REMOVE;
}

static void ga_switch_agent(gint index)
{
	if (!agent_term || index < 0 || index >= (gint)agents->len || index == active_agent)
		return;

	active_agent = index;
	update_agent_label();
	ga_save_config();

	/* Block child-exited during switch to prevent auto-restart of old process */
	g_signal_handlers_block_by_func(agent_term,
	                                G_CALLBACK(on_child_exited), NULL);
	ga_spawn();

	/* Unblock after the event queue has drained any pending child-exited */
	g_idle_add(ga_unblock_child_exited_idle, NULL);

	/* Bring the agent tab to front and grab focus */
	if (tab_index >= 0)
		gtk_notebook_set_current_page(
		    GTK_NOTEBOOK(geany_data->main_widgets->message_window_notebook),
		    tab_index);
	g_idle_add(grab_agent_focus_idle, NULL);
}

/* Kill the running agent and let on_child_exited trigger a fresh respawn.
 * The new spawn will inherit the updated process environment (g_setenv). */
static void ga_restart(void)
{
	if (agent_pid > 0) {
		kill(agent_pid, SIGHUP);
		agent_pid = 0;
	} else {
		/* No tracked PID — try to kill the PTY foreground process group */
		VtePty *pty = agent_term ? vte_terminal_get_pty(agent_term) : NULL;
		if (pty) {
			int fd = vte_pty_get_fd(pty);
			pid_t pg = tcgetpgrp(fd);
			if (pg > 0)
				killpg(pg, SIGHUP);
		}
	}
}

/* Signal handler — emitted by geanyenv (or anything else) on geany->object */
static void on_restart_signal(G_GNUC_UNUSED GObject *obj,
                               G_GNUC_UNUSED gpointer data)
{
	ga_restart();
}

G_MODULE_EXPORT void geanyagent_restart(void)
{
	ga_restart();
}


/* ------------------------------------------------------------------ */
/* Right-click context menu                                            */

static void on_copy_activate(G_GNUC_UNUSED GtkMenuItem *item, gpointer user_data)
{
#if VTE_CHECK_VERSION(0, 50, 0)
	vte_terminal_copy_clipboard_format(VTE_TERMINAL(user_data), VTE_FORMAT_TEXT);
#else
	vte_terminal_copy_clipboard(VTE_TERMINAL(user_data));
#endif
}

static void on_paste_activate(G_GNUC_UNUSED GtkMenuItem *item, gpointer user_data)
{
	vte_terminal_paste_clipboard(VTE_TERMINAL(user_data));
}

typedef struct { VteTerminal *vte; gchar *text; } ClipCtxData;

static void on_clip_ctx_data_free(gpointer data, G_GNUC_UNUSED GClosure *closure)
{
	ClipCtxData *ctx = (ClipCtxData *)data;
	g_free(ctx->text);
	g_free(ctx);
}

static void on_add_to_context_activate(G_GNUC_UNUSED GtkMenuItem *item, gpointer user_data)
{
	ClipCtxData *ctx     = (ClipCtxData *)user_data;
	gchar       *payload = g_strconcat("@", ctx->text, NULL);
	gsize        len     = strlen(payload);

#if VTE_CHECK_VERSION(0, 70, 0)
	VtePty *pty = vte_terminal_get_pty(ctx->vte);
	if (pty)
		write(vte_pty_get_fd(pty), payload, len);
#else
	vte_terminal_feed_child(ctx->vte, payload, (gssize)len);
#endif
	g_free(payload);
}

/* Returns TRUE if text looks like a file-path argument suitable for @ context. */
static gboolean clip_looks_like_context_ref(const gchar *text)
{
	if (!text || !*text)
		return FALSE;
	if (text[0] == '/')
		return TRUE;
	/* single line, no spaces, under 255 chars */
	gsize len = strlen(text);
	return len < 255 && !strchr(text, ' ') && !strchr(text, '\n');
}

/* Intercept DnD drops on the agent VTE: prepend '@' so the path is added
 * to the agent's context.  Runs before VTE's class-level handler. */
static void
on_agent_vte_drag_data_received(GtkWidget *widget, GdkDragContext *context,
    G_GNUC_UNUSED gint x, G_GNUC_UNUSED gint y,
    GtkSelectionData *data, G_GNUC_UNUSED guint info, guint time,
    G_GNUC_UNUSED gpointer user_data)
{
    GdkAtom target        = gtk_selection_data_get_target(data);
    GdkAtom text_plain    = gdk_atom_intern_static_string("text/plain");
    GdkAtom text_uri_list = gdk_atom_intern_static_string("text/uri-list");
    gchar  *path          = NULL;

    if (target == text_plain)
    {
        const guchar *raw = gtk_selection_data_get_data(data);
        gint len = gtk_selection_data_get_length(data);
        if (raw && len > 0)
        {
            path = g_strndup((const gchar *)raw, len);
            g_strstrip(path);
        }
        if (!path || !clip_looks_like_context_ref(path))
        {
            g_free(path);
            return; /* not a path — let VTE handle normally */
        }
    }
    else if (target == text_uri_list)
    {
        gchar **uris = gtk_selection_data_get_uris(data);
        if (uris && uris[0])
            path = g_filename_from_uri(uris[0], NULL, NULL);
        g_strfreev(uris);
        if (!path)
            return; /* not a local file — let VTE handle */
    }
    else
        return;

    gchar *payload = g_strconcat("@", path, NULL);
    gsize  plen    = strlen(payload);
    g_free(path);

#if VTE_CHECK_VERSION(0, 70, 0)
    VtePty *pty = vte_terminal_get_pty(agent_term);
    if (pty)
        write(vte_pty_get_fd(pty), payload, plen);
#else
    vte_terminal_feed_child(agent_term, payload, (gssize)plen);
#endif
    g_free(payload);

    gtk_drag_finish(context, TRUE, FALSE, time);
    g_signal_stop_emission_by_name(widget, "drag-data-received");
}


/* ------------------------------------------------------------------ */
/* Editor notebook tab right-click — "Add to Agent"                   */

/* Return the notebook page index whose tab label covers (event_x, event_y),
 * or -1 if no tab was hit.  Coordinates are in the notebook widget's frame. */
static gint notebook_tab_at(GtkNotebook *nb, gdouble event_x, gdouble event_y)
{
    gint n = gtk_notebook_get_n_pages(nb);
    for (gint i = 0; i < n; i++)
    {
        GtkWidget *page = gtk_notebook_get_nth_page(nb, i);
        GtkWidget *tab  = gtk_notebook_get_tab_label(nb, page);
        if (!tab || !gtk_widget_get_visible(tab))
            continue;
        GtkAllocation alloc;
        gtk_widget_get_allocation(tab, &alloc);
        if (event_x >= alloc.x && event_x < alloc.x + alloc.width &&
            event_y >= alloc.y && event_y < alloc.y + alloc.height)
            return i;
    }
    return -1;
}

/* Find the GeanyDocument whose Scintilla widget is the nth notebook page. */
static GeanyDocument *doc_from_notebook_page(GtkNotebook *nb, gint page)
{
    if (page < 0)
        return NULL;
    GtkWidget *page_widget = gtk_notebook_get_nth_page(nb, page);
    if (!page_widget)
        return NULL;
    guint len = geany_data->documents_array->len;
    for (guint i = 0; i < len; i++)
    {
        GeanyDocument *d = (GeanyDocument *)geany_data->documents_array->pdata[i];
        if (d && d->is_valid && GTK_WIDGET(d->editor->sci) == page_widget)
            return d;
    }
    return NULL;
}

/* Return file_path relative to the open project's base_path, or file_path as-is. */
static gchar *path_for_agent(const gchar *file_path)
{
    if (!file_path)
        return NULL;
    if (geany->app->project && geany->app->project->base_path)
    {
        const gchar *base    = geany->app->project->base_path;
        gsize        baselen = strlen(base);
        if (g_str_has_prefix(file_path, base))
        {
            const gchar *rel = file_path + baselen;
            while (*rel == G_DIR_SEPARATOR)
                rel++;
            if (*rel)
                return g_strdup(rel);
        }
    }
    return g_strdup(file_path);
}

static void on_tab_add_to_agent(G_GNUC_UNUSED GtkMenuItem *item, gpointer user_data)
{
    const gchar *file_path = (const gchar *)user_data;
    if (!agent_term || !file_path)
        return;

    gchar *payload = g_strconcat("@", file_path, NULL);
    gsize  len     = strlen(payload);

#if VTE_CHECK_VERSION(0, 70, 0)
    VtePty *pty = vte_terminal_get_pty(agent_term);
    if (pty)
        write(vte_pty_get_fd(pty), payload, len);
#else
    vte_terminal_feed_child(agent_term, payload, (gssize)len);
#endif
    g_free(payload);
}

static gboolean on_editor_notebook_button_press(GtkWidget   *widget,
                                                GdkEventButton *event,
                                                G_GNUC_UNUSED gpointer data)
{
    if (event->button != 3)
        return FALSE;

    GtkNotebook   *nb   = GTK_NOTEBOOK(widget);
    gint           page = notebook_tab_at(nb, event->x, event->y);
    if (page < 0)
        return FALSE;

    GeanyDocument *doc = doc_from_notebook_page(nb, page);
    if (!doc || !doc->file_name)
        return FALSE;

    gchar *rel_path = path_for_agent(doc->file_name);

    GtkWidget *menu     = gtk_menu_new();
    GtkWidget *add_item = gtk_menu_item_new_with_label(_("Add to Agent"));
    gtk_widget_set_sensitive(add_item, agent_term != NULL);
    g_signal_connect_data(add_item, "activate",
                          G_CALLBACK(on_tab_add_to_agent),
                          rel_path, (GClosureNotify)g_free, 0);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), add_item);
    gtk_widget_show_all(menu);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);

    return TRUE;
}


static void on_rename_tab_activate(G_GNUC_UNUSED GtkMenuItem *item,
                                   G_GNUC_UNUSED gpointer data)
{
	if (!agent_label)
		return;

	GtkWidget *dialog = gtk_dialog_new_with_buttons(
	    _("Rename Tab"),
	    GTK_WINDOW(geany_data->main_widgets->window),
	    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
	    _("_Cancel"), GTK_RESPONSE_CANCEL,
	    _("_OK"),     GTK_RESPONSE_OK,
	    NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

	GtkWidget *entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(entry), gtk_label_get_text(GTK_LABEL(agent_label)));
	gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);

	GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	gtk_container_set_border_width(GTK_CONTAINER(content), 8);
	gtk_box_pack_start(GTK_BOX(content), gtk_label_new(_("Tab title:")), FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(content), entry, FALSE, FALSE, 2);
	gtk_widget_show_all(dialog);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
	{
		const gchar *new_title = gtk_entry_get_text(GTK_ENTRY(entry));
		if (new_title && *new_title)
			gtk_label_set_text(GTK_LABEL(agent_label), new_title);
	}
	gtk_widget_destroy(dialog);
}

static void ga_create_skill(const gchar *name)
{
	gchar *base_path;
	GeanyApp *app = geany->app;
	if (app->project && app->project->base_path && *app->project->base_path)
		base_path = g_strdup(app->project->base_path);
	else
		base_path = g_strdup(g_get_home_dir());

	gchar *skill_dir  = g_build_filename(base_path, ".claude", "skills", name, NULL);
	gchar *skill_path = g_build_filename(skill_dir, "SKILL.md", NULL);

	g_mkdir_with_parents(skill_dir, 0755);

	gchar *file_content = g_strdup_printf(SKILL_TEMPLATE, name);
	utils_write_file(skill_path, file_content);
	g_free(file_content);

	document_open_file(skill_path, FALSE, NULL, NULL);

	g_free(skill_path);
	g_free(skill_dir);
	g_free(base_path);
}

static void on_new_skill_activate(G_GNUC_UNUSED GtkMenuItem *item,
                                  G_GNUC_UNUSED gpointer data)
{
	GtkWidget *dialog = gtk_dialog_new_with_buttons(
	    _("New Skill"),
	    GTK_WINDOW(geany_data->main_widgets->window),
	    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
	    _("_Cancel"), GTK_RESPONSE_CANCEL,
	    _("_OK"),     GTK_RESPONSE_OK,
	    NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

	GtkWidget *entry = gtk_entry_new();
	gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
	gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "my-skill");

	GtkWidget *err_label = gtk_label_new("");

	GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	gtk_container_set_border_width(GTK_CONTAINER(content_area), 8);
	gtk_box_pack_start(GTK_BOX(content_area), gtk_label_new(_("Skill name (no spaces):")), FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(content_area), entry, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(content_area), err_label, FALSE, FALSE, 2);
	gtk_widget_show_all(dialog);

	while (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
	{
		const gchar *name = gtk_entry_get_text(GTK_ENTRY(entry));
		if (!name || !*name)
		{
			gtk_label_set_text(GTK_LABEL(err_label), _("Name cannot be empty."));
			continue;
		}
		if (strchr(name, ' '))
		{
			gtk_label_set_text(GTK_LABEL(err_label), _("Name cannot contain spaces."));
			continue;
		}

		ga_create_skill(name);
		break;
	}
	gtk_widget_destroy(dialog);
}

static void on_internet_search_activate(G_GNUC_UNUSED GtkMenuItem *item, gpointer user_data)
{
	const gchar *text = (const gchar *)user_data;
	if (!text || !*text)
		return;

	gchar *encoded = g_uri_escape_string(text, NULL, TRUE);
	const gchar *tmpl = search_url ? search_url : DEFAULT_SEARCH_URL;
	gchar **parts = g_strsplit(tmpl, "%s", 2);
	gchar *url;
	if (parts && parts[0] && parts[1])
		url = g_strconcat(parts[0], encoded, parts[1], NULL);
	else
		url = g_strdup(encoded);
	g_strfreev(parts);
	g_free(encoded);
	gtk_show_uri(NULL, url, GDK_CURRENT_TIME, NULL);
	g_free(url);
}

static void on_switch_agent_activate(GtkMenuItem *item, gpointer user_data)
{
	gint *idx = (gint *)user_data;
	if (*idx != active_agent && gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item)))
		ga_switch_agent(*idx);
}

static void on_send_to_cli_activate(G_GNUC_UNUSED GtkMenuItem *item, gpointer user_data)
{
	const gchar *text = (const gchar *)user_data;
	if (!text || !*text)
		return;
	GType obj_type = G_OBJECT_TYPE(geany->object);
	if (g_signal_lookup("geanycli-feed-text", obj_type))
		g_signal_emit_by_name(geany->object, "geanycli-feed-text", text);
}

static gboolean on_vte_button_press(GtkWidget *widget,
                                    GdkEventButton *event,
                                    G_GNUC_UNUSED gpointer data)
{
	if (event->button != 3)
		return FALSE;

	VteTerminal *vte        = VTE_TERMINAL(widget);
	gboolean     has_sel    = vte_terminal_get_has_selection(vte);

	/* Capture selection to CLIPBOARD now, before the menu event loop may
	 * clear VTE's selection state. */
	gchar *sel_text = NULL;
	if (has_sel)
	{
#if VTE_CHECK_VERSION(0, 50, 0)
		vte_terminal_copy_clipboard_format(vte, VTE_FORMAT_TEXT);
#else
		vte_terminal_copy_clipboard(vte);
#endif
		GtkClipboard *sel_cb = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
		sel_text = gtk_clipboard_wait_for_text(sel_cb);
		if (sel_text)
			g_strstrip(sel_text);
	}

	GtkWidget *menu = gtk_menu_new();

	GtkWidget *copy_item = gtk_menu_item_new_with_label(_("Copy"));
	gtk_widget_set_sensitive(copy_item, has_sel);
	g_signal_connect(copy_item, "activate", G_CALLBACK(on_copy_activate), vte);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), copy_item);

	GtkWidget *paste_item = gtk_menu_item_new_with_label(_("Paste"));
	g_signal_connect(paste_item, "activate", G_CALLBACK(on_paste_activate), vte);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), paste_item);

	if (sel_text && *sel_text)
	{
		GType obj_type = G_OBJECT_TYPE(geany->object);
		if (g_signal_lookup("geanycli-feed-text", obj_type))
		{
			GtkWidget *send_cli_item = gtk_menu_item_new_with_label(_("Send to CLI"));
			g_signal_connect_data(send_cli_item, "activate",
			                      G_CALLBACK(on_send_to_cli_activate),
			                      g_strdup(sel_text), (GClosureNotify)g_free, 0);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), send_cli_item);
		}
	}
	g_free(sel_text);

	gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

	if (agents && agents->len > 1)
	{
		GtkWidget *agent_submenu = gtk_menu_new();
		GtkWidget *agent_item = gtk_menu_item_new_with_label(_("Switch Agent"));
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(agent_item), agent_submenu);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), agent_item);

		GSList *group = NULL;
		for (guint i = 0; i < agents->len; i++)
		{
			AgentConfig *ac = (AgentConfig *)g_ptr_array_index(agents, i);
			GtkWidget *mi = gtk_radio_menu_item_new_with_label(group, ac->name);
			group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(mi));
			if ((gint)i == active_agent)
				gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mi), TRUE);
			gint *idx = g_new(gint, 1);
			*idx = (gint)i;
			g_signal_connect_data(mi, "activate",
			                      G_CALLBACK(on_switch_agent_activate),
			                      idx, (GClosureNotify)g_free, 0);
			gtk_menu_shell_append(GTK_MENU_SHELL(agent_submenu), mi);
		}
		gtk_widget_show(agent_submenu);
	}

	gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

	GtkWidget *rename_item = gtk_menu_item_new_with_label(_("Rename Tab"));
	g_signal_connect(rename_item, "activate", G_CALLBACK(on_rename_tab_activate), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), rename_item);

	GtkWidget *new_skill_item = gtk_menu_item_new_with_label(_("New Skill"));
	g_signal_connect(new_skill_item, "activate", G_CALLBACK(on_new_skill_activate), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), new_skill_item);

	GtkWidget *new_prompt_item = gtk_menu_item_new_with_label(_("New Prompt"));
	g_signal_connect(new_prompt_item, "activate", G_CALLBACK(on_new_prompt_activate), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), new_prompt_item);

	GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	gchar        *clip_text = gtk_clipboard_wait_for_text(clipboard);
	if (clip_text)
	{
		g_strstrip(clip_text);
		if (*clip_text)
		{
			gtk_menu_shell_append(GTK_MENU_SHELL(menu),
			                      gtk_separator_menu_item_new());

			if (clip_looks_like_context_ref(clip_text))
			{
				ClipCtxData *ctx = g_new(ClipCtxData, 1);
				ctx->vte  = vte;
				ctx->text = g_strdup(clip_text);

				GtkWidget *ctx_item = gtk_menu_item_new_with_label(_("Add to Context"));
				g_signal_connect_data(ctx_item, "activate",
				                      G_CALLBACK(on_add_to_context_activate),
				                      ctx, on_clip_ctx_data_free, 0);
				gtk_menu_shell_append(GTK_MENU_SHELL(menu), ctx_item);
			}

			gchar *search_text = g_strdup(clip_text);
			GtkWidget *search_item = gtk_menu_item_new_with_label(_("Internet Search"));
			g_signal_connect_data(search_item, "activate",
			                      G_CALLBACK(on_internet_search_activate),
			                      search_text, (GClosureNotify)g_free, 0);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), search_item);
		}
		g_free(clip_text);
	}

	gtk_widget_show_all(menu);
	gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);

	return TRUE;
}


/* ------------------------------------------------------------------ */
/* Widget construction                                                 */

static void create_agent_tab(void)
{
	GtkWidget *vte, *scrollbar, *label;

	vte = vte_terminal_new();
	agent_term = VTE_TERMINAL(vte);

	scrollbar = gtk_scrollbar_new(GTK_ORIENTATION_VERTICAL,
	                              gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(vte)));
	gtk_widget_set_can_focus(scrollbar, FALSE);

	agent_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(agent_hbox), vte, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(agent_hbox), scrollbar, FALSE, FALSE, 0);

	gtk_widget_set_size_request(vte, 10, 10);
	vte_terminal_set_size(agent_term, 80, 24);
	vte_terminal_set_mouse_autohide(agent_term, TRUE);
	vte_terminal_set_scrollback_lines(agent_term, 10000);

	g_signal_connect(agent_term, "child-exited",
	                 G_CALLBACK(on_child_exited), NULL);
	g_signal_connect(vte, "button-press-event",
	                 G_CALLBACK(on_vte_button_press), NULL);
	/* Connect before VTE's class handler so we can intercept file-path drops
	 * and feed them as @path context references instead of pasting raw text. */
	g_signal_connect(vte, "drag-data-received",
	                 G_CALLBACK(on_agent_vte_drag_data_received), NULL);

	gtk_widget_show_all(agent_hbox);

	/* Tag the page widget so geanycontrol's switch-tab can find it by keyword "agent" */
	g_object_set_data_full(G_OBJECT(agent_hbox), "geanyagent-tab-keywords",
	                       g_strdup("agent"), g_free);

	/* 🤖 Agent — UTF-8 encoded inline */
	label = gtk_label_new(NULL);
	agent_label = label;
	update_agent_label();
	tab_index = gtk_notebook_append_page(
	    GTK_NOTEBOOK(geany_data->main_widgets->message_window_notebook),
	    agent_hbox, label);
}


/* ------------------------------------------------------------------ */
/* Config                                                              */

static void ga_load_config(void)
{
	GKeyFile *kf   = g_key_file_new();
	gboolean  migrated = FALSE;

	agents_free();
	agents = g_ptr_array_new();
	active_agent = 0;

	if (g_key_file_load_from_file(kf, config_file, G_KEY_FILE_NONE, NULL))
	{
		gchar *agents_str = g_key_file_get_string(kf, "agent", "agents", NULL);

		if (agents_str)
		{
			gchar **keys = g_strsplit(agents_str, ",", -1);
			for (gint i = 0; keys[i]; i++)
			{
				gchar *key = g_strstrip(keys[i]);
				if (!*key)
					continue;
				gchar *group = g_strdup_printf("agent:%s", key);
				if (g_key_file_has_group(kf, group))
				{
					AgentConfig *ac = g_new0(AgentConfig, 1);
					ac->key      = g_strdup(key);
					ac->name     = g_key_file_get_string(kf, group, "name", NULL);
					ac->cmd      = g_key_file_get_string(kf, group, "cmd", NULL);
					ac->work_dir = g_key_file_get_string(kf, group, "work_dir", NULL);
					if (!ac->name)
						ac->name = g_strdup(key);
					g_ptr_array_add(agents, ac);
				}
				g_free(group);
			}
			g_strfreev(keys);
			g_free(agents_str);

			gchar *active_str = g_key_file_get_string(kf, "agent", "active", NULL);
			if (active_str)
			{
				for (guint i = 0; i < agents->len; i++)
				{
					AgentConfig *a = (AgentConfig *)g_ptr_array_index(agents, i);
					if (g_strcmp0(a->key, active_str) == 0)
					{
						active_agent = (gint)i;
						break;
					}
				}
				g_free(active_str);
			}
		}

		/* Legacy: no agents key — migrate single cmd to new format */
		if (agents->len == 0)
		{
			gchar *v = g_key_file_get_string(kf, "agent", "cmd", NULL);
			gchar *cmd = v ? v : g_strdup(DEFAULT_CMD);
			AgentConfig *ac = g_new0(AgentConfig, 1);

			/* Derive key and display name from the command */
			gchar *base = g_path_get_basename(cmd);
			ac->key = base; /* takes ownership */

			if (*base)
			{
				gchar *n = g_strdup(base);
				n[0] = g_ascii_toupper(n[0]);
				ac->name = n;
			}
			else
			{
				ac->name = g_strdup("Agent");
			}
			ac->cmd  = cmd;
			g_ptr_array_add(agents, ac);
			migrated = TRUE;
		}

		use_askpass = g_key_file_get_boolean(kf, "agent", "use_askpass", NULL);
		if (g_key_file_has_key(kf, "agent", "focus_on_stop", NULL))
			focus_on_stop = g_key_file_get_boolean(kf, "agent", "focus_on_stop", NULL);
		else
			focus_on_stop = TRUE;
		gchar *su = g_key_file_get_string(kf, "agent", "search_url", NULL);
		if (su)
		{
			g_free(search_url);
			search_url = su;
		}
	}

	g_key_file_free(kf);

	/* Guarantee at least one agent */
	if (agents->len == 0)
	{
		AgentConfig *ac = g_new0(AgentConfig, 1);
		ac->key  = g_strdup(DEFAULT_CMD);
		ac->name = g_strdup_printf("%c%s",
		           g_ascii_toupper(DEFAULT_CMD[0]), DEFAULT_CMD + 1);
		ac->cmd  = g_strdup(DEFAULT_CMD);
		g_ptr_array_add(agents, ac);
		migrated = TRUE;
	}

	if (!search_url)
		search_url = g_strdup(DEFAULT_SEARCH_URL);

	/* Persist migration so the next load finds the new format */
	if (migrated)
		ga_save_config();
}

static void ga_save_config(void)
{
	GKeyFile *kf   = g_key_file_new();
	gchar    *data = NULL;

	/* Build comma-separated agent key list */
	GString *keys = g_string_new("");
	for (guint i = 0; i < agents->len; i++)
	{
		AgentConfig *ac = (AgentConfig *)g_ptr_array_index(agents, i);
		if (i > 0)
			g_string_append_c(keys, ',');
		g_string_append(keys, ac->key);

		gchar *group = g_strdup_printf("agent:%s", ac->key);
		g_key_file_set_string(kf, group, "name", ac->name);
		g_key_file_set_string(kf, group, "cmd",  ac->cmd);
		if (ac->work_dir && *ac->work_dir)
			g_key_file_set_string(kf, group, "work_dir", ac->work_dir);
		g_free(group);
	}

	g_key_file_set_string(kf, "agent", "agents", keys->str);
	g_string_free(keys, TRUE);

	AgentConfig *active = (AgentConfig *)g_ptr_array_index(agents, active_agent);
	g_key_file_set_string(kf, "agent", "active", active->key);

	/* Legacy single cmd field for backward compat */
	g_key_file_set_string(kf,  "agent", "cmd",          active->cmd);
	g_key_file_set_boolean(kf, "agent", "use_askpass",  use_askpass);
	g_key_file_set_boolean(kf, "agent", "focus_on_stop", focus_on_stop);
	g_key_file_set_string(kf,  "agent", "search_url",   search_url ? search_url : DEFAULT_SEARCH_URL);

	data = g_key_file_to_data(kf, NULL, NULL);
	utils_write_file(config_file, data);
	g_free(data);
	g_key_file_free(kf);
}


/* ------------------------------------------------------------------ */
/* Status-bar focus toggle                                             */

static void focus_status_update(void)
{
    if (!focus_label)
        return;
    /* ⊙ = U+2299 CIRCLED DOT, ⊗ = U+2297 CIRCLED TIMES */
    const gchar *sym   = focus_on_stop ? "\xe2\x8a\x99" : "\xe2\x8a\x97";
    const gchar *color = focus_on_stop ? "#22aa22"       : "#666666";
    gchar *markup = g_strdup_printf("<span foreground='%s'>%s</span>", color, sym);
    gtk_label_set_markup(GTK_LABEL(focus_label), markup);
    g_free(markup);
}

static gboolean on_focus_toggle_click(G_GNUC_UNUSED GtkWidget *w,
                                       GdkEventButton *event,
                                       G_GNUC_UNUSED gpointer data)
{
    if (event->button != 1)
        return FALSE;
    focus_on_stop = !focus_on_stop;
    focus_status_update();
    ga_save_config();
    return TRUE;
}

static void create_focus_toggle(void)
{
    GtkWidget *pb = geany->main_widgets->progressbar;
    if (!pb)
        return;
    GtkWidget *sb_box = gtk_widget_get_parent(pb);
    if (!sb_box || !GTK_IS_BOX(sb_box))
        return;

    focus_label = gtk_label_new(NULL);
    focus_status_update();
    gtk_widget_show(focus_label);

    focus_event_box = gtk_event_box_new();
    gtk_container_add(GTK_CONTAINER(focus_event_box), focus_label);
    gtk_widget_set_tooltip_text(focus_event_box,
                                "Click to toggle auto-focus on agent response");
    gtk_widget_show(focus_event_box);
    gtk_box_pack_end(GTK_BOX(sb_box), focus_event_box, FALSE, FALSE, 4);

    g_signal_connect(focus_event_box, "button-press-event",
                     G_CALLBACK(on_focus_toggle_click), NULL);
}


/* ------------------------------------------------------------------ */
/* Configure dialog                                                    */

typedef struct {
	GtkWidget *agent_combo;
	GtkWidget *name_entry;
	GtkWidget *cmd_entry;
	GtkWidget *work_dir_entry;
	GtkWidget *askpass_check;
	GtkWidget *focus_check;
	GtkWidget *search_url_entry;
} ConfigWidgets;

static void on_agent_combo_changed(GtkComboBox *combo, ConfigWidgets *cw)
{
	gint idx = gtk_combo_box_get_active(combo);
	if (idx < 0 || idx >= (gint)agents->len)
		return;
	AgentConfig *ac = (AgentConfig *)g_ptr_array_index(agents, idx);
	gtk_entry_set_text(GTK_ENTRY(cw->name_entry),     ac->name     ? ac->name     : "");
	gtk_entry_set_text(GTK_ENTRY(cw->cmd_entry),      ac->cmd      ? ac->cmd      : "");
	gtk_entry_set_text(GTK_ENTRY(cw->work_dir_entry), ac->work_dir ? ac->work_dir : "");
}

static void on_agent_add_clicked(G_GNUC_UNUSED GtkButton *btn, ConfigWidgets *cw)
{
	gchar *key = g_strdup_printf("agent_%d", (gint)agents->len + 1);
	AgentConfig *ac = g_new0(AgentConfig, 1);
	ac->key  = key;
	ac->name = g_strdup("New Agent");
	ac->cmd  = g_strdup(DEFAULT_CMD);
	g_ptr_array_add(agents, ac);

	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(cw->agent_combo), ac->name);
	gtk_combo_box_set_active(GTK_COMBO_BOX(cw->agent_combo), (gint)agents->len - 1);
}

static void on_agent_remove_clicked(G_GNUC_UNUSED GtkButton *btn, ConfigWidgets *cw)
{
	if (agents->len <= 1)
		return;
	gint idx = gtk_combo_box_get_active(GTK_COMBO_BOX(cw->agent_combo));
	if (idx < 0)
		return;

	agent_config_free((AgentConfig *)g_ptr_array_index(agents, idx));
	g_ptr_array_remove_index(agents, idx);

	if (active_agent >= (gint)agents->len)
		active_agent = (gint)agents->len - 1;
	if (active_agent < 0)
		active_agent = 0;

	/* Rebuild combo */
	gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(cw->agent_combo));
	for (guint i = 0; i < agents->len; i++)
	{
		AgentConfig *a = (AgentConfig *)g_ptr_array_index(agents, i);
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(cw->agent_combo), a->name);
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(cw->agent_combo), active_agent);

	update_agent_label();
	ga_save_config();
}

static void on_configure_response(G_GNUC_UNUSED GtkDialog *dialog,
                                  gint response, ConfigWidgets *cw)
{
	if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY)
	{
		gint idx = gtk_combo_box_get_active(GTK_COMBO_BOX(cw->agent_combo));
		if (idx >= 0 && idx < (gint)agents->len)
		{
			AgentConfig *ac = (AgentConfig *)g_ptr_array_index(agents, idx);
			g_free(ac->name);
			ac->name = g_strdup(gtk_entry_get_text(GTK_ENTRY(cw->name_entry)));
			g_free(ac->cmd);
			ac->cmd  = g_strdup(gtk_entry_get_text(GTK_ENTRY(cw->cmd_entry)));
			g_free(ac->work_dir);
			const gchar *wd = gtk_entry_get_text(GTK_ENTRY(cw->work_dir_entry));
			ac->work_dir = (wd && *wd) ? g_strdup(wd) : NULL;
			active_agent = idx;
		}
		use_askpass = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cw->askpass_check));
		focus_on_stop = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cw->focus_check));
		focus_status_update();
		const gchar *new_url = gtk_entry_get_text(GTK_ENTRY(cw->search_url_entry));
		g_free(search_url);
		search_url  = g_strdup(new_url && *new_url ? new_url : DEFAULT_SEARCH_URL);
		update_agent_label();
		ga_save_config();
		ga_restart();
	}
	g_free(cw);
}

static GtkWidget *ga_configure(G_GNUC_UNUSED GeanyPlugin *plugin,
                               GtkDialog *dialog,
                               G_GNUC_UNUSED gpointer data)
{
	ConfigWidgets *cw   = g_new0(ConfigWidgets, 1);
	GtkWidget     *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);

	/* Agent selection row */
	GtkWidget *agent_hdr = gtk_label_new(_("Agents:"));
	gtk_widget_set_halign(agent_hdr, GTK_ALIGN_START);

	cw->agent_combo = gtk_combo_box_text_new();
	for (guint i = 0; i < agents->len; i++)
	{
		AgentConfig *a = (AgentConfig *)g_ptr_array_index(agents, i);
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(cw->agent_combo), a->name);
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(cw->agent_combo), active_agent);

	GtkWidget *add_btn    = gtk_button_new_with_label(_("Add"));
	GtkWidget *remove_btn = gtk_button_new_with_label(_("Remove"));

	GtkWidget *agent_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_box_pack_start(GTK_BOX(agent_row), cw->agent_combo, TRUE,  TRUE,  0);
	gtk_box_pack_start(GTK_BOX(agent_row), add_btn,         FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(agent_row), remove_btn,      FALSE, FALSE, 0);

	GtkWidget *name_label = gtk_label_new(_("Name:"));
	gtk_widget_set_halign(name_label, GTK_ALIGN_START);
	cw->name_entry = gtk_entry_new();

	GtkWidget *cmd_label = gtk_label_new(_("Command:"));
	gtk_widget_set_halign(cmd_label, GTK_ALIGN_START);
	cw->cmd_entry = gtk_entry_new();

	GtkWidget *work_dir_label = gtk_label_new(_("Working directory (empty = project root):"));
	gtk_widget_set_halign(work_dir_label, GTK_ALIGN_START);
	cw->work_dir_entry = gtk_entry_new();
	gtk_entry_set_placeholder_text(GTK_ENTRY(cw->work_dir_entry), "%r");
	gtk_widget_set_tooltip_text(cw->work_dir_entry,
	    "Optional working directory for the agent process.\n"
	    "Supports the same replacements as filetypetools.conf:\n"
	    "  %r = project root   %d = current file's directory\n"
	    "  %p = current file (absolute)   %f = filename\n"
	    "  %e = stem (no extension)   %F = file relative to root\n"
	    "Leave empty to use the project root (or $HOME if no project is open).");

	/* Load current agent's values */
	{
		AgentConfig *ac = (AgentConfig *)g_ptr_array_index(agents, active_agent);
		gtk_entry_set_text(GTK_ENTRY(cw->name_entry),     ac->name     ? ac->name     : "");
		gtk_entry_set_text(GTK_ENTRY(cw->cmd_entry),      ac->cmd      ? ac->cmd      : "");
		gtk_entry_set_text(GTK_ENTRY(cw->work_dir_entry), ac->work_dir ? ac->work_dir : "");
	}

	cw->askpass_check = gtk_check_button_new_with_label(
	    _("Intercept sudo with GUI password dialog (Linux only, requires zenity or ssh-askpass)"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cw->askpass_check), use_askpass);
	gtk_widget_set_tooltip_text(cw->askpass_check,
	    "When enabled, a sudo wrapper is injected into PATH so that sudo commands\n"
	    "issued by the agent show a graphical password dialog instead of prompting\n"
	    "on the terminal. This prevents passwords appearing in command strings or history.");

	cw->focus_check = gtk_check_button_new_with_label(
	    _("Auto-focus agent tab when Claude finishes responding (Stop hook)"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cw->focus_check), focus_on_stop);
	gtk_widget_set_tooltip_text(cw->focus_check,
	    "When enabled and GEANY_AGENT_SOCK receives {\"event\":\"stop\"},\n"
	    "the agent tab is brought to the front and focused so you can\n"
	    "immediately type a reply.\n\n"
	    "Hook entry for ~/.config/claude/settings.json:\n"
	    "  \"Stop\": [{\"type\":\"command\",\"command\":\n"
	    "    \"printf '{\\\"event\\\":\\\"stop\\\"}' | nc -U \\\"$GEANY_AGENT_SOCK\\\"\"}]");

	GtkWidget *search_url_label = gtk_label_new(_("Search URL (%s = query):"));
	gtk_widget_set_halign(search_url_label, GTK_ALIGN_START);
	cw->search_url_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(cw->search_url_entry),
	                   search_url ? search_url : DEFAULT_SEARCH_URL);
	gtk_widget_set_tooltip_text(cw->search_url_entry,
	    "URL template for Internet Search. %s is replaced with the URL-encoded query.\n"
	    "Example: https://www.qwant.com/?q=%s");

	/* Pack everything */
	gtk_box_pack_start(GTK_BOX(vbox), agent_hdr,        FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), agent_row,         FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), gtk_label_new(""), FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(vbox), name_label,        FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), cw->name_entry,    FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), cmd_label,          FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), cw->cmd_entry,      FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), work_dir_label,     FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), cw->work_dir_entry, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), gtk_label_new(""),  FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(vbox), cw->askpass_check,  FALSE, FALSE, 4);
	gtk_box_pack_start(GTK_BOX(vbox), cw->focus_check,     FALSE, FALSE, 4);
	gtk_box_pack_start(GTK_BOX(vbox), search_url_label,    FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), cw->search_url_entry, FALSE, FALSE, 0);

	g_signal_connect(cw->agent_combo, "changed",
	                 G_CALLBACK(on_agent_combo_changed), cw);
	g_signal_connect(add_btn, "clicked",
	                 G_CALLBACK(on_agent_add_clicked), cw);
	g_signal_connect(remove_btn, "clicked",
	                 G_CALLBACK(on_agent_remove_clicked), cw);
	g_signal_connect(dialog, "response",
	                 G_CALLBACK(on_configure_response), cw);
	return vbox;
}


/* ------------------------------------------------------------------ */
/* geanycontrol inter-plugin signals: new-skill, new-prompt           */

static void on_signal_new_skill(G_GNUC_UNUSED GObject *obj,
                                const gchar *name,
                                G_GNUC_UNUSED gpointer data)
{
	if (name && *name)
		ga_create_skill(name);
	else
		on_new_skill_activate(NULL, NULL);
}

static void on_signal_new_prompt(G_GNUC_UNUSED GObject *obj,
                                 const gchar *name,
                                 G_GNUC_UNUSED gpointer data)
{
	if (name && *name)
		ga_create_prompt(name);
	else
		on_new_prompt_activate(NULL, NULL);
}


/* ------------------------------------------------------------------ */
/* geanycontrol inter-plugin signal: voice sends commands to agent VTE */

static void on_geanycontrol_send_to_agent(G_GNUC_UNUSED GObject *obj,
                                          const gchar *text,
                                          G_GNUC_UNUSED gpointer data)
{
	if (!agent_term || !text || !*text)
		return;
#if VTE_CHECK_VERSION(0, 70, 0)
	VtePty *pty = vte_terminal_get_pty(agent_term);
	if (pty) {
		int fd = vte_pty_get_fd(pty);
		write(fd, text, strlen(text));
	}
#else
	vte_terminal_feed_child(agent_term, text, (gssize)strlen(text));
#endif
}

/* Insert text into the agent input line (no newline), switch to the agent
 * tab, and grab focus.  Emitted by geanyprogress for opus-exec actions. */
static void on_insert_to_agent_signal(G_GNUC_UNUSED GObject *obj,
                                       const gchar *text,
                                       G_GNUC_UNUSED gpointer data)
{
	if (!agent_term || !text || !*text)
		return;
	if (tab_index >= 0)
		gtk_notebook_set_current_page(
		    GTK_NOTEBOOK(geany_data->main_widgets->message_window_notebook),
		    tab_index);
#if VTE_CHECK_VERSION(0, 70, 0)
	VtePty *pty = vte_terminal_get_pty(agent_term);
	if (pty) {
		int fd = vte_pty_get_fd(pty);
		write(fd, "\x15", 1);
		write(fd, text, strlen(text));
	}
#else
	vte_terminal_feed_child(agent_term, "\x15", 1);
	vte_terminal_feed_child(agent_term, text, (gssize)strlen(text));
#endif
	g_idle_add(grab_agent_focus_idle, NULL);
}

/* Run a command in the agent (clears line, sends command + newline).
 * Emitted by geanyprogress "Run with opus-exec" and "Continue phase N". */
static void on_run_in_agent_signal(G_GNUC_UNUSED GObject *obj,
                                    const gchar *text,
                                    G_GNUC_UNUSED gpointer data)
{
	if (!agent_term || !text || !*text)
		return;
	if (tab_index >= 0)
		gtk_notebook_set_current_page(
		    GTK_NOTEBOOK(geany_data->main_widgets->message_window_notebook),
		    tab_index);
	ga_run_agent_cmd(text);
}


/* ------------------------------------------------------------------ */
/* Plugin lifecycle                                                    */

/*
 * Claude Code Stop-hook integration
 * ----------------------------------
 * geanyagent listens on $GEANY_AGENT_SOCK (Unix domain socket).
 * Add this to ~/.config/claude/settings.json to auto-focus Geany
 * when Claude finishes responding:
 *
 *   "hooks": {
 *     "Stop": [{
 *       "type": "command",
 *       "command": "printf '{\"event\":\"stop\"}' | nc -U \"$GEANY_AGENT_SOCK\" 2>/dev/null || true"
 *     }]
 *   }
 *
 * The hook must be silent (stdout/stderr suppressed) and fast.
 * nc exits immediately after delivering the single message.
 * The toggle in the status bar (⊙/⊗) and the plugin config dialog
 * both control whether incoming stop events actually steal focus.
 */
static gboolean ga_init(GeanyPlugin *plugin, G_GNUC_UNUSED gpointer data)
{
	geany_plugin = plugin;
	geany_data   = plugin->geany_data;

	config_file = g_build_filename(geany->app->configdir,
	                               "plugins", "geanyagent",
	                               "geanyagent.conf", NULL);
	gchar *config_dir = g_path_get_dirname(config_file);
	g_mkdir_with_parents(config_dir, 0755);
	g_free(config_dir);

	ga_load_config();
	if (use_askpass)
		setup_askpass();
	create_agent_tab();
	agent_socket_init();
	create_focus_toggle();

	running = TRUE;
	g_idle_add(ga_spawn_idle, NULL);

	/* Agent tools menu */
	agent_tools_load();
	agent_tools_menu_item = gtk_menu_item_new_with_label(_("Agent Tools"));
	gtk_widget_show(agent_tools_menu_item);
	gtk_menu_shell_append(GTK_MENU_SHELL(geany_data->main_widgets->tools_menu),
	                      agent_tools_menu_item);
	GeanyDocument *cur = document_get_current();
	agent_tools_menu_populate(cur ? cur->file_name : NULL);

	plugin_signal_connect(plugin, geany->object, "document-activate", FALSE,
	                      G_CALLBACK(on_agent_doc_activate), NULL);
	plugin_signal_connect(plugin, geany->object, "document-open", FALSE,
	                      G_CALLBACK(on_agent_doc_activate), NULL);
	plugin_signal_connect(plugin, geany->object, "project-open", FALSE,
	                      G_CALLBACK(on_agent_project_open), NULL);
	plugin_signal_connect(plugin, geany->object, "project-close", FALSE,
	                      G_CALLBACK(on_agent_project_close), NULL);

	/* Register and connect inter-plugin signals regardless of load order.
	 * If geanycontrol loaded first it already registered them; if not, we do it. */
	GType obj_type = G_OBJECT_TYPE(geany->object);

	if (!g_signal_lookup("geanycontrol-send-to-agent", obj_type))
		g_signal_new("geanycontrol-send-to-agent", obj_type, G_SIGNAL_RUN_LAST,
		             0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING);
	plugin_signal_connect(plugin, geany->object, "geanycontrol-send-to-agent",
	                      FALSE, G_CALLBACK(on_geanycontrol_send_to_agent), NULL);

	if (!g_signal_lookup("geanyagent-new-skill", obj_type))
		g_signal_new("geanyagent-new-skill", obj_type, G_SIGNAL_RUN_LAST,
		             0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING);
	plugin_signal_connect(plugin, geany->object, "geanyagent-new-skill",
	                      FALSE, G_CALLBACK(on_signal_new_skill), NULL);

	if (!g_signal_lookup("geanyagent-new-prompt", obj_type))
		g_signal_new("geanyagent-new-prompt", obj_type, G_SIGNAL_RUN_LAST,
		             0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING);
	plugin_signal_connect(plugin, geany->object, "geanyagent-new-prompt",
	                      FALSE, G_CALLBACK(on_signal_new_prompt), NULL);

	if (!g_signal_lookup("geanyagent-insert-to-agent", obj_type))
		g_signal_new("geanyagent-insert-to-agent", obj_type, G_SIGNAL_RUN_LAST,
		             0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING);
	plugin_signal_connect(plugin, geany->object, "geanyagent-insert-to-agent",
	                      FALSE, G_CALLBACK(on_insert_to_agent_signal), NULL);

	if (!g_signal_lookup("geanyagent-run-in-agent", obj_type))
		g_signal_new("geanyagent-run-in-agent", obj_type, G_SIGNAL_RUN_LAST,
		             0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING);
	plugin_signal_connect(plugin, geany->object, "geanyagent-run-in-agent",
	                      FALSE, G_CALLBACK(on_run_in_agent_signal), NULL);

	return TRUE;
}

static void ga_cleanup(G_GNUC_UNUSED GeanyPlugin *plugin,
                       G_GNUC_UNUSED gpointer data)
{
	running = FALSE;

	if (editor_nb_handler_id)
	{
		g_signal_handler_disconnect(geany_data->main_widgets->notebook,
		                            editor_nb_handler_id);
		editor_nb_handler_id = 0;
	}

	if (agent_term)
	{
		g_signal_handlers_disconnect_by_func(agent_term,
		                                     G_CALLBACK(on_child_exited), NULL);
		agent_term = NULL;
	}

	if (focus_event_box) {
		gtk_widget_destroy(focus_event_box);
		focus_event_box = NULL;
		focus_label     = NULL;
	}
	agent_socket_cleanup();

	if (tab_index >= 0)
	{
		gtk_notebook_remove_page(
		    GTK_NOTEBOOK(geany_data->main_widgets->message_window_notebook),
		    tab_index);
		agent_hbox  = NULL;
		agent_label = NULL;
		tab_index   = -1;
	}

	cleanup_askpass();

	if (agent_tools_menu_item) {
		gtk_widget_destroy(agent_tools_menu_item);
		agent_tools_menu_item = NULL;
	}
	if (agent_tools_config) {
		g_key_file_free(agent_tools_config);
		agent_tools_config = NULL;
	}

	agents_free();

	g_free(search_url);
	search_url = NULL;
	g_free(config_file);
	config_file = NULL;
}


/* ------------------------------------------------------------------ */

G_MODULE_EXPORT void geany_load_module(GeanyPlugin *plugin)
{
	plugin->info->name        = "Agent";
	plugin->info->description = "AI agent terminal tab in the message window. "
	                            "Runs a configurable command (default: claude) "
	                            "and restarts it automatically on exit.";
	plugin->info->version     = "0.1";
	plugin->info->author      = "teknopaul";

	plugin->funcs->init      = ga_init;
	plugin->funcs->cleanup   = ga_cleanup;
	plugin->funcs->configure = ga_configure;

	GEANY_PLUGIN_REGISTER(plugin, 235);
}
