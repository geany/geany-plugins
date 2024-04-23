/* jsonrpc-version-macros.h
 *
 * Copyright © 2014 Emmanuele Bassi
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef JSONRPC_VERSION_MACROS_H
#define JSONRPC_VERSION_MACROS_H

#if !defined(JSONRPC_GLIB_INSIDE) && !defined(JSONRPC_GLIB_COMPILATION)
# error "Only <jsonrpc-glib.h> can be included directly."
#endif

#include "jsonrpc-version.h"

#ifndef _JSONRPC_EXTERN
#define _JSONRPC_EXTERN extern
#endif

#ifdef JSONRPC_DISABLE_DEPRECATION_WARNINGS
#define JSONRPC_DEPRECATED _JSONRPC_EXTERN
#define JSONRPC_DEPRECATED_FOR(f) _JSONRPC_EXTERN
#define JSONRPC_UNAVAILABLE(maj,min) _JSONRPC_EXTERN
#else
#define JSONRPC_DEPRECATED G_DEPRECATED _JSONRPC_EXTERN
#define JSONRPC_DEPRECATED_FOR(f) G_DEPRECATED_FOR(f) _JSONRPC_EXTERN
#define JSONRPC_UNAVAILABLE(maj,min) G_UNAVAILABLE(maj,min) _JSONRPC_EXTERN
#endif

#define JSONRPC_VERSION_3_26 (G_ENCODE_VERSION (3, 26))
#define JSONRPC_VERSION_3_28 (G_ENCODE_VERSION (3, 28))
#define JSONRPC_VERSION_3_30 (G_ENCODE_VERSION (3, 30))
#define JSONRPC_VERSION_3_40 (G_ENCODE_VERSION (3, 40))
#define JSONRPC_VERSION_3_44 (G_ENCODE_VERSION (3, 44))

#if (JSONRPC_MINOR_VERSION == 99)
# define JSONRPC_VERSION_CUR_STABLE (G_ENCODE_VERSION (JSONRPC_MAJOR_VERSION + 1, 0))
#elif (JSONRPC_MINOR_VERSION % 2)
# define JSONRPC_VERSION_CUR_STABLE (G_ENCODE_VERSION (JSONRPC_MAJOR_VERSION, JSONRPC_MINOR_VERSION + 1))
#else
# define JSONRPC_VERSION_CUR_STABLE (G_ENCODE_VERSION (JSONRPC_MAJOR_VERSION, JSONRPC_MINOR_VERSION))
#endif

#if (JSONRPC_MINOR_VERSION == 99)
# define JSONRPC_VERSION_PREV_STABLE (G_ENCODE_VERSION (JSONRPC_MAJOR_VERSION + 1, 0))
#elif (JSONRPC_MINOR_VERSION % 2)
# define JSONRPC_VERSION_PREV_STABLE (G_ENCODE_VERSION (JSONRPC_MAJOR_VERSION, JSONRPC_MINOR_VERSION - 1))
#else
# define JSONRPC_VERSION_PREV_STABLE (G_ENCODE_VERSION (JSONRPC_MAJOR_VERSION, JSONRPC_MINOR_VERSION - 2))
#endif

/**
 * JSONRPC_VERSION_MIN_REQUIRED:
 *
 * A macro that should be defined by the user prior to including
 * the jsonrpc-glib.h header.
 *
 * The definition should be one of the predefined JSONRPC version
 * macros: %JSONRPC_VERSION_3_26, JSONRPC_VERSION_3_28, ...
 *
 * This macro defines the lower bound for the JSONRPC-GLib API to use.
 *
 * If a function has been deprecated in a newer version of JSONRPC-GLib,
 * it is possible to use this symbol to avoid the compiler warnings
 * without disabling warning for every deprecated function.
 *
 * Since: 3.28
 */
#ifndef JSONRPC_VERSION_MIN_REQUIRED
# define JSONRPC_VERSION_MIN_REQUIRED (JSONRPC_VERSION_CUR_STABLE)
#endif

/**
 * JSONRPC_VERSION_MAX_ALLOWED:
 *
 * A macro that should be defined by the user prior to including
 * the jsonrpc-glib.h header.

 * The definition should be one of the predefined JSONRPC-GLib version
 * macros: %JSONRPC_VERSION_1_0, %JSONRPC_VERSION_1_2,...
 *
 * This macro defines the upper bound for the JSONRPC API to use.
 *
 * If a function has been introduced in a newer version of JSONRPC-GLib,
 * it is possible to use this symbol to get compiler warnings when
 * trying to use that function.
 *
 * Since: 3.26
 */
#ifndef JSONRPC_VERSION_MAX_ALLOWED
# if JSONRPC_VERSION_MIN_REQUIRED > JSONRPC_VERSION_PREV_STABLE
#  define JSONRPC_VERSION_MAX_ALLOWED (JSONRPC_VERSION_MIN_REQUIRED)
# else
#  define JSONRPC_VERSION_MAX_ALLOWED (JSONRPC_VERSION_CUR_STABLE)
# endif
#endif

#if JSONRPC_VERSION_MAX_ALLOWED < JSONRPC_VERSION_MIN_REQUIRED
#error "JSONRPC_VERSION_MAX_ALLOWED must be >= JSONRPC_VERSION_MIN_REQUIRED"
#endif
#if JSONRPC_VERSION_MIN_REQUIRED < JSONRPC_VERSION_3_26
#error "JSONRPC_VERSION_MIN_REQUIRED must be >= JSONRPC_VERSION_3_26"
#endif

#if JSONRPC_VERSION_MIN_REQUIRED >= JSONRPC_VERSION_3_26
# define JSONRPC_DEPRECATED_IN_3_26                JSONRPC_DEPRECATED
# define JSONRPC_DEPRECATED_IN_3_26_FOR(f)         JSONRPC_DEPRECATED_FOR(f)
#else
# define JSONRPC_DEPRECATED_IN_3_26                _JSONRPC_EXTERN
# define JSONRPC_DEPRECATED_IN_3_26_FOR(f)         _JSONRPC_EXTERN
#endif

#if JSONRPC_VERSION_MAX_ALLOWED < JSONRPC_VERSION_3_26
# define JSONRPC_AVAILABLE_IN_3_26                 JSONRPC_UNAVAILABLE(3, 26)
#else
# define JSONRPC_AVAILABLE_IN_3_26                 _JSONRPC_EXTERN
#endif

#if JSONRPC_VERSION_MIN_REQUIRED >= JSONRPC_VERSION_3_28
# define JSONRPC_DEPRECATED_IN_3_28                JSONRPC_DEPRECATED
# define JSONRPC_DEPRECATED_IN_3_28_FOR(f)         JSONRPC_DEPRECATED_FOR(f)
#else
# define JSONRPC_DEPRECATED_IN_3_28                _JSONRPC_EXTERN
# define JSONRPC_DEPRECATED_IN_3_28_FOR(f)         _JSONRPC_EXTERN
#endif

#if JSONRPC_VERSION_MAX_ALLOWED < JSONRPC_VERSION_3_28
# define JSONRPC_AVAILABLE_IN_3_28                 JSONRPC_UNAVAILABLE(3, 28)
#else
# define JSONRPC_AVAILABLE_IN_3_28                 _JSONRPC_EXTERN
#endif

#if JSONRPC_VERSION_MIN_REQUIRED >= JSONRPC_VERSION_3_30
# define JSONRPC_DEPRECATED_IN_3_30                JSONRPC_DEPRECATED
# define JSONRPC_DEPRECATED_IN_3_30_FOR(f)         JSONRPC_DEPRECATED_FOR(f)
#else
# define JSONRPC_DEPRECATED_IN_3_30                _JSONRPC_EXTERN
# define JSONRPC_DEPRECATED_IN_3_30_FOR(f)         _JSONRPC_EXTERN
#endif

#if JSONRPC_VERSION_MAX_ALLOWED < JSONRPC_VERSION_3_30
# define JSONRPC_AVAILABLE_IN_3_30                 JSONRPC_UNAVAILABLE(3, 30)
#else
# define JSONRPC_AVAILABLE_IN_3_30                 _JSONRPC_EXTERN
#endif

#if JSONRPC_VERSION_MIN_REQUIRED >= JSONRPC_VERSION_3_40
# define JSONRPC_DEPRECATED_IN_3_40                JSONRPC_DEPRECATED
# define JSONRPC_DEPRECATED_IN_3_40_FOR(f)         JSONRPC_DEPRECATED_FOR(f)
#else
# define JSONRPC_DEPRECATED_IN_3_40                _JSONRPC_EXTERN
# define JSONRPC_DEPRECATED_IN_3_40_FOR(f)         _JSONRPC_EXTERN
#endif

#if JSONRPC_VERSION_MAX_ALLOWED < JSONRPC_VERSION_3_40
# define JSONRPC_AVAILABLE_IN_3_40                 JSONRPC_UNAVAILABLE(3, 40)
#else
# define JSONRPC_AVAILABLE_IN_3_40                 _JSONRPC_EXTERN
#endif

#if JSONRPC_VERSION_MIN_REQUIRED >= JSONRPC_VERSION_3_44
# define JSONRPC_DEPRECATED_IN_3_44                JSONRPC_DEPRECATED
# define JSONRPC_DEPRECATED_IN_3_44_FOR(f)         JSONRPC_DEPRECATED_FOR(f)
#else
# define JSONRPC_DEPRECATED_IN_3_44                _JSONRPC_EXTERN
# define JSONRPC_DEPRECATED_IN_3_44_FOR(f)         _JSONRPC_EXTERN
#endif

#if JSONRPC_VERSION_MAX_ALLOWED < JSONRPC_VERSION_3_44
# define JSONRPC_AVAILABLE_IN_3_44                 JSONRPC_UNAVAILABLE(3, 44)
#else
# define JSONRPC_AVAILABLE_IN_3_44                 _JSONRPC_EXTERN
#endif

#endif /* JSONRPC_VERSION_MACROS_H */
