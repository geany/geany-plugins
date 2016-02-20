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

#include "PrettyPrinter.h"

/*======================= FUNCTIONS ====================================================================*/

/* error reporting functions */
static void PP_ERROR(const char* fmt, ...) G_GNUC_PRINTF(1,2);  /* prints an error message */

/* xml pretty printing functions */
static void putCharInBuffer(char charToAdd);                     /* put a char into the new char buffer */
static void putCharsInBuffer(const char* charsToAdd);            /* put the chars into the new char buffer */
static void putNextCharsInBuffer(int nbChars);                   /* put the next nbChars of the input buffer into the new buffer */
static int readWhites(bool considerLineBreakAsWhite);            /* read the next whites into the input buffer */
static char readNextChar(void);                                  /* read the next char into the input buffer; */
static char getNextChar(void);                                   /* returns the next char but do not increase the input buffer index (use readNextChar for that) */
static char getPreviousInsertedChar(void);                       /* returns the last inserted char into the new buffer */
static bool isWhite(char c);                                     /* check if the specified char is a white */
static bool isSpace(char c);                                     /* check if the specified char is a space */
static bool isLineBreak(char c);                                 /* check if the specified char is a new line */
static bool isQuote(char c);                                     /* check if the specified char is a quote (simple or double) */
static int putNewLine(void);                                     /* put a new line into the new char buffer with the correct number of whites (indentation) */
static bool isInlineNodeAllowed(void);                           /* check if it is possible to have an inline node */
static bool isOnSingleLine(int skip, char stop1, char stop2);    /* check if the current node data is on one line (for inlining) */
static void resetBackwardIndentation(bool resetLineBreak);       /* reset the indentation for the current depth (just reset the index in fact) */
                                                             
/* specific parsing functions */
static int processElements(void);                                /* returns the number of elements processed */
static void processElementAttribute(void);                       /* process on attribute of a node */
static void processElementAttributes(void);                      /* process all the attributes of a node */
static void processHeader(void);                                 /* process the header <?xml version="..." ?> */
static void processNode(void);                                   /* process an XML node */
static void processTextNode(void);                               /* process a text node */
static void processComment(void);                                /* process a comment */
static void processCDATA(void);                                  /* process a CDATA node */
static void processDoctype(void);                                /* process a DOCTYPE node */
static void processDoctypeElement(void);                         /* process a DOCTYPE ELEMENT node */

/* debug function */
static void printError(const char *msg, ...) G_GNUC_PRINTF(1,2); /* just print a message like the printf method */
static void printDebugStatus(void);                              /* just print some variables into the console for debugging */

/*============================================ PRIVATE PROPERTIES ======================================*/

/* those are variables that are shared by the functions and
 * shouldn't be altered. */

static int result;                                                /* result of the pretty printing */
static char* xmlPrettyPrinted;                                    /* new buffer for the formatted XML */
static int xmlPrettyPrintedLength;                                /* buffer size */
static int xmlPrettyPrintedIndex;                                 /* buffer index (position of the next char to insert) */
static const char* inputBuffer;                                   /* input buffer */
static int inputBufferLength;                                     /* input buffer size */
static int inputBufferIndex;                                      /* input buffer index (position of the next char to read into the input string) */
static int currentDepth;                                          /* current depth (for indentation) */
static char* currentNodeName;                                     /* current node name */
static bool appendIndentation;                                /* if the indentation must be added (with a line break before) */
static bool lastNodeOpen;                                     /* defines if the last action was a not opening or not */
static PrettyPrintingOptions* options;                            /* options of PrettyPrinting */

/*============================================ GENERAL FUNCTIONS =======================================*/

static void PP_ERROR(const char* fmt, ...)
{
    va_list va;
    
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    putc('\n', stderr);
    va_end(va);
}

int processXMLPrettyPrinting(const char *xml, int xml_length, char** output, int* output_length, PrettyPrintingOptions* ppOptions)
{
    bool freeOptions;
    char* reallocated;
    
    /* empty buffer, nothing to process */
    if (xml_length == 0) { return PRETTY_PRINTING_EMPTY_XML; }
    if (xml == NULL) { return PRETTY_PRINTING_EMPTY_XML; }
    
    /* initialize the variables */
    result = PRETTY_PRINTING_SUCCESS;
    freeOptions = FALSE;
    if (ppOptions == NULL) 
    { 
        ppOptions = createDefaultPrettyPrintingOptions(); 
        freeOptions = TRUE; 
    }
    
    options = ppOptions;
    currentNodeName = NULL;
    appendIndentation = FALSE;
    lastNodeOpen = FALSE;
    xmlPrettyPrintedIndex = 0;
    inputBufferIndex = 0;
    currentDepth = -1;
    
    inputBuffer = xml;
    inputBufferLength = xml_length;
    
    xmlPrettyPrintedLength = xml_length;
    xmlPrettyPrinted = (char*)g_try_malloc(sizeof(char)*(xml_length));
    if (xmlPrettyPrinted == NULL) { PP_ERROR("Allocation error (initialisation)"); return PRETTY_PRINTING_SYSTEM_ERROR; }
    
    /* go to the first char */
    readWhites(TRUE);

    /* process the pretty-printing */
    processElements();
    
    /* close the buffer */
    putCharInBuffer('\0');
    
    /* adjust the final size */
    reallocated = (char*)g_try_realloc(xmlPrettyPrinted, xmlPrettyPrintedIndex);
    if (reallocated == NULL) {
        PP_ERROR("Allocation error (reallocation size is %d)", xmlPrettyPrintedIndex);
        g_free(xmlPrettyPrinted);
        xmlPrettyPrinted = NULL;
        return PRETTY_PRINTING_SYSTEM_ERROR;
    }
    xmlPrettyPrinted = reallocated;
    
    /* freeing the unused values */
    if (freeOptions) { g_free(options); }
    
    /* if success, then update the values */
    if (result == PRETTY_PRINTING_SUCCESS)
    {
        *output = xmlPrettyPrinted;
        *output_length = xmlPrettyPrintedIndex-2; /* the '\0' is not in the length */
    }
    /* else clean the other values */
    else
    {
        g_free(xmlPrettyPrinted);
    }
    
    /* updating the pointers for the using into the caller function */
    xmlPrettyPrinted = NULL; /* avoid reference */
    inputBuffer = NULL; /* avoid reference */
    currentNodeName = NULL; /* avoid reference */
    options = NULL; /* avoid reference */
    
    /* and finally the result */
    return result;
}

PrettyPrintingOptions* createDefaultPrettyPrintingOptions(void)
{
    PrettyPrintingOptions* defaultOptions = (PrettyPrintingOptions*)g_try_malloc(sizeof(PrettyPrintingOptions));
    if (defaultOptions == NULL) 
    { 
        PP_ERROR("Unable to allocate memory for PrettyPrintingOptions");
        return NULL;
    }
    
    defaultOptions->newLineChars = "\r\n";
    defaultOptions->indentChar = ' ';
    defaultOptions->indentLength = 2;
    defaultOptions->oneLineText = FALSE;
    defaultOptions->inlineText = TRUE;
    defaultOptions->oneLineComment = FALSE;
    defaultOptions->inlineComment = TRUE;
    defaultOptions->oneLineCdata = FALSE;
    defaultOptions->inlineCdata = TRUE;
    defaultOptions->emptyNodeStripping = TRUE;
    defaultOptions->emptyNodeStrippingSpace = TRUE;
    defaultOptions->forceEmptyNodeSplit = FALSE;
    defaultOptions->trimLeadingWhites = TRUE;
    defaultOptions->trimTrailingWhites = TRUE;
    defaultOptions->alignComment = TRUE;
    defaultOptions->alignText = TRUE;
    defaultOptions->alignCdata = TRUE;
    
    return defaultOptions;
}

void putNextCharsInBuffer(int nbChars)
{
    int i;
    for (i=0 ; i<nbChars ; ++i)
    {
        char c = readNextChar();
        putCharInBuffer(c);
    }
}

void putCharInBuffer(char charToAdd)
{
    /* check if the buffer is full and reallocation if needed */
    if (xmlPrettyPrintedIndex >= xmlPrettyPrintedLength)
    {
        char* reallocated;
        
        if (charToAdd == '\0') { ++xmlPrettyPrintedLength; }
        else { xmlPrettyPrintedLength += inputBufferLength; }
        reallocated = (char*)g_try_realloc(xmlPrettyPrinted, xmlPrettyPrintedLength);
        if (reallocated == NULL) { PP_ERROR("Allocation error (char was %c)", charToAdd); return; }
        xmlPrettyPrinted = reallocated;
    }
    
    /* putting the char and increase the index for the next one */
    xmlPrettyPrinted[xmlPrettyPrintedIndex] = charToAdd;
    ++xmlPrettyPrintedIndex;
}

void putCharsInBuffer(const char* charsToAdd)
{
    int currentIndex = 0;
    while (charsToAdd[currentIndex] != '\0')
    {
        putCharInBuffer(charsToAdd[currentIndex]);
        ++currentIndex;
    }
}

char getPreviousInsertedChar(void)
{
    return xmlPrettyPrinted[xmlPrettyPrintedIndex-1];
}

int putNewLine(void)
{
    int spaces;
    int i;
    
    putCharsInBuffer(options->newLineChars);
    spaces = currentDepth*options->indentLength;
    for(i=0 ; i<spaces ; ++i)
    {
        putCharInBuffer(options->indentChar);
    }
    
    return spaces;
}

char getNextChar(void)
{
    return inputBuffer[inputBufferIndex];
}

char readNextChar(void)
{   
    return inputBuffer[inputBufferIndex++];
}

int readWhites(bool considerLineBreakAsWhite)
{
    int counter = 0;
    while(isWhite(inputBuffer[inputBufferIndex]) && 
          (!isLineBreak(inputBuffer[inputBufferIndex]) || 
           considerLineBreakAsWhite))
    {
        ++counter;
        ++inputBufferIndex;
    }
    
    return counter;
}

bool isQuote(char c)
{
    return (c == '\'' ||
            c == '\"');
}

bool isWhite(char c)
{
    return (isSpace(c) ||
            isLineBreak(c));
}

bool isSpace(char c)
{
    return (c == ' ' ||
            c == '\t');
}

bool isLineBreak(char c)
{
    return (c == '\n' || 
            c == '\r');
}

bool isInlineNodeAllowed(void)
{
    int firstChar;
    int secondChar;
    int thirdChar;
    int currentIndex;
    char currentChar;
    
    /* the last action was not an opening => inline not allowed */
    if (!lastNodeOpen) { return FALSE; }
    
    firstChar = getNextChar(); /* should be '<' or we are in a text node */
    secondChar = inputBuffer[inputBufferIndex+1]; /* should be '!' */
    thirdChar = inputBuffer[inputBufferIndex+2]; /* should be '-' or '[' */
    
    /* loop through the content up to the next opening/closing node */
    currentIndex = inputBufferIndex+1;
    if (firstChar == '<')
    {
        char closingComment = '-';
        char oldChar = ' ';
        bool loop = TRUE;
        
        /* another node is being open ==> no inline ! */
        if (secondChar != '!') { return FALSE; }
        
        /* okay we are in a comment/cdata node, so read until it is closed */
        
        /* select the closing char */
        if (thirdChar == '[') { closingComment = ']'; }
        
        /* read until closing */
        currentIndex += 3; /* that bypass meanless chars */
        while (loop)
        {
            char current = inputBuffer[currentIndex];
            if (current == closingComment && oldChar == closingComment) { loop = FALSE; } /* end of comment/cdata */
            oldChar = current;
            ++currentIndex;
        }
        
        /* okay now avoid blanks */
        /*  inputBuffer[index] is now '>' */
        ++currentIndex;
        while (isWhite(inputBuffer[currentIndex])) { ++currentIndex; }
    }
    else
    {
        /* this is a text node. Simply loop to the next '<' */
        while (inputBuffer[currentIndex] != '<') { ++currentIndex; }
    }
    
    /* check what do we have now */
    currentChar = inputBuffer[currentIndex];
    if (currentChar == '<')
    {
        /* check if that is a closing node */
        currentChar = inputBuffer[currentIndex+1];
        if (currentChar == '/')
        {
            /* as we are in a correct XML (so far...), if the node is  */
            /* being directly closed, the inline is allowed !!! */
            return TRUE;
        }
    }
    
    /* inline not allowed... */
    return FALSE;
}

bool isOnSingleLine(int skip, char stop1, char stop2)
{
    int currentIndex = inputBufferIndex+skip; /* skip the n first chars (in comment <!--) */
    bool onSingleLine = TRUE;
    
    char oldChar = inputBuffer[currentIndex];
    char currentChar = inputBuffer[currentIndex+1];
    while(onSingleLine && oldChar != stop1 && currentChar != stop2)
    {
        onSingleLine = !isLineBreak(oldChar);
        
        ++currentIndex;
        oldChar = currentChar;
        currentChar = inputBuffer[currentIndex+1];
        
        /**
         * A line break inside the node has been reached. But we should check
         * if there is something before the end of the node (otherwise, there
         * are only spaces and it may be wanted to be considered as a single
         * line). //TODO externalize an option for that ?
         */
        if (!onSingleLine)
        {
            while(oldChar != stop1 && currentChar != stop2)
            {
                /* okay there is something else => this is not on one line */
                if (!isWhite(oldChar)) return FALSE;
              
                ++currentIndex;
                oldChar = currentChar;
                currentChar = inputBuffer[currentIndex+1];
            }
            
            /* the end of the node has been reached with only whites. Then
             * the node can be considered being one single line */
            return TRUE;
        }
    }
    
    return onSingleLine;
}

void resetBackwardIndentation(bool resetLineBreak)
{
    xmlPrettyPrintedIndex -= (currentDepth*options->indentLength);
    if (resetLineBreak) 
    { 
        int len = strlen(options->newLineChars);
        xmlPrettyPrintedIndex -= len; 
    }
}

/*#########################################################################################################################################*/
/*-----------------------------------------------------------------------------------------------------------------------------------------*/
                                                                                                
/*-----------------------------------------------------------------------------------------------------------------------------------------*/
/*=============================================================== NODE FUNCTIONS ==========================================================*/
/*-----------------------------------------------------------------------------------------------------------------------------------------*/
                                                                                                
/*-----------------------------------------------------------------------------------------------------------------------------------------*/
/*#########################################################################################################################################*/

int processElements(void)
{
    int counter = 0;
    bool loop = TRUE;
    ++currentDepth;
    while (loop && result == PRETTY_PRINTING_SUCCESS)
    {
        bool indentBackward;
        char nextChar;
        
        /* strip unused whites */
        readWhites(TRUE);
        
        nextChar = getNextChar();
        if (nextChar == '\0') { return 0; } /* no more data to read */
        
        /* put a new line with indentation */
        if (appendIndentation) { putNewLine(); }
        
        /* always append indentation (but need to store the state) */
        indentBackward = appendIndentation;
        appendIndentation = TRUE; 
        
        /* okay what do we have now ? */
        if (nextChar != '<')
        { 
            /* a simple text node */
            processTextNode(); 
            ++counter; 
        } 
        else /* some more check are needed */
        {
            nextChar = inputBuffer[inputBufferIndex+1];
            if (nextChar == '!') 
            {
                char oneMore = inputBuffer[inputBufferIndex+2];
                if (oneMore == '-') { processComment(); ++counter; } /* a comment */
                else if (oneMore == '[') { processCDATA(); ++counter; } /* cdata */
                else if (oneMore == 'D') { processDoctype(); ++counter; } /* doctype <!DOCTYPE ... > */
                else if (oneMore == 'E') { processDoctypeElement(); ++counter; } /* doctype element <!ELEMENT ... > */
                else 
                { 
                    printError("processElements : Invalid char '%c' afer '<!'", oneMore); 
                    result = PRETTY_PRINTING_INVALID_CHAR_ERROR; 
                }
            } 
            else if (nextChar == '/')
            { 
                /* close a node => stop the loop !! */
                loop = FALSE; 
                if (indentBackward) 
                { 
                    /* INDEX HACKING */
                    xmlPrettyPrintedIndex -= options->indentLength; 
                } 
            }
            else if (nextChar == '?')
            {
                /* this is a header */
                processHeader();
            }
            else 
            {
                /* a new node is open */
                processNode();
                ++counter;
            } 
        }
    }
    
    --currentDepth;
    return counter;
}

void processElementAttribute(void)
{
    char quote;
    char value;
    /* process the attribute name */
    char nextChar = readNextChar();
    while (nextChar != '=')
    {
        putCharInBuffer(nextChar);
        nextChar = readNextChar();
    }
    
    putCharInBuffer(nextChar); /* that's the '=' */
    
    /* read the simple quote or double quote and put it into the buffer */
    quote = readNextChar();
    putCharInBuffer(quote); 
    
    /* process until the last quote */
    value = readNextChar();
    while(value != quote)
    {
        putCharInBuffer(value);
        value = readNextChar();
    }
    
    /* simply add the last quote */
    putCharInBuffer(quote);
}

void processElementAttributes(void)
{
    bool loop = TRUE;
    char current = getNextChar(); /* should not be a white */
    if (isWhite(current)) 
    { 
        printError("processElementAttributes : first char shouldn't be a white"); 
        result = PRETTY_PRINTING_INVALID_CHAR_ERROR; 
        return; 
    }
    
    while (loop)
    {
        char next;
        
        readWhites(TRUE); /* strip the whites */
        
        next = getNextChar(); /* don't read the last char (processed afterwards) */
        if (next == '/') { loop = FALSE; } /* end of node */
        else if (next == '>') { loop = FALSE; } /* end of tag */
        else if (next == '?') { loop = FALSE; } /* end of header */
        else 
        { 
            putCharInBuffer(' '); /* put only one space to separate attributes */
            processElementAttribute(); 
        }
    }
}

void processHeader(void)
{
    int firstChar = inputBuffer[inputBufferIndex]; /* should be '<' */
    int secondChar = inputBuffer[inputBufferIndex+1]; /* must be '?' */
    
    if (firstChar != '<') 
    { 
        /* what ?????? invalid xml !!! */ 
        printError("processHeader : first char should be '<' (not '%c')", firstChar); 
        result = PRETTY_PRINTING_INVALID_CHAR_ERROR; return; 
    }
    
    if (secondChar == '?')
    { 
        /* puts the '<' and '?' chars into the new buffer */
        putNextCharsInBuffer(2); 
        
        while(!isWhite(getNextChar())) { putNextCharsInBuffer(1); }
        
        readWhites(TRUE);
        processElementAttributes(); 
        
        /* puts the '?' and '>' chars into the new buffer */
        putNextCharsInBuffer(2); 
    }
}

void processNode(void)
{
    char closeChar;
    int subElementsProcessed = 0;
    char nextChar;
    char* nodeName;
    int nodeNameLength = 0;
    int i;
    int opening = readNextChar();
    if (opening != '<') 
    { 
        printError("processNode : The first char should be '<' (not '%c')", opening); 
        result = PRETTY_PRINTING_INVALID_CHAR_ERROR; 
        return; 
    }
    
    putCharInBuffer(opening);
    
    /* read the node name */
    while (!isWhite(getNextChar()) && 
           getNextChar() != '>' &&  /* end of the tag */
           getNextChar() != '/') /* tag is being closed */
    {
        putNextCharsInBuffer(1);
        ++nodeNameLength;
    }

    /* store the name */
    nodeName = (char*)g_try_malloc(sizeof(char)*nodeNameLength+1);
    if (nodeName == NULL) { PP_ERROR("Allocation error (node name length is %d)", nodeNameLength); return ; }
    nodeName[nodeNameLength] = '\0';
    for (i=0 ; i<nodeNameLength ; ++i)
    {
        int tempIndex = xmlPrettyPrintedIndex-nodeNameLength+i;
        nodeName[i] = xmlPrettyPrinted[tempIndex];
    }
    
    currentNodeName = nodeName; /* set the name for using in other methods */
    lastNodeOpen = TRUE;

    /* process the attributes     */
    readWhites(TRUE);
    processElementAttributes();
    
    /* process the end of the tag */
    subElementsProcessed = 0;
    nextChar = getNextChar(); /* should be either '/' or '>' */
    if (nextChar == '/') /* the node is being closed immediatly */
    { 
        /* closing node directly */
        if (options->emptyNodeStripping || !options->forceEmptyNodeSplit)
        {
            if (options->emptyNodeStrippingSpace) { putCharInBuffer(' '); }
            putNextCharsInBuffer(2); 
        }
        /* split the closing nodes */
        else
        {
            readNextChar(); /* removing '/' */
            readNextChar(); /* removing '>' */
            
            putCharInBuffer('>');
            if (!options->inlineText) 
            {
                /* no inline text => new line ! */
                putNewLine(); 
            } 
            
            putCharsInBuffer("</");
            putCharsInBuffer(currentNodeName);
            putCharInBuffer('>');
        }
        
        lastNodeOpen=FALSE; 
        return; 
    }
    else if (nextChar == '>') 
    { 
        /* the tag is just closed (maybe some content) */
        putNextCharsInBuffer(1); 
        subElementsProcessed = processElements();
    } 
    else 
    { 
        printError("processNode : Invalid character '%c'", nextChar);
        result = PRETTY_PRINTING_INVALID_CHAR_ERROR; 
        return; 
    }
    
    /* if the code reaches this area, then the processElements has been called and we must
     * close the opening tag */
    closeChar = getNextChar();
    if (closeChar != '<') 
    { 
        printError("processNode : Invalid character '%c' for closing tag (should be '<')", closeChar); 
        result = PRETTY_PRINTING_INVALID_CHAR_ERROR; 
        return; 
    }
    
    do
    {
        closeChar = readNextChar();
        putCharInBuffer(closeChar);
    }
    while(closeChar != '>');
    
    /* there is no elements */
    if (subElementsProcessed == 0)
    {
        /* the node will be stripped */
        if (options->emptyNodeStripping)
        {
            /* because we have '<nodeName ...></nodeName>' */
            xmlPrettyPrintedIndex -= nodeNameLength+4; 
            resetBackwardIndentation(TRUE);
            
            if (options->emptyNodeStrippingSpace) { putCharInBuffer(' '); }
            putCharsInBuffer("/>");
        }
        /* the closing tag will be put on the same line */
        else if (options->inlineText)
        {
            /* correct the index because we have '</nodeName>' */
            xmlPrettyPrintedIndex -= nodeNameLength+3; 
            resetBackwardIndentation(TRUE);
            
            /* rewrite the node name */
            putCharsInBuffer("</");
            putCharsInBuffer(currentNodeName);
            putCharInBuffer('>');
        }
    }
    
    /* the node is closed */
    lastNodeOpen = FALSE;
    
    /* freeeeeeee !!! */
    g_free(nodeName);
    nodeName = NULL;
    currentNodeName = NULL;
}

void processComment(void)
{
    char lastChar;
    bool loop = TRUE;
    char oldChar;
    bool inlineAllowed = FALSE;
    if (options->inlineComment) { inlineAllowed = isInlineNodeAllowed(); }
    if (inlineAllowed && !options->oneLineComment) { inlineAllowed = isOnSingleLine(4, '-', '-'); }
    if (inlineAllowed) { resetBackwardIndentation(TRUE); }
    
    putNextCharsInBuffer(4); /* add the chars '<!--' */
    
    oldChar = '-';
    while (loop)
    {
        char nextChar = readNextChar();
        if (oldChar == '-' && nextChar == '-') /* comment is being closed */
        {
            loop = FALSE;
        }
        
        if (!isLineBreak(nextChar)) /* the comment simply continues */
        {
            if (options->oneLineComment && isSpace(nextChar))
            {
                /* removes all the unecessary spaces */
                while(isSpace(getNextChar()))
                {
                    nextChar = readNextChar();
                }
                putCharInBuffer(' ');
                oldChar = ' ';
            }
            else
            {
                /* comment is left untouched */
                putCharInBuffer(nextChar);
                oldChar = nextChar;
            }
            
            if (!loop && options->alignComment) /* end of comment */
            {
                /* ensures the chars preceding the first '-' are all spaces (there are at least
                 * 5 spaces in front of the '-->' for the alignment with '<!--') */
                bool onlySpaces = xmlPrettyPrinted[xmlPrettyPrintedIndex-3] == ' ' &&
                                  xmlPrettyPrinted[xmlPrettyPrintedIndex-4] == ' ' &&
                                  xmlPrettyPrinted[xmlPrettyPrintedIndex-5] == ' ' &&
                                  xmlPrettyPrinted[xmlPrettyPrintedIndex-6] == ' ' &&
                                  xmlPrettyPrinted[xmlPrettyPrintedIndex-7] == ' ';
                
                /* if all the preceding chars are white, then go for replacement */
                if (onlySpaces)
                {
                    xmlPrettyPrintedIndex -= 7; /* remove indentation spaces */
                    putCharsInBuffer("--"); /* reset the first chars of '-->' */
                }
            }
        }
        else if (!options->oneLineComment && !inlineAllowed) /* oh ! there is a line break */
        {
            /* if the comments need to be aligned, just add 5 spaces */
            if (options->alignComment) 
            {
                int read = readWhites(FALSE); /* strip the whites and new line */
                if (nextChar == '\r' && read == 0 && getNextChar() == '\n') /* handles the \r\n return line */
                {
                    readNextChar(); 
                    readWhites(FALSE);
                }
              
                putNewLine(); /* put a new indentation line */
                putCharsInBuffer("     "); /* align with <!--  */
                oldChar = ' '; /* and update the last char */
            }
            else
            {
                putCharInBuffer(nextChar);
                oldChar = nextChar;
            }
        }
        else /* the comments must be inlined */
        {
            readWhites(TRUE); /* strip the whites and add a space if needed */
            if (getPreviousInsertedChar() != ' ' &&
                strncmp(xmlPrettyPrinted+xmlPrettyPrintedIndex-4, "<!--", 4) != 0) /* prevents adding a space at the beginning  */
            { 
                putCharInBuffer(' '); 
                oldChar = ' ';
            }
        }
    }
    
    lastChar = readNextChar(); /* should be '>' */
    if (lastChar != '>') 
    { 
        printError("processComment : last char must be '>' (not '%c')", lastChar); 
        result = PRETTY_PRINTING_INVALID_CHAR_ERROR; 
        return; 
    }
    putCharInBuffer(lastChar);
    
    if (inlineAllowed) { appendIndentation = FALSE; }
    
    /* there vas no node open */
    lastNodeOpen = FALSE;
}

void processTextNode(void)
{
    /* checks if inline is allowed */
    bool inlineTextAllowed = FALSE;
    if (options->inlineText) { inlineTextAllowed = isInlineNodeAllowed(); }
    if (inlineTextAllowed && !options->oneLineText) { inlineTextAllowed = isOnSingleLine(0, '<', '/'); }
    if (inlineTextAllowed || !options->alignText) 
    { 
        resetBackwardIndentation(TRUE); /* remove previous indentation */
        if (!inlineTextAllowed) { putNewLine(); }
    } 
   
    /* the leading whites are automatically stripped. So we re-add it */
    if (!options->trimLeadingWhites)
    {
        int backwardIndex = inputBufferIndex-1;
        while (isSpace(inputBuffer[backwardIndex])) 
        { 
            --backwardIndex; /* backward rolling */
        } 
        
        /* now the input[backwardIndex] IS NOT a white. So we go to
         * the next char... */
        ++backwardIndex;
        
        /* and then re-add the whites */
        while (inputBuffer[backwardIndex] == ' ' || 
               inputBuffer[backwardIndex] == '\t') 
        {
            putCharInBuffer(inputBuffer[backwardIndex]);
            ++backwardIndex;
        }
    }
    
    /* process the text into the node */
    while(getNextChar() != '<')
    {
        char nextChar = readNextChar();
        if (isLineBreak(nextChar))
        {
            if (options->oneLineText)
            { 
                readWhites(TRUE);
              
                /* as we can put text on one line, remove the line break 
                 * and replace it by a space but only if the previous 
                 * char wasn't a space */
                if (getPreviousInsertedChar() != ' ') { putCharInBuffer(' '); }
            }
            else if (options->alignText)
            {
                int read = readWhites(FALSE);
                if (nextChar == '\r' && read == 0 && getNextChar() == '\n') /* handles the '\r\n' */
                {
                   nextChar = readNextChar();
                   readWhites(FALSE);
                }
              
                /* put a new line only if the closing tag is not reached */
                if (getNextChar() != '<') 
                {   
                    putNewLine(); 
                } 
            }
            else
            {
                putCharInBuffer(nextChar);
            }
        }
        else
        {
            putCharInBuffer(nextChar);
        }
    }
    
    /* strip the trailing whites */
    if (options->trimTrailingWhites)
    {
        while(getPreviousInsertedChar() == ' ' || 
              getPreviousInsertedChar() == '\t')
        {
            --xmlPrettyPrintedIndex;
        }
    }
    
    /* remove the indentation for the closing tag */
    if (inlineTextAllowed) { appendIndentation = FALSE; }
    
    /* there vas no node open */
    lastNodeOpen = FALSE;
}

void processCDATA(void)
{
    char lastChar;
    bool loop = TRUE;
    char oldChar;
    bool inlineAllowed = FALSE;
    if (options->inlineCdata) { inlineAllowed = isInlineNodeAllowed(); }
    if (inlineAllowed && !options->oneLineCdata) { inlineAllowed = isOnSingleLine(9, ']', ']'); }
    if (inlineAllowed) { resetBackwardIndentation(TRUE); }
    
    putNextCharsInBuffer(9); /* putting the '<![CDATA[' into the buffer */
    
    oldChar = '[';
    while(loop)
    {
        char nextChar = readNextChar();
        char nextChar2 = getNextChar();
        if (oldChar == ']' && nextChar == ']' && nextChar2 == '>') { loop = FALSE; } /* end of cdata */
        
        if (!isLineBreak(nextChar)) /* the cdata simply continues */
        {
            if (options->oneLineCdata && isSpace(nextChar))
            {
                /* removes all the unecessary spaces */
                while(isSpace(nextChar2))
                {
                    nextChar = readNextChar();
                    nextChar2 = getNextChar();
                }
                
                putCharInBuffer(' ');
                oldChar = ' ';
            }
            else
            {
                /* comment is left untouched */
                putCharInBuffer(nextChar);
                oldChar = nextChar;
            }
            
            if (!loop && options->alignCdata) /* end of cdata */
            {
                /* ensures the chars preceding the first '-' are all spaces (there are at least
                 * 10 spaces in front of the ']]>' for the alignment with '<![CDATA[') */
                bool onlySpaces = xmlPrettyPrinted[xmlPrettyPrintedIndex-3] == ' ' &&
                                  xmlPrettyPrinted[xmlPrettyPrintedIndex-4] == ' ' &&
                                  xmlPrettyPrinted[xmlPrettyPrintedIndex-5] == ' ' &&
                                  xmlPrettyPrinted[xmlPrettyPrintedIndex-6] == ' ' &&
                                  xmlPrettyPrinted[xmlPrettyPrintedIndex-7] == ' ' &&
                                  xmlPrettyPrinted[xmlPrettyPrintedIndex-8] == ' ' &&
                                  xmlPrettyPrinted[xmlPrettyPrintedIndex-9] == ' ' &&
                                  xmlPrettyPrinted[xmlPrettyPrintedIndex-10] == ' ' &&
                                  xmlPrettyPrinted[xmlPrettyPrintedIndex-11] == ' ';
                
                /* if all the preceding chars are white, then go for replacement */
                if (onlySpaces)
                {
                    xmlPrettyPrintedIndex -= 11; /* remove indentation spaces */
                    putCharsInBuffer("]]"); /* reset the first chars of '-->' */
                }
            }
        }
        else if (!options->oneLineCdata && !inlineAllowed) /* line break */
        {
            /* if the cdata need to be aligned, just add 9 spaces */
            if (options->alignCdata) 
            {
                int read = readWhites(FALSE); /* strip the whites and new line */
                if (nextChar == '\r' && read == 0 && getNextChar() == '\n') /* handles the \r\n return line */
                {
                    readNextChar(); 
                    readWhites(FALSE);
                }
              
                putNewLine(); /* put a new indentation line */
                putCharsInBuffer("         "); /* align with <![CDATA[ */
                oldChar = ' '; /* and update the last char */
            }
            else
            {
                putCharInBuffer(nextChar);
                oldChar = nextChar;
            }
        }
        else /* cdata are inlined */
        {
            readWhites(TRUE); /* strip the whites and add a space if necessary */
            if(getPreviousInsertedChar() != ' ' &&
               strncmp(xmlPrettyPrinted+xmlPrettyPrintedIndex-9, "<![CDATA[", 9) != 0) /* prevents adding a space at the beginning  */
            { 
                putCharInBuffer(' '); 
                oldChar = ' ';
            }
        }
    }
    
    /* if the cdata is inline, then all the trailing spaces are removed */
    if (options->oneLineCdata)
    {
        xmlPrettyPrintedIndex -= 2; /* because of the last ']]' inserted */
        while(isWhite(xmlPrettyPrinted[xmlPrettyPrintedIndex-1]))
        {
            --xmlPrettyPrintedIndex;
        }
        putCharsInBuffer("]]");
    }
    
    /* finalize the cdata */
    lastChar = readNextChar(); /* should be '>' */
    if (lastChar != '>') 
    { 
        printError("processCDATA : last char must be '>' (not '%c')", lastChar); 
        result = PRETTY_PRINTING_INVALID_CHAR_ERROR; 
        return; 
    }
    
    putCharInBuffer(lastChar);
    
    if (inlineAllowed) { appendIndentation = FALSE; }
    
    /* there was no node open */
    lastNodeOpen = FALSE;
}

void processDoctype(void)
{
    bool loop = TRUE;
    
    putNextCharsInBuffer(9); /* put the '<!DOCTYPE' into the buffer */
    
    while(loop)
    {
        int nextChar;
        
        readWhites(TRUE);
        putCharInBuffer(' '); /* only one space for the attributes */
        
        nextChar = readNextChar();
        while(!isWhite(nextChar) && 
              !isQuote(nextChar) &&  /* begins a quoted text */
              nextChar != '=' && /* begins an attribute */
              nextChar != '>' &&  /* end of doctype */
              nextChar != '[') /* inner <!ELEMENT> types */
        {
            putCharInBuffer(nextChar);
            nextChar = readNextChar();
        }
        
        if (isWhite(nextChar)) {} /* do nothing, just let the next loop do the job */
        else if (isQuote(nextChar) || nextChar == '=')
        {
            char quote;
            
            if (nextChar == '=')
            {
                putCharInBuffer(nextChar);
                nextChar = readNextChar(); /* now we should have a quote */
                
                if (!isQuote(nextChar)) 
                { 
                    printError("processDoctype : the next char should be a quote (not '%c')", nextChar); 
                    result = PRETTY_PRINTING_INVALID_CHAR_ERROR; 
                    return; 
                }
            }
            
            /* simply process the content */
            quote = nextChar;
            do
            {
                putCharInBuffer(nextChar);
                nextChar = readNextChar();
            }
            while (nextChar != quote);
            putCharInBuffer(nextChar); /* now the last char is the last quote */
        }
        else if (nextChar == '>') /* end of doctype */
        {
            putCharInBuffer(nextChar);
            loop = FALSE;
        }
        else /* the char is a '[' => not supported yet */
        {
            printError("DOCTYPE inner ELEMENT is currently not supported by PrettyPrinter\n");
            result = PRETTY_PRINTING_NOT_SUPPORTED_YET;
            loop = FALSE;
        }
    }
}

void processDoctypeElement(void)
{
    printError("ELEMENT is currently not supported by PrettyPrinter\n");
    result = PRETTY_PRINTING_NOT_SUPPORTED_YET;
}

void printError(const char *msg, ...)
{
    va_list va;
    va_start(va, msg);
    #ifdef HAVE_GLIB
    g_logv(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, msg, va);
    #else
    vfprintf(stderr, msg, va);
    putc('\n', stderr);
    #endif
    va_end(va);

    printDebugStatus();
}

void printDebugStatus(void)
{
    #ifdef HAVE_GLIB
    g_debug("\n===== INPUT =====\n%s\n=================\ninputLength = %d\ninputIndex = %d\noutputLength = %d\noutputIndex = %d\n", 
            inputBuffer, 
            inputBufferLength, 
            inputBufferIndex,
            xmlPrettyPrintedLength,
            xmlPrettyPrintedIndex);
    #else
    PP_ERROR("\n===== INPUT =====\n%s\n=================\ninputLength = %d\ninputIndex = %d\noutputLength = %d\noutputIndex = %d\n", 
            inputBuffer, 
            inputBufferLength, 
            inputBufferIndex,
            xmlPrettyPrintedLength,
            xmlPrettyPrintedIndex);
    #endif
}
