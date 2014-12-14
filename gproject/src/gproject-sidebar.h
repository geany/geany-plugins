/*
 * Copyright 2010 Jiri Techet <techet@gmail.com>
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

#ifndef __GPROJECT_SIDEBAR_H__
#define __GPROJECT_SIDEBAR_H__


void gprj_sidebar_init(void);
void gprj_sidebar_cleanup(void);
void gprj_sidebar_activate(gboolean activate);

void gprj_sidebar_find_file_in_active(void);
void gprj_sidebar_find_tag_in_active(void);

void gprj_sidebar_update(gboolean reload);



#endif
