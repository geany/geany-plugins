/*
 * glspi_doc.c - This file is part of the Lua scripting plugin for the Geany IDE
 * See the file "geanylua.c" for copyright information.
 */

#define NEED_FAIL_ARG_TYPE
#define NEED_FAIL_ARG_TYPES
#include "glspi.h"


#define NOTEBOOK GTK_NOTEBOOK(main_widgets->notebook)


#ifdef G_OS_WIN32
#define fncmp(a,b) ( a && b && (strcasecmp(a,b)==0))
#else
#define fncmp(a,b) ( a && b && (strcmp(a,b)==0))
#endif



/* Return the filename of the currently active Geany document */
static gint glspi_filename(lua_State* L)
{
	DOC_REQUIRED
	lua_pushstring(L, (const gchar *) doc->file_name);
	return 1;
}


/* Create a new Geany document tab */
static gint glspi_newfile(lua_State* L)
{
	const gchar *fn=NULL;
	GeanyFiletype *ft=NULL;
	switch (lua_gettop(L)) {
	case 0: break;
	case 2:
		if (!lua_isstring(L, 2))	{ return FAIL_STRING_ARG(2); }
		const gchar *tmp=lua_tostring(L, 2);
		if ( '\0' == tmp[0] ) {
			ft=NULL;
		} else {
			ft=filetypes_lookup_by_name(tmp);
		}
	default:
		if (!lua_isstring(L, 1))	{ return FAIL_STRING_ARG(1); }
		fn=lua_tostring(L, 1);
		if ( '\0' == fn[0] ) { fn=NULL; }
	}
	document_new_file(fn, ft, NULL);
	return 0;
}



/*
	Try to find the geany->documents_array index of the specified filename.
	Returns -1 if the filename doesn't match any open tabs.
*/
static gint filename_to_doc_idx(const gchar*fn)
{
	if (fn && *fn) {
		guint i=0;
		foreach_document(i)
		{
			if fncmp(fn,documents[i]->file_name) {return i; }
		}
	}
	return -1;
}


/* Converts a geany->documents_array index to a notebook tab index */
static gint doc_idx_to_tab_idx(gint idx)
{
	return (
		(idx>=0) && ((guint)idx<geany->documents_array->len) && documents[idx]->is_valid
	) ? gtk_notebook_page_num(NOTEBOOK, GTK_WIDGET(documents[idx]->editor->sci)):-1;
}



/* Returns the filename of the specified document, or NULL on bad index */
static const gchar* doc_idx_to_filename(gint idx) {
	if ( (idx >= 0 ) && ( ((guint)idx) < geany->documents_array->len ) ) {
		GeanyDocument *doc=g_ptr_array_index(geany->documents_array, idx);
		if (doc) { return doc->file_name?doc->file_name:GEANY_STRING_UNTITLED; }
	}
	return NULL;
}



/* Actvate and focus the specified document */
static gint glspi_activate(lua_State* L)
{
	gint idx=-1;
	if (lua_gettop(L)>0) {
		if (lua_isnumber(L,1)) {
			idx=(lua_tonumber(L,1));
			if (idx<0) { /* Negative number refers to (absolute) GtkNotebook index */
				idx=(0-idx)-1;
				if (idx>=gtk_notebook_get_n_pages(NOTEBOOK)) { idx=-1;}
			} else { /* A positive number refers to the geany->documents_array index */
				idx=doc_idx_to_tab_idx(idx-1);
			}

		} else {
			if (lua_isstring(L,1)) {
				idx=doc_idx_to_tab_idx(filename_to_doc_idx(lua_tostring(L, 1)));
			} else {
				if (!lua_isnil(L,1)) { return FAIL_STR_OR_NUM_ARG(1); }
			}
		}
	}
	if (idx>=0) {
		if (idx!=gtk_notebook_get_current_page(NOTEBOOK)) {
			gtk_notebook_set_current_page(NOTEBOOK, idx);
		}
	}
	lua_pushboolean(L, (idx>0));
	return 1;
}



/* Lua "closure" function to iterate through the list of open documents */
static gint documents_closure(lua_State *L)
{
	gint idx=lua_tonumber(L, lua_upvalueindex(1));
	int max=geany->documents_array->len;
	do {
		/* Find next valid index, skipping invalid (closed)  files */
		idx++;
	} while (( idx < max ) && !documents[idx]->is_valid );
	if ( idx < max ){
		push_number(L, idx);
		lua_pushvalue(L, -1);
		lua_replace(L, lua_upvalueindex(1));
		lua_pushstring(L,doc_idx_to_filename(idx));
		return 1;
	} else {
		return 0;
	}
}


/* Access the list of open documents */
static gint glspi_documents(lua_State *L)
{
	if (lua_gettop(L)==0) {
		push_number(L,-1);
		lua_pushcclosure(L, &documents_closure, 1);
		return 1;
	} else {
		gint idx;
		const gchar *name;
		DOC_REQUIRED
		if (lua_isnumber(L,1)) {
			idx=lua_tonumber(L,1)-1;
			name=doc_idx_to_filename(idx);
			if (name) {
				lua_pushstring(L,name);
				return 1;
			} else {
				return 0;
			}
		} else {
			if (lua_isstring(L,1)) {
				name=lua_tostring(L,1);
				idx=filename_to_doc_idx(name);
				if (idx>=0) {
					push_number(L,idx+1);
					return 1;
				} else { return 0; }
			} else { return FAIL_STR_OR_NUM_ARG(1); }
		}
	}
}


/* Returns the number of open documents */
static gint glspi_count(lua_State* L)
{
	guint i=0, n=0;
	foreach_document(i)
	{
		if (documents[i]->is_valid){n++;}
	}
	push_number(L,n);
	return 1;
}


/* Save a file to disk */
static gint glspi_save(lua_State* L)
{
	gboolean status=FALSE;
	if (lua_gettop(L)==0){
		DOC_REQUIRED
		status=document_save_file(document_get_current(), TRUE);
	} else {
		if (lua_isnumber(L,1)) {
			gint idx=(gint)lua_tonumber(L,1)-1;
			status=document_save_file(documents[idx], TRUE);
		} else {
			if (lua_isstring(L,1)) {
				gint idx=filename_to_doc_idx(lua_tostring(L,1));
				status=document_save_file(documents[idx], TRUE);
			} else { return FAIL_STR_OR_NUM_ARG(1);	}
		}
	}
	lua_pushboolean(L,status);
	return 1;
}


/* Open or reload a file */
static gint glspi_open(lua_State* L)
{
	gint status=-1;
	const gchar*fn=NULL;
	gint idx=-1;

	if (lua_gettop(L)==0) {
		DOC_REQUIRED
		idx=document_get_current()->index;
	} else {
		if (lua_isnumber(L,1)) {
			idx=lua_tonumber(L,1)-1;
		} else {
			if (lua_isstring(L,1)) {
				fn=lua_tostring(L,1);
			} else { return FAIL_STR_OR_NUM_ARG(1); }
		}
	}
	if (!fn) {
		status=document_reload_force(documents[idx],NULL) ? idx : -1;
	} else {
		guint len=geany->documents_array->len;
		GeanyDocument*doc=document_open_file(fn,FALSE,NULL,NULL);
		status=doc?doc->index:-1;
		if ( (status>=0) && (len==geany->documents_array->len))
		{
			/* if len doesn't change, it means we are reloading an already open file */
			/* ntrel: actually, len can stay the same when reusing invalid document slots. */
			idx=document_get_current()->index;
			status=document_reload_force(documents[idx],NULL) ? idx : -1;
		}
	}
	push_number(L,status+1);
	return 1;
}


/* Close a document */
static gint glspi_close(lua_State* L)
{
	gboolean status=FALSE;
	if (lua_gettop(L)==0){
		DOC_REQUIRED
		status=document_close(document_get_current());
	} else {
		if (lua_isnumber(L,1)) {
			guint idx=(guint)lua_tonumber(L,1)-1;
			status=document_close(documents[idx]);
		} else {
			if (lua_isstring(L,1)) {
				guint idx=(guint)filename_to_doc_idx(lua_tostring(L,1));
				status=document_close(documents[idx]);
			} else { return FAIL_STR_OR_NUM_ARG(1);	}
		}
	}
	lua_pushboolean(L,status);
	return 1;
}


#define StrField(rec,field) rec?rec->field?rec->field:"":""

#define FileTypeStr(field) StrField(doc->file_type,field)

#define BuildCmdStr(field) \
	doc->file_type?StrField(doc->file_type->programs,field):""


/* Retrieve Geany's information about an open document */
static gint glspi_fileinfo(lua_State* L)
{
	DOC_REQUIRED
	lua_newtable(L);
	if (doc->file_name) {
		gchar*tmp,*p;
		tmp=g_path_get_dirname (doc->file_name);
		p=strchr(tmp,'\0');
		if (p>tmp) {p--;}
		lua_pushstring(L, "path");
		if (p && (*p==G_DIR_SEPARATOR)){
			lua_pushstring(L, tmp);
		} else {
			lua_pushfstring(L, "%s%s", tmp, G_DIR_SEPARATOR_S);
		}
		lua_rawset(L,-3);
		g_free(tmp);

		tmp=g_path_get_basename (doc->file_name);
		p=strrchr(tmp,'.');
		if (p==tmp) {p=NULL;}
		SetTableStr("name", tmp);
		SetTableStr("ext", p?p:"");
		g_free(tmp);
	} else {
		SetTableStr("name", "")
		SetTableStr("path", "")
	}
	SetTableStr("type",   FileTypeStr(name));
	SetTableStr("desc",   FileTypeStr(title));
	SetTableStr("opener", FileTypeStr(comment_open));
	SetTableStr("closer", FileTypeStr(comment_close));
	SetTableStr("action", FileTypeStr(context_action_cmd));
/*
	SetTableStr("compiler", BuildCmdStr(compiler));
	SetTableStr("linker",   BuildCmdStr(linker));
	SetTableStr("exec",     BuildCmdStr(run_cmd));
	SetTableStr("exec2",    BuildCmdStr(run_cmd2));
*/
	SetTableNum("ftid", GPOINTER_TO_INT(doc->file_type?doc->file_type->id:GEANY_FILETYPES_NONE));
	SetTableStr("encoding", StrField(doc,encoding));
	SetTableBool("bom",doc->has_bom);
	SetTableBool("changed",doc->changed);
	SetTableBool("readonly",doc->readonly);
	return 1;
}


/* Change the current file type */
static gint glspi_setfiletype(lua_State* L)
{
	GeanyDocument *doc=NULL;
	GeanyFiletype *ft=NULL;
	const gchar *ftn=NULL;

	if (lua_gettop(L)==1){
		if (!lua_isstring(L, 1)) { return FAIL_STRING_ARG(1); }
		doc = document_get_current();
		if (!(doc && doc->is_valid)) { return 0; }
		ftn=lua_tostring(L, 1);
		if ('\0' == ftn[0]) { return 0; }
		ft=filetypes_lookup_by_name(ftn);
		if (ft != NULL){
			document_set_filetype(doc, ft);
			return 1;
		}
	}
	return 0;
}




static const struct luaL_Reg glspi_doc_funcs[] = {
	{"filename",  glspi_filename},
	{"fileinfo",  glspi_fileinfo},
	{"settype",   glspi_setfiletype},
	{"documents", glspi_documents},
	{"count",     glspi_count},
	{"activate",  glspi_activate},
	{"newfile",   glspi_newfile},
	{"save",      glspi_save},
	{"open",      glspi_open},
	{"close",     glspi_close},
	{NULL,NULL}
};

void glspi_init_doc_funcs(lua_State *L) {
	luaL_register(L, NULL,glspi_doc_funcs);
}
