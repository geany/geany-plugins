/* json-scanner.h: Tokenizer for JSON
 *
 * This file is part of JSON-GLib
 * Copyright (C) 2008 OpenedHand
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * JsonScanner is a specialized tokenizer for JSON adapted from
 * the GScanner tokenizer in GLib; GScanner came with this notice:
 * 
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/. 
 *
 * JsonScanner: modified by Emmanuele Bassi <ebassi@openedhand.com>
 */

#pragma once

#include <stdbool.h>
#include <glib.h>

G_BEGIN_DECLS

typedef struct _JsonScanner       JsonScanner;

typedef void (* JsonScannerMsgFunc) (JsonScanner *scanner,
                                     const char  *message,
                                     gpointer     user_data);

/* Token types */
typedef enum
{
  JSON_TOKEN_EOF		=   0,

  JSON_TOKEN_LEFT_CURLY		= '{',
  JSON_TOKEN_RIGHT_CURLY	= '}',
  JSON_TOKEN_LEFT_BRACE		= '[',
  JSON_TOKEN_RIGHT_BRACE	= ']',
  JSON_TOKEN_EQUAL_SIGN		= '=',
  JSON_TOKEN_COMMA		= ',',
  JSON_TOKEN_COLON              = ':',

  JSON_TOKEN_NONE		= 256,

  JSON_TOKEN_ERROR,

  JSON_TOKEN_INT,
  JSON_TOKEN_FLOAT,
  JSON_TOKEN_STRING,

  JSON_TOKEN_SYMBOL,
  JSON_TOKEN_IDENTIFIER,

  JSON_TOKEN_COMMENT_SINGLE,
  JSON_TOKEN_COMMENT_MULTI,

  JSON_TOKEN_TRUE,
  JSON_TOKEN_FALSE,
  JSON_TOKEN_NULL,
  JSON_TOKEN_VAR,

  JSON_TOKEN_LAST
} JsonTokenType;

G_GNUC_INTERNAL
JsonScanner *json_scanner_new                  (bool strict);
G_GNUC_INTERNAL
void         json_scanner_destroy              (JsonScanner *scanner);
G_GNUC_INTERNAL
void         json_scanner_input_text           (JsonScanner  *scanner,
                                                const char   *text,
                                                unsigned int  text_len);
G_GNUC_INTERNAL
unsigned int json_scanner_get_next_token       (JsonScanner *scanner);
G_GNUC_INTERNAL
unsigned int json_scanner_peek_next_token      (JsonScanner *scanner);
G_GNUC_INTERNAL
void         json_scanner_set_msg_handler      (JsonScanner        *scanner,
                                                JsonScannerMsgFunc  msg_handler,
                                                gpointer            user_data);
G_GNUC_INTERNAL
void         json_scanner_unknown_token        (JsonScanner  *scanner,
                                                unsigned int  token);

G_GNUC_INTERNAL
gint64       json_scanner_get_int64_value      (const JsonScanner *scanner);
G_GNUC_INTERNAL
double       json_scanner_get_float_value      (const JsonScanner *scanner);
G_GNUC_INTERNAL
const char * json_scanner_get_string_value     (const JsonScanner *scanner);
G_GNUC_INTERNAL
char *       json_scanner_dup_string_value     (const JsonScanner *scanner);
G_GNUC_INTERNAL
const char * json_scanner_get_identifier       (const JsonScanner *scanner);
G_GNUC_INTERNAL
char *       json_scanner_dup_identifier       (const JsonScanner *scanner);

G_GNUC_INTERNAL
unsigned int json_scanner_get_current_line     (const JsonScanner *scanner);
G_GNUC_INTERNAL
unsigned int json_scanner_get_current_position (const JsonScanner *scanner);
G_GNUC_INTERNAL
unsigned int json_scanner_get_current_token    (const JsonScanner *scanner);

G_END_DECLS
