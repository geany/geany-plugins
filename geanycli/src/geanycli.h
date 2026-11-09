/*
 * geanycli.h — public IPC interface for the geanycli plugin
 *
 * Two ways for other plugins to run a command in a terminal:
 *
 * 1) GLib signal (preferred – no header or module handle needed):
 *
 *      g_signal_emit_by_name(geany->object,
 *                            "geanycli-run-command",
 *                            "make clean", FALSE);
 *
 * 2) Direct function call via GModule symbol lookup:
 *
 *      #include "geanycli.h"
 *      typedef void (*GtRunCmdFn)(const gchar *, gboolean);
 *      GtRunCmdFn fn;
 *      if (g_module_symbol(terminal_plugin->module,
 *                          "geanycli_run_command",
 *                          (gpointer *)&fn))
 *          fn("make clean", FALSE);
 *
 * Parameters for geanycli-run-command / geanycli_run_command:
 *   cmd     – shell command string to execute
 *   new_tab – TRUE  : open a new terminal tab, run cmd there
 *             FALSE : run cmd in the currently active terminal tab
 *
 * To feed a command to EVERY open terminal tab (e.g. export an env var):
 *
 *   Signal:
 *      g_signal_emit_by_name(geany->object,
 *                            "geanycli-run-all-tabs",
 *                            "export JAVA_HOME=/opt/java/21");
 *
 *   Direct call:
 *      typedef void (*GtRunAllFn)(const gchar *);
 *      GtRunAllFn fn;
 *      if (g_module_symbol(terminal_plugin->module,
 *                          "geanycli_run_all_tabs",
 *                          (gpointer *)&fn))
 *          fn("export JAVA_HOME=/opt/java/21");
 */

#ifndef GEANYCLI_H
#define GEANYCLI_H

#include <glib.h>

void geanycli_run_command(const gchar *cmd, gboolean new_tab);
void geanycli_run_all_tabs(const gchar *cmd);

#endif /* GEANYCLI_H */
