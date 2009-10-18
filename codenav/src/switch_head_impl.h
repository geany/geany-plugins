/*
 *      switch_head_impl.h - this file is part of "codenavigation", which is
 *      part of the "geany-plugins" project.
 *
 *      Copyright 2009 Lionel Fuentes <funto66(at)gmail(dot)com>
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

#ifndef SWITCH_HEAD_IMPL_H
#define SWITCH_HEAD_IMPL_H

#include "codenavigation.h"

/* Initialization */
void
switch_head_impl_init();

/* Cleanup */
void
switch_head_impl_cleanup();

/* Configuration widget */
GtkWidget*
switch_head_impl_config_widget();

/* Write the configuration of the feature */
void
write_switch_head_impl_config(GKeyFile* key_file);

#endif /* SWITCH_HEAD_IMPL_H */
