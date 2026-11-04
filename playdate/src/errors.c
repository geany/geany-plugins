/*
 *      errors.c - parse pdc output into Geany's Compiler tab
 *
 *      Matches the midouest vscode-playdate-debug problem matchers:
 *        warning: FILE:LINE: MESSAGE
 *        error:   FILE:LINE: MESSAGE
 *        error:   MESSAGE
 */

#include "playdate.h"

#include <string.h>

/* Number of compiler messages added in the current build (for summary). */
static guint msg_count = 0;
static guint warn_count = 0;
static guint error_count = 0;


void pd_errors_begin(void)
{
	msg_count = 0;
	warn_count = 0;
	error_count = 0;

	msgwin_clear_tab(MSG_COMPILER);
	msgwin_switch_tab(MSG_COMPILER, TRUE);
}


/* Emit in Geany's clickable form: path:line: severity: message */
static void emit(const gchar *severity, const gchar *file, gint line, const gchar *message)
{
	gint color = COLOR_BLACK;

	if (g_strcmp0(severity, "error") == 0)
	{
		color = COLOR_RED;
		error_count++;
	}
	else if (g_strcmp0(severity, "warning") == 0)
	{
		color = COLOR_DARK_RED;
		warn_count++;
	}
	msg_count++;

	if (line > 0 && !EMPTY(file))
		msgwin_compiler_add(color, "%s:%d: %s: %s", file, line, severity, message);
	else if (!EMPTY(file))
		msgwin_compiler_add(color, "%s: %s: %s", file, severity, message);
	else
		msgwin_compiler_add(color, "%s: %s", severity, message);
}


/*
 * pdc formats we accept (both observed in the wild):
 *   file:line: error: message          ← actual pdc output
 *   file:line: warning: message
 *   error: file:line: message          ← midouest problem-matcher style
 *   error: message
 */
void pd_errors_parse_line(const gchar *line)
{
	gchar *work;
	gchar *p;
	const gchar *severity = NULL;

	if (line == NULL)
		return;

	work = g_strdup(line);
	g_strchomp(work);
	p = work;

	while (*p == ' ' || *p == '\t')
		p++;

	if (*p == '\0')
	{
		g_free(work);
		return;
	}

	/* --- midouest style: severity first --- */
	if (g_str_has_prefix(p, "warning:"))
	{
		severity = "warning";
		p += 8;
	}
	else if (g_str_has_prefix(p, "error:") ||
	         g_str_has_prefix(p, "Error:") ||
	         g_str_has_prefix(p, "ERROR:"))
	{
		severity = "error";
		p += 6;
	}

	if (severity != NULL)
	{
		while (*p == ' ' || *p == '\t')
			p++;

		/* try file:line: message */
		{
			gchar *c = p;
			gboolean ok = FALSE;

			while ((c = strchr(c, ':')) != NULL)
			{
				gchar *num_start = c + 1;
				gchar *num_end = NULL;
				gint line_no = (gint) g_ascii_strtoll(num_start, &num_end, 10);

				if (num_end > num_start &&
				    (*num_end == ':' || *num_end == ' ' || *num_end == '\t' || *num_end == '\0'))
				{
					gchar *rest = num_end;
					if (*rest == ':')
						rest++;
					while (*rest == ' ' || *rest == '\t')
						rest++;

					{
						gchar *file = g_strndup(p, (gsize) (c - p));
						emit(severity, file, line_no, rest);
						g_free(file);
						ok = TRUE;
					}
					break;
				}
				c++;
			}

			if (!ok)
				emit(severity, NULL, 0, p);
		}

		g_free(work);
		return;
	}

	/* --- pdc style: file:line: severity: message --- */
	{
		gchar *sev_err = strstr(p, ": error:");
		gchar *sev_warn = strstr(p, ": warning:");
		gchar *sev = NULL;
		const gchar *kind = NULL;

		if (sev_err != NULL)
		{
			sev = sev_err;
			kind = "error";
		}
		else if (sev_warn != NULL)
		{
			sev = sev_warn;
			kind = "warning";
		}

		if (sev != NULL)
		{
			/* expect file:line before the severity token */
			gchar *c = p;
			gboolean ok = FALSE;

			while ((c = strchr(c, ':')) != NULL && c < sev)
			{
				gchar *num_start = c + 1;
				gchar *num_end = NULL;
				gint line_no = (gint) g_ascii_strtoll(num_start, &num_end, 10);

				if (num_end > num_start && num_end <= sev &&
				    (*num_end == ':' || *num_end == ' ' || *num_end == '\t'))
				{
					gchar *msg = sev + strlen(kind) + 2; /* skip ": error:" / ": warning:" */
					while (*msg == ' ' || *msg == '\t')
						msg++;

					{
						gchar *file = g_strndup(p, (gsize) (c - p));
						emit(kind, file, line_no, msg);
						g_free(file);
						ok = TRUE;
					}
					break;
				}
				c++;
			}

			if (!ok)
				emit(kind, NULL, 0, p);

			g_free(work);
			return;
		}
	}

	/* passthrough for non-pdc noise */
	msgwin_compiler_add_string(COLOR_BLACK, work);
	msg_count++;
	g_free(work);
}


void pd_errors_end(gint exit_status)
{
	if (exit_status == 0)
	{
		msgwin_compiler_add(COLOR_BLUE, _("Playdate: build succeeded (%u message%s)"),
			msg_count, msg_count == 1 ? "" : "s");
	}
	else
	{
		msgwin_compiler_add(COLOR_RED,
			_("Playdate: build failed (exit %d) — %u error(s), %u warning(s)"),
			exit_status, error_count, warn_count);
	}
}
