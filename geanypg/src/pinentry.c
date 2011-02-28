//      pinentry.c
//
//      Copyright 2011 Hans Alves <alves.h88@gmail.com>
//
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU General Public License as published by
//      the Free Software Foundation; either version 2 of the License, or
//      (at your option) any later version.
//
//      This program is distributed in the hope that it will be useful,
//      but WITHOUT ANY WARRANTY; without even the implied warranty of
//      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//      GNU General Public License for more details.
//
//      You should have received a copy of the GNU General Public License
//      along with this program; if not, write to the Free Software
//      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
//      MA 02110-1301, USA.

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
        unsigned long rv = read(fd, &val, 1);
        if (!rv || val == delim)
            break;
    }
}

static int geanypg_read(int fd, char delim, int max, char * buffer)
{
    int index, rv = 1;
    char ch = 0;
    for (index = 0; (index < max - 1) && rv && ch != delim; ++index)
    {
        rv = read(fd, &ch, 1);
        buffer[index] = ch;
    }
    buffer[index ? index - 1 : 0] = 0;
    return index ? index - 1 : 0;
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
    char readbuffer[2080]; // pinentry should at least support passphrases of up to 2048 characters
    FILE * childin;

    if (pipe(outpipe))
    {
        fprintf(stderr, "GEANYPG: %s\n", strerror(errno));
        return gpgme_error_from_errno(errno);
    }
    if (pipe(inpipe))
    {
        fprintf(stderr, "GEANYPG: %s\n", strerror(errno));
        return gpgme_error_from_errno(errno);
    }

    childpid = fork();
    if (!childpid)
    { // pinentry
        char * argv[] = {"pinentry", NULL};

        close(outpipe[READ]);
        dup2(outpipe[WRITE], STDOUT_FILENO);

        close(inpipe[WRITE]);
        dup2(inpipe[READ], STDIN_FILENO);

        execvp(*argv, argv);
        // shouldn't get here
        fprintf(stderr, "GEANYPG: could not use pinentry.\n%s\n", strerror(errno));
        exit(1); // kill the child
    }
    // GeanpyPG
    close(outpipe[WRITE]);
    close(inpipe[READ]);
    childin = fdopen(inpipe[WRITE], "w");

    // To understand what's happening here, read the pinentry documentation
    geanypg_read(outpipe[READ], ' ', 2049, readbuffer);
    if (strncmp(readbuffer, "OK", 3))
    {
        fprintf(stderr, "GEANYPG: unexpected output from pinentry\n");
        fclose(childin);
        waitpid(childpid, &status, 0);
        close(outpipe[READ]);
        close(fd);
        return gpgme_err_make(GPG_ERR_SOURCE_PINENTRY, GPG_ERR_GENERAL);
    }
    geanypg_read_till(outpipe[READ], '\n'); // read the rest of the first line after OK
    fprintf(childin, "SETTITLE GeanyPG Passphrase entry\n");
    fflush(childin);
    geanypg_read_till(outpipe[READ], '\n');

    fprintf(childin, "SETPROMPT%s\n", (uid_hint && *uid_hint ? "" : " Passphrase:"));
    fflush(childin);
    geanypg_read_till(outpipe[READ], '\n');

    fprintf(childin, "SETDESC %s%s\n",
                     (uid_hint && *uid_hint ? "Enter passphrase for:%0A" : ""),
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
            register unsigned long rv = read(outpipe[READ], &val, 1);
            if (!rv || val == '\n')
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
            fprintf(stderr, "GEANYPG: pinentry gave error %lu %s\n", errval, readbuffer);
        }
        else
            fprintf(stderr, "GEANYPG: unexpected error from pinentry\n");
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
    dialogs_show_msgbox(GTK_MESSAGE_ERROR, "Error, Passphrase input without using gpg-agent is not supported on Windows yet.");
    return gpgme_err_make(GPG_ERR_SOURCE_PINENTRY, GPG_ERR_CANCELED);
}
#endif

