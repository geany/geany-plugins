/*
 * codesearch.h
 *
 * Copyright 2011 Matthew Brush <mbrush@desktop>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA.
 *
 */

#ifndef CODESEARCH_H
#define CODESEARCH_H
#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>
#include "devhelpplugin.h"


void devhelp_plugin_search_code(DevhelpPlugin *self, const gchar *term, const gchar *lang);


#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* CODESEARCH_H */
