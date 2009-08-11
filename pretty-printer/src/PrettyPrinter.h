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

//========================================== INCLUDES ==========================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <glib.h>

//========================================== DEFINES ===========================================================

//defines
#define PRETTY_PRINTING_SUCCESS 0
#define PRETTY_PRINTING_INVALID_CHAR_ERROR 1
#define PRETTY_PRINTING_EMPTY_XML 2
#define PRETTY_PRINTING_NOT_SUPPORTED_YET 3

//========================================== STRUCTURES =======================================================

typedef struct 
{
      char indentChar;                                                                           //char used for indentation
      int indentLength;                                                                          //number of char to use for indentation (by default 2 spaces)
      gboolean oneLineText;                                                                      //text is put on one line  
      gboolean inlineText;                                                                       //if possible text are inline (no return after the opening node and before closing node)
      gboolean oneLineComment;                                                                   //comments are put on one line
      gboolean inlineComment;                                                                    //if possible comments are inline (no return after the opening node and before closing node)
      gboolean oneLineCdata;                                                                     //cdata are put on one line
      gboolean inlineCdata;                                                                      //if possible cdata are inline (no return after the opening node and before closing node)
      gboolean emptyNodeStripping;                                                               //the empty nodes such <node></node> are set to <node/>
      gboolean emptyNodeStrippingSpace;                                                          //put a space before the '/>' when a node is stripped
      gboolean forceEmptyNodeSplit;                                                              //force an empty node to be splitted : <node /> becomes <node></node> (only if emptyNodeStripping = false)
	  gboolean trimLeadingWhites;                                                                //trim the leading whites in a text node
	  gboolean trimTrailingWhites;                                                               //trim the trailing whites in a text node
} PrettyPrintingOptions;

//========================================== FUNCTIONS =========================================================

int processXMLPrettyPrinting(char** xml, int* length, PrettyPrintingOptions* ppOptions);         //process the pretty-printing on a valid xml string (no check done !!!). The ppOptions ARE NOT FREE-ED after processing. The method returns 0 if the pretty-printing has been done.
PrettyPrintingOptions* createDefaultPrettyPrintingOptions();                                     //creates a default PrettyPrintingOptions object

#endif
