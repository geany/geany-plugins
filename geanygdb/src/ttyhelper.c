/*
 * Copyright 2008 Jeff Pohlmeyer <yetanothergeek(at)gmail(dot)com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * A little "shell" application to grab the tty name of a console..
 * The tty name is written to the file specified in argv[1] of the
 * command line. After that the program just runs in a loop that
 * calls nanosleep() until some external force causes it to exit.
 */

#include <stdio.h>
#include <time.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
	FILE *f;
	char *tty = NULL;

	if (argc != 2)
	{
		return 1;
	}

	if (!isatty(0))
	{
		return 1;
	}

	tty = ttyname(0);

	if (!(tty && *tty))
	{
		return 1;
	}

	f = fopen(argv[1], "w");

	if (!f)
	{
		return 1;
	}

	fprintf(f, "%s", tty);

	if (fclose(f) != 0)
	{
		return 1;
	}

	while (1)
	{
		struct timespec req = { 1, 0 }, rem;
		nanosleep(&req, &rem);
	}

	return 0;
}
