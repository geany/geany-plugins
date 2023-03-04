
/*
 * glspi_sci.c - This file is part of the Lua scripting plugin for the Geany IDE
 * See the file "geanylua.c" for copyright information.
 */

#define NEED_FAIL_ARG_TYPE
#define NEED_FAIL_ELEM_TYPE

#include "glspi.h"
#include "glspi_sci.h"


/* Get or Set the entire text of the currently active Geany document */
static gint glspi_text(lua_State* L)
{

	GeanyDocument *doc = document_get_current();

	if (!doc) { return 0; }
	if (0 == lua_gettop(L)) { /* Called with no args, GET the current text */
		gchar *txt = sci_get_contents(doc->editor->sci, -1);
		lua_pushstring(L, txt ? txt : "");
		g_free(txt);
		return 1;
	} else { /* Called with one arg, SET the current text */
		const gchar*txt;
		if (!lua_isstring(L, 1)) {
			return FAIL_STRING_ARG(1);
		}
		txt = lua_tostring(L, 1);
		sci_set_text(doc->editor->sci, txt);
		return 0;
	}
}


/* Get or Set the selection text of the currently active Geany document */
static gint glspi_selection(lua_State* L)
{

	DOC_REQUIRED
	if (0 == lua_gettop(L)) { /* Called with no args, GET the selection */
		gchar *txt = sci_get_selection_contents(doc->editor->sci);
		lua_pushstring(L, txt ? txt : "");
		g_free(txt);
		return 1;
	} else { /* Called with one arg, SET the selection */
		const gchar*txt=NULL;
		if (!lua_isstring(L, 1)) { return FAIL_STRING_ARG(1); }
		txt = lua_tostring(L, 1);
		sci_replace_sel(doc->editor->sci, txt);
		return 0;
	}
}



/* Set or get the endpoints of the scintilla selection */
static gint glspi_select(lua_State* L)
{
	gint argc=lua_gettop(L);
	gint sel_start, sel_end;
	gboolean rectsel=FALSE;
	DOC_REQUIRED
	if (0==argc) {
		rectsel=scintilla_send_message(doc->editor->sci, SCI_SELECTIONISRECTANGLE, 0, 0);
	}

	lua_getglobal(L, LUA_MODULE_NAME);
	if ( lua_istable(L, -1) ) {
		lua_pushstring(L,tokenRectSel);
		lua_gettable(L, -2);
		if ( (argc>0) && lua_isboolean(L, -1) ) {
			rectsel=lua_toboolean(L, -1);
		} else {
			lua_getglobal(L, LUA_MODULE_NAME);
			lua_pushstring(L, tokenRectSel);
			lua_pushboolean(L, rectsel);
			lua_settable(L, -3);
		}
	}
	if (0==argc) {
		sel_end=sci_get_current_position(doc->editor->sci);
		sel_start=scintilla_send_message(doc->editor->sci, SCI_GETANCHOR, 0, 0);
		push_number(L, sel_start);
		push_number(L, sel_end);
		return 2;
	} else {
		if (!lua_isnumber(L, 1)) { return FAIL_NUMERIC_ARG(1);	}
		sel_start=lua_tonumber(L,1);
		if (1==argc) {
			sel_end=sel_start;
		} else {
			if (!lua_isnumber(L, 2)) { return FAIL_NUMERIC_ARG(2); }
			sel_end=lua_tonumber(L,2);
		}
		scintilla_send_message(doc->editor->sci,
			SCI_SETSELECTIONMODE, rectsel?SC_SEL_RECTANGLE:SC_SEL_STREAM, 0);
		sci_set_current_position(doc->editor->sci, sel_end, FALSE);
		scintilla_send_message(doc->editor->sci, SCI_SETANCHOR, sel_start, 0);
		sci_ensure_line_is_visible(doc->editor->sci,
			sci_get_line_from_position(doc->editor->sci, sel_end));
		sci_scroll_caret(doc->editor->sci);
		scintilla_send_message(doc->editor->sci,
			SCI_SETSELECTIONMODE, rectsel?SC_SEL_RECTANGLE:SC_SEL_STREAM, 0);
		return 0;
	}
}


/* Returns the total number of lines in the current document */
static gint glspi_height(lua_State* L)
{
	DOC_REQUIRED
	push_number(L, sci_get_line_count(doc->editor->sci));
	return 1;
}


/* Returns the total number of characters in the current document */
static gint glspi_length(lua_State* L)
{
	DOC_REQUIRED
	push_number(L, sci_get_length(doc->editor->sci));
	return 1;
}



/* Get or set the caret position */
static gint glspi_caret(lua_State* L)
{
	DOC_REQUIRED
	if (lua_gettop(L)==0) {
		push_number(L,sci_get_current_position(doc->editor->sci));
		return 1;
	} else {
		if (!lua_isnumber(L,1)) { return FAIL_NUMERIC_ARG(1); }
		sci_set_current_position(doc->editor->sci,(gint)lua_tonumber(L,1),TRUE);
		return 0;
	}
}


/*
	Translate between rectangular (line/column) and linear (position) locations.
*/
static gint glspi_rowcol(lua_State* L)
{
	gint argc=lua_gettop(L);
	gint line,col,pos,len,cnt;
	DOC_REQUIRED
	switch (argc) {
		case 0:
		case 1:
			if (argc==0) {
				pos=sci_get_current_position(doc->editor->sci);
			} else {
				if (!lua_isnumber(L,1)) { return FAIL_NUMERIC_ARG(1); }
				pos=lua_tonumber(L,1);
				if ( pos < 0 ) {
					pos=0;
				} else {
					len=sci_get_length(doc->editor->sci);
					if ( pos >= len ) { pos=len-1; }
				}
			}
			line=sci_get_line_from_position(doc->editor->sci,pos);
			col=sci_get_col_from_position(doc->editor->sci,pos);
			push_number(L,line+1);
			push_number(L,col);
			return 2;
		default:
			if (!lua_isnumber(L,2)) { return FAIL_NUMERIC_ARG(2); }
			if (!lua_isnumber(L,1)) { return FAIL_NUMERIC_ARG(1); }
			line=lua_tonumber(L,1);
			if ( line < 1 ) {
				line=1;
			} else {
				cnt=sci_get_line_count(doc->editor->sci);
				if ( line > cnt ) { line=cnt; }
			}
			col=lua_tonumber(L,2);
			if ( col < 0 ) {
				col=0;
			} else {
				len=sci_get_line_length(doc->editor->sci,line);
				if (col>=len) {col=len-1;}
			}
			pos=sci_get_position_from_line(doc->editor->sci,line-1)+col;
			push_number(L,pos);
			return 1;
	}
}


/* Set or unset the undo marker */
static gint glspi_batch(lua_State* L)
{
	DOC_REQUIRED
	if ( (lua_gettop(L)==0) || (!lua_isboolean(L,1)) ) {
		return FAIL_BOOL_ARG(1);
	}
	if (lua_toboolean(L,1)) {
		sci_start_undo_action(doc->editor->sci);
	} else {
		sci_end_undo_action(doc->editor->sci);
	}
	return 0;
}



/* Return the "word" at the given position */
static gint glspi_word(lua_State* L)
{
	const gchar* word_chars = GEANY_WORDCHARS;
	gint pos,linenum, bol, bow, eow;
	gchar *text=NULL;
	DOC_REQUIRED
	if (lua_gettop(L)>0) {
		if (!lua_isnumber(L,1)) { return FAIL_NUMERIC_ARG(1); }
		pos=lua_tonumber(L,1);
	} else {
		pos = sci_get_current_position(doc->editor->sci);
	}
	linenum = sci_get_line_from_position(doc->editor->sci, pos);
	bol = sci_get_position_from_line(doc->editor->sci, linenum);
	bow = pos - bol;
	eow = pos - bol;
	text=sci_get_line(doc->editor->sci, linenum);
	lua_getglobal(L, LUA_MODULE_NAME);
	if ( lua_istable(L, -1) ) {
		lua_pushstring(L,tokenWordChars);
		lua_gettable(L, -2);
		if (lua_isstring(L, -1)) {
			word_chars=lua_tostring(L, -1);
		} else {
			lua_getglobal(L, LUA_MODULE_NAME);
			lua_pushstring(L,tokenWordChars);
			lua_pushstring(L,word_chars);
			lua_settable(L, -3);
		}
	}
	while ( (bow>0) && (strchr(word_chars,text[bow-1])!=NULL) ) {bow--;}
	while (text[eow] && (strchr(word_chars, text[eow])!=NULL) ) {eow++;}
	text[eow]='\0';
	lua_pushstring(L, text+bow);
	g_free(text);
	return 1;
}



/*
	Pushes the line of text onto the Lua stack from the specified
	line number. Return FALSE only if the index is out of bounds.
*/
static gchar* get_line_text(GeanyDocument*doc,gint linenum)
{
	gint count=sci_get_line_count(doc->editor->sci);
	if ((linenum>0)&&(linenum<=count)) {
		gchar *text=sci_get_line(doc->editor->sci, linenum-1);
		return text?text:g_strdup("");
	} else {
		return FALSE;
	}
}



/*
	Lua "closure" function to iterate through each line in the current document
*/
static gint lines_closure(lua_State *L)
{
	gint idx=lua_tonumber(L, lua_upvalueindex(1))+1;
	GeanyDocument *doc=lua_touserdata(L,lua_upvalueindex(2));
	gchar *text=get_line_text(doc,idx);
	if ( text ) {
		push_number(L, idx);
		lua_pushvalue(L, -1);
		lua_replace(L, lua_upvalueindex(1));
		lua_pushstring(L,text);
		g_free(text);
		return 2;
	} else {
		return 0;
	}
}



/* Access the individual lines in the current document */
static gint glspi_lines(lua_State* L)
{
	DOC_REQUIRED
	if (lua_gettop(L)==0) {
		push_number(L,0);
		lua_pushlightuserdata(L,doc); /* Pass the doc pointer to our iterator */
		lua_pushcclosure(L, &lines_closure, 2);
		return 1;
	} else {
		int idx;
		gchar *text;
		if (!lua_isnumber(L,1)) { return FAIL_NUMERIC_ARG(1); }
		idx=lua_tonumber(L,1);
		text=get_line_text(doc,idx);
		if (text) {
			lua_pushstring(L,text);
			g_free(text);
			return 1;
		} else {
			return 0;
		}
	}
}




static gint get_sci_nav_cmd(const gchar*str, gboolean fwd, gboolean sel, gboolean rect)
{
	if (g_ascii_strncasecmp(str, "char", 4) == 0) {
		if (fwd) {
			return sel?(rect?SCI_CHARRIGHTRECTEXTEND:SCI_CHARRIGHTEXTEND):SCI_CHARRIGHT;
		} else {
			return sel?(rect?SCI_CHARLEFTRECTEXTEND:SCI_CHARLEFTEXTEND):SCI_CHARLEFT;
		}
	} else if (g_ascii_strncasecmp(str, "word", 4) == 0) {
		if (fwd) {
			return sel?SCI_WORDRIGHTEXTEND:SCI_WORDRIGHT;
		} else {
			return sel?SCI_WORDLEFTEXTEND:SCI_WORDLEFT;
		}
	} else if (g_ascii_strncasecmp(str, "part", 4) == 0) {
		if (fwd) {
			return sel?SCI_WORDPARTRIGHTEXTEND:SCI_WORDPARTRIGHT;
		} else {
			return sel?SCI_WORDPARTLEFTEXTEND:SCI_WORDPARTLEFT;
		}
	} else if (g_ascii_strncasecmp(str, "edge", 4) == 0) {
		if (fwd) {
			return sel?(rect?SCI_LINEENDRECTEXTEND:SCI_LINEENDEXTEND):SCI_LINEEND;
		} else {
			return sel?(rect?SCI_HOMERECTEXTEND:SCI_HOMEEXTEND):SCI_HOME;
		}
	} else if (g_ascii_strncasecmp(str, "line", 4) == 0) {
		if (fwd) {
			return sel?(rect?SCI_LINEDOWNRECTEXTEND:SCI_LINEDOWNEXTEND):SCI_LINEDOWN;
		} else {
			return sel?(rect?SCI_LINEUPRECTEXTEND:SCI_LINEUPEXTEND):SCI_LINEUP;
		}
	} else if (g_ascii_strncasecmp(str, "para", 4) == 0) {
		if (fwd) {
			return sel?SCI_PARADOWNEXTEND:SCI_PARADOWN;
		} else {
			return sel?SCI_PARAUPEXTEND:SCI_PARAUP;
		}
	} else if (g_ascii_strncasecmp(str, "page", 4) == 0) {
		if (fwd) {
			return sel?(rect?SCI_PAGEDOWNRECTEXTEND:SCI_PAGEDOWNEXTEND):SCI_PAGEDOWN;
		} else {
			return sel?(rect?SCI_PAGEUPRECTEXTEND:SCI_PAGEUPEXTEND):SCI_PAGEUP;
		}
	} else if (g_ascii_strncasecmp(str, "body", 4) == 0) {
		if (fwd) {
			return sel?SCI_DOCUMENTENDEXTEND:SCI_DOCUMENTEND;
		} else {
			return sel?SCI_DOCUMENTSTARTEXTEND:SCI_DOCUMENTSTART;
		}
	} else return SCI_NULL;
}






static gint glspi_navigate(lua_State* L)
{
	gint scicmd;
	const gchar *strcmd="char";
	gboolean sel=FALSE;
	gboolean fwd=TRUE;
	gboolean rect=FALSE;
	gint count=1;
	DOC_REQUIRED
	switch (lua_gettop(L)){
		case 0: break;
		case 4:
			if (!lua_isboolean(L,4)) { return FAIL_BOOL_ARG(4); }
			rect=lua_toboolean(L,4);
		case 3:
			if (!lua_isboolean(L,3)) { return FAIL_BOOL_ARG(3); }
			sel=lua_toboolean(L,3);
		case 2:
			if (!lua_isnumber(L,2)) { return FAIL_NUMERIC_ARG(2); }
			count=lua_tonumber(L,2);
			if (count<0) {
				fwd=FALSE;
				count=0-count;
			}
		case 1:
			if (!lua_isstring(L,1)) { return FAIL_STRING_ARG(1); }
			strcmd=lua_tostring(L,1);
	}
	scicmd=get_sci_nav_cmd(strcmd,fwd,sel, rect);
	if ( SCI_NULL == scicmd ) {
		lua_pushfstring(
			L, _( "Error in module \"%s\" at function navigate():\n"
						"unknown navigation mode \"%s\" for argument #1.\n"),
						LUA_MODULE_NAME, strcmd);
		lua_error(L);
	} else {
		gint n;
		for (n=0; n<count; n++) {
			sci_send_command(doc->editor->sci,scicmd);
		}
	}
	return 0;
}



static void swap(gint*a,gint*b)
{
	gint tmp=*a;
	*a=*b;
	*b=tmp;
}


static gint glspi_copy(lua_State* L)
{
	const gchar *content=NULL;
	gint start=-1, stop=-1;
	gint len=0;
	DOC_REQUIRED


	switch (lua_gettop(L)) {
		case 0:
			start=sci_get_selection_start(doc->editor->sci);
			stop=sci_get_selection_end(doc->editor->sci);
			if (start>stop) { swap(&start,&stop); }
			if (start!=stop) sci_send_command(doc->editor->sci, SCI_COPY);
			push_number(L, stop-start);
			return 1;
		case 1:
			if (!lua_isstring(L,1)) {return FAIL_STRING_ARG(1);}
			content=lua_tostring(L,1);
			len=strlen(content);
			if (len) { scintilla_send_message(doc->editor->sci,SCI_COPYTEXT,len,(sptr_t)content); }
			push_number(L, len);
			return 1;
		default:
			if (!lua_isnumber(L,2)) { return FAIL_NUMERIC_ARG(2); }
			if (!lua_isnumber(L,1)) { return FAIL_NUMERIC_ARG(1); }
			start=lua_tonumber(L,1);
			stop=lua_tonumber(L,2);
			if (start<0) { return FAIL_UNSIGNED_ARG(1); }
			if (stop <0) { return FAIL_UNSIGNED_ARG(2); }
			if (start>stop) { swap(&start,&stop); }
			if (start!=stop) scintilla_send_message(doc->editor->sci,SCI_COPYRANGE,start,stop);
			push_number(L, stop-start);
			return 1;
	}
}



static gint glspi_cut(lua_State* L)
{
	gint start,stop,len;
	DOC_REQUIRED
	start=sci_get_selection_start(doc->editor->sci);
	stop=sci_get_selection_end(doc->editor->sci);
	len=sci_get_length(doc->editor->sci);
	if (start>stop) {swap(&start,&stop); }
	if (start!=stop) {sci_send_command(doc->editor->sci, SCI_CUT); }
	push_number(L,len-sci_get_length(doc->editor->sci));
	return 1;
}





static gint glspi_paste(lua_State* L)
{
	DOC_REQUIRED
	if (scintilla_send_message(doc->editor->sci,SCI_CANPASTE,0,0)) {
		gint len=sci_get_length(doc->editor->sci);
		sci_send_command(doc->editor->sci,SCI_PASTE);
		push_number(L,sci_get_length(doc->editor->sci)-len);
	} else {
		lua_pushnil(L);
	}
	return 1;
}



static gint glspi_match(lua_State* L)
{
	gint pos;
	DOC_REQUIRED
	if (lua_gettop(L)==0) {
		pos=sci_get_current_position(doc->editor->sci);
	} else {
		if ( !lua_isnumber(L,1) ) {return FAIL_NUMERIC_ARG(1);}
		pos=lua_tonumber(L,1);
	}
	push_number(L,sci_find_matching_brace(doc->editor->sci,pos));
	return 1;
}




static gint glspi_byte(lua_State* L)
{
	gint pos;
	DOC_REQUIRED
	if (lua_gettop(L)==0) {
		pos=sci_get_current_position(doc->editor->sci);
	} else {
		if ( !lua_isnumber(L,1) ) {return FAIL_NUMERIC_ARG(1);}
		pos=lua_tonumber(L,1);
	}
	push_number(L,sci_get_char_at(doc->editor->sci,pos));
	return 1;
}





static GHashTable*sci_cmd_hash=NULL;

static void glspi_init_sci_cmd_hash(void)
{
	gint i;
	sci_cmd_hash=g_hash_table_new(g_str_hash,g_str_equal);
	for (i=0; sci_cmd_hash_entries[i].name; i++) {
		g_hash_table_insert(
			sci_cmd_hash,
			(gpointer) sci_cmd_hash_entries[i].name,&sci_cmd_hash_entries[i]);
	}
}


static void glspi_free_sci_cmd_hash(void) {
	if (sci_cmd_hash) {
		g_hash_table_destroy(sci_cmd_hash);
		sci_cmd_hash=NULL;
	}
}

void glspi_set_sci_cmd_hash(gboolean create) {
	if (create) {
		glspi_init_sci_cmd_hash ();
	} else {
		glspi_free_sci_cmd_hash();
	}
}


static SciCmdHashEntry* lookup_cmd_id(gint cmd)
{
	gint i;
	for (i=0;sci_cmd_hash_entries[i].name; i++) {
		if (sci_cmd_hash_entries[i].msgid==cmd) { return &sci_cmd_hash_entries[i];}
	}
	return NULL;
}


#define lookup_cmd_str(cmd) g_hash_table_lookup(sci_cmd_hash,cmd);



static gint glspi_fail_not_implemented(lua_State* L, const gchar*funcname, const gchar*cmdname)
{
	lua_pushfstring(
		L, _( "Error in module \"%s\" at function %s():\n"
			"API command \"%s\" not implemented.\n"),
		LUA_MODULE_NAME, &funcname[6], cmdname);
	lua_error(L);
	return 0;
}


static gint glspi_fail_arg_count(lua_State* L, const gchar*funcname, const gchar*cmdname)
{
	lua_pushfstring(
		L, _( "Error in module \"%s\" at function %s():\n"
			"not enough arguments for command \"%s\".\n"),
		LUA_MODULE_NAME, &funcname[6], cmdname);
	lua_error(L);
	return 0;
}



#define FAIL_API glspi_fail_not_implemented(L,__FUNCTION__,he->name)

#define FAIL_ARGC glspi_fail_arg_count(L,__FUNCTION__,he->name)

static uptr_t glspi_scintilla_param(lua_State* L, int ptype, int pnum, SciCmdHashEntry *he)
{
	switch (ptype) {
		case SLT_VOID:
			return 0;
		case SLT_BOOL:
			if (!lua_isboolean(L,pnum)) { return FAIL_BOOL_ARG(pnum); };
			return lua_toboolean(L,pnum);
		case SLT_STRING:
			if (!lua_isstring(L,pnum)) { return FAIL_STRING_ARG(pnum); };
			return (uptr_t)lua_tostring(L,pnum);

		/* treat most parameters as number */
		case SLT_ACCESSIBILITY:
		case SLT_ALPHA:
		case SLT_ANNOTATIONVISIBLE:
		case SLT_AUTOCOMPLETEOPTION:
		case SLT_AUTOMATICFOLD:
		case SLT_BIDIRECTIONAL:
		case SLT_CARETPOLICY:
		case SLT_CARETSTICKY:
		case SLT_CARETSTYLE:
		case SLT_CASEINSENSITIVEBEHAVIOUR:
		case SLT_CASEVISIBLE:
		case SLT_CELLS:
		case SLT_CHARACTERSET:
		case SLT_COLOURALPHA:
		case SLT_CURSORSHAPE:
		case SLT_DOCUMENTOPTION:
		case SLT_EDGEVISUALSTYLE:
		case SLT_ELEMENT:
		case SLT_ENDOFLINE:
		case SLT_EOLANNOTATIONVISIBLE:
		case SLT_FINDOPTION:
		case SLT_FINDTEXT:
		case SLT_FOLDACTION:
		case SLT_FOLDDISPLAYTEXTSTYLE:
		case SLT_FOLDFLAG:
		case SLT_FOLDLEVEL:
		case SLT_FONTQUALITY:
		case SLT_FONTWEIGHT:
		case SLT_FORMATRANGE:
		case SLT_IDLESTYLING:
		case SLT_IMEINTERACTION:
		case SLT_INDENTVIEW:
		case SLT_INDICATORSTYLE:
		case SLT_INDICFLAG:
		case SLT_LAST:
		case SLT_LAYER:
		case SLT_LINE:
		case SLT_LINECACHE:
		case SLT_LINECHARACTERINDEXTYPE:
		case SLT_LINEENDTYPE:
		case SLT_MARGINOPTION:
		case SLT_MARGINTYPE:
		case SLT_MARKERSYMBOL:
		case SLT_MODIFICATIONFLAGS:
		case SLT_MULTIAUTOCOMPLETE:
		case SLT_MULTIPASTE:
		case SLT_ORDERING:
		case SLT_PHASESDRAW:
		case SLT_POINTER:
		case SLT_POPUP:
		case SLT_PRINTOPTION:
		case SLT_REPRESENTATIONAPPEARANCE:
		case SLT_SELECTIONMODE:
		case SLT_STATUS:
		case SLT_SUPPORTS:
		case SLT_TABDRAWMODE:
		case SLT_TECHNOLOGY:
		case SLT_TEXTRANGE:
		case SLT_TYPEPROPERTY:
		case SLT_UNDOFLAGS:
		case SLT_VIRTUALSPACE:
		case SLT_VISIBLEPOLICY:
		case SLT_WHITESPACE:
		case SLT_WRAP:
		case SLT_WRAPINDENTMODE:
		case SLT_WRAPVISUALFLAG:
		case SLT_WRAPVISUALLOCATION:
		case SLT_INT:
			if (!lua_isnumber(L,pnum)) { return FAIL_NUMERIC_ARG(pnum); };
			return lua_tonumber(L,pnum);

		case SLT_STRINGRESULT:
		default:
			return FAIL_API;
	}

	return FAIL_API;
}

static gint glspi_scintilla(lua_State* L)
{
	uptr_t wparam=0;
	sptr_t lparam=0;
	SciCmdHashEntry*he=NULL;
	gint argc=lua_gettop(L);
	gchar*resultbuf=NULL;
	gint bufsize=0;
	DOC_REQUIRED
	if (argc==0) { return FAIL_STRING_ARG(1); }

	if (lua_isnumber(L,1)) {
		he=lookup_cmd_id((gint)lua_tonumber(L,1));
	} else {
		if (lua_isstring(L,1)) {
			gchar cmdbuf[64];
			gint i;
			memset(cmdbuf,'\0', sizeof(cmdbuf));
			strncpy(cmdbuf,lua_tostring(L,1),sizeof(cmdbuf)-1);
			for (i=0;cmdbuf[i];i++) {cmdbuf[i]=g_ascii_toupper(cmdbuf[i]);}
			he=lookup_cmd_str((strncmp(cmdbuf,"SCI_",4)==0)?&cmdbuf[4]:cmdbuf);
		} else { return FAIL_STRING_ARG(1); }
	}
	if ( !he ) {
		lua_pushfstring(
			L, _( "Error in module \"%s\" at function %s():\n"
				"unknown command \"%s\" given for argument #1.\n"),
			LUA_MODULE_NAME, &__FUNCTION__[6], lua_tostring(L,1));
		lua_error(L);
		return 0;
	}

	/* Don't allow lexer changes, but allow getting lexer info */
	switch (he->msgid) {
		case SCI_CHANGELEXERSTATE:
		case SCI_PRIVATELEXERCALL:
		case SCI_SETILEXER:
			return FAIL_STRING_ARG(1);

		case SCI_GETLEXER:
		case SCI_GETLEXERLANGUAGE:
		default:
			break;
	}

	if (he->lparam==SLT_STRINGRESULT) {
	/* We can allow missing wparam (length) for some string result types */
	} else {
		if ((he->lparam!=SLT_VOID)&&(argc<3)) { return FAIL_ARGC; }
		if ((he->wparam!=SLT_VOID)&&(argc<2)) { return FAIL_ARGC; }
	}

	/* first scintilla parameter */
	wparam = glspi_scintilla_param(L, he->wparam, 2, he);

	/* second scintilla parameter */
	switch (he->lparam) {
		case SLT_STRINGRESULT:
			if ((he->msgid==SCI_GETTEXT)&&(wparam==0)) {
				wparam=scintilla_send_message(doc->editor->sci, SCI_GETLENGTH, 0,0);
			}
			switch (he->msgid) {
				case SCI_GETTEXT:
					if (wparam==0){
						wparam=scintilla_send_message(doc->editor->sci, SCI_GETLENGTH, 0,0);
					} else { wparam++; }
					break;
				case SCI_GETCURLINE:
					if (wparam>0) { wparam++; }
					break;
			}
			bufsize=scintilla_send_message(doc->editor->sci, he->msgid, wparam, 0);
			if (bufsize) {
				resultbuf=g_malloc0((guint)(bufsize+1));
				lparam=(sptr_t)resultbuf;
			} else {
				lua_pushnil(L);
				return 1;
			}
			break;
		default:
			lparam = (sptr_t)glspi_scintilla_param(L, he->lparam, 3, he);
			break;
	}

	/* send scintilla message and process result */
	switch (he->result) {
		case SLT_VOID:
			scintilla_send_message(doc->editor->sci, he->msgid, wparam, lparam);
			lua_pushnil(L);
			return 1;
		case SLT_BOOL:
			lua_pushboolean(L, scintilla_send_message(doc->editor->sci, he->msgid, wparam, lparam));
			return 1;

		case SLT_ACCESSIBILITY:
		case SLT_ALPHA:
		case SLT_ANNOTATIONVISIBLE:
		case SLT_AUTOCOMPLETEOPTION:
		case SLT_AUTOMATICFOLD:
		case SLT_BIDIRECTIONAL:
		case SLT_CARETPOLICY:
		case SLT_CARETSTICKY:
		case SLT_CARETSTYLE:
		case SLT_CASEINSENSITIVEBEHAVIOUR:
		case SLT_CASEVISIBLE:
		case SLT_CELLS:
		case SLT_CHARACTERSET:
		case SLT_COLOURALPHA:
		case SLT_CURSORSHAPE:
		case SLT_DOCUMENTOPTION:
		case SLT_EDGEVISUALSTYLE:
		case SLT_ELEMENT:
		case SLT_ENDOFLINE:
		case SLT_EOLANNOTATIONVISIBLE:
		case SLT_FINDOPTION:
		case SLT_FINDTEXT:
		case SLT_FOLDACTION:
		case SLT_FOLDDISPLAYTEXTSTYLE:
		case SLT_FOLDFLAG:
		case SLT_FOLDLEVEL:
		case SLT_FONTQUALITY:
		case SLT_FONTWEIGHT:
		case SLT_FORMATRANGE:
		case SLT_IDLESTYLING:
		case SLT_IMEINTERACTION:
		case SLT_INDENTVIEW:
		case SLT_INDICATORSTYLE:
		case SLT_INDICFLAG:
		case SLT_LAST:
		case SLT_LAYER:
		case SLT_LINE:
		case SLT_LINECACHE:
		case SLT_LINECHARACTERINDEXTYPE:
		case SLT_LINEENDTYPE:
		case SLT_MARGINOPTION:
		case SLT_MARGINTYPE:
		case SLT_MARKERSYMBOL:
		case SLT_MODIFICATIONFLAGS:
		case SLT_MULTIAUTOCOMPLETE:
		case SLT_MULTIPASTE:
		case SLT_ORDERING:
		case SLT_PHASESDRAW:
		case SLT_POINTER:
		case SLT_POPUP:
		case SLT_PRINTOPTION:
		case SLT_REPRESENTATIONAPPEARANCE:
		case SLT_SELECTIONMODE:
		case SLT_STATUS:
		case SLT_SUPPORTS:
		case SLT_TABDRAWMODE:
		case SLT_TECHNOLOGY:
		case SLT_TEXTRANGE:
		case SLT_TYPEPROPERTY:
		case SLT_UNDOFLAGS:
		case SLT_VIRTUALSPACE:
		case SLT_VISIBLEPOLICY:
		case SLT_WHITESPACE:
		case SLT_WRAP:
		case SLT_WRAPINDENTMODE:
		case SLT_WRAPVISUALFLAG:
		case SLT_WRAPVISUALLOCATION:
		case SLT_INT:
			if (he->lparam==SLT_STRINGRESULT) {
				scintilla_send_message(doc->editor->sci, he->msgid, wparam, lparam);
				lua_pushstring(L,resultbuf);
				g_free(resultbuf);
			} else {
				push_number(L, scintilla_send_message(doc->editor->sci, he->msgid, wparam, lparam));
			}
			return 1;

		case SLT_STRING:
		case SLT_STRINGRESULT:
		default:
			return FAIL_API;
	}
}

static gint glspi_find(lua_State* L)
{
	struct Sci_TextToFind ttf;

	gint flags=0;
	gint i,n;
	gchar *text;
	DOC_REQUIRED
	switch (lua_gettop(L)) {
		case 0:return FAIL_STRING_ARG(1);
		case 1:return FAIL_NUMERIC_ARG(2);
		case 2:return FAIL_NUMERIC_ARG(3);
		case 3:return FAIL_TABLE_ARG(4);
	}

	if (!lua_isstring(L,1)) { return FAIL_STRING_ARG(1); }
	if (!lua_isnumber(L,2)) { return FAIL_NUMERIC_ARG(2); }
	if (!lua_isnumber(L,3)) { return FAIL_NUMERIC_ARG(3); }
	if (!lua_istable(L,4)) { return FAIL_TABLE_ARG(4); }

	text=g_strdup(lua_tostring(L,1));
	ttf.lpstrText=text;
	ttf.chrg.cpMin=lua_tonumber(L,2);
	ttf.chrg.cpMax=lua_tonumber(L,3);

	n=lua_objlen(L,4);
	for (i=1;i<=n; i++) {
		lua_rawgeti(L,4,i);
		if (lua_isstring(L, -1)) {
			const gchar*flagname=lua_tostring(L,-1);
			if (g_ascii_strcasecmp(flagname, "matchcase")==0){
				flags += SCFIND_MATCHCASE;
			} else if (g_ascii_strcasecmp(flagname, "wholeword")==0) {
				flags += SCFIND_WHOLEWORD;
			} else if (g_ascii_strcasecmp(flagname, "wordstart")==0) {
				flags += SCFIND_WORDSTART;
			} else if (g_ascii_strcasecmp(flagname, "regexp")==0) {
				flags += SCFIND_REGEXP;
			} else if (g_ascii_strcasecmp(flagname, "posix")==0) {
				flags += SCFIND_POSIX;
			} else {
				lua_pushfstring(L, _("Error in module \"%s\" at function %s():\n"
					" invalid table in argument #%d:\n"
					" unknown flag \"%s\" for element #%d\n"),
					LUA_MODULE_NAME, &__FUNCTION__[6], 4,
					(strlen(flagname)>64)?_("<too large to display>"):flagname, i);
				lua_error(L);
			}
		} else return glspi_fail_elem_type(L, __FUNCTION__, 4, i, "string");
		lua_pop(L, 1);
	}

	if (scintilla_send_message(doc->editor->sci,SCI_FINDTEXT,flags,(sptr_t)&ttf)!=-1) {
		push_number(L,ttf.chrgText.cpMin);
		push_number(L,ttf.chrgText.cpMax);
		g_free(text);
		return 2;
	} else {
		g_free(text);
		return 0;
	}
}



/*
SCFIND_MATCHCASE  A match only occurs with text that matches the case of the search string.
SCFIND_WHOLEWORD  A match only occurs if the characters before and after are not word characters.
SCFIND_WORDSTART  A match only occurs if the character before is not a word character.
SCFIND_REGEXP  The search string should be interpreted as a regular expression.
SCFIND_POSIX   Interpret bare ( and ) for tagged regex sections rather than \( and \).
*/


/*
struct CharacterRange {
		long cpMin;
		long cpMax;
};

*/

/*
struct Sci_TextToFind {
		struct CharacterRange chrg;     // range to search
		char *lpstrText;                // the search pattern (zero terminated)
		struct CharacterRange chrgText; // returned as position of matching text
};
*/



static const struct luaL_Reg glspi_sci_funcs[] = {
	{"text",      glspi_text},
	{"selection", glspi_selection},
	{"select",    glspi_select},
	{"height",    glspi_height},
	{"length",    glspi_length},
	{"caret",     glspi_caret},
	{"rowcol",    glspi_rowcol},
	{"batch",     glspi_batch},
	{"word",      glspi_word},
	{"lines",     glspi_lines},
	{"navigate",  glspi_navigate},
	{"cut",       glspi_cut},
	{"copy",      glspi_copy},
	{"paste",     glspi_paste},
	{"match",     glspi_match},
	{"byte",      glspi_byte},
	{"scintilla", glspi_scintilla},
	{"find",      glspi_find},
	{NULL,NULL}
};

void glspi_init_sci_funcs(lua_State *L) {
	luaL_register(L, NULL,glspi_sci_funcs);
}
