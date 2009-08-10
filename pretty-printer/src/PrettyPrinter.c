/**
 * Written by Cédric Tabin
 * http://www.astorm.ch/
 * Version 1.0 - 08.08.2009
 * 
 * Code under licence GPLv2
 * Geany - http://www.geany.org/
 */

#include "PrettyPrinter.h"

//======================= FUNCTIONS ====================================================================

//xml pretty printing functions
static void putCharInBuffer(char charToAdd);                                                            //put a char into the new char buffer
static void putCharsInBuffer(char* charsToAdd);                                                         //put the chars into the new char buffer
static void putNextCharsInBuffer(int nbChars);                                                          //put the next nbChars of the input buffer into the new buffer
static int readWhites();                                                                                //read the next whites into the input buffer
static char readNextChar();                                                                             //read the next char into the input buffer;
static char getNextChar();                                                                              //returns the next char but do not increase the input buffer index (use readNextChar for that)
static char getPreviousInsertedChar();                                                                  //returns the last inserted char into the new buffer
static boolean isWhite(char c);                                                                         //check if the specified char is a white
static boolean isLineBreak(char c);                                                                     //check if the specified char is a new line
static int putNewLine();                                                                                //put a new line into the new char buffer with the correct number of whites (indentation)
static boolean isInlineNodeAllowed();                                                                   //check if it is possible to have an inline node
static void resetBackwardIndentation(boolean resetLineBreak);                                           //reset the indentation for the current depth (just reset the index in fact)
                                                             
//specific parsing functions                                 
static int processElements();                                                                           //returns the number of elements processed
static void processElementAttribute();                                                                  //process on attribute of a node
static void processElementAttributes();                                                                 //process all the attributes of a node
static void processHeader();                                                                            //process the header <?xml version="..." ?>
static void processNode();                                                                              //process an XML node
static void processTextNode();                                                                          //process a text node
static void processComment();                                                                           //process a comment
static void processCDATA();                                                                             //process a CDATA node
static void processDoctype();                                                                           //process a DOCTYPE node
static void processDoctypeElement();                                                                    //process a DOCTYPE ELEMENT node
                                                             
//debug function                                             
static void printDebugStatus();                                                                         //just print some variables into the console for debugging

//============================================ PRIVATE PROPERTIES ======================================

//those are variables that are shared by the functions and
//shouldn't be altered.

static int result;                                                                                      //result of the pretty printing
static char* xmlPrettyPrinted;                                                                          //new buffer for the formatted XML
static int xmlPrettyPrintedLength;                                                                      //buffer size
static int xmlPrettyPrintedIndex;                                                                       //buffer index (position of the next char to insert)
static char* inputBuffer;                                                                               //input buffer
static int inputBufferLength;                                                                           //input buffer size
static int inputBufferIndex;                                                                            //input buffer index (position of the next char to read into the input string)
static int currentDepth;                                                                                //current depth (for indentation)
static char* currentNodeName;                                                                           //current node name
static boolean appendIndentation;                                                                       //if the indentation must be added (with a line break before)
static boolean lastNodeOpen;                                                                            //defines if the last action was a not opening or not
static PrettyPrintingOptions* options;                                                                  //options of PrettyPrinting

//============================================ GENERAL FUNCTIONS =======================================

int processXMLPrettyPrinting(char** buffer, int* length, PrettyPrintingOptions* ppOptions)
{
	//empty buffer, nothing to process
	if (*length == 0) { return PRETTY_PRINTING_EMPTY_XML; }
	if (buffer == NULL || *buffer == NULL) { return PRETTY_PRINTING_EMPTY_XML; }
	
	//initialize the variables
	result = PRETTY_PRINTING_SUCCESS;
	boolean freeOptions = FALSE;
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
	if (xmlPrettyPrinted == NULL) { exit(PRETTY_PRINTING_MALLOC_ERROR); }
	
	//go to the first char
	readWhites();

	//process the pretty-printing
	processElements();
	
	//close the buffer
	putCharInBuffer('\0');
	
	//adjust the final size
	xmlPrettyPrinted = realloc(xmlPrettyPrinted, xmlPrettyPrintedIndex);
	if (xmlPrettyPrinted == NULL) { exit(PRETTY_PRINTING_MALLOC_ERROR); }
	
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
	if (options == NULL) { fprintf(stderr, "createDefaultPrettyPrintingOptions : Unable to allocate memory"); return NULL; }
	
	options->indentChar = ' ';
	options->indentLength = 2;
	options->oneLineText = TRUE;
	options->inlineText = TRUE;
	options->oneLineComment = TRUE;
	options->inlineComment = TRUE;
	options->oneLineCdata = TRUE;
	options->inlineCdata = TRUE;
	options->emptyNodeStripping = TRUE;
	options->emptyNodeStrippingSpace = TRUE;
	options->forceEmptyNodeSplit = FALSE;
	options->trimLeadingWhites = TRUE;
	options->trimTrailingWhites = TRUE;
	
	return options;
}

void putNextCharsInBuffer(int nbChars)
{
	for (int i=0 ; i<nbChars ; ++i)
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
		if (xmlPrettyPrinted == NULL) { exit(PRETTY_PRINTING_MALLOC_ERROR); }
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
	putCharInBuffer('\n');
	int spaces = currentDepth*options->indentLength;
	for(int i=0 ; i<spaces ; ++i)
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

int readWhites()
{
	int counter = 0;
	while(isWhite(inputBuffer[inputBufferIndex]))
	{
		++counter;
		++inputBufferIndex;
	}
	
	return counter;
}

boolean isWhite(char c)
{
	if (c == ' ') return TRUE;
	if (c == '\t') return TRUE;
	if (c == '\r') return TRUE;
	if (c == '\n') return TRUE;

	return FALSE;
}

boolean isLineBreak(char c)
{
	if (c == '\n') return TRUE;
	if (c == '\r') return TRUE;
	
	return FALSE;
}

boolean isInlineNodeAllowed()
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
		boolean loop = TRUE;
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
			//as we are in a correct XML (so far...), if the node is being
			//directly close, the inline is allowed !!!
			return TRUE;
		}
	}
	
	//inline not allowed...
	return FALSE;
}

void resetBackwardIndentation(boolean resetLineBreak)
{
	xmlPrettyPrintedIndex -= (currentDepth*options->indentLength);
	if (resetLineBreak) { --xmlPrettyPrintedIndex; }
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
	boolean loop = TRUE;
	while (loop && result == PRETTY_PRINTING_SUCCESS)
	{
		//strip unused whites
		readWhites();
		
		char nextChar = getNextChar();
		if (nextChar == '\0') { return 0; } //no more data to read
		
		//put a new line with indentation
		if (appendIndentation) { putNewLine(); }
		
		//always append indentation (but need to store the state)
		boolean indentBackward = appendIndentation;
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
				else { fprintf(stderr, "processElements : Invalid char '%c' afer '<!'", oneMore); printDebugStatus(); result = PRETTY_PRINTING_INVALID_CHAR_ERROR; }
			} 
			else if (nextChar == '/')
			{ 
				//close a node => stop the loop !!
				loop = FALSE; 
				if (indentBackward) { xmlPrettyPrintedIndex -= options->indentLength; } //INDEX HACKING
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
	if (isWhite(current)) { fprintf(stderr, "processElementAttributes : first char shouldn't be a white"); printDebugStatus(); result = PRETTY_PRINTING_INVALID_CHAR_ERROR; return; }
	
	boolean loop = TRUE;
	while (loop)
	{
		readWhites(); //strip the whites
		
		char next = getNextChar(); //don't read the last char => will be processed afterwards
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
	
	if (firstChar != '<') { fprintf(stderr, "processHeader : first char should be '<' (not '%c')", firstChar); printDebugStatus(); result = PRETTY_PRINTING_INVALID_CHAR_ERROR; return; } /* what ?????? invalid xml !!! */ 
	if (secondChar == '?')
	{ 
		putNextCharsInBuffer(2); //puts the '<' and '?' chars into the new buffer
		
		while(!isWhite(getNextChar())) { putNextCharsInBuffer(1); }
		
		readWhites();
		processElementAttributes(); 
		putNextCharsInBuffer(2); //puts the '?' and '>' chars into the new buffer
	}
}

void processNode()
{
	int opening = readNextChar();
	if (opening != '<') { fprintf(stderr, "processNode : The first char should be '<' (not '%c')", opening); printDebugStatus(); result = PRETTY_PRINTING_INVALID_CHAR_ERROR; return; }
	putCharInBuffer(opening);
	
	//read the node name
	int nodeNameLength = 0;
	while (!isWhite(getNextChar()) && getNextChar() != '>' && getNextChar() != '/')
	{
		putNextCharsInBuffer(1);
		++nodeNameLength;
	}

	//store the name
	char* nodeName = (char*)malloc(sizeof(char)*nodeNameLength+1);
	if (nodeName == NULL) { exit(PRETTY_PRINTING_MALLOC_ERROR); }
	nodeName[nodeNameLength] = '\0';
	for (int i=0 ; i<nodeNameLength ; ++i)
	{
		int index = xmlPrettyPrintedIndex-nodeNameLength+i;
		nodeName[i] = xmlPrettyPrinted[index];
	}
	
	currentNodeName = nodeName; //set the name for using in other methods
	lastNodeOpen = TRUE;

	//process the attributes	
	readWhites();
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
			if (!options->inlineText) { putNewLine(); } //no inline text => new line !
			putCharsInBuffer("</");
			putCharsInBuffer(currentNodeName);
			putCharInBuffer('>');
		}
		
		lastNodeOpen=FALSE; 
		return; 
	}
	else if (nextChar == '>') { putNextCharsInBuffer(1); subElementsProcessed = processElements(TRUE); } //the tag is just closed (maybe some content)
	else { fprintf(stderr, "processNode : Invalid character '%c'", nextChar); printDebugStatus(); result = PRETTY_PRINTING_INVALID_CHAR_ERROR; return; }
	
	//if the code reaches this area, then the processElements has been called and we must
	//close the opening tag
	char closeChar = getNextChar();
	if (closeChar != '<') { fprintf(stderr, "processNode : Invalid character '%c' for closing tag (should be '<')", closeChar); printDebugStatus(); result = PRETTY_PRINTING_INVALID_CHAR_ERROR; return; }
	
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
			xmlPrettyPrintedIndex -= nodeNameLength+4; //because we have '<nodeName ...></nodeName>'
			resetBackwardIndentation(TRUE);
			
			if (options->emptyNodeStrippingSpace) { putCharInBuffer(' '); }
			putCharsInBuffer("/>");
		}
		//the closing tag will be put on the same line
		else if (options->inlineText)
		{
			xmlPrettyPrintedIndex -= nodeNameLength+3; //because we have '</nodeName>'
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
	boolean inlineAllowed = FALSE;
	if (options->inlineComment) { inlineAllowed = isInlineNodeAllowed(); }
	if (inlineAllowed) { resetBackwardIndentation(TRUE); }
	
	putNextCharsInBuffer(4); //add the chars '<!--'
	
	char oldChar = '-';
	boolean loop = TRUE;
	while (loop)
	{
		char nextChar = readNextChar();
		if (oldChar == '-' && nextChar == '-') //the comment is being closed
		{
			loop = FALSE;
		}
		
		if (!isLineBreak(nextChar)) //the comment simply continues
		{
			putCharInBuffer(nextChar);
			oldChar = nextChar;
		}
		else if (!options->oneLineComment) //oh ! there is a line break !
		{
			readWhites(); //strip the whites and new line
			putNewLine(); //put a new indentation line
			oldChar = ' '; //and update the last char
			
			//TODO manage relative spacing
		}
		else //the comments must be inlined
		{
			readWhites(); //strip the whites and add a space if necessary
			if (getPreviousInsertedChar() != ' ') { putCharInBuffer(' '); }
		}
	}
	
	char lastChar = readNextChar(); //should be '>'
	if (lastChar != '>') { fprintf(stderr, "processComment : last char must be '>' (not '%c')", lastChar); printDebugStatus(); result = PRETTY_PRINTING_INVALID_CHAR_ERROR; return; }
	putCharInBuffer(lastChar);
	
	if (inlineAllowed) { appendIndentation = FALSE; }
	
	//there vas no node open
	lastNodeOpen = FALSE;
}

void processTextNode()
{
	//checks if inline is allowed
	boolean inlineTextAllowed = FALSE;
	if (options->inlineText) { inlineTextAllowed = isInlineNodeAllowed(); }
	if (inlineTextAllowed) { resetBackwardIndentation(TRUE); } //remove previous indentation
	
	//the leading whites are automatically stripped. So we re-add it
	if (!options->trimLeadingWhites)
	{
		int backwardIndex = inputBufferIndex-1;
		while (inputBuffer[backwardIndex] == ' ' || inputBuffer[backwardIndex] == '\t') { --backwardIndex; } //backward rolling
		
		//now the input[backwardIndex] IS NOT a white. So we go to the next char...
		++backwardIndex;
		
		//and then re-add the whites
		while (inputBuffer[backwardIndex] == ' ' || inputBuffer[backwardIndex] == '\t') 
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
			readWhites();
			if (options->oneLineText)
			{ 
				//as we can put text on one line, remove the line break and replace it by a space
				//but only if the previous char wasn't a space
				if (getPreviousInsertedChar() != ' ') { putCharInBuffer(' '); }
			}
			else 
			{
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
		while(getPreviousInsertedChar() == ' ' || getPreviousInsertedChar() == '\t')
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
	boolean inlineAllowed = FALSE;
	if (options->inlineCdata) { inlineAllowed = isInlineNodeAllowed(); }
	if (inlineAllowed) { resetBackwardIndentation(TRUE); }
	
	putNextCharsInBuffer(9); //putting the '<![CDATA[' into the buffer
	
	boolean loop = TRUE;
	char oldChar = '[';
	while(loop)
	{
		char nextChar = readNextChar();
		if (oldChar == ']' && nextChar == ']') { loop = FALSE; } //end of cdata
		
		if (!isLineBreak(nextChar)) //the cdata simply continues
		{
			putCharInBuffer(nextChar);
			oldChar = nextChar;
		}
		else if (!options->oneLineCdata)
		{
			readWhites(); //strip the whites and new line
			putNewLine(); //put a new indentation line
			oldChar = ' '; //and update the last char
			
			//TODO manage relative spacing
		}
		else //cdata are inlined
		{
			readWhites(); //strip the whites and add a space if necessary
			if(getPreviousInsertedChar() != ' ') { putCharInBuffer(' '); }
		}
	}
	
	char lastChar = readNextChar(); //should be '>'
	if (lastChar != '>') { fprintf(stderr, "processCDATA : last char must be '>' (not '%c')", lastChar); printDebugStatus(); result = PRETTY_PRINTING_INVALID_CHAR_ERROR; return; }
	putCharInBuffer(lastChar);
	
	if (inlineAllowed) { appendIndentation = FALSE; }
	
	//there vas no node open
	lastNodeOpen = FALSE;
}

void processDoctype()	
{
	fprintf(stderr, "DOCTYPE is currently not supported by PrettyPrinter\n");
	fflush(stderr);
	
	result = PRETTY_PRINTING_NOT_SUPPORTED_YET;
}

void processDoctypeElement()
{
	fprintf(stderr, "ELEMENT is currently not supported by PrettyPrinter\n");
	fflush(stderr);
	
	result = PRETTY_PRINTING_NOT_SUPPORTED_YET;
}

void printDebugStatus()
{
	fprintf(stderr, "\n===== INPUT =====\n%s\n=================\ninputLength = %d\ninputIndex = %d\noutputLength = %d\noutputIndex = %d\n", 
					inputBuffer, 
					inputBufferLength, 
					inputBufferIndex,
					xmlPrettyPrintedLength,
					xmlPrettyPrintedIndex);
	fflush(stderr);
}
