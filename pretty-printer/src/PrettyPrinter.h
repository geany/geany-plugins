/**
 *   Copyright (C) 2009  Cedric Tabin
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License along
 *   with this program; if not, write to the Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef PRETTY_PRINTER_H
#define PRETTY_PRINTER_H

/*========================================== INCLUDES ==========================================================*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>

#ifdef HAVE_GLIB
#include <glib.h>
#endif

/*========================================== DEFINES ===========================================================*/

#define PRETTY_PRINTER_VERSION "1.3"

#define PRETTY_PRINTING_SUCCESS 0
#define PRETTY_PRINTING_INVALID_CHAR_ERROR 1
#define PRETTY_PRINTING_EMPTY_XML 2
#define PRETTY_PRINTING_NOT_SUPPORTED_YET 3
#define PRETTY_PRINTING_SYSTEM_ERROR 4

#ifndef FALSE
#define FALSE (0)
#endif

#ifndef TRUE
#define TRUE !(FALSE)
#endif

/*========================================== STRUCTURES =======================================================*/

/**
 * The PrettyPrintingOptions struct allows the programmer to tell the
 * PrettyPrinter how it must format the XML output.
 */
typedef struct
{
      const char* newLineChars;                                                             /* char used to generate a new line (generally \r\n) */
      char indentChar;                                                                      /* char used for indentation */
      int indentLength;                                                                     /* number of char to use for indentation (by default 2 spaces) */
      bool oneLineText;                                                                     /* text is put on one line   */
      bool inlineText;                                                                      /* if possible text are inline (no return after the opening node and before closing node) */
      bool oneLineComment;                                                                  /* comments are put on one line */
      bool inlineComment;                                                                   /* if possible comments are inline (no return after the opening node and before closing node) */
      bool oneLineCdata;                                                                    /* cdata are put on one line */
      bool inlineCdata;                                                                     /* if possible cdata are inline (no return after the opening node and before closing node) */
      bool emptyNodeStripping;                                                              /* the empty nodes such <node></node> are set to <node/> */
      bool emptyNodeStrippingSpace;                                                         /* put a space before the '/>' when a node is stripped */
      bool forceEmptyNodeSplit;                                                             /* force an empty node to be splitted : <node /> becomes <node></node> (only if emptyNodeStripping = false) */
      bool trimLeadingWhites;                                                               /* trim the leading whites in a text node */
      bool trimTrailingWhites;                                                              /* trim the trailing whites in a text node */
      bool alignComment;                                                                    /* align the comments. If false, comments are untouched (only if oneLineComment = false) */
      bool alignText;                                                                       /* align the text in a node. If false, text is untouched (only if oneLineText = false) */
      bool alignCdata;                                                                      /* align the cdata. If false, cdata is untouched (only if oneLineCdata = false) */
}
PrettyPrintingOptions;

/*========================================== FUNCTIONS =========================================================*/

int processXMLPrettyPrinting(const char *xml, int xml_length, char** output, int* output_length, PrettyPrintingOptions* ppOptions); /* process the pretty-printing on a valid xml string (no check done !!!). The ppOptions ARE NOT FREE-ED after processing. The method returns 0 if the pretty-printing has been done. */
PrettyPrintingOptions* createDefaultPrettyPrintingOptions(void);                                                                    /* creates a default PrettyPrintingOptions object */

#endif
