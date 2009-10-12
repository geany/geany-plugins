/*
 *  geanydoc.h
 *
 *  Copyright 2008 Yura Siamashka <yurand2@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GEANYDOC_GEANYDOC_H__
#define __GEANYDOC_GEANYDOC_H__

/* from config.c */

void config_init();
void config_uninit();
GKeyFile *config_clone();
void config_set(GKeyFile * cfg);
gchar *config_get_command(const gchar * lang, gint cmd_num, gboolean * intern);


#endif
