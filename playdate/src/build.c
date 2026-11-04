/*
 *      build.c - invoke pdc / PlaydateSimulator / clean
 *
 *      Command shapes follow orta/vscode-playdate:
 *        $SDK/bin/pdc <source> <output>
 *        $SDK/bin/PlaydateSimulator <output>          (Linux)
 *      plus pdc flags from midouest/playdate-debug.
 */

#include "playdate.h"

#include <glib/gstdio.h>
#include <string.h>

typedef struct
{
	PDData *data;
	gboolean run_after;
	gchar *output;
} PDBuildJob;


static void free_job(PDBuildJob *job)
{
	if (job == NULL)
		return;
	g_free(job->output);
	g_free(job);
}


/*
 * pdc resolves import "CoreLibs/..." via PLAYDATE_SDK_PATH. Always pass it
 * explicitly so the build works even when Geany was started without the var
 * (prefs SDK path is enough).
 */
static gchar **make_sdk_env(const gchar *sdk)
{
	gchar **env = g_environ_setenv(g_get_environ(), "PLAYDATE_SDK_PATH", sdk, TRUE);
	return env;
}


static gchar *build_pdc_command(const PDConfig *cfg, const gchar *sdk,
	const gchar *source, const gchar *output)
{
	GString *cmd = g_string_new(NULL);
	gchar *pdc = g_build_filename(sdk, "bin", "pdc", NULL);

	g_string_append_printf(cmd, "\"%s\"", pdc);

	if (cfg->strip)
		g_string_append(cmd, " -s");
	if (cfg->no_compress)
		g_string_append(cmd, " --no-compress");
	if (cfg->verbose)
		g_string_append(cmd, " -v");
	if (cfg->quiet)
		g_string_append(cmd, " -q");
	if (cfg->skip_unknown)
		g_string_append(cmd, " -k");

	g_string_append_printf(cmd, " \"%s\" \"%s\"", source, output);

	g_free(pdc);
	return g_string_free(cmd, FALSE);
}


static gboolean launch_simulator(const gchar *sdk, const gchar *output)
{
	gchar *sim;
	GError *error = NULL;
	gboolean ok;
	/* Named array: a compound literal here is a dangling temporary after the
	 * assignment (GCC -O2), so argv[0] is stack garbage and spawn returns ENOENT. */
	gchar *argv[3];
	gchar **envp;

#ifdef G_OS_WIN32
	sim = g_build_filename(sdk, "bin", "PlaydateSimulator.exe", NULL);
#else
	sim = g_build_filename(sdk, "bin", "PlaydateSimulator", NULL);
#endif

	if (!g_file_test(sim, G_FILE_TEST_IS_EXECUTABLE) || g_file_test(sim, G_FILE_TEST_IS_DIR))
	{
		msgwin_compiler_add(COLOR_RED, _("Playdate: simulator not found: %s"), sim);
		ui_set_statusbar(FALSE, _("Playdate: simulator not found"));
		g_free(sim);
		return FALSE;
	}

	argv[0] = sim;
	argv[1] = (gchar *) output;
	argv[2] = NULL;

	envp = make_sdk_env(sdk);

	/* Detach: the simulator is long-lived. */
	ok = spawn_async(NULL, NULL, argv, envp, NULL, &error);
	if (!ok && error != NULL)
	{
		msgwin_compiler_add(COLOR_RED, _("Playdate: failed to start simulator: %s (%s)"),
			error->message, sim);
		g_error_free(error);
	}

	g_strfreev(envp);
	g_free(sim);
	return ok;
}


static void on_pdc_exit(GPid pid, gint status, gpointer user_data)
{
	PDBuildJob *job = user_data;
	gint exit_code = 0;

	if (SPAWN_WIFEXITED(status))
		exit_code = SPAWN_WEXITSTATUS(status);

	pd_errors_end(exit_code);

	if (exit_code == 0)
	{
		ui_set_statusbar(TRUE, _("Playdate: build succeeded → %s"), job->output);

		if (job->run_after)
		{
			gchar *sdk_path = pd_resolve_sdk_path(&job->data->config);
			if (sdk_path != NULL)
			{
				launch_simulator(sdk_path, job->output);
				g_free(sdk_path);
			}
			else
			{
				msgwin_compiler_add(COLOR_RED,
					_("Playdate: PLAYDATE_SDK_PATH not set and no SDK path in preferences"));
			}
		}
	}
	else
	{
		ui_set_statusbar(TRUE, _("Playdate: build failed (exit %d)"), exit_code);
	}

	g_spawn_close_pid(pid);
	free_job(job);
}


static void on_pdc_stdout(GString *string, GIOCondition condition, gpointer data)
{
	gchar **lines;
	guint i;

	if (condition & (G_IO_IN | G_IO_PRI) && string->len > 0)
	{
		lines = g_strsplit(string->str, "\n", -1);
		for (i = 0; lines[i] != NULL; i++)
		{
			if (*lines[i] != '\0')
				pd_errors_parse_line(lines[i]);
		}
		g_strfreev(lines);
	}
}


static gboolean require_paths(PDData *data, gchar **sdk_out, gchar **source_out, gchar **output_out)
{
	gchar *sdk = pd_resolve_sdk_path(&data->config);
	gchar *source = NULL;
	gchar *output = NULL;

	if (EMPTY(sdk))
	{
		msgwin_compiler_add(COLOR_RED,
			_("Playdate: SDK path not configured. Set PLAYDATE_SDK_PATH or open Plugins → Playdate → Preferences."));
		ui_set_statusbar(FALSE, _("Playdate: SDK path not configured"));
		g_free(sdk);
		return FALSE;
	}

	source = pd_resolve_source_path(&data->config);
	output = pd_resolve_output_path(&data->config);

	if (EMPTY(source) || !g_file_test(source, G_FILE_TEST_IS_DIR))
	{
		msgwin_compiler_add(COLOR_RED,
			_("Playdate: source directory not found: %s"),
			source ? source : _("(no document)"));
		ui_set_statusbar(FALSE, _("Playdate: source directory not found"));
		g_free(sdk);
		g_free(source);
		g_free(output);
		return FALSE;
	}

	if (EMPTY(output))
	{
		msgwin_compiler_add(COLOR_RED, _("Playdate: output path is empty"));
		g_free(sdk);
		g_free(source);
		g_free(output);
		return FALSE;
	}

	*sdk_out = sdk;
	*source_out = source;
	*output_out = output;
	return TRUE;
}


void pd_build_project(PDData *data, gboolean run_after)
{
	gchar *sdk = NULL;
	gchar *source = NULL;
	gchar *output = NULL;
	gchar *cmd;
	GError *error = NULL;
	PDBuildJob *job;
	GPid pid = 0;

	if (!require_paths(data, &sdk, &source, &output))
		return;

	cmd = build_pdc_command(&data->config, sdk, source, output);

	job = g_new0(PDBuildJob, 1);
	job->data = data;
	job->run_after = run_after;
	job->output = g_strdup(output);

	pd_errors_begin();
	msgwin_compiler_add(COLOR_BLACK, _("Playdate: %s"), cmd);
	ui_set_statusbar(TRUE, _("Playdate: building %s …"), source);

	{
		gchar **envp = make_sdk_env(sdk);

		if (!spawn_with_callbacks(NULL, cmd, NULL, envp,
				SPAWN_ASYNC | SPAWN_LINE_BUFFERED,
				NULL, NULL,
				on_pdc_stdout, NULL, 0,
				on_pdc_stdout, NULL, 0,
				on_pdc_exit, job, &pid, &error))
		{
			msgwin_compiler_add(COLOR_RED, _("Playdate: failed to run pdc: %s"),
				error ? error->message : "?");
			if (error)
				g_error_free(error);
			pd_errors_end(1);
			free_job(job);
		}

		g_strfreev(envp);
	}

	g_free(cmd);
	g_free(sdk);
	g_free(source);
	g_free(output);
}


void pd_run_simulator(PDData *data)
{
	gchar *sdk = pd_resolve_sdk_path(&data->config);
	gchar *output = pd_resolve_output_path(&data->config);

	if (EMPTY(sdk))
	{
		msgwin_compiler_add(COLOR_RED,
			_("Playdate: SDK path not configured. Set PLAYDATE_SDK_PATH or open Plugins → Playdate → Preferences."));
		ui_set_statusbar(FALSE, _("Playdate: SDK path not configured"));
		g_free(sdk);
		g_free(output);
		return;
	}

	if (EMPTY(output) || !g_file_test(output, G_FILE_TEST_EXISTS))
	{
		msgwin_compiler_add(COLOR_RED,
			_("Playdate: game bundle not found: %s (build first)"),
			output ? output : "?");
		ui_set_statusbar(FALSE, _("Playdate: build the project first"));
		g_free(sdk);
		g_free(output);
		return;
	}

	if (launch_simulator(sdk, output))
		ui_set_statusbar(TRUE, _("Playdate: simulator launched"));

	g_free(sdk);
	g_free(output);
}


void pd_open_simulator_only(PDData *data)
{
	gchar *sdk = pd_resolve_sdk_path(&data->config);

	if (EMPTY(sdk))
	{
		msgwin_compiler_add(COLOR_RED,
			_("Playdate: SDK path not configured. Set PLAYDATE_SDK_PATH or open Plugins → Playdate → Preferences."));
		ui_set_statusbar(FALSE, _("Playdate: SDK path not configured"));
		g_free(sdk);
		return;
	}

	launch_simulator(sdk, NULL);
	g_free(sdk);
}


void pd_clean_output(PDData *data)
{
	gchar *output = pd_resolve_output_path(&data->config);

	if (EMPTY(output))
	{
		ui_set_statusbar(FALSE, _("Playdate: nothing to clean (no project)"));
		g_free(output);
		return;
	}

	if (!g_file_test(output, G_FILE_TEST_EXISTS))
	{
		ui_set_statusbar(TRUE, _("Playdate: output already clean (%s)"), output);
		g_free(output);
		return;
	}

	if (g_file_test(output, G_FILE_TEST_IS_DIR))
	{
		/* pdc produces a .pdx directory */
		gchar *cmd = g_strdup_printf("rm -rf -- \"%s\"", output);
		GError *error = NULL;

		if (!spawn_sync(NULL, cmd, NULL, NULL, NULL, NULL, NULL, NULL, &error))
		{
			msgwin_compiler_add(COLOR_RED, _("Playdate: clean failed: %s"),
				error ? error->message : "?");
			if (error)
				g_error_free(error);
		}
		else
		{
			ui_set_statusbar(TRUE, _("Playdate: cleaned %s"), output);
		}
		g_free(cmd);
	}
	else
	{
		if (g_remove(output) != 0)
			msgwin_compiler_add(COLOR_RED, _("Playdate: could not remove %s"), output);
		else
			ui_set_statusbar(TRUE, _("Playdate: cleaned %s"), output);
	}

	g_free(output);
}
