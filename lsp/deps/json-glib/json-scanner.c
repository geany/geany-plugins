/* json-scanner.c: Tokenizer for JSON
 * Copyright (C) 2008 OpenedHand
 *
 * Based on JsonScanner: Flexible lexical scanner for general purpose.
 * Copyright (C) 1997, 1998 Tim Janik
 *
 * Modified by Emmanuele Bassi <ebassi@openedhand.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "json-scanner.h"

#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <glib.h>
#include <glib/gprintf.h>

typedef enum
{
  JSON_ERROR_TYPE_UNKNOWN,
  JSON_ERROR_TYPE_UNEXP_EOF,
  JSON_ERROR_TYPE_UNEXP_EOF_IN_STRING,
  JSON_ERROR_TYPE_UNEXP_EOF_IN_COMMENT,
  JSON_ERROR_TYPE_NON_DIGIT_IN_CONST,
  JSON_ERROR_TYPE_DIGIT_RADIX,
  JSON_ERROR_TYPE_FLOAT_RADIX,
  JSON_ERROR_TYPE_FLOAT_MALFORMED,
  JSON_ERROR_TYPE_MALFORMED_SURROGATE_PAIR,
  JSON_ERROR_TYPE_LEADING_ZERO,
  JSON_ERROR_TYPE_UNESCAPED_CTRL,
  JSON_ERROR_TYPE_MALFORMED_UNICODE
} JsonErrorType;

typedef struct
{
  const char *cset_skip_characters;
  const char *cset_identifier_first;
  const char *cset_identifier_nth;
  const char *cpair_comment_single;
  bool strict;
} JsonScannerConfig;

typedef union
{
  gpointer v_symbol;
  char *v_identifier;
  gint64 v_int64;
  double v_float;
  char *v_string;
  unsigned int v_error;
} JsonTokenValue;

/*< private >
 * JsonScanner:
 *
 * Tokenizer scanner for JSON. See #GScanner
 *
 * Since: 0.6
 */
struct _JsonScanner
{
  /* name of input stream, featured by the default message handler */
  const char *input_name;

  /* link into the scanner configuration */
  JsonScannerConfig config;

  /* fields filled in after json_scanner_get_next_token() */
  unsigned int token;
  JsonTokenValue value;
  unsigned int line;
  unsigned int position;

  /* fields filled in after json_scanner_peek_next_token() */
  unsigned int next_token;
  JsonTokenValue next_value;
  unsigned int next_line;
  unsigned int next_position;

  /* to be considered private */
  const char *text;
  const char *text_end;
  char *buffer;

  /* handler function for _warn and _error */
  JsonScannerMsgFunc msg_handler;
  gpointer user_data;
};

#define	to_lower(c) ( \
  (guchar) ( \
    ( (((guchar)(c))>='A' && ((guchar)(c))<='Z') * ('a'-'A') ) | \
    ( (((guchar)(c))>=192 && ((guchar)(c))<=214) * (224-192) ) | \
    ( (((guchar)(c))>=216 && ((guchar)(c))<=222) * (248-216) ) | \
    ((guchar)(c)) \
  ) \
)

static const gchar json_symbol_names[] =
  "true\0"
  "false\0"
  "null\0"
  "var\0";

static const struct
{
  guint name_offset;
  guint token;
} json_symbols[] = {
  {  0, JSON_TOKEN_TRUE  },
  {  5, JSON_TOKEN_FALSE },
  { 11, JSON_TOKEN_NULL  },
  { 16, JSON_TOKEN_VAR   }
};

static void     json_scanner_get_token_ll    (JsonScanner    *scanner,
                                              unsigned int   *token_p,
                                              JsonTokenValue *value_p,
                                              guint          *line_p,
                                              guint          *position_p);
static void	json_scanner_get_token_i     (JsonScanner    *scanner,
                                              unsigned int   *token_p,
                                              JsonTokenValue *value_p,
                                              guint          *line_p,
                                              guint          *position_p);

static guchar   json_scanner_peek_next_char  (JsonScanner *scanner);
static guchar   json_scanner_get_char        (JsonScanner *scanner,
                                              guint       *line_p,
                                              guint       *position_p);
static bool     json_scanner_get_unichar     (JsonScanner *scanner,
                                              gunichar    *ucs,
                                              guint       *line_p,
                                              guint       *position_p);
static void     json_scanner_error           (JsonScanner *scanner,
                                              const char  *format,
                                              ...) G_GNUC_PRINTF (2,3);

static inline gint
json_scanner_char_2_num (guchar c,
                         guchar base)
{
  if (c >= '0' && c <= '9')
    c -= '0';
  else if (c >= 'A' && c <= 'Z')
    c -= 'A' - 10;
  else if (c >= 'a' && c <= 'z')
    c -= 'a' - 10;
  else
    return -1;
  
  if (c < base)
    return c;
  
  return -1;
}

JsonScanner *
json_scanner_new (bool strict)
{
  JsonScanner *scanner;
  
  scanner = g_new0 (JsonScanner, 1);
  
  scanner->config = (JsonScannerConfig) {
    // Skip whitespace
    .cset_skip_characters = ( " \t\r\n" ),

    // Identifiers can only be lower case
    .cset_identifier_first = (
      G_CSET_a_2_z
    ),
    .cset_identifier_nth = (
      G_CSET_a_2_z
    ),

    // Only used if strict = false
    .cpair_comment_single = ( "//\n" ),
    .strict = strict,
  };

  scanner->token = JSON_TOKEN_NONE;
  scanner->value.v_int64 = 0;
  scanner->line = 1;
  scanner->position = 0;

  scanner->next_token = JSON_TOKEN_NONE;
  scanner->next_value.v_int64 = 0;
  scanner->next_line = 1;
  scanner->next_position = 0;

  return scanner;
}

static inline void
json_scanner_free_value (JsonTokenType  *token_p,
                         JsonTokenValue *value_p)
{
  switch (*token_p)
    {
    case JSON_TOKEN_STRING:
    case JSON_TOKEN_IDENTIFIER:
    case JSON_TOKEN_COMMENT_SINGLE:
    case JSON_TOKEN_COMMENT_MULTI:
      g_free (value_p->v_string);
      break;
      
    default:
      break;
    }
  
  *token_p = JSON_TOKEN_NONE;
}

void
json_scanner_destroy (JsonScanner *scanner)
{
  g_return_if_fail (scanner != NULL);
  
  json_scanner_free_value (&scanner->token, &scanner->value);
  json_scanner_free_value (&scanner->next_token, &scanner->next_value);

  g_free (scanner->buffer);
  g_free (scanner);
}

void
json_scanner_set_msg_handler (JsonScanner        *scanner,
                              JsonScannerMsgFunc  msg_handler,
                              gpointer            user_data)
{
  g_return_if_fail (scanner != NULL);

  scanner->msg_handler = msg_handler;
  scanner->user_data = user_data;
}

static void
json_scanner_error (JsonScanner *scanner,
                    const char  *format,
                    ...)
{
  g_return_if_fail (scanner != NULL);
  g_return_if_fail (format != NULL);

  if (scanner->msg_handler)
    {
      va_list args;
      char *string;
      
      va_start (args, format);
      string = g_strdup_vprintf (format, args);
      va_end (args);
      
      scanner->msg_handler (scanner, string, scanner->user_data);
      
      g_free (string);
    }
}

unsigned int
json_scanner_peek_next_token (JsonScanner *scanner)
{
  g_return_val_if_fail (scanner != NULL, JSON_TOKEN_EOF);

  if (scanner->next_token == JSON_TOKEN_NONE)
    {
      scanner->next_line = scanner->line;
      scanner->next_position = scanner->position;
      json_scanner_get_token_i (scanner,
                                &scanner->next_token,
                                &scanner->next_value,
                                &scanner->next_line,
                                &scanner->next_position);
    }

  return scanner->next_token;
}

unsigned int
json_scanner_get_next_token (JsonScanner *scanner)
{
  g_return_val_if_fail (scanner != NULL, JSON_TOKEN_EOF);

  if (scanner->next_token != JSON_TOKEN_NONE)
    {
      json_scanner_free_value (&scanner->token, &scanner->value);

      scanner->token = scanner->next_token;
      scanner->value = scanner->next_value;
      scanner->line = scanner->next_line;
      scanner->position = scanner->next_position;
      scanner->next_token = JSON_TOKEN_NONE;
    }
  else
    json_scanner_get_token_i (scanner,
                              &scanner->token,
                              &scanner->value,
                              &scanner->line,
                              &scanner->position);

  return scanner->token;
}

void
json_scanner_input_text (JsonScanner *scanner,
                         const char  *text,
                         guint        text_len)
{
  g_return_if_fail (scanner != NULL);
  if (text_len)
    g_return_if_fail (text != NULL);
  else
    text = NULL;

  scanner->token = JSON_TOKEN_NONE;
  scanner->value.v_int64 = 0;
  scanner->line = 1;
  scanner->position = 0;
  scanner->next_token = JSON_TOKEN_NONE;

  scanner->text = text;
  scanner->text_end = text + text_len;

  g_clear_pointer (&scanner->buffer, g_free);
}

static guchar
json_scanner_peek_next_char (JsonScanner *scanner)
{
  if (scanner->text < scanner->text_end)
    return *scanner->text;
  else
    return 0;
}

static guchar
json_scanner_get_char (JsonScanner *scanner,
                       guint       *line_p,
                       guint       *position_p)
{
  guchar fchar;

  if (scanner->text < scanner->text_end)
    fchar = *(scanner->text++);
  else
    fchar = 0;
  
  if (fchar == '\n')
    {
      (*position_p) = 0;
      (*line_p)++;
    }
  else if (fchar)
    {
      (*position_p)++;
    }
  
  return fchar;
}

#define is_hex_digit(c)         (((c) >= '0' && (c) <= '9') || \
                                 ((c) >= 'a' && (c) <= 'f') || \
                                 ((c) >= 'A' && (c) <= 'F'))
#define to_hex_digit(c)         (((c) <= '9') ? (c) - '0' : ((c) & 7) + 9)

static bool
json_scanner_get_unichar (JsonScanner *scanner,
                          gunichar    *ucs,
                          guint       *line_p,
                          guint       *position_p)
{
  gunichar uchar;

  uchar = 0;
  for (int i = 0; i < 4; i++)
    {
      char ch = json_scanner_get_char (scanner, line_p, position_p);

      if (is_hex_digit (ch))
        uchar += ((gunichar) to_hex_digit (ch) << ((3 - i) * 4));
      else
        return false;
    }

  *ucs = uchar;

  return true;
}

/*
 * decode_utf16_surrogate_pair:
 * @units: (array length=2): a pair of UTF-16 code points
 *
 * Decodes a surrogate pair of UTF-16 code points into the equivalent
 * Unicode code point.
 *
 * Returns: the Unicode code point equivalent to the surrogate pair
 */
static inline gunichar
decode_utf16_surrogate_pair (const gunichar units[2])
{
  gunichar ucs;

  /* Already checked by caller */
  g_assert (0xd800 <= units[0] && units[0] <= 0xdbff);
  g_assert (0xdc00 <= units[1] && units[1] <= 0xdfff);

  ucs = 0x10000;
  ucs += (units[0] & 0x3ff) << 10;
  ucs += (units[1] & 0x3ff);

  return ucs;
}

static void
json_scanner_unexp_token (JsonScanner  *scanner,
                          unsigned int  expected_token,
                          const char   *identifier_spec,
                          const char   *symbol_spec,
                          const char   *symbol_name,
                          const char   *message)
{
  char *token_string;
  gsize token_string_len;
  char *expected_string;
  gsize expected_string_len;
  const char *message_prefix;
  bool print_unexp;
  
  g_return_if_fail (scanner != NULL);
  
  if (identifier_spec == NULL)
    identifier_spec = "identifier";
  if (symbol_spec == NULL)
    symbol_spec = "symbol";
  
  token_string_len = 56;
  token_string = g_new (char, token_string_len + 1);
  expected_string_len = 64;
  expected_string = g_new (char, expected_string_len + 1);
  print_unexp = true;
  
  switch (scanner->token)
    {
    case JSON_TOKEN_EOF:
      g_snprintf (token_string, token_string_len, "end of file");
      break;
      
    default:
      if (scanner->token >= 1 && scanner->token <= 255)
	{
	  if ((scanner->token >= ' ' && scanner->token <= '~') ||
	      strchr (scanner->config.cset_identifier_first, scanner->token) ||
	      strchr (scanner->config.cset_identifier_nth, scanner->token))
	    g_snprintf (token_string, token_string_len, "character `%c'", scanner->token);
	  else
	    g_snprintf (token_string, token_string_len, "character `\\%o'", scanner->token);
	  break;
	}
      /* fall through */
    case JSON_TOKEN_SYMBOL:
      if (expected_token == JSON_TOKEN_SYMBOL || expected_token > JSON_TOKEN_LAST)
	print_unexp = false;
      if (symbol_name)
	g_snprintf (token_string, token_string_len,
                    "%s%s `%s'",
                    print_unexp ? "" : "invalid ",
                    symbol_spec,
                    symbol_name);
      else
	g_snprintf (token_string, token_string_len,
                    "%s%s",
                    print_unexp ? "" : "invalid ",
                    symbol_spec);
      break;
 
    case JSON_TOKEN_ERROR:
      print_unexp = false;
      expected_token = JSON_TOKEN_NONE;
      switch (scanner->value.v_error)
	{
	case JSON_ERROR_TYPE_UNEXP_EOF:
	  g_snprintf (token_string, token_string_len, "scanner: unexpected end of file");
	  break;
	  
	case JSON_ERROR_TYPE_UNEXP_EOF_IN_STRING:
	  g_snprintf (token_string, token_string_len, "scanner: unterminated string constant");
	  break;
	  
	case JSON_ERROR_TYPE_UNEXP_EOF_IN_COMMENT:
	  g_snprintf (token_string, token_string_len, "scanner: unterminated comment");
	  break;
	  
	case JSON_ERROR_TYPE_NON_DIGIT_IN_CONST:
	  g_snprintf (token_string, token_string_len, "scanner: non digit in constant");
	  break;
	  
	case JSON_ERROR_TYPE_FLOAT_RADIX:
	  g_snprintf (token_string, token_string_len, "scanner: invalid radix for floating constant");
	  break;
	  
	case JSON_ERROR_TYPE_FLOAT_MALFORMED:
	  g_snprintf (token_string, token_string_len, "scanner: malformed floating constant");
	  break;
	  
	case JSON_ERROR_TYPE_DIGIT_RADIX:
	  g_snprintf (token_string, token_string_len, "scanner: digit is beyond radix");
	  break;

	case JSON_ERROR_TYPE_MALFORMED_SURROGATE_PAIR:
	  g_snprintf (token_string, token_string_len, "scanner: malformed surrogate pair");
	  break;

        case JSON_ERROR_TYPE_LEADING_ZERO:
          g_snprintf (token_string, token_string_len, "scanner: leading zero in number");
          break;

        case JSON_ERROR_TYPE_UNESCAPED_CTRL:
          g_snprintf (token_string, token_string_len, "scanner: unescaped control charater");
          break;

        case JSON_ERROR_TYPE_MALFORMED_UNICODE:
          g_snprintf (token_string, token_string_len, "scanner: malformed Unicode escape");
          break;

	case JSON_ERROR_TYPE_UNKNOWN:
	default:
	  g_snprintf (token_string, token_string_len, "scanner: unknown error");
	  break;
	}
      break;
      
    case JSON_TOKEN_IDENTIFIER:
      if (expected_token == JSON_TOKEN_IDENTIFIER)
	print_unexp = false;
      g_snprintf (token_string, token_string_len,
                  "%s%s `%s'",
                  print_unexp ? "" : "invalid ",
                  identifier_spec,
                  scanner->value.v_string);
      break;
      
    case JSON_TOKEN_INT:
      g_snprintf (token_string, token_string_len, "number `%" G_GINT64_FORMAT "'", scanner->value.v_int64);
      break;
      
    case JSON_TOKEN_FLOAT:
      g_snprintf (token_string, token_string_len, "number `%.3f'", scanner->value.v_float);
      break;
      
    case JSON_TOKEN_STRING:
      if (expected_token == JSON_TOKEN_STRING)
	print_unexp = false;
      g_snprintf (token_string, token_string_len,
                  "%s%sstring constant \"%s\"",
                  print_unexp ? "" : "invalid ",
                  scanner->value.v_string[0] == 0 ? "empty " : "",
                  scanner->value.v_string);
      token_string[token_string_len - 2] = '"';
      token_string[token_string_len - 1] = 0;
      break;
      
    case JSON_TOKEN_COMMENT_SINGLE:
    case JSON_TOKEN_COMMENT_MULTI:
      g_snprintf (token_string, token_string_len, "comment");
      break;
      
    case JSON_TOKEN_NONE:
      /* somehow the user's parsing code is screwed, there isn't much
       * we can do about it.
       * Note, a common case to trigger this is
       * json_scanner_peek_next_token(); json_scanner_unexp_token();
       * without an intermediate json_scanner_get_next_token().
       */
      g_assert_not_reached ();
      break;
    }
  
  
  switch (expected_token)
    {
    case JSON_TOKEN_EOF:
      g_snprintf (expected_string, expected_string_len, "end of file");
      break;
    default:
      if (expected_token >= 1 && expected_token <= 255)
	{
	  if ((expected_token >= ' ' && expected_token <= '~') ||
	      strchr (scanner->config.cset_identifier_first, expected_token) ||
	      strchr (scanner->config.cset_identifier_nth, expected_token))
	    g_snprintf (expected_string, expected_string_len, "character `%c'", expected_token);
	  else
	    g_snprintf (expected_string, expected_string_len, "character `\\%o'", expected_token);
	  break;
	}
      /* fall through */
    case JSON_TOKEN_SYMBOL:
      {
        bool need_valid = (scanner->token == JSON_TOKEN_SYMBOL || scanner->token > JSON_TOKEN_LAST);
        g_snprintf (expected_string, expected_string_len,
                    "%s%s",
                    need_valid ? "valid " : "",
                    symbol_spec);
      }
      break;
    case JSON_TOKEN_INT:
      g_snprintf (expected_string,
                  expected_string_len,
                  "%snumber (integer)",
		  scanner->token == expected_token ? "valid " : "");
      break;
    case JSON_TOKEN_FLOAT:
      g_snprintf (expected_string,
                  expected_string_len,
                  "%snumber (float)",
		  scanner->token == expected_token ? "valid " : "");
      break;
    case JSON_TOKEN_STRING:
      g_snprintf (expected_string,
		  expected_string_len,
		  "%sstring constant",
		  scanner->token == JSON_TOKEN_STRING ? "valid " : "");
      break;
    case JSON_TOKEN_IDENTIFIER:
      g_snprintf (expected_string,
		  expected_string_len,
		  "%s%s",
		  scanner->token == JSON_TOKEN_IDENTIFIER ? "valid " : "",
		  identifier_spec);
      break;
    case JSON_TOKEN_COMMENT_SINGLE:
      g_snprintf (expected_string,
                  expected_string_len,
                  "%scomment (single-line)",
		  scanner->token == expected_token ? "valid " : "");
      break;
    case JSON_TOKEN_COMMENT_MULTI:
      g_snprintf (expected_string,
                  expected_string_len,
                  "%scomment (multi-line)",
		  scanner->token == expected_token ? "valid " : "");
      break;
    case JSON_TOKEN_NONE:
    case JSON_TOKEN_ERROR:
      /* this is handled upon printout */
      break;
    }
  
  if (message && message[0] != 0)
    message_prefix = " - ";
  else
    {
      message_prefix = "";
      message = "";
    }
  if (expected_token == JSON_TOKEN_ERROR)
    {
      json_scanner_error (scanner,
                          "failure around %s%s%s",
                          token_string,
                          message_prefix,
                          message);
    }
  else if (expected_token == JSON_TOKEN_NONE)
    {
      if (print_unexp)
	json_scanner_error (scanner,
                            "unexpected %s%s%s",
                            token_string,
                            message_prefix,
                            message);
      else
	json_scanner_error (scanner,
                            "%s%s%s",
                            token_string,
                            message_prefix,
                            message);
    }
  else
    {
      if (print_unexp)
	json_scanner_error (scanner,
                            "unexpected %s, expected %s%s%s",
                            token_string,
                            expected_string,
                            message_prefix,
                            message);
      else
	json_scanner_error (scanner,
                            "%s, expected %s%s%s",
                            token_string,
                            expected_string,
                            message_prefix,
                            message);
    }
  
  g_free (token_string);
  g_free (expected_string);
}

void
json_scanner_unknown_token (JsonScanner  *scanner,
                            unsigned int  token)
{
  const char *symbol_name;
  char *msg;
  unsigned int cur_token;

  cur_token = json_scanner_get_current_token (scanner);
  msg = NULL;

  symbol_name = NULL;
  for (unsigned i = 0; i < G_N_ELEMENTS (json_symbols); i++)
    if (json_symbols[i].token == token)
      symbol_name = json_symbol_names + json_symbols[i].name_offset;

  if (symbol_name != NULL)
    msg = g_strconcat ("e.g. '", symbol_name, "'", NULL);

  symbol_name = "???";
  for (unsigned i = 0; i < G_N_ELEMENTS (json_symbols); i++)
    if (json_symbols[i].token == cur_token)
      symbol_name = json_symbol_names + json_symbols[i].name_offset;

  json_scanner_unexp_token (scanner, token,
                            NULL, "value",
                            symbol_name,
                            msg);

  g_free (msg);
}

static void
json_scanner_get_token_i (JsonScanner    *scanner,
		          unsigned int   *token_p,
		          JsonTokenValue *value_p,
		          guint	         *line_p,
		          guint	         *position_p)
{
  do
    {
      json_scanner_free_value (token_p, value_p);
      json_scanner_get_token_ll (scanner, token_p, value_p, line_p, position_p);
    }
  while (((*token_p > 0 && *token_p < 256) &&
	  strchr (scanner->config.cset_skip_characters, *token_p)) ||
	 *token_p == JSON_TOKEN_COMMENT_MULTI ||
	 *token_p == JSON_TOKEN_COMMENT_SINGLE);

  switch (*token_p)
    {
    case JSON_TOKEN_IDENTIFIER:
      break;

    case JSON_TOKEN_SYMBOL:
      *token_p = GPOINTER_TO_UINT (value_p->v_symbol);
      break;

    default:
      break;
    }
  
  errno = 0;
}

static void
json_scanner_get_token_ll (JsonScanner    *scanner,
                           unsigned int   *token_p,
                           JsonTokenValue *value_p,
                           guint          *line_p,
                           guint          *position_p)
{
  const JsonScannerConfig *config;
  unsigned int token;
  bool in_comment_multi = false;
  bool in_comment_single = false;
  bool in_string_sq = false;
  bool in_string_dq = false;
  GString *gstring = NULL;
  JsonTokenValue value;
  guchar ch;
  
  config = &scanner->config;
  (*value_p).v_int64 = 0;
  
  if (scanner->text >= scanner->text_end ||
      scanner->token == JSON_TOKEN_EOF)
    {
      *token_p = JSON_TOKEN_EOF;
      return;
    }
  
  gstring = NULL;
  
  do /* while (ch != 0) */
    {
      ch = json_scanner_get_char (scanner, line_p, position_p);

      value.v_int64 = 0;
      token = JSON_TOKEN_NONE;

      /* this is *evil*, but needed ;(
       * we first check for identifier first character, because	 it
       * might interfere with other key chars like slashes or numbers
       */
      if (ch != 0 && strchr (config->cset_identifier_first, ch))
	goto identifier_precedence;

      switch (ch)
	{
	case 0:
	  token = JSON_TOKEN_EOF;
	  (*position_p)++;
	  /* ch = 0; */
	  break;

	case '/':
	  if (config->strict || json_scanner_peek_next_char (scanner) != '*')
	    goto default_case;
	  json_scanner_get_char (scanner, line_p, position_p);
	  token = JSON_TOKEN_COMMENT_MULTI;
	  in_comment_multi = true;
	  gstring = g_string_new (NULL);
	  while ((ch = json_scanner_get_char (scanner, line_p, position_p)) != 0)
	    {
	      if (ch == '*' && json_scanner_peek_next_char (scanner) == '/')
		{
		  json_scanner_get_char (scanner, line_p, position_p);
		  in_comment_multi = false;
		  break;
		}
	      else
		gstring = g_string_append_c (gstring, ch);
	    }
	  ch = 0;
	  break;

	case '"':
	  token = JSON_TOKEN_STRING;
	  in_string_dq = true;
	  gstring = g_string_new (NULL);
	  while ((ch = json_scanner_get_char (scanner, line_p, position_p)) != 0)
	    {
	      if (ch == '"' || token == JSON_TOKEN_ERROR)
		{
		  in_string_dq = false;
		  break;
		}
	      else
		{
		  if (ch == '\\')
		    {
		      ch = json_scanner_get_char (scanner, line_p, position_p);
		      switch (ch)
			{
			  guint	i;
			  guint	fchar;
			  
			case 0:
			  break;

                        case '"':
                          gstring = g_string_append_c (gstring, '"');
                          break;
			  
			case '\\':
			  gstring = g_string_append_c (gstring, '\\');
			  break;

                        case '/':
                          gstring = g_string_append_c (gstring, '/');
                          break;
			  
			case 'n':
			  gstring = g_string_append_c (gstring, '\n');
			  break;
			  
			case 't':
			  gstring = g_string_append_c (gstring, '\t');
			  break;
			  
			case 'r':
			  gstring = g_string_append_c (gstring, '\r');
			  break;
			  
			case 'b':
			  gstring = g_string_append_c (gstring, '\b');
			  break;
			  
			case 'f':
			  gstring = g_string_append_c (gstring, '\f');
			  break;

                        case 'u':
                          fchar = json_scanner_peek_next_char (scanner);
                          if (is_hex_digit (fchar))
                            {
                              gunichar ucs;

                              if (!json_scanner_get_unichar (scanner, &ucs, line_p, position_p))
                                {
                                  token = JSON_TOKEN_ERROR;
                                  value.v_error = JSON_ERROR_TYPE_MALFORMED_UNICODE;
                                  g_string_free (gstring, TRUE);
                                  gstring = NULL;
                                  break;
                                }

                              /* resolve UTF-16 surrogates for Unicode characters not in the BMP,
                               * as per ECMA 404, § 9, "String"
                               */
                              if (g_unichar_type (ucs) == G_UNICODE_SURROGATE)
                                {
                                  unsigned int next_ch;

                                  next_ch = json_scanner_peek_next_char (scanner);
                                  if (next_ch != '\\')
                                    {
                                      token = JSON_TOKEN_ERROR;
                                      value.v_error = JSON_ERROR_TYPE_MALFORMED_SURROGATE_PAIR;
                                      g_string_free (gstring, TRUE);
                                      gstring = NULL;
                                      break;
                                    }
                                  else
                                    ch = json_scanner_get_char (scanner, line_p, position_p);

                                  next_ch = json_scanner_peek_next_char (scanner);
                                  if (next_ch != 'u')
                                    {
                                      token = JSON_TOKEN_ERROR;
                                      value.v_error = JSON_ERROR_TYPE_MALFORMED_SURROGATE_PAIR;
                                      g_string_free (gstring, TRUE);
                                      gstring = NULL;
                                      break;
                                    }
                                  else
                                    ch = json_scanner_get_char (scanner, line_p, position_p);

                                  /* read next surrogate */
                                  gunichar units[2];

                                  units[0] = ucs;

                                  if (!json_scanner_get_unichar (scanner, &ucs, line_p, position_p))
                                    {
                                      token = JSON_TOKEN_ERROR;
                                      value.v_error = JSON_ERROR_TYPE_MALFORMED_UNICODE;
                                      g_string_free (gstring, TRUE);
                                      gstring = NULL;
                                      break;
                                    }

                                  units[1] = ucs;

                                  if (0xdc00 <= units[1] && units[1] <= 0xdfff &&
                                      0xd800 <= units[0] && units[0] <= 0xdbff)
                                    {
                                      ucs = decode_utf16_surrogate_pair (units);
                                      if (!g_unichar_validate (ucs))
                                        {
                                          token = JSON_TOKEN_ERROR;
                                          value.v_error = JSON_ERROR_TYPE_MALFORMED_UNICODE;
                                          g_string_free (gstring, TRUE);
                                          gstring = NULL;
                                          break;
                                        }
                                    }
                                  else
                                    {
                                      token = JSON_TOKEN_ERROR;
                                      value.v_error = JSON_ERROR_TYPE_MALFORMED_SURROGATE_PAIR;
                                      g_string_free (gstring, TRUE);
                                      gstring = NULL;
                                      break;
                                    }
                                }
                              else
                                {
                                  if (!g_unichar_validate (ucs))
                                    {
                                      token = JSON_TOKEN_ERROR;
                                      value.v_error = JSON_ERROR_TYPE_MALFORMED_UNICODE;
                                      g_string_free (gstring, TRUE);
                                      gstring = NULL;
                                      break;
                                    }
                                }

                              gstring = g_string_append_unichar (gstring, ucs);
                            }
                          else
                            {
                              token = JSON_TOKEN_ERROR;
                              value.v_error = JSON_ERROR_TYPE_MALFORMED_UNICODE;
                              g_string_free (gstring, TRUE);
                              gstring = NULL;
                            }
                          break;
			  
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			  i = ch - '0';
			  fchar = json_scanner_peek_next_char (scanner);
			  if (fchar >= '0' && fchar <= '7')
			    {
			      ch = json_scanner_get_char (scanner, line_p, position_p);
			      i = i * 8 + ch - '0';
			      fchar = json_scanner_peek_next_char (scanner);
			      if (fchar >= '0' && fchar <= '7')
				{
				  ch = json_scanner_get_char (scanner, line_p, position_p);
				  i = i * 8 + ch - '0';
				}
			    }
			  gstring = g_string_append_c (gstring, i);
			  break;

			default:
                          token = JSON_TOKEN_ERROR;
                          value.v_error = JSON_ERROR_TYPE_UNESCAPED_CTRL;
                          g_string_free (gstring, TRUE);
                          gstring = NULL;
			  break;
			}
		    }
                  else if (ch == '\n' || ch == '\t' || ch == '\r' || ch == '\f' || ch == '\b')
                    {
                      token = JSON_TOKEN_ERROR;
                      value.v_error = JSON_ERROR_TYPE_UNESCAPED_CTRL;
                      g_string_free (gstring, TRUE);
                      gstring = NULL;
                      break;
                    }
		  else
		    gstring = g_string_append_c (gstring, ch);
		}
	    }
	  ch = 0;
	  break;

        /* {{{ number parsing */
        case '-':
          if (!g_ascii_isdigit (json_scanner_peek_next_char (scanner)))
            {
              token = JSON_TOKEN_ERROR;
              value.v_error = JSON_ERROR_TYPE_NON_DIGIT_IN_CONST;
              ch = 0;
              break;
            }
            G_GNUC_FALLTHROUGH;

	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	{
          bool in_number = true;
          bool leading_sign = ch == '-';
          bool leading_zero = ch == '0';
	  char *endptr;
	  
	  if (token == JSON_TOKEN_NONE)
	    token = JSON_TOKEN_INT;
	  
	  gstring = g_string_new ("");
	  gstring = g_string_append_c (gstring, ch);

          if (leading_sign)
            {
              ch = json_scanner_get_char (scanner, line_p, position_p);
              leading_zero = ch == '0';
              g_string_append_c (gstring, ch);
            }

	  do /* while (in_number) */
	    {
	      bool is_E = token == JSON_TOKEN_FLOAT && (ch == 'e' || ch == 'E');

	      ch = json_scanner_peek_next_char (scanner);

	      if (json_scanner_char_2_num (ch, 36) >= 0 ||
		  ch == '.' ||
		  (is_E && (ch == '+' || ch == '-')))
		{
		  ch = json_scanner_get_char (scanner, line_p, position_p);

		  switch (ch)
		    {
		    case '.':
                      {
                        unsigned int next_ch = json_scanner_peek_next_char (scanner);

                        if (!g_ascii_isdigit (next_ch))
                          {
                            token = JSON_TOKEN_ERROR;
                            value.v_error = JSON_ERROR_TYPE_FLOAT_MALFORMED;
                            in_number = false;
                          }
                        else
			  {
                            token = JSON_TOKEN_FLOAT;
                            gstring = g_string_append_c (gstring, ch);
                          }
                      }
                      break;

		    case '0':
		    case '1':
		    case '2':
		    case '3':
		    case '4':
		    case '5':
		    case '6':
		    case '7':
		    case '8':
		    case '9':
                      if (leading_zero && token != JSON_TOKEN_FLOAT)
                        {
                          token = JSON_TOKEN_ERROR;
                          value.v_error= JSON_ERROR_TYPE_LEADING_ZERO;
                          in_number = false;
                        }
                      else
                        gstring = g_string_append_c (gstring, ch);
		      break;

		    case '-':
		    case '+':
		      if (token != JSON_TOKEN_FLOAT)
			{
			  token = JSON_TOKEN_ERROR;
			  value.v_error = JSON_ERROR_TYPE_NON_DIGIT_IN_CONST;
			  in_number = false;
			}
		      else
			gstring = g_string_append_c (gstring, ch);
		      break;

		    case 'e':
		    case 'E':
                      token = JSON_TOKEN_FLOAT;
                      gstring = g_string_append_c (gstring, ch);
		      break;

		    default:
                      token = JSON_TOKEN_ERROR;
                      value.v_error = JSON_ERROR_TYPE_NON_DIGIT_IN_CONST;
                      in_number = false;
                      break;
		    }
		}
	      else
		in_number = false;
	    }
	  while (in_number);

          if (token != JSON_TOKEN_ERROR)
            {
              endptr = NULL;
              if (token == JSON_TOKEN_FLOAT)
                value.v_float = g_strtod (gstring->str, &endptr);
              else if (token == JSON_TOKEN_INT)
                value.v_int64 = g_ascii_strtoll (gstring->str, &endptr, 10);

              if (endptr && *endptr)
                {
                  token = JSON_TOKEN_ERROR;
                  if (*endptr == 'e' || *endptr == 'E')
                    value.v_error = JSON_ERROR_TYPE_NON_DIGIT_IN_CONST;
                  else
                    value.v_error = JSON_ERROR_TYPE_DIGIT_RADIX;
                }
            }
	  g_string_free (gstring, TRUE);
	  gstring = NULL;
	  ch = 0;
	}
	break; /* number parsing }}} */

	default:
	default_case:
	{
	  if (!config->strict &&
              config->cpair_comment_single &&
	      ch == config->cpair_comment_single[0])
	    {
	      token = JSON_TOKEN_COMMENT_SINGLE;
	      in_comment_single = true;
	      gstring = g_string_new (NULL);
	      ch = json_scanner_get_char (scanner, line_p, position_p);
	      while (ch != 0)
		{
		  if (ch == config->cpair_comment_single[1])
		    {
		      in_comment_single = false;
		      ch = 0;
		      break;
		    }
		  
		  gstring = g_string_append_c (gstring, ch);
		  ch = json_scanner_get_char (scanner, line_p, position_p);
		}
	      /* ignore a missing newline at EOF for single line comments */
	      if (in_comment_single &&
		  config->cpair_comment_single[1] == '\n')
		in_comment_single = false;
	    }
	  else if (ch && strchr (config->cset_identifier_first, ch))
	    {
	    identifier_precedence:
	      
	      if (config->cset_identifier_nth && ch &&
		  strchr (config->cset_identifier_nth,
			  json_scanner_peek_next_char (scanner)))
		{
		  token = JSON_TOKEN_IDENTIFIER;
		  gstring = g_string_new (NULL);
		  gstring = g_string_append_c (gstring, ch);
		  do
		    {
		      ch = json_scanner_get_char (scanner, line_p, position_p);
		      gstring = g_string_append_c (gstring, ch);
		      ch = json_scanner_peek_next_char (scanner);
		    }
		  while (ch && strchr (config->cset_identifier_nth, ch));
		  ch = 0;
		}
	    }
	  if (ch)
	    {
              token = ch;
	      ch = 0;
	    }
	} /* default_case:... */
	break;
	}
      g_assert (ch == 0 && token != JSON_TOKEN_NONE); /* paranoid */
    }
  while (ch != 0);
  
  if (in_comment_multi || in_comment_single ||
      in_string_sq || in_string_dq)
    {
      token = JSON_TOKEN_ERROR;
      if (gstring)
	{
	  g_string_free (gstring, TRUE);
	  gstring = NULL;
	}
      (*position_p)++;
      if (in_comment_multi || in_comment_single)
	value.v_error = JSON_ERROR_TYPE_UNEXP_EOF_IN_COMMENT;
      else /* (in_string_sq || in_string_dq) */
	value.v_error = JSON_ERROR_TYPE_UNEXP_EOF_IN_STRING;
    }
  
  if (gstring)
    {
      value.v_string = g_string_free (gstring, FALSE);
      gstring = NULL;
    }
  
  if (token == JSON_TOKEN_IDENTIFIER)
    {
      for (unsigned i = 0; i < G_N_ELEMENTS (json_symbols); i++)
        {
          const char *symbol = json_symbol_names + json_symbols[i].name_offset;
          if (strcmp (value.v_identifier, symbol) == 0)
            {
              g_free (value.v_identifier);
              token = JSON_TOKEN_SYMBOL;
              value.v_symbol = GUINT_TO_POINTER (json_symbols[i].token);
              break;
            }
        }
    }
  
  *token_p = token;
  *value_p = value;
}

gint64
json_scanner_get_int64_value (const JsonScanner *scanner)
{
  return scanner->value.v_int64;
}

double
json_scanner_get_float_value (const JsonScanner *scanner)
{
  return scanner->value.v_float;
}

const char *
json_scanner_get_string_value (const JsonScanner *scanner)
{
  return scanner->value.v_string;
}

char *
json_scanner_dup_string_value (const JsonScanner *scanner)
{
  return g_strdup (scanner->value.v_string);
}

const char *
json_scanner_get_identifier (const JsonScanner *scanner)
{
  return scanner->value.v_identifier;
}

char *
json_scanner_dup_identifier (const JsonScanner *scanner)
{
  return g_strdup (scanner->value.v_identifier);
}

unsigned int
json_scanner_get_current_line (const JsonScanner *scanner)
{
  return scanner->line;
}

unsigned int
json_scanner_get_current_position (const JsonScanner *scanner)
{
  return scanner->position;
}

unsigned int
json_scanner_get_current_token (const JsonScanner *scanner)
{
  return scanner->token;
}
