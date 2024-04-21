/* jsonrpc-version.h.in
 *
 * Copyright (C) 2016 Christian Hergert
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

#ifndef JSONRPC_GLIB_VERSION_H
#define JSONRPC_GLIB_VERSION_H

#if !defined(JSONRPC_GLIB_INSIDE) && !defined(JSONRPC_GLIB_COMPILATION)
# error "Only <jsonrpc-glib.h> can be included directly."
#endif

/**
 * SECTION:jsonrpc-version
 * @short_description: jsonrpc-glib version checking
 *
 * jsonrpc-glib provides macros to check the version of the library
 * at compile-time
 */

/**
 * JSONRPC_MAJOR_VERSION:
 *
 * jsonrpc-glib major version component (e.g. 1 if %JSONRPC_VERSION is 1.2.3)
 */
#define JSONRPC_MAJOR_VERSION (3)

/**
 * JSONRPC_MINOR_VERSION:
 *
 * jsonrpc-glib minor version component (e.g. 2 if %JSONRPC_VERSION is 1.2.3)
 */
#define JSONRPC_MINOR_VERSION (44)

/**
 * JSONRPC_MICRO_VERSION:
 *
 * jsonrpc-glib micro version component (e.g. 3 if %JSONRPC_VERSION is 1.2.3)
 */
#define JSONRPC_MICRO_VERSION (1)

/**
 * JSONRPC_VERSION
 *
 * jsonrpc-glib version.
 */
#define JSONRPC_VERSION (3.44.1)

/**
 * JSONRPC_VERSION_S:
 *
 * jsonrpc-glib version, encoded as a string, useful for printing and
 * concatenation.
 */
#define JSONRPC_VERSION_S "3.44.1"

#define JSONRPC_ENCODE_VERSION(major,minor,micro) \
        ((major) << 24 | (minor) << 16 | (micro) << 8)

/**
 * JSONRPC_VERSION_HEX:
 *
 * jsonrpc-glib version, encoded as an hexadecimal number, useful for
 * integer comparisons.
 */
#define JSONRPC_VERSION_HEX \
        (JSONRPC_ENCODE_VERSION (JSONRPC_MAJOR_VERSION, JSONRPC_MINOR_VERSION, JSONRPC_MICRO_VERSION))

/**
 * JSONRPC_CHECK_VERSION:
 * @major: required major version
 * @minor: required minor version
 * @micro: required micro version
 *
 * Compile-time version checking. Evaluates to %TRUE if the version
 * of jsonrpc-glib is greater than the required one.
 */
#define JSONRPC_CHECK_VERSION(major,minor,micro)   \
        (JSONRPC_MAJOR_VERSION > (major) || \
         (JSONRPC_MAJOR_VERSION == (major) && JSONRPC_MINOR_VERSION > (minor)) || \
         (JSONRPC_MAJOR_VERSION == (major) && JSONRPC_MINOR_VERSION == (minor) && \
          JSONRPC_MICRO_VERSION >= (micro)))

#endif /* JSONRPC_GLIB_VERSION_H */
