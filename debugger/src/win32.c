/*
 *      win32.c
 *
 *      Copyright 2012 Vadim Kochan <vadim4j@gmail.com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License along
 *      with this program; if not, write to the Free Software Foundation, Inc.,
 *      51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 * Special functions for the win32 platform
 */

#include <glib.h>

#ifdef G_OS_WIN32

#include <windows.h>
#include <stdio.h>
#include <fcntl.h>

#include "win32.h"

#define BUFSIZE 4096
#define CMDSIZE 32768

/* Kills windows process */
void win32_kill(GPid pid, gint exit_status)
{
	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, (DWORD)pid);
	TerminateProcess(hProcess, exit_status);
}

/* Spawns windows process with I/O redirection */
/* through the pipe                            */
gboolean	win32_spawn_async_with_pipes( const gchar *working_directory, 
										gchar **argv, gchar **envp,
										GPid *child_pid,
										gint *standard_input,
										gint *standard_output,
										gint *standard_error,
										GError **error
										)
{
	TCHAR  buffer[CMDSIZE]=TEXT("");
	TCHAR  cmdline[CMDSIZE] = TEXT("");

	gint argc = 0, i;
	gint cmdpos = 0;
	
	STARTUPINFO si;
	SECURITY_ATTRIBUTES sa;
	SECURITY_DESCRIPTOR sd;
	PROCESS_INFORMATION pi;

	HANDLE child_stdin, child_stdout, read_stdout,write_stdin;
	
	while (argv[argc])
	{
		++argc;
	}
	
	for (i = cmdpos; i < argc; i++)
	{
		if (argv[i])
		{
			if (strlen(cmdline) > 0)
			{
				g_snprintf(cmdline, sizeof(cmdline), "%s %s", cmdline, argv[i]);
			}
			else
			{
				g_snprintf(cmdline, sizeof(cmdline), "%s", argv[i]);
			}
		}
	}
	
	InitializeSecurityDescriptor(&sd,SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);
    SetSecurityDescriptorOwner(&sd, NULL, TRUE);
    
    sa.lpSecurityDescriptor = &sd;
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = TRUE;


	if (!CreatePipe(&child_stdin, &write_stdin,&sa,0))
	{
		gchar *msg = g_win32_error_message(GetLastError());
		g_set_error(error, G_SPAWN_ERROR, G_FILE_ERROR_PIPE, "%s", msg);
		MessageBox(NULL, msg, "error", MB_OK);
		g_free(msg);
	
		return FALSE;
	}

	if (!CreatePipe(&read_stdout, &child_stdout,&sa,0))
	{
		gchar *msg = g_win32_error_message(GetLastError());
		g_set_error(error, G_SPAWN_ERROR, G_FILE_ERROR_PIPE, "%s", msg);
		g_free(msg);
		
		return FALSE;
	}

	GetStartupInfo(&si);

	si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;
	si.hStdOutput = child_stdout;
	si.hStdError = child_stdout;
	si.hStdInput = child_stdin;
	
	if (!CreateProcess(NULL, cmdline, NULL,NULL,TRUE, CREATE_NO_WINDOW,
					 NULL, working_directory, &si, &pi))
	{
		CloseHandle(child_stdin);
		CloseHandle(child_stdout);
		CloseHandle(read_stdout);
		CloseHandle(write_stdin);
		
		gchar *msg = g_win32_error_message(GetLastError());
		g_set_error(error, G_SPAWN_ERROR, -1, "%s", msg);
		g_free(msg);
		
		return FALSE;
	}
	
	//opening new C runtime file descriptor's
	*standard_output = _open_osfhandle( (intptr_t)read_stdout, _O_TEXT );
	*standard_input  = _open_osfhandle( (intptr_t)write_stdin, _O_TEXT );	
	
	*child_pid = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pi.dwProcessId);
	
	CloseHandle(child_stdin);
	CloseHandle(child_stdout);
	CloseHandle(pi.hProcess);
	
	return TRUE;
}


#endif /* G_OS_WIN32 */
