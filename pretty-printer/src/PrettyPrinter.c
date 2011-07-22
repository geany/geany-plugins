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

//======================= FUNCTIONS ====================================================================

//xml pretty printing functions
static void putCharInBuffer(char charToAdd);                     //put a char into the new char buffer
static void putCharsInBuffer(char* charsToAdd);                  //put the chars into the new char buffer
static void putNextCharsInBuffer(int nbChars);                   //put the next nbChars of the input buffer into the new buffer
static int readWhites(gboolean considerLineBreakAsWhite);        //read the next whites into the input buffer
static char readNextChar();                                      //read the next char into the input buffer;
static char getNextChar();                                       //returns the next char but do not increase the input buffer index (use readNextChar for that)
static char getPreviousInsertedChar();                           //returns the last inserted char into the new buffer
static gboolean isWhite(char c);                                 //check if the specified char is a white
static gboolean isLineBreak(char c);                             //check if the specified char is a new line
static gboolean isQuote(char c);                                 //check if the specified char is a quote (simple or double)
static int putNewLine();                                         //put a new line into the new char buffer with the correct number of whites (indentation)
static gboolean isInlineNodeAllowed();                           //check if it is possible to have an inline node
static gboolean isOnSingleLine(int skip, char stop1, char stop2);//check if the current node data is on one line (for inlining)
static void resetBackwardIndentation(gboolean resetLineBreak);   //reset the indentation for the current depth (just reset the index in fact)
                                                             
//specific parsing functions                                 
static int processElements();                                    //returns the number of elements processed
static void processElementAttribute();                           //process on attribute of a node
static void processElementAttributes();                          //process all the attributes of a node
static void processHeader();                                     //process the header <?xml version="..." ?>
static void processNode();                                       //process an XML node
static void processTextNode();                                   //process a text node
static void processComment();                                    //process a comment
static void processCDATA();                                      //process a CDATA node
static void processDoctype();                                    //process a DOCTYPE node
static void processDoctypeElement();                             //process a DOCTYPE ELEMENT node
                                                             
//debug function                                             
static void printError(char *msg, ...);                          //just print a message like the printf method
static void printDebugStatus();                                  //just print some variables into the console for debugging

//============================================ PRIVATE PROPERTIES ======================================

//those are variables that are shared by the functions and
//shouldn't be altered.

static int result;                                                //result of the pretty printing
static char* xmlPrettyPrinted;                                    //new buffer for the formatted XML
static int xmlPrettyPrintedLength;                                //buffer size
static int xmlPrettyPrintedIndex;                                 //buffer index (position of the next char to insert)
static char* inputBuffer;                                         //input buffer
static int inputBufferLength;                                     //input buffer size
static int inputBufferIndex;                                      //input buffer index (position of the next char to read into the input string)
static int currentDepth;                                          //current depth (for indentation)
static char* currentNodeName;                                     //current node name
static gboolean appendIndentation;                                //if the indentation must be added (with a line break before)
static gboolean lastNodeOpen;                                     //defines if the last action was a not opening or not
static PrettyPrintingOptions* options;                            //options of PrettyPrinting

//============================================ GENERAL FUNCTIONS =======================================

int processXMLPrettyPrinting(char** buffer, int* length, PrettyPrintingOptions* ppOptions)
{
    //empty buffer, nothing to process
    if (*length == 0) { return PRETTY_PRINTING_EMPTY_XML; }
    if (buffer == NULL || *buffer == NULL) { return PRETTY_PRINTING_EMPTY_XML; }
    
    //initialize the variables
    result = PRETTY_PRINTING_SUCCESS;
    gboolean freeOptions = FALSE;
    if (ppOptions == NULL) { ppOptions = createDefaultPrettyPrintingOptions(); freeOptions = TRUE; }
    options = ppOptions;
    
    currentNodeName = NULL;
    appendIndentation = FALSE;
    lastNodeOpen = FALSE;
    xmlPrettyPrintedIndex = 0;
    inputBufferIndex = 0;
    currentDepth = -1;
    
    inputBuffer = *buffer;
    inputBufferLength = *length;
    
    xmlPrettyPrintedLength = *length;
    xmlPrettyPrinted = (char*)malloc(sizeof(char)*(*length));
    if (xmlPrettyPrinted == NULL) { g_error("Allocation error"); }
    
    //go to the first char
    readWhites(TRUE);

    //process the pretty-printing
    processElements();
    
    //close the buffer
    putCharInBuffer('\0');
    
    //adjust the final size
    xmlPrettyPrinted = realloc(xmlPrettyPrinted, xmlPrettyPrintedIndex);
    if (xmlPrettyPrinted == NULL) { g_error("Allocation error"); }
    
    //freeing the unused values
    if (freeOptions) { free(options); }
    
    //if success, then update the values
    if (result == PRETTY_PRINTING_SUCCESS)
    {
        free(*buffer);
        *buffer = xmlPrettyPrinted;
        *length = xmlPrettyPrintedIndex-2; //the '\0' is not in the length
    }
    //else clean the other values
    else
    {
        free(xmlPrettyPrinted);
    }
    
    //updating the pointers for the using into the caller function
    xmlPrettyPrinted = NULL; //avoid reference
    inputBuffer = NULL; //avoid reference
    currentNodeName = NULL; //avoid reference
    options = NULL; //avoid reference
    
    //and finally the result
    return result;
}

PrettyPrintingOptions* createDefaultPrettyPrintingOptions()
{
    PrettyPrintingOptions* options = (PrettyPrintingOptions*)malloc(sizeof(PrettyPrintingOptions));
    if (options == NULL) 
    { 
        g_error("Unable to allocate memory for PrettyPrintingOptions");
        return NULL; 
    }
    
    options->newLineChars = "\r\n";
    options->indentChar = ' ';
    options->indentLength = 2;
    options->oneLineText = FALSE;
    options->inlineText = TRUE;
    options->oneLineComment = FALSE;
    options->inlineComment = TRUE;
    options->oneLineCdata = FALSE;
    options->inlineCdata = TRUE;
    options->emptyNodeStripping = TRUE;
    options->emptyNodeStrippingSpace = TRUE;
    options->forceEmptyNodeSplit = FALSE;
    options->trimLeadingWhites = TRUE;
    options->trimTrailingWhites = TRUE;
    options->commentAlign = TRUE;
    
    return options;
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
    //check if the buffer is full and reallocation if needed
    if (xmlPrettyPrintedIndex >= xmlPrettyPrintedLength)
    {
        if (charToAdd == '\0') { ++xmlPrettyPrintedLength; }
        else { xmlPrettyPrintedLength += inputBufferLength; }
        xmlPrettyPrinted = (char*)realloc(xmlPrettyPrinted, xmlPrettyPrintedLength);
        if (xmlPrettyPrinted == NULL) { g_error("Allocation error"); }
    }
    
    //putting the char and increase the index for the next one
    xmlPrettyPrinted[xmlPrettyPrintedIndex] = charToAdd;
    ++xmlPrettyPrintedIndex;
}

void putCharsInBuffer(char* charsToAdd)
{
    int index = 0;
    while (charsToAdd[index] != '\0')
    {
        putCharInBuffer(charsToAdd[index]);
        ++index;
    }
}

char getPreviousInsertedChar()
{
    return xmlPrettyPrinted[xmlPrettyPrintedIndex-1];
}

int putNewLine()
{
    putCharsInBuffer(options->newLineChars);
    int spaces = currentDepth*options->indentLength;
    int i;
    for(i=0 ; i<spaces ; ++i)
    {
        putCharInBuffer(options->indentChar);
    }
    
    return spaces;
}

char getNextChar()
{
    return inputBuffer[inputBufferIndex];
}

char readNextChar()
{   
    return inputBuffer[inputBufferIndex++];
}

int readWhites(gboolean considerLineBreakAsWhite)
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

gboolean isQuote(char c)
{
    if (c == '\'') return TRUE;
    if (c == '"') return TRUE;
    
    return FALSE;
}

gboolean isWhite(char c)
{
    if (c == ' ') return TRUE;
    if (c == '\t') return TRUE;
    if (c == '\r') return TRUE;
    if (c == '\n') return TRUE;

    return FALSE;
}

gboolean isLineBreak(char c)
{
    if (c == '\n') return TRUE;
    if (c == '\r') return TRUE;
    
    return FALSE;
}

gboolean isInlineNodeAllowed()
{
    //the last action was not an opening => inline not allowed
    if (!lastNodeOpen) { return FALSE; }
    
    int firstChar = getNextChar(); //should be '<' or we are in a text node
    int secondChar = inputBuffer[inputBufferIndex+1]; //should be '!'
    int thirdChar = inputBuffer[inputBufferIndex+2]; //should be '-' or '['
    
    int index = inputBufferIndex+1;
    if (firstChar == '<')
    {
        //another node is being open ==> no inline !
        if (secondChar != '!') { return FALSE; }
        
        //okay we are in a comment node, so read until it is closed
        
        //select the closing char
        char closingComment = '-';
        if (thirdChar == '[') { closingComment = ']'; }
        
        //read until closing
        char oldChar = ' ';
        index += 3; //that by pass meanless chars
        gboolean loop = TRUE;
        while (loop)
        {
            char current = inputBuffer[index];
            if (current == closingComment && oldChar == closingComment) { loop = FALSE; } //end of comment
            oldChar = current;
            ++index;
        }
        
        //okay now avoid blanks
        // inputBuffer[index] is now '>'
        ++index;
        while (isWhite(inputBuffer[index])) { ++index; }
    }
    else
    {
        //this is a text node. Simply loop to the next '<'
        while (inputBuffer[index] != '<') { ++index; }
    }
    
    //check what do we have now
    char currentChar = inputBuffer[index];
    if (currentChar == '<')
    {
        //check if that is a closing node
        currentChar = inputBuffer[index+1];
        if (currentChar == '/')
        {
            //as we are in a correct XML (so far...), if the node is 
            //being directly close, the inline is allowed !!!
            return TRUE;
        }
    }
    
    //inline not allowed...
    return FALSE;
}

gboolean isOnSingleLine(int skip, char stop1, char stop2)
{
    int currentIndex = inputBufferIndex+skip; //skip the n first chars (in comment <!--)
    gboolean isOnSingleLine = TRUE;
    
    char oldChar = inputBuffer[currentIndex];
    char currentChar = inputBuffer[currentIndex+1];
    while(isOnSingleLine && oldChar != stop1 && currentChar != stop2)
    {
        isOnSingleLine = !isLineBreak(oldChar);
        
        ++currentIndex;
        oldChar = currentChar;
        currentChar = inputBuffer[currentIndex+1];
        
        /**
         * A line break inside the node has been reached. But we should check
         * if there is something before the end of the node (otherwise, there
         * are only spaces and it may be wanted to be considered as a single
         * line). //TODO externalize an option for that ?
         */
        if (!isOnSingleLine)
        {
            while(oldChar != stop1 && currentChar != stop2)
            {
                //okay there is something else => this is not on one line
                if (!isWhite(oldChar)) return FALSE;
              
                ++currentIndex;
                oldChar = currentChar;
                currentChar = inputBuffer[currentIndex+1];
            }
            
            //the end of the node has been reached with only whites. Then
            //the node can be considered being one single line
            return TRUE;
        }
    }
    
    return isOnSingleLine;
}

void resetBackwardIndentation(gboolean resetLineBreak)
{
    xmlPrettyPrintedIndex -= (currentDepth*options->indentLength);
    if (resetLineBreak) 
    { 
        int len = strlen(options->newLineChars);
        xmlPrettyPrintedIndex -= len; 
    }
}

//#########################################################################################################################################
//-----------------------------------------------------------------------------------------------------------------------------------------
                                                                                                
//-----------------------------------------------------------------------------------------------------------------------------------------
//=============================================================== NODE FUNCTIONS ==========================================================
//-----------------------------------------------------------------------------------------------------------------------------------------
                                                                                                
//-----------------------------------------------------------------------------------------------------------------------------------------
//#########################################################################################################################################

int processElements()
{
    int counter = 0;
    ++currentDepth;
    gboolean loop = TRUE;
    while (loop && result == PRETTY_PRINTING_SUCCESS)
    {
        //strip unused whites
        readWhites(TRUE);
        
        char nextChar = getNextChar();
        if (nextChar == '\0') { return 0; } //no more data to read
        
        //put a new line with indentation
        if (appendIndentation) { putNewLine(); }
        
        //always append indentation (but need to store the state)
        gboolean indentBackward = appendIndentation;
        appendIndentation = TRUE; 
        
        //okay what do we have now ?
        if (nextChar != '<')
        { 
            //a simple text node
            processTextNode(); 
            ++counter; 
        } 
        else //some more check are needed
        {
            nextChar = inputBuffer[inputBufferIndex+1];
            if (nextChar == '!') 
            {
                char oneMore = inputBuffer[inputBufferIndex+2];
                if (oneMore == '-') { processComment(); ++counter; } //a comment
                else if (oneMore == '[') { processCDATA(); ++counter; } //cdata
                else if (oneMore == 'D') { processDoctype(); ++counter; } //doctype <!DOCTYPE ... >
                else if (oneMore == 'E') { processDoctypeElement(); ++counter; } //doctype element <!ELEMENT ... >
                else 
                { 
                    printError("processElements : Invalid char '%c' afer '<!'", oneMore); 
                    result = PRETTY_PRINTING_INVALID_CHAR_ERROR; 
                }
            } 
            else if (nextChar == '/')
            { 
                //close a node => stop the loop !!
                loop = FALSE; 
                if (indentBackward) 
                { 
                    //INDEX HACKING
                    xmlPrettyPrintedIndex -= options->indentLength; 
                } 
            }
            else if (nextChar == '?')
            {
                //this is a header
                processHeader();
            }
            else 
            {
                //a new node is open
                processNode();
                ++counter;
            } 
        }
    }
    
    --currentDepth;
    return counter;
}

void processElementAttribute()
{
    //process the attribute name
    char nextChar = readNextChar();
    while (nextChar != '=')
    {
        putCharInBuffer(nextChar);
        nextChar = readNextChar();
    }
    
    putCharInBuffer(nextChar); //that's the '='
    
    //read the simple quote or double quote and put it into the buffer
    char quote = readNextChar();
    putCharInBuffer(quote); 
    
    //process until the last quote
    char value = readNextChar();
    while(value != quote)
    {
        putCharInBuffer(value);
        value = readNextChar();
    }
    
    //simply add the last quote
    putCharInBuffer(quote);
}

void processElementAttributes()
{
    char current = getNextChar(); //should not be a white
    if (isWhite(current)) 
    { 
        printError("processElementAttributes : first char shouldn't be a white"); 
        result = PRETTY_PRINTING_INVALID_CHAR_ERROR; 
        return; 
    }
    
    gboolean loop = TRUE;
    while (loop)
    {
        readWhites(TRUE); //strip the whites
        
        char next = getNextChar(); //don't read the last char (processed afterwards)
        if (next == '/') { loop = FALSE; } /* end of node */
        else if (next == '>') { loop = FALSE; } /* end of tag */
        else if (next == '?') { loop = FALSE; } /* end of header */
        else 
        { 
            putCharInBuffer(' '); //put only one space to separate attributes
            processElementAttribute(); 
        }
    }
}

void processHeader()
{
    int firstChar = inputBuffer[inputBufferIndex]; //should be '<'
    int secondChar = inputBuffer[inputBufferIndex+1]; //must be '?'
    
    if (firstChar != '<') 
    { 
        /* what ?????? invalid xml !!! */ 
        printError("processHeader : first char should be '<' (not '%c')", firstChar); 
        result = PRETTY_PRINTING_INVALID_CHAR_ERROR; return; 
    }
    
    if (secondChar == '?')
    { 
        //puts the '<' and '?' chars into the new buffer
        putNextCharsInBuffer(2); 
        
        while(!isWhite(getNextChar())) { putNextCharsInBuffer(1); }
        
        readWhites(TRUE);
        processElementAttributes(); 
        
        //puts the '?' and '>' chars into the new buffer
        putNextCharsInBuffer(2); 
    }
}

void processNode()
{
    int opening = readNextChar();
    if (opening != '<') 
    { 
        printError("processNode : The first char should be '<' (not '%c')", opening); 
        result = PRETTY_PRINTING_INVALID_CHAR_ERROR; 
        return; 
    }
    
    putCharInBuffer(opening);
    
    //read the node name
    int nodeNameLength = 0;
    while (!isWhite(getNextChar()) && 
           getNextChar() != '>' &&  //end of the tag
           getNextChar() != '/') //tag is being closed
    {
        putNextCharsInBuffer(1);
        ++nodeNameLength;
    }

    //store the name
    char* nodeName = (char*)malloc(sizeof(char)*nodeNameLength+1);
    if (nodeName == NULL) { g_error("Allocation error"); }
    nodeName[nodeNameLength] = '\0';
    int i;
    for (i=0 ; i<nodeNameLength ; ++i)
    {
        int index = xmlPrettyPrintedIndex-nodeNameLength+i;
        nodeName[i] = xmlPrettyPrinted[index];
    }
    
    currentNodeName = nodeName; //set the name for using in other methods
    lastNodeOpen = TRUE;

    //process the attributes    
    readWhites(TRUE);
    processElementAttributes();
    
    //process the end of the tag
    int subElementsProcessed = 0;
    char nextChar = getNextChar(); //should be either '/' or '>'
    if (nextChar == '/') //the node is being closed immediatly
    { 
        //closing node directly
        if (options->emptyNodeStripping || !options->forceEmptyNodeSplit)
        {
            if (options->emptyNodeStrippingSpace) { putCharInBuffer(' '); }
            putNextCharsInBuffer(2); 
        }
        //split the closing nodes
        else
        {
            readNextChar(); //removing '/'
            readNextChar(); //removing '>'
            
            putCharInBuffer('>');
            if (!options->inlineText) 
            {
                //no inline text => new line !
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
        //the tag is just closed (maybe some content)
        putNextCharsInBuffer(1); 
        subElementsProcessed = processElements(TRUE); 
    } 
    else 
    { 
        printError("processNode : Invalid character '%c'", nextChar);
        result = PRETTY_PRINTING_INVALID_CHAR_ERROR; 
        return; 
    }
    
    //if the code reaches this area, then the processElements has been called and we must
    //close the opening tag
    char closeChar = getNextChar();
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
    
    //there is no elements
    if (subElementsProcessed == 0)
    {
        //the node will be stripped
        if (options->emptyNodeStripping)
        {
            //because we have '<nodeName ...></nodeName>'
            xmlPrettyPrintedIndex -= nodeNameLength+4; 
            resetBackwardIndentation(TRUE);
            
            if (options->emptyNodeStrippingSpace) { putCharInBuffer(' '); }
            putCharsInBuffer("/>");
        }
        //the closing tag will be put on the same line
        else if (options->inlineText)
        {
            //correct the index because we have '</nodeName>'
            xmlPrettyPrintedIndex -= nodeNameLength+3; 
            resetBackwardIndentation(TRUE);
            
            //rewrite the node name
            putCharsInBuffer("</");
            putCharsInBuffer(currentNodeName);
            putCharInBuffer('>');
        }
    }
    
    //the node is closed
    lastNodeOpen = FALSE;
    
    //freeeeeeee !!!
    free(nodeName);
    nodeName = NULL;
    currentNodeName = NULL;
}

void processComment()
{
    gboolean inlineAllowed = FALSE;
    if (options->inlineComment) { inlineAllowed = isInlineNodeAllowed(); }
    if (inlineAllowed && !options->oneLineComment) { inlineAllowed = isOnSingleLine(4, '-', '-'); }
    if (inlineAllowed) { resetBackwardIndentation(TRUE); }
    
    putNextCharsInBuffer(4); //add the chars '<!--'
    
    char oldChar = '-';
    gboolean loop = TRUE;
    while (loop)
    {
        char nextChar = readNextChar();
        if (oldChar == '-' && nextChar == '-') //comment is being closed
        {
            loop = FALSE;
        }
        
        if (!isLineBreak(nextChar)) //the comment simply continues
        {
            putCharInBuffer(nextChar);
            oldChar = nextChar;
            
            if (!loop && options->commentAlign) //end of comment
            {
                //ensures the chars preceding the first '-' are all spaces
                gboolean rewind = xmlPrettyPrinted[xmlPrettyPrintedIndex-3] == ' ' &&
                                  xmlPrettyPrinted[xmlPrettyPrintedIndex-4] == ' ' &&
                                  xmlPrettyPrinted[xmlPrettyPrintedIndex-5] == ' ' &&
                                  xmlPrettyPrinted[xmlPrettyPrintedIndex-6] == ' ' &&
                                  xmlPrettyPrinted[xmlPrettyPrintedIndex-7] == ' ';
                
                //if all the preceding chars are white, then go for replacement
                if (rewind)
                {
                    xmlPrettyPrintedIndex -= 7; //remove indentation spaces
                    putCharsInBuffer("--"); //reset the first chars of '-->'
                }
            }
        }
        else if (!options->oneLineComment && !inlineAllowed) //oh ! there is a line break
        {
            int read = readWhites(FALSE); //strip the whites and new line
            if (nextChar == '\r' && read == 0 && getNextChar() == '\n') //handles the \r\n return line
            {
                readNextChar(); 
                readWhites(FALSE);
            }
            
            //if the comments need to be aligned, just add 5 spaces
            if (options->commentAlign) 
            {
                putNewLine(); //put a new indentation line
                putCharsInBuffer("     ");
            }
            
            oldChar = ' '; //and update the last char
        }
        else //the comments must be inlined
        {
            readWhites(TRUE); //strip the whites and add a space if needed
            if (getPreviousInsertedChar() != ' ') 
            { 
                putCharInBuffer(' '); 
            }
        }
    }
    
    char lastChar = readNextChar(); //should be '>'
    if (lastChar != '>') 
    { 
        printError("processComment : last char must be '>' (not '%c')", lastChar); 
        result = PRETTY_PRINTING_INVALID_CHAR_ERROR; 
        return; 
    }
    putCharInBuffer(lastChar);
    
    if (inlineAllowed) { appendIndentation = FALSE; }
    
    //there vas no node open
    lastNodeOpen = FALSE;
}

void processTextNode()
{
    //checks if inline is allowed
    gboolean inlineTextAllowed = FALSE;
    if (options->inlineText) { inlineTextAllowed = isInlineNodeAllowed(); }
    if (inlineTextAllowed && !options->oneLineText) { inlineTextAllowed = isOnSingleLine(0, '<', '/'); }
    if (inlineTextAllowed) { resetBackwardIndentation(TRUE); } //remove previous indentation
    
    //the leading whites are automatically stripped. So we re-add it
    if (!options->trimLeadingWhites)
    {
        int backwardIndex = inputBufferIndex-1;
        while (inputBuffer[backwardIndex] == ' ' || 
               inputBuffer[backwardIndex] == '\t') 
        { 
            --backwardIndex; //backward rolling
        } 
        
        //now the input[backwardIndex] IS NOT a white. So we go to 
        //the next char...
        ++backwardIndex;
        
        //and then re-add the whites
        while (inputBuffer[backwardIndex] == ' ' || 
               inputBuffer[backwardIndex] == '\t') 
        {
            putCharInBuffer(inputBuffer[backwardIndex]);
            ++backwardIndex;
        }
    }
    
    //process the text into the node
    while(getNextChar() != '<')
    {
        char nextChar = readNextChar();
        if (isLineBreak(nextChar))
        {
            if (options->oneLineText)
            { 
                readWhites(TRUE);
              
                //as we can put text on one line, remove the line break 
                //and replace it by a space but only if the previous 
                //char wasn't a space
                if (getPreviousInsertedChar() != ' ') 
                { 
                    putCharInBuffer(' '); 
                }
            }
            else 
            {
                int read = readWhites(FALSE);
                if (nextChar == '\r' && read == 0 && getNextChar() == '\n') //handles the '\r\n'
                {
                   nextChar = readNextChar();
                   readWhites(FALSE);
                }
              
                //put a new line only if the closing tag is not reached
                if (getNextChar() != '<') 
                {   
                    putNewLine(); 
                } 
            }
        }
        else
        {
            putCharInBuffer(nextChar);
        }
    }
    
    //strip the trailing whites
    if (options->trimTrailingWhites)
    {
        while(getPreviousInsertedChar() == ' ' || 
              getPreviousInsertedChar() == '\t')
        {
            --xmlPrettyPrintedIndex;
        }
    }
    
    //remove the indentation for the closing tag
    if (inlineTextAllowed) { appendIndentation = FALSE; }
    
    //there vas no node open
    lastNodeOpen = FALSE;
}

void processCDATA()
{
    gboolean inlineAllowed = FALSE;
    if (options->inlineCdata) { inlineAllowed = isInlineNodeAllowed(); }
    if (inlineAllowed) { resetBackwardIndentation(TRUE); }
    
    putNextCharsInBuffer(9); //putting the '<![CDATA[' into the buffer
    
    gboolean loop = TRUE;
    char oldChar = '[';
    while(loop)
    {
        char nextChar = readNextChar();
        char nextChar2 = getNextChar();
        if (oldChar == ']' && nextChar == ']' && nextChar2 == '>') { loop = FALSE; } //end of cdata
        
        if (!isLineBreak(nextChar)) //the cdata simply continues
        {
            putCharInBuffer(nextChar);
            oldChar = nextChar;
        }
        else if (!options->oneLineCdata)
        {
            readWhites(TRUE); //strip the whites and new line
            putNewLine(); //put a new indentation line
            oldChar = ' '; //and update the last char
            
            //TODO manage relative spacing
        }
        else //cdata are inlined
        {
            readWhites(TRUE); //strip the whites and add a space if necessary
            if(getPreviousInsertedChar() != ' ') { putCharInBuffer(' '); }
        }
    }
    
    char lastChar = readNextChar(); //should be '>'
    if (lastChar != '>') 
    { 
        printError("processCDATA : last char must be '>' (not '%c')", lastChar); 
        result = PRETTY_PRINTING_INVALID_CHAR_ERROR; 
        return; 
    }
    
    putCharInBuffer(lastChar);
    
    if (inlineAllowed) { appendIndentation = FALSE; }
    
    //there vas no node open
    lastNodeOpen = FALSE;
}

void processDoctype()   
{
    putNextCharsInBuffer(9); //put the '<!DOCTYPE' into the buffer
    
    gboolean loop = TRUE;
    while(loop)
    {
        readWhites(TRUE);
        putCharInBuffer(' '); //only one space for the attributes
        
        int nextChar = readNextChar();
        while(!isWhite(nextChar) && 
              !isQuote(nextChar) &&  //begins a quoted text
              nextChar != '=' && //begins an attribute
              nextChar != '>' &&  //end of doctype
              nextChar != '[') //inner <!ELEMENT> types
        {
            putCharInBuffer(nextChar);
            nextChar = readNextChar();
        }
        
        if (isWhite(nextChar)) {} //do nothing, just let the next loop do the job
        else if (isQuote(nextChar) || nextChar == '=')
        {
            if (nextChar == '=')
            {
                putCharInBuffer(nextChar);
                nextChar = readNextChar(); //now we should have a quote
                
                if (!isQuote(nextChar)) 
                { 
                    printError("processDoctype : the next char should be a quote (not '%c')", nextChar); 
                    result = PRETTY_PRINTING_INVALID_CHAR_ERROR; 
                    return; 
                }
            }
            
            //simply process the content
            char quote = nextChar;
            do
            {
                putCharInBuffer(nextChar);
                nextChar = readNextChar();
            }
            while (nextChar != quote);
            putCharInBuffer(nextChar); //now the last char is the last quote
        }
        else if (nextChar == '>') //end of doctype
        {
            putCharInBuffer(nextChar);
            loop = FALSE;
        }
        else //the char is a '[' => not supported yet
        {
            printError("DOCTYPE inner ELEMENT is currently not supported by PrettyPrinter\n");
            result = PRETTY_PRINTING_NOT_SUPPORTED_YET;
            loop = FALSE;
        }
    }
}

void processDoctypeElement()
{
    printError("ELEMENT is currently not supported by PrettyPrinter\n");
    result = PRETTY_PRINTING_NOT_SUPPORTED_YET;
}

void printError(char *msg, ...)
{
    va_list va;
    va_start(va, msg);
    g_warning(msg, va);
    va_end(va);
    
    //TODO also do a fprintf on stderr ?
    
    printDebugStatus();
}

void printDebugStatus()
{
    g_debug("\n===== INPUT =====\n%s\n=================\ninputLength = %d\ninputIndex = %d\noutputLength = %d\noutputIndex = %d\n", 
            inputBuffer, 
            inputBufferLength, 
            inputBufferIndex,
            xmlPrettyPrintedLength,
            xmlPrettyPrintedIndex);
}
