/*
 *			ao_wrapwords.h - this file is part of Addons, a Geany plugin
 *
 *			This program is free software; you can redistribute it and/or modify
 *			it under the terms of the GNU General Public License as published by
 *			the Free Software Foundation; either version 2 of the License, or
 *			(at your option) any later version.
 *
 *			This program is distributed in the hope that it will be useful,
 *			but WITHOUT ANY WARRANTY; without even the implied warranty of
 *			MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 *			GNU General Public License for more details.
 *
 *			You should have received a copy of the GNU General Public License
 *			along with this program; if not, write to the Free Software
 *			Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *			MA 02110-1301, USA.
 */

#ifndef __AO_WRAPWORDS_H__
#define __AO_WRAPWORDS_H__


void ao_wrapwords_init(gchar *, GeanyKeyGroup *);
void ao_wrapwords_config(GtkButton *, GtkWidget *);
void ao_wrapwords_set_enabled(gboolean, gboolean);

#endif

