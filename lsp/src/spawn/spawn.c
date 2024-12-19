/*
 * Copyright 2013 The Geany contributors
 * Copyright 2023 Jiri Techet <techet@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

// stolen from Geany, made lsp_spawn_async_with_pipes() public, removed unneeded stuff

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <string.h>

#include "spawn/spawn.h"

#ifdef G_OS_WIN32
# include <ctype.h>    /* isspace() */
# include <fcntl.h>    /* _O_RDONLY, _O_WRONLY */
# include <io.h>       /* _open_osfhandle, _close */
# include <windows.h>
#else  /* G_OS_WIN32 */
# include <signal.h>
#endif  /* G_OS_WIN32 */


#if ! GLIB_CHECK_VERSION(2, 31, 20) && ! defined(G_SPAWN_ERROR_TOO_BIG)
# define G_SPAWN_ERROR_TOO_BIG G_SPAWN_ERROR_2BIG
#endif

#ifdef G_OS_WIN32
/* Each 4KB under Windows seem to come in 2 portions, so 2K + 2K is more
   balanced than 4095 + 1. May be different on the latest Windows/glib? */
# define DEFAULT_IO_LENGTH 2048
#else
# define DEFAULT_IO_LENGTH 4096

/* helper function that cuts glib citing of the original text on bad quoting: it may be long,
   and only the caller knows whether it's UTF-8. Thought we lose the ' or " failed info. */
static gboolean spawn_parse_argv(const gchar *command_line, gint *argcp, gchar ***argvp,
	GError **error)
{
	GError *gerror = NULL;

	if (g_shell_parse_argv(command_line, argcp, argvp, &gerror))
		return TRUE;

	g_set_error_literal(error, gerror->domain, gerror->code,
		gerror->code == G_SHELL_ERROR_BAD_QUOTING ?
		"Text ended before matching quote was found" : gerror->message);
	g_error_free(gerror);
	return FALSE;
}
#endif


#ifdef G_OS_WIN32

/*
 *  Checks whether a command line is syntactically valid and extracts the program name from it.
 *
 *  See @c spawn_check_command() for details.
 *
 *  @param command_line the command line to check and get the program name from.
 *  @param error return location for error.
 *
 *  @return allocated string with the program name on success, @c NULL on error.
 */
static gchar *spawn_get_program_name(const gchar *command_line, GError **error)
{
	gchar *program;
	gboolean open_quote = FALSE;
	const gchar *s, *arguments;

	g_return_val_if_fail(command_line != NULL, FALSE);

	while (*command_line && strchr(" \t\r\n", *command_line))
		command_line++;

	if (!*command_line)
	{
		g_set_error_literal(error, G_SHELL_ERROR, G_SHELL_ERROR_EMPTY_STRING,
			/* TL note: from glib */
			"Text was empty (or contained only whitespace)");
		return FALSE;
	}

	/* To prevent Windows from doing something weird, we want to be 100% sure that the
	   character after the program name is a delimiter, so we allow space and tab only. */

	if (*command_line == '"')
	{
		command_line++;
		/* Windows allows "foo.exe, but we may have extra arguments */
		if ((s = strchr(command_line, '"')) == NULL)
		{
			g_set_error_literal(error, G_SHELL_ERROR, G_SHELL_ERROR_BAD_QUOTING,
				"Text ended before matching quote was found");
			return FALSE;
		}

		if (!strchr(" \t", s[1]))  /* strchr() catches s[1] == '\0' */
		{
			g_set_error_literal(error, G_SHELL_ERROR, G_SHELL_ERROR_BAD_QUOTING,
				"A quoted Windows program name must be entirely inside the quotes");
			return FALSE;
		}
	}
	else
	{
		const gchar *quote = strchr(command_line, '"');

		/* strchr() catches *s == '\0', and the for body is empty */
		for (s = command_line; !strchr(" \t", *s); s++);

		if (quote && quote < s)
		{
			g_set_error_literal(error, G_SHELL_ERROR, G_SHELL_ERROR_BAD_QUOTING,
				"A quoted Windows program name must be entirely inside the quotes");
			return FALSE;
		}
	}

	program = g_strndup(command_line, s - command_line);
	arguments = s + (*s == '"');

	for (s = arguments; *s; s++)
	{
		if (*s == '"')
		{
			const char *slash;

			for (slash = s; slash > arguments && slash[-1] == '\\'; slash--);
			if ((s - slash) % 2 == 0)
				open_quote ^= TRUE;
		}
	}

	if (open_quote)
	{
		g_set_error_literal(error, G_SHELL_ERROR, G_SHELL_ERROR_BAD_QUOTING,
			"Text ended before matching quote was found");
		g_free(program);
		return FALSE;
	}

	return program;
}


static gchar *spawn_create_process_with_pipes(wchar_t *w_command_line, const wchar_t *w_working_directory,
	void *w_environment, HANDLE *hprocess, int *stdin_fd, int *stdout_fd, int *stderr_fd)
{
	enum { WRITE_STDIN, READ_STDOUT, READ_STDERR, READ_STDIN, WRITE_STDOUT, WRITE_STDERR };
	STARTUPINFOW startup;
	PROCESS_INFORMATION process;
	HANDLE hpipe[6] = { NULL, NULL, NULL, NULL, NULL, NULL };
	int *fd[3] = { stdin_fd, stdout_fd, stderr_fd };
	const char *failed;     /* failed WIN32/CRTL function, if any */
	gchar *message = NULL;  /* glib WIN32/CTRL error message */
	gchar *failure = NULL;  /* full error text */
	gboolean pipe_io;
	int i;

	ZeroMemory(&startup, sizeof startup);
	startup.cb = sizeof startup;
	pipe_io = stdin_fd || stdout_fd || stderr_fd;

	if (pipe_io)
	{
		startup.dwFlags |= STARTF_USESTDHANDLES;

		/* not all programs accept mixed NULL and non-NULL hStd*, so we create all */
		for (i = 0; i < 3; i++)
		{
			static int pindex[3][2] = { { READ_STDIN, WRITE_STDIN },
					{ READ_STDOUT, WRITE_STDOUT }, { READ_STDERR, WRITE_STDERR } };

			if (!CreatePipe(&hpipe[pindex[i][0]], &hpipe[pindex[i][1]], NULL, 0))
			{
				hpipe[pindex[i][0]] = hpipe[pindex[i][1]] = NULL;
				failed = "create pipe";
				goto leave;
			}

			if (fd[i])
			{
				static int mode[3] = { _O_WRONLY, _O_RDONLY, _O_RDONLY };

				if ((*fd[i] = _open_osfhandle((intptr_t) hpipe[i], mode[i])) == -1)
				{
					failed = "convert pipe handle to file descriptor";
					message = g_strdup(g_strerror(errno));
					goto leave;
				}
			}
			else if (!CloseHandle(hpipe[i]))
			{
				failed = "close pipe";
				goto leave;
			}

			if (!SetHandleInformation(hpipe[i + 3], HANDLE_FLAG_INHERIT,
				HANDLE_FLAG_INHERIT))
			{
				failed = "set pipe handle to inheritable";
				goto leave;
			}
		}
	}

	startup.hStdInput = hpipe[READ_STDIN];
	startup.hStdOutput = hpipe[WRITE_STDOUT];
	startup.hStdError = 0;

	if (!CreateProcessW(NULL, w_command_line, NULL, NULL, TRUE,
		CREATE_UNICODE_ENVIRONMENT | (pipe_io ? CREATE_NO_WINDOW : 0),
		w_environment, w_working_directory, &startup, &process))
	{
		failed = "";  /* report the message only */
		/* further errors will not be reported */
	}
	else
	{
		failed = NULL;
		CloseHandle(process.hThread);  /* we don't need this */

		if (hprocess)
			*hprocess = process.hProcess;
		else
			CloseHandle(process.hProcess);
	}

leave:
	if (failed)
	{
		if (!message)
		{
			size_t len;

			message = g_win32_error_message(GetLastError());
			len = strlen(message);

			/* unlike g_strerror(), the g_win32_error messages may include a final '.' */
			if (len > 0 && message[len - 1] == '.')
				message[len - 1] = '\0';
		}

		if (*failed == '\0')
			failure = message;
		else
		{
			failure = g_strdup_printf("Failed to %s (%s)", failed, message);
			g_free(message);
		}
	}

	if (pipe_io)
	{
		for (i = 0; i < 3; i++)
		{
			if (failed)
			{
				if (fd[i] && *fd[i] != -1)
					_close(*fd[i]);
				else if (hpipe[i])
					CloseHandle(hpipe[i]);
			}

			if (hpipe[i + 3])
				CloseHandle(hpipe[i + 3]);
		}
	}

	return failure;
}


static void spawn_append_argument(GString *command, const char *text)
{
	const char *s;

	if (command->len)
		g_string_append_c(command, ' ');

	for (s = text; *s; s++)
	{
		/* g_ascii_isspace() fails for '\v', and locale spaces (if any) will do no harm */
		if (*s == '"' || isspace(*s))
			break;
	}

	if (*text && !*s)
		g_string_append(command, text);
	else
	{
		g_string_append_c(command, '"');

		for (s = text; *s; s++)
		{
			const char *slash;

			for (slash = s; *slash == '\\'; slash++);

			if (slash > s)
			{
				g_string_append_len(command, s, slash - s);

				if (!*slash || *slash == '"')
				{
					g_string_append_len(command, s, slash - s);

					if (!*slash)
						break;
				}

				s = slash;
			}

			if (*s == '"')
				g_string_append_c(command, '\\');

			g_string_append_c(command, *s);
		}

		g_string_append_c(command, '"');
	}
}
#endif /* G_OS_WIN32 */


/*
 *  Executes a child program asynchronously and setups pipes.
 *
 *  This is the low-level spawning function. Please use @c spawn_with_callbacks() unless
 *  you need to setup specific event source(s).
 *
 *  A command line or an argument vector must be passed. If both are present, the argument
 *  vector is appended to the command line. An empty command line is not allowed.
 *
 *  Under Windows, if the child is a console application, and at least one file descriptor is
 *  specified, the new child console (if any) will be hidden.
 *
 *  If a @a child_pid is passed, it's your responsibility to invoke @c g_spawn_close_pid().
 *
 *  @param working_directory child's current working directory, or @c NULL.
 *  @param command_line child program and arguments, or @c NULL.
 *  @param argv child's argument vector, or @c NULL.
 *  @param envp child's environment, or @c NULL.
 *  @param child_pid return location for child process ID, or @c NULL.
 *  @param stdin_fd return location for file descriptor to write to child's stdin, or @c NULL.
 *  @param stdout_fd return location for file descriptor to read child's stdout, or @c NULL.
 *  @param stderr_fd return location for file descriptor to read child's stderr, or @c NULL.
 *  @param error return location for error.
 *
 *  @return @c TRUE on success, @c FALSE on error.
 */
gboolean lsp_spawn_async_with_pipes(const gchar *working_directory, const gchar *command_line,
	gchar **argv, gchar **envp, GPid *child_pid, gint *stdin_fd, gint *stdout_fd,
	gint *stderr_fd, GError **error)
{
	g_return_val_if_fail(command_line != NULL || argv != NULL, FALSE);

#ifdef G_OS_WIN32
	GString *command;
	GArray *w_environment;
	wchar_t *w_working_directory = NULL;
	wchar_t *w_command = NULL;
	gboolean success = TRUE;

	if (command_line)
	{
		gchar *program = spawn_get_program_name(command_line, error);
		const gchar *arguments;

		if (!program)
			return FALSE;

		command = g_string_new(NULL);
		arguments = strstr(command_line, program) + strlen(program);

		if (*arguments == '"')
		{
			g_string_append(command, program);
			arguments++;
		}
		else
		{
			/* quote the first token, to avoid Windows attemps to run two or more
			   unquoted tokens as a program until an existing file name is found */
			g_string_printf(command, "\"%s\"", program);
		}

		g_string_append(command, arguments);
		g_free(program);
	}
	else
		command = g_string_new(NULL);

	w_environment = g_array_new(TRUE, FALSE, sizeof(wchar_t));

	while (argv && *argv)
		spawn_append_argument(command, *argv++);

#if defined(SPAWN_TEST) || defined(GEANY_DEBUG)
	g_message("full spawn command line: %s", command->str);
#endif

	while (envp && *envp && success)
	{
		glong w_entry_len;
		wchar_t *w_entry;
		gchar *tmp = NULL;

		// FIXME: remove this and rely on UTF-8 input
		if (! g_utf8_validate(*envp, -1, NULL))
		{
			tmp = g_locale_to_utf8(*envp, -1, NULL, NULL, NULL);
			if (tmp)
				*envp = tmp;
		}
		/* TODO: better error message */
		w_entry = g_utf8_to_utf16(*envp, -1, NULL, &w_entry_len, error);

		if (! w_entry)
			success = FALSE;
		else
		{
			/* copy the entry, including NUL terminator */
			g_array_append_vals(w_environment, w_entry, w_entry_len + 1);
			g_free(w_entry);
		}

		g_free(tmp);
		envp++;
	}

	/* convert working directory into locale encoding */
	if (success && working_directory)
	{
		GError *gerror = NULL;
		const gchar *utf8_working_directory;
		gchar *tmp = NULL;

		// FIXME: remove this and rely on UTF-8 input
		if (! g_utf8_validate(working_directory, -1, NULL))
		{
			tmp = g_locale_to_utf8(working_directory, -1, NULL, NULL, NULL);
			if (tmp)
				utf8_working_directory = tmp;
		}
		else
			utf8_working_directory = working_directory;

		w_working_directory = g_utf8_to_utf16(utf8_working_directory, -1, NULL, NULL, &gerror);
		if (! w_working_directory)
		{
			/* TODO use the code below post-1.28 as it introduces a new string
			g_set_error(error, gerror->domain, gerror->code,
				"Failed to convert working directory into locale encoding: %s", gerror->message);
			*/
			g_propagate_error(error, gerror);
			success = FALSE;
		}
		g_free(tmp);
	}
	/* convert command into locale encoding */
	if (success)
	{
		GError *gerror = NULL;
		const gchar *utf8_cmd;
		gchar *tmp = NULL;

		// FIXME: remove this and rely on UTF-8 input
		if (! g_utf8_validate(command->str, -1, NULL))
		{
			tmp = g_locale_to_utf8(command->str, -1, NULL, NULL, NULL);
			if (tmp)
				utf8_cmd = tmp;
		}
		else
			utf8_cmd = command->str;

		w_command = g_utf8_to_utf16(utf8_cmd, -1, NULL, NULL, &gerror);
		if (! w_command)
		{
			/* TODO use the code below post-1.28 as it introduces a new string
			g_set_error(error, gerror->domain, gerror->code,
				"Failed to convert command into locale encoding: %s", gerror->message);
			*/
			g_propagate_error(error, gerror);
			success = FALSE;
		}
	}

	if (success)
	{
		gchar *failure;

		failure = spawn_create_process_with_pipes(w_command, w_working_directory,
			envp ? w_environment->data : NULL, child_pid, stdin_fd, stdout_fd, stderr_fd);

		if (failure)
		{
			g_set_error_literal(error, G_SPAWN_ERROR, G_SPAWN_ERROR_FAILED, failure);
			g_free(failure);
		}

		success = failure == NULL;
	}

	g_string_free(command, TRUE);
	g_array_free(w_environment, TRUE);
	g_free(w_working_directory);
	g_free(w_command);

	return success;
#else  /* G_OS_WIN32 */
	int cl_argc;
	char **full_argv;
	gboolean spawned;
	GError *gerror = NULL;

	if (command_line)
	{
		int argc = 0;
		char **cl_argv;

		if (!spawn_parse_argv(command_line, &cl_argc, &cl_argv, error))
			return FALSE;

		if (argv)
			for (argc = 0; argv[argc]; argc++);

		full_argv = g_renew(gchar *, cl_argv, cl_argc + argc + 1);
		memcpy(full_argv + cl_argc, argv, argc * sizeof(gchar *));
		full_argv[cl_argc + argc] = NULL;
	}
	else
		full_argv = argv;

	spawned = g_spawn_async_with_pipes(working_directory, full_argv, envp,
		G_SPAWN_SEARCH_PATH | (child_pid ? G_SPAWN_DO_NOT_REAP_CHILD : 0), NULL, NULL,
		child_pid, stdin_fd, stdout_fd, stderr_fd, &gerror);

	if (!spawned)
	{
		gint en = 0;
		const gchar *message = gerror->message;

		/* try to cut glib citing of the program name or working directory: they may be long,
		   and only the caller knows whether they're UTF-8. We lose the exact chdir error. */
		switch (gerror->code)
		{
		#ifdef EACCES
			case G_SPAWN_ERROR_ACCES : en = EACCES; break;
		#endif
		#ifdef EPERM
			case G_SPAWN_ERROR_PERM : en = EPERM; break;
		#endif
		#ifdef E2BIG
			case G_SPAWN_ERROR_TOO_BIG : en = E2BIG; break;
		#endif
		#ifdef ENOEXEC
			case G_SPAWN_ERROR_NOEXEC : en = ENOEXEC; break;
		#endif
		#ifdef ENAMETOOLONG
			case G_SPAWN_ERROR_NAMETOOLONG : en = ENAMETOOLONG; break;
		#endif
		#ifdef ENOENT
			case G_SPAWN_ERROR_NOENT : en = ENOENT; break;
		#endif
		#ifdef ENOMEM
			case G_SPAWN_ERROR_NOMEM : en = ENOMEM; break;
		#endif
		#ifdef ENOTDIR
			case G_SPAWN_ERROR_NOTDIR : en = ENOTDIR; break;
		#endif
		#ifdef ELOOP
			case G_SPAWN_ERROR_LOOP : en = ELOOP; break;
		#endif
		#ifdef ETXTBUSY
			case G_SPAWN_ERROR_TXTBUSY : en = ETXTBUSY; break;
		#endif
		#ifdef EIO
			case G_SPAWN_ERROR_IO : en = EIO; break;
		#endif
		#ifdef ENFILE
			case G_SPAWN_ERROR_NFILE : en = ENFILE; break;
		#endif
		#ifdef EMFILE
			case G_SPAWN_ERROR_MFILE : en = EMFILE; break;
		#endif
		#ifdef EINVAL
			case G_SPAWN_ERROR_INVAL : en = EINVAL; break;
		#endif
		#ifdef EISDIR
			case G_SPAWN_ERROR_ISDIR : en = EISDIR; break;
		#endif
		#ifdef ELIBBAD
			case G_SPAWN_ERROR_LIBBAD : en = ELIBBAD; break;
		#endif
			case G_SPAWN_ERROR_CHDIR :
			{
				message = "Failed to change to the working directory";
				break;
			}
			case G_SPAWN_ERROR_FAILED :
			{
				message = "Unknown error executing child process";
				break;
			}
		}

		if (en)
			message = g_strerror(en);

		g_set_error_literal(error, gerror->domain, gerror->code, message);
		g_error_free(gerror);
	}

	if (full_argv != argv)
	{
		full_argv[cl_argc] = NULL;
		g_strfreev(full_argv);
	}

	return spawned;
#endif  /* G_OS_WIN32 */
}
