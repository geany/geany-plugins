/**
 * Written by Cédric Tabin
 * http://www.astorm.ch/
 * Version 1.0 - 08.08.2009
 * 
 * Code under licence GPLv2
 * Geany - http://www.geany.org/
 */

#ifndef PRETTY_PRINTER_H
#define PRETTY_PRINTER_H

//========================================== INCLUDES ==========================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//========================================== DEFINES ===========================================================

//defines
#define PRETTY_PRINTING_SUCCESS 0
#define PRETTY_PRINTING_MALLOC_ERROR -1
#define PRETTY_PRINTING_INVALID_CHAR_ERROR 1
#define PRETTY_PRINTING_EMPTY_XML 2
#define PRETTY_PRINTING_NOT_SUPPORTED_YET 3

//base type
#define boolean int

//========================================== STRUCTURES =======================================================

typedef struct 
{
      char indentChar;                                                                           //char used for indentation
      int indentLength;                                                                          //number of char to use for indentation (by default 2 spaces)
      boolean oneLineText;                                                                       //text is put on one line  
      boolean inlineText;                                                                        //if possible text are inline (no return after the opening node and before closing node)
      boolean oneLineComment;                                                                    //comments are put on one line
      boolean inlineComment;                                                                     //if possible comments are inline (no return after the opening node and before closing node)
      boolean oneLineCdata;                                                                      //cdata are put on one line
      boolean inlineCdata;                                                                       //if possible cdata are inline (no return after the opening node and before closing node)
      boolean emptyNodeStripping;                                                                //the empty nodes such <node></node> are set to <node/>
      boolean emptyNodeStrippingSpace;                                                           //put a space before the '/>' when a node is stripped
      boolean forceEmptyNodeSplit;                                                               //force an empty node to be splitted : <node /> becomes <node></node> (only if emptyNodeStripping = false)
	  boolean trimLeadingWhites;                                                                 //trim the leading whites in a text node
	  boolean trimTrailingWhites;                                                                //trim the trailing whites in a text node
} PrettyPrintingOptions;

//========================================== FUNCTIONS =========================================================

int processXMLPrettyPrinting(char** xml, int* length, PrettyPrintingOptions* ppOptions);         //process the pretty-printing on a valid xml string (no check done !!!). The ppOptions ARE NOT FREE-ED after processing. The method returns 0 if the pretty-printing has been done.
PrettyPrintingOptions* createDefaultPrettyPrintingOptions();                                     //creates a default PrettyPrintingOptions object

#endif
