/*      pinentry.c
 *
 *      Copyright 2011 Hans Alves <alves.h88@gmail.com>
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
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

/* for fdopen() */
#define _POSIX_SOURCE 1
#define _POSIX_C_SOURCE 1

#include "geanypg.h"

static const char * geanypg_getname(const char * uid_hint)
{
    int space = 0;
    if (!uid_hint)
        return NULL;
    while (*uid_hint && !(space && *uid_hint != ' '))
    {
        if (*uid_hint == ' ')
            space = 1;
        ++uid_hint;
    }
    return uid_hint;
}

#ifdef __unix__

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

static void geanypg_read_till(int fd, char delim)
{
    while (1)
    {
        char val;
        ssize_t rv = read(fd, &val, 1);
        if (rv <= 0 || val == delim)
            break;
    }
}

static int geanypg_read(int fd, char delim, int max, char * buffer)
{
    int idx;
    ssize_t rv = 1;
    char ch = 0;
    for (idx = 0; (idx < max - 1) && rv > 0 && ch != delim; ++idx)
    {
        rv = read(fd, &ch, 1);
        buffer[idx] = ch;
    }
    buffer[idx ? idx - 1 : 0] = 0;
    return idx ? idx - 1 : 0;
}
gpgme_error_t geanypg_passphrase_cb(void * hook,
                                    const char * uid_hint,
                                    const char * passphrase_info,
                                    int prev_was_bad ,
                                    int fd)
{
    int outpipe[2];
    int inpipe[2];
    int childpid;
    int status;
    char readbuffer[2080] = {0}; /* pinentry should at least support passphrases of up to 2048 characters */
    FILE * childin;

    if (pipe(outpipe))
    {
        fprintf(stderr, "GeanyPG: %s\n", strerror(errno));
        return gpgme_error_from_errno(errno);
    }
    if (pipe(inpipe))
    {
        fprintf(stderr, "GeanyPG: %s\n", strerror(errno));
        return gpgme_error_from_errno(errno);
    }

    childpid = fork();
    if (!childpid)
    { /* pinentry */
        char arg1[] = "pinentry";
        char * argv[] = {NULL, NULL};

        argv[0] = arg1;

        close(outpipe[READ]);
        dup2(outpipe[WRITE], STDOUT_FILENO);

        close(inpipe[WRITE]);
        dup2(inpipe[READ], STDIN_FILENO);

        execvp(*argv, argv);
        /* shouldn't get here */
        fprintf(stderr, "GeanyPG: %s\n%s\n", _("Could not use pinentry."), strerror(errno));
        exit(1); /* kill the child */
    }
    /* GeanpyPG */
    close(outpipe[WRITE]);
    close(inpipe[READ]);
    childin = fdopen(inpipe[WRITE], "w");

    /* To understand what's happening here, read the pinentry documentation */
    geanypg_read(outpipe[READ], ' ', 2049, readbuffer);
    if (strncmp(readbuffer, "OK", 3))
    {
        fprintf(stderr, "GeanyPG: %s\n", _("Unexpected output from pinentry."));
        fclose(childin);
        waitpid(childpid, &status, 0);
        close(outpipe[READ]);
        close(fd);
        return gpgme_err_make(GPG_ERR_SOURCE_PINENTRY, GPG_ERR_GENERAL);
    }
    geanypg_read_till(outpipe[READ], '\n'); /* read the rest of the first line after OK */
    fprintf(childin, "SETTITLE GeanyPG %s\n", _("Passphrase entry"));
    fflush(childin);
    geanypg_read_till(outpipe[READ], '\n');

    fprintf(childin, "SETPROMPT %s:\n", (uid_hint && *uid_hint ? "" : _("Passphrase")));
    fflush(childin);
    geanypg_read_till(outpipe[READ], '\n');

    fprintf(childin, "SETDESC %s: %s\n",
                     (uid_hint && *uid_hint ? _("Enter passphrase for") : ""),
                     (uid_hint && *uid_hint ? geanypg_getname(uid_hint) : ""));
    fflush(childin);
    geanypg_read_till(outpipe[READ], '\n');

    fprintf(childin, "GETPIN\n");
    fflush(childin);

    geanypg_read(outpipe[READ], ' ', 2049, readbuffer);
    if (!strncmp(readbuffer, "D", 2))
    {
        while (1)
        {
            char val;
            register ssize_t rv = read(outpipe[READ], &val, 1);
            if (rv <= 0 || val == '\n')
            {
                while (!write(fd, "\n", 1));
                break;
            }
            while (!write(fd, &val, 1));
        }
    }
    else
    {
        unsigned long errval;
        if (!strncmp(readbuffer, "ERR", 4))
        {
            geanypg_read(outpipe[READ], ' ', 2049, readbuffer);
            sscanf(readbuffer, "%lu", &errval);
            geanypg_read(outpipe[READ], '\n', 2049, readbuffer);
            fprintf(stderr, "GeanyPG: %s %lu %s\n", _("pinentry gave error"), errval, readbuffer);
        }
        else
            fprintf(stderr, "GeanyPG: %s\n", _("Unexpected error from pinentry."));
        fclose(childin);
        waitpid(childpid, &status, 0);
        close(outpipe[READ]);
        close(fd);
        return gpgme_err_make(GPG_ERR_SOURCE_PINENTRY,
            (!strncmp(readbuffer, "canceled", 9) ? GPG_ERR_CANCELED : GPG_ERR_GENERAL));
    }


    fclose(childin);
    waitpid(childpid, &status, 0);
    close(outpipe[READ]);
    close(fd);
    return GPG_ERR_NO_ERROR;
}

#else

gpgme_error_t geanypg_passphrase_cb(void *hook,
                                    const char *uid_hint,
                                    const char *passphrase_info,
                                    int prev_was_bad ,
                                    int fd)
{
    dialogs_show_msgbox(GTK_MESSAGE_ERROR, _("Error, Passphrase input without using gpg-agent is not supported on Windows yet."));
    return gpgme_err_make(GPG_ERR_SOURCE_PINENTRY, GPG_ERR_CANCELED);
}
#endif

