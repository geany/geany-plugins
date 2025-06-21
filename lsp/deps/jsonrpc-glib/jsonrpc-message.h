/* jsonrpc-message.h
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef JSONRPC_MESSAGE_H
#define JSONRPC_MESSAGE_H

#include <glib.h>

#include "jsonrpc-version-macros.h"

G_BEGIN_DECLS

typedef struct
{
  char bytes[8];
} JsonrpcMessageMagic __attribute__((aligned (8)));

typedef struct
{
  JsonrpcMessageMagic magic;
} JsonrpcMessageAny __attribute__((aligned (8)));

typedef struct
{
  JsonrpcMessageMagic magic;
  const char *val;
} JsonrpcMessagePutString __attribute__((aligned (8)));

typedef struct
{
  JsonrpcMessageMagic magic;
  const char * const *val;
} JsonrpcMessagePutStrv __attribute__((aligned (8)));

typedef struct
{
  JsonrpcMessageMagic magic;
  const char **valptr;
} JsonrpcMessageGetString __attribute__((aligned (8)));

typedef struct
{
  JsonrpcMessageMagic magic;
  char ***valptr;
} JsonrpcMessageGetStrv __attribute__((aligned (8)));

typedef struct
{
  JsonrpcMessageMagic magic;
  gint32 val;
} JsonrpcMessagePutInt32 __attribute__((aligned (8)));

typedef struct
{
  JsonrpcMessageMagic magic;
  gint32 *valptr;
} JsonrpcMessageGetInt32 __attribute__((aligned (8)));

typedef struct
{
  JsonrpcMessageMagic magic;
  gint64 val;
} JsonrpcMessagePutInt64 __attribute__((aligned (8)));

typedef struct
{
  JsonrpcMessageMagic magic;
  gint64 *valptr;
} JsonrpcMessageGetInt64 __attribute__((aligned (8)));

typedef struct
{
  JsonrpcMessageMagic magic;
  gboolean val;
} JsonrpcMessagePutBoolean __attribute__((aligned (8)));

typedef struct
{
  JsonrpcMessageMagic magic;
  gboolean *valptr;
} JsonrpcMessageGetBoolean __attribute__((aligned (8)));

typedef struct
{
  JsonrpcMessageMagic magic;
  double val;
} JsonrpcMessagePutDouble __attribute__((aligned (8)));

typedef struct
{
  JsonrpcMessageMagic magic;
  double *valptr;
} JsonrpcMessageGetDouble __attribute__((aligned (8)));

typedef struct
{
  JsonrpcMessageMagic magic;
  GVariantIter **iterptr;
} JsonrpcMessageGetIter __attribute__((aligned (8)));

typedef struct
{
  JsonrpcMessageMagic magic;
  GVariantDict **dictptr;
} JsonrpcMessageGetDict __attribute__((aligned (8)));

typedef struct
{
  JsonrpcMessageMagic magic;
  GVariant *val;
} JsonrpcMessagePutVariant __attribute__((aligned (8)));

typedef struct
{
  JsonrpcMessageMagic magic;
  GVariant **variantptr;
} JsonrpcMessageGetVariant __attribute__((aligned (8)));

#define _JSONRPC_MAGIC(s) ("@!^%" s)
#define _JSONRPC_MAGIC_C(a,b,c,d) {'@','!','^','%',a,b,c,d}

#define _JSONRPC_MESSAGE_PUT_STRING_MAGIC  _JSONRPC_MAGIC("PUTS")
#define _JSONRPC_MESSAGE_GET_STRING_MAGIC  _JSONRPC_MAGIC("GETS")
#define _JSONRPC_MESSAGE_PUT_STRV_MAGIC    _JSONRPC_MAGIC("PUTZ")
#define _JSONRPC_MESSAGE_GET_STRV_MAGIC    _JSONRPC_MAGIC("GETZ")
#define _JSONRPC_MESSAGE_PUT_INT32_MAGIC   _JSONRPC_MAGIC("PUTI")
#define _JSONRPC_MESSAGE_GET_INT32_MAGIC   _JSONRPC_MAGIC("GETI")
#define _JSONRPC_MESSAGE_PUT_INT64_MAGIC   _JSONRPC_MAGIC("PUTX")
#define _JSONRPC_MESSAGE_GET_INT64_MAGIC   _JSONRPC_MAGIC("GETX")
#define _JSONRPC_MESSAGE_PUT_BOOLEAN_MAGIC _JSONRPC_MAGIC("PUTB")
#define _JSONRPC_MESSAGE_GET_BOOLEAN_MAGIC _JSONRPC_MAGIC("GETB")
#define _JSONRPC_MESSAGE_PUT_DOUBLE_MAGIC  _JSONRPC_MAGIC("PUTD")
#define _JSONRPC_MESSAGE_GET_DOUBLE_MAGIC  _JSONRPC_MAGIC("GETD")
#define _JSONRPC_MESSAGE_GET_ITER_MAGIC    _JSONRPC_MAGIC("GETT")
#define _JSONRPC_MESSAGE_GET_DICT_MAGIC    _JSONRPC_MAGIC("GETC")
#define _JSONRPC_MESSAGE_PUT_VARIANT_MAGIC _JSONRPC_MAGIC("PUTV")
#define _JSONRPC_MESSAGE_GET_VARIANT_MAGIC _JSONRPC_MAGIC("GETV")

#define _JSONRPC_MESSAGE_PUT_STRING_MAGIC_C  _JSONRPC_MAGIC_C('P','U','T','S')
#define _JSONRPC_MESSAGE_GET_STRING_MAGIC_C  _JSONRPC_MAGIC_C('G','E','T','S')
#define _JSONRPC_MESSAGE_PUT_STRV_MAGIC_C    _JSONRPC_MAGIC_C('P','U','T','Z')
#define _JSONRPC_MESSAGE_GET_STRV_MAGIC_C    _JSONRPC_MAGIC_C('G','E','T','Z')
#define _JSONRPC_MESSAGE_PUT_INT32_MAGIC_C   _JSONRPC_MAGIC_C('P','U','T','I')
#define _JSONRPC_MESSAGE_GET_INT32_MAGIC_C   _JSONRPC_MAGIC_C('G','E','T','I')
#define _JSONRPC_MESSAGE_PUT_INT64_MAGIC_C   _JSONRPC_MAGIC_C('P','U','T','X')
#define _JSONRPC_MESSAGE_GET_INT64_MAGIC_C   _JSONRPC_MAGIC_C('G','E','T','X')
#define _JSONRPC_MESSAGE_PUT_BOOLEAN_MAGIC_C _JSONRPC_MAGIC_C('P','U','T','B')
#define _JSONRPC_MESSAGE_GET_BOOLEAN_MAGIC_C _JSONRPC_MAGIC_C('G','E','T','B')
#define _JSONRPC_MESSAGE_PUT_DOUBLE_MAGIC_C  _JSONRPC_MAGIC_C('P','U','T','D')
#define _JSONRPC_MESSAGE_GET_DOUBLE_MAGIC_C  _JSONRPC_MAGIC_C('G','E','T','D')
#define _JSONRPC_MESSAGE_GET_ITER_MAGIC_C    _JSONRPC_MAGIC_C('G','E','T','T')
#define _JSONRPC_MESSAGE_GET_DICT_MAGIC_C    _JSONRPC_MAGIC_C('G','E','T','C')
#define _JSONRPC_MESSAGE_PUT_VARIANT_MAGIC_C _JSONRPC_MAGIC_C('P','U','T','V')
#define _JSONRPC_MESSAGE_GET_VARIANT_MAGIC_C _JSONRPC_MAGIC_C('G','E','T','V')

#define JSONRPC_MESSAGE_NEW(first_, ...) \
  jsonrpc_message_new(first_, __VA_ARGS__, NULL)
#define JSONRPC_MESSAGE_NEW_ARRAY(first_, ...) \
  jsonrpc_message_new_array(first_, __VA_ARGS__, NULL)
#define JSONRPC_MESSAGE_PARSE(message, ...) \
  jsonrpc_message_parse(message,  __VA_ARGS__, NULL)
#define JSONRPC_MESSAGE_PARSE_ARRAY(iter, ...) \
  jsonrpc_message_parse_array(iter, __VA_ARGS__, NULL)

#define JSONRPC_MESSAGE_PUT_STRING(_val) \
  (&((JsonrpcMessagePutString) { .magic = {_JSONRPC_MESSAGE_PUT_STRING_MAGIC_C}, .val = _val }))
#define JSONRPC_MESSAGE_GET_STRING(_valptr) \
  (&((JsonrpcMessageGetString) { .magic = {_JSONRPC_MESSAGE_GET_STRING_MAGIC_C}, .valptr = _valptr }))

#define JSONRPC_MESSAGE_PUT_STRV(_val) \
  (&((JsonrpcMessagePutStrv) { .magic = {_JSONRPC_MESSAGE_PUT_STRV_MAGIC_C}, .val = _val }))
#define JSONRPC_MESSAGE_GET_STRV(_valptr) \
  (&((JsonrpcMessageGetStrv) { .magic = {_JSONRPC_MESSAGE_GET_STRV_MAGIC_C}, .valptr = _valptr }))

#define JSONRPC_MESSAGE_PUT_INT32(_val) \
  (&((JsonrpcMessagePutInt32) { .magic = {_JSONRPC_MESSAGE_PUT_INT32_MAGIC_C}, .val = _val }))
#define JSONRPC_MESSAGE_GET_INT32(_valptr) \
  (&((JsonrpcMessageGetInt32) { .magic = {_JSONRPC_MESSAGE_GET_INT32_MAGIC_C}, .valptr = _valptr }))

#define JSONRPC_MESSAGE_PUT_INT64(_val) \
  (&((JsonrpcMessagePutInt64) { .magic = {_JSONRPC_MESSAGE_PUT_INT64_MAGIC_C}, .val = _val }))
#define JSONRPC_MESSAGE_GET_INT64(_valptr) \
  (&((JsonrpcMessageGetInt64) { .magic = {_JSONRPC_MESSAGE_GET_INT64_MAGIC_C}, .valptr = _valptr }))

#define JSONRPC_MESSAGE_PUT_BOOLEAN(_val) \
  (&((JsonrpcMessagePutBoolean) { .magic = {_JSONRPC_MESSAGE_PUT_BOOLEAN_MAGIC_C}, .val = _val }))
#define JSONRPC_MESSAGE_GET_BOOLEAN(_valptr) \
  (&((JsonrpcMessageGetBoolean) { .magic = {_JSONRPC_MESSAGE_GET_BOOLEAN_MAGIC_C}, .valptr = _valptr }))

#define JSONRPC_MESSAGE_PUT_DOUBLE(_val) \
  (&((JsonrpcMessagePutDouble) { .magic = {_JSONRPC_MESSAGE_PUT_DOUBLE_MAGIC_C}, .val = _val }))
#define JSONRPC_MESSAGE_GET_DOUBLE(_valptr) \
  (&((JsonrpcMessageGetDouble) { .magic = {_JSONRPC_MESSAGE_GET_DOUBLE_MAGIC_C}, .valptr = _valptr }))

#define JSONRPC_MESSAGE_GET_ITER(_valptr) \
  (&((JsonrpcMessageGetIter) { .magic = {_JSONRPC_MESSAGE_GET_ITER_MAGIC_C}, .iterptr = _valptr }))

#define JSONRPC_MESSAGE_GET_DICT(_valptr) \
  (&((JsonrpcMessageGetDict) { .magic = {_JSONRPC_MESSAGE_GET_DICT_MAGIC_C}, .dictptr = _valptr }))

#define JSONRPC_MESSAGE_PUT_VARIANT(_val) \
  (&((JsonrpcMessagePutVariant) { .magic = {_JSONRPC_MESSAGE_PUT_VARIANT_MAGIC_C}, .val = _val }))
#define JSONRPC_MESSAGE_GET_VARIANT(_valptr) \
  (&((JsonrpcMessageGetVariant) { .magic = {_JSONRPC_MESSAGE_GET_VARIANT_MAGIC_C}, .variantptr = _valptr }))

JSONRPC_AVAILABLE_IN_3_26
GVariant *jsonrpc_message_new         (gconstpointer first_param, ...) G_GNUC_NULL_TERMINATED;

JSONRPC_AVAILABLE_IN_3_26
GVariant *jsonrpc_message_new_array   (gconstpointer first_param, ...) G_GNUC_NULL_TERMINATED;

JSONRPC_AVAILABLE_IN_3_26
gboolean  jsonrpc_message_parse       (GVariant *message, ...) G_GNUC_NULL_TERMINATED;

JSONRPC_AVAILABLE_IN_3_26
gboolean  jsonrpc_message_parse_array (GVariantIter *iter, ...) G_GNUC_NULL_TERMINATED;

G_END_DECLS

#endif /* JSONRPC_MESSAGE_H */
