/*
 * This code is supplied as is, and is used at your own risk.
 * The GNU GPL version 2 rules apply to this code (see http://fsf.org/>
 * You can alter it, and pass it on as you want.
 * If you alter it, or pass it on, the only restriction is that this disclamour and licence be
 * left intact
 *
 * Features of Macro implementation adapted from gPHPedit (currently maintained by Anoop John)
 *
 * william.fraser@virgin.net
 * 2010-11-01
*/


#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif
#include <geanyplugin.h>

#include "utils.h"
#include "Scintilla.h"
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

/* structure to hold details of Macro event */
typedef struct
{
	gint message;
	gulong wparam;
	sptr_t lparam;
} MacroEvent;

/* structure to hold details of a macro */
typedef struct
{
	gchar *name;
	/* trigger codes */
	guint keyval;
	guint state;
	GSList *MacroEvents;
} Macro;

/* structure to hold details of Macro for macro editor */
typedef struct
{
	gint message;
	const gchar *description;
} MacroDetailEntry;

/* list of editor messages this plugin can handle & a description */
const MacroDetailEntry MacroDetails[]={
{SCI_CUT,N_("Cut to Clipboard")},
{SCI_COPY,N_("Copy to Clipboard")},
{SCI_PASTE,N_("Paste from Clipboard")},
{SCI_LINECUT,N_("Cut current line to Clipboard")},
{SCI_LINECOPY,N_("Copy current line to Clipboard")},

{SCI_DELETEBACK,N_("Delete character to the left")},
{SCI_CLEAR,N_("Delete character to the right")},
{SCI_DELETEBACKNOTLINE,N_("Delete character to the left (but not newline)")},
{SCI_DELWORDLEFT,N_("Delete up to start of word to the Left")},
{SCI_DELWORDRIGHT,N_("Delete up to start of word to the Right")},
{SCI_DELWORDRIGHTEND,N_("Delete up to end of word to the Right")},
{SCI_DELLINELEFT,N_("Delete to beginning of line")},
{SCI_DELLINERIGHT,N_("Delete to end of line")},
{SCI_LINEDELETE,N_("Delete current line")},
{SCI_BACKTAB,N_("Backwards Tab (deletes tab if nothing after it)")},

{SCI_LINESCROLLDOWN,N_("Scroll Display down a line")},
{SCI_LINESCROLLUP,N_("Scroll Display up a line")},
{SCI_ZOOMIN,N_("Zoom view in")},
{SCI_ZOOMOUT,N_("Zoom view out")},

{SCI_LINEDOWN,N_("Move Cursor Down")},
{SCI_LINEUP,N_("Move Cursor Up")},
{SCI_CHARLEFT,N_("Move Cursor Left")},
{SCI_CHARRIGHT,N_("Move Cursor Right")},
{SCI_WORDLEFT,N_("Move Cursor to start of Word to the Left")},
{SCI_WORDRIGHT,N_("Move Cursor to start of Word to the Right")},
{SCI_WORDPARTLEFT,N_("Move Cursor to start of Part of Word to the Left")},
{SCI_WORDPARTRIGHT,N_("Move Cursor to start of Part of Word to the Right")},
{SCI_HOME,N_("Move Cursor to start of line")},
{SCI_LINEEND,N_("Move Cursor to end of line")},
{SCI_DOCUMENTSTART,N_("Move Cursor to 1st line of Document")},
{SCI_DOCUMENTEND,N_("Move Cursor to last line of document")},
{SCI_PAGEUP,N_("Move Cursor up one Page")},
{SCI_PAGEDOWN,N_("Move Cursor down one Page")},
{SCI_HOMEDISPLAY,N_("Move Cursor to first visible character")},
{SCI_LINEENDDISPLAY,N_("Move Cursor to last visible character")},
{SCI_VCHOME,N_("Move Cursor to 1st non-whitespace character of line, or 1st character of line if\
 already at 1st non-whitespace character")},
{SCI_PARADOWN,N_("Move Cursor to beginning of next paragraph")},
{SCI_PARAUP,N_("Move Cursor up to beginning of current/previous paragraph")},
{SCI_WORDLEFTEND,N_("Move Cursor to end of Word to the Left")},
{SCI_WORDRIGHTEND,N_("Move Cursor to end of Word to the Right")},

{SCI_LINEDOWNEXTEND,N_("Extend Selection down a line")},
{SCI_LINEUPEXTEND,N_("Extend Selection up a line")},
{SCI_CHARLEFTEXTEND,N_("Extend Selection Left a line")},
{SCI_CHARRIGHTEXTEND,N_("Extend Selection Right a line")},
{SCI_WORDLEFTEXTEND,N_("Extend Selection to start of Word to the Left")},
{SCI_WORDRIGHTEXTEND,N_("Extend Selection to start of Word to the Right")},
{SCI_WORDPARTLEFTEXTEND,N_("Extend Selection to start of Part of Word to the Left")},
{SCI_WORDPARTRIGHTEXTEND,N_("Extend Selection to start of Part of Word to the Right")},
{SCI_HOMEEXTEND,N_("Extend Selection to start of line")},
{SCI_LINEENDEXTEND,N_("Extend Selection to end of line")},
{SCI_DOCUMENTSTARTEXTEND,N_("Extend Selection to start of document")},
{SCI_DOCUMENTENDEXTEND,N_("Extend Selection to end of document")},
{SCI_PAGEUPEXTEND,N_("Extend Selection up one Page")},
{SCI_PAGEDOWNEXTEND,N_("Extend Selection down one Page")},
{SCI_HOMEDISPLAYEXTEND,N_("Extend Selection to fist visible character")},
{SCI_LINEENDDISPLAYEXTEND,N_("Extend Selection to last visible character")},
{SCI_VCHOMEEXTEND,N_("Extend Selection to 1st non-whitespace character of line, or 1st character of\
 line if already at 1st non-whitespace character")},
{SCI_PARADOWNEXTEND,N_("Extend Selection to beginning of next paragraph")},
{SCI_PARAUPEXTEND,N_("Extend Selection up to beginning of current/previous paragraph")},
{SCI_WORDLEFTENDEXTEND,N_("Extend Selection to end of Word to the Left")},
{SCI_WORDRIGHTENDEXTEND,N_("Extend Selection to end of Word to the Right")},

{SCI_LINEDOWNRECTEXTEND,N_("Extend Rectangular Selection down a line")},
{SCI_LINEUPRECTEXTEND,N_("Extend Rectangular Selection up a line")},
{SCI_CHARLEFTRECTEXTEND,N_("Extend Rectangular Selection Left a line")},
{SCI_CHARRIGHTRECTEXTEND,N_("Extend Rectangular Selection Right a line")},
{SCI_HOMERECTEXTEND,N_("Extend Rectangular Selection to start of line")},
{SCI_LINEENDRECTEXTEND,N_("Extend Rectangular Selection to end of line")},
{SCI_PAGEUPRECTEXTEND,N_("Extend Rectangular Selection up one Page")},
{SCI_PAGEDOWNRECTEXTEND,N_("Extend Rectangular Selection down one Page")},
{SCI_VCHOMERECTEXTEND,N_("Extend Rectangular Selection to 1st non-whitespace character of line, or\
 1st character of line if already at 1st non-whitespace character")},

{SCI_CANCEL,N_("Cancel Selection")},

{SCI_EDITTOGGLEOVERTYPE,N_("Toggle Insert/Overwrite mode")},
{SCI_TAB,N_("Tab")},
{SCI_NEWLINE,N_("Newline")},

{SCI_REPLACESEL,N_("Insert/replace with \"\"")},

{SCI_LINETRANSPOSE,N_("Swap current line with one above")},
{SCI_LOWERCASE,N_("Change selected text to lowercase")},
{SCI_UPPERCASE,N_("Change selected text to uppercase")},

{SCI_LINEDUPLICATE,N_("Insert duplicate of current line below")},
{SCI_SELECTIONDUPLICATE,N_("Insert duplicate of selected text after selection. If nothing selected,\
 duplicate line")},

{SCI_SEARCHNEXT,N_("Search for next \"\"")},
{SCI_SEARCHPREV,N_("Search for previous \"\"")},
{SCI_SEARCHANCHOR,N_("Set start of search to beginning of selection")},

/* editor commands that don't seem to work well in editing
 * {SCI_FORMFEED,N_("FormFeed")},
 *
 * other commands ommited as they don't appear to do anything different to existing commands
*/
{0,NULL}
};

/* define IDs for dialog buttons */
enum GEANY_MACRO_BUTTON {
	GEANY_MACRO_BUTTON_CANCEL,
	GEANY_MACRO_BUTTON_DELETE,
	GEANY_MACRO_BUTTON_EDIT,
	GEANY_MACRO_BUTTON_RERECORD,
	GEANY_MACRO_BUTTON_UP,
	GEANY_MACRO_BUTTON_DOWN,
	GEANY_MACRO_BUTTON_ABOVE,
	GEANY_MACRO_BUTTON_BELOW,
	GEANY_MACRO_BUTTON_APPLY
};

GeanyPlugin     *geany_plugin;
GeanyData       *geany_data;

PLUGIN_VERSION_CHECK(224)

PLUGIN_SET_TRANSLATABLE_INFO(LOCALEDIR, GETTEXT_PACKAGE,
                             _("Macros"),_("Macros for Geany"),
                             "1.1","William Fraser <william.fraser@virgin.net>")

/* Plugin user alterable settings */
static gboolean bSaveMacros=TRUE;
static gboolean bQueryOverwriteMacros=TRUE;

/* internal variables */
static gint iShiftNumbers[]={41,33,34,163,36,37,94,38,42,40};
static gulong key_release_signal_id;
static GtkWidget *Record_Macro_menu_item=NULL;
static GtkWidget *Stop_Record_Macro_menu_item=NULL;
static GtkWidget *Edit_Macro_menu_item=NULL;
static Macro *RecordingMacro=NULL;
static GSList *mList=NULL;
static gboolean bMacrosHaveChanged=FALSE;

/* default config file */
const gchar default_config[] =
	"[Settings]\n"
	"Save_Macros = true\n"
	"Question_Macro_Overwrite = true\n"
	"[Macros]";

/* clear macro events list and free up any memory they are using */
static GSList * ClearMacroList(GSList *gsl)
{
	MacroEvent *me;
	GSList * gslTemp=gsl;

	/* free data held in GSLIST structure */
	while(gslTemp!=NULL)
	{
		me=gslTemp->data;
		/* check to see if it's a message that has string attached, and free it if so
		 * lparam might be NULL for SCI_SEARCHNEXT or SCI_SEARCHPREV but g_free is ok
		 * with this
		*/
		if(me->message==SCI_REPLACESEL ||
		   me->message==SCI_SEARCHNEXT ||
		   me->message==SCI_SEARCHPREV)
			g_free((void*)(me->lparam));

		g_free((void*)(gslTemp->data));
		gslTemp=g_slist_next(gslTemp);
	}

	/* free SLIST structure */
	g_slist_free(gsl);

	return NULL;
}


/* create new Macro */
static Macro * CreateMacro(void)
{
	Macro *m;

	if((m=(Macro*)(g_malloc(sizeof *m)))!=NULL)
	{
		m->name=NULL;
		m->MacroEvents=NULL;
		return m;
	}
	return NULL;
}


/* delete macro */
static Macro * FreeMacro(Macro *m)
{
	if(m==NULL)
		return NULL;

	g_free(m->name);
	ClearMacroList(m->MacroEvents);
	g_free(m);

	return NULL;
}


/* add a macro to the list of defined macros */
static void AddMacroToList(Macro *m)
{
	mList=g_slist_append(mList,m);
}


/* remove macro from list of defined macros */
static void RemoveMacroFromList(Macro *m)
{
	mList=g_slist_remove(mList,m);
}


/* returns a macro in the defined list by name, or NULL if no macro exists with the specified name
*/
static Macro * FindMacroByName(gchar *name)
{
	GSList *gsl=mList;

	if(name==NULL)
		return NULL;

	while(gsl!=NULL)
	{
		if(strcmp(name,((Macro*)(gsl->data))->name)==0)
			return gsl->data;

		gsl=g_slist_next(gsl);
	}

	return NULL;
}


/* returns a macro in the defined list by key press combination, or NULL if no macro exists
 * with the specified key combination */
static Macro * FindMacroByKey(guint keyval,guint state)
{
	GSList *gsl=mList;

	while(gsl!=NULL)
	{
		if(((Macro*)(gsl->data))->keyval==keyval && ((Macro*)(gsl->data))->state==state)
			return gsl->data;

		gsl=g_slist_next(gsl);
	}

	return NULL;
}


/* completely wipe all saved macros and ascosiated memory */
static void ClearAllMacros(void)
{
	GSList *gsl=mList;

	while(gsl!=NULL)
	{
		FreeMacro((Macro*)(gsl->data));
		gsl=g_slist_next(gsl);
	}

	g_slist_free(mList);
	mList=NULL;
}


/* Repeat a macro to the editor */
static void ReplayMacro(Macro *m)
{
	MacroEvent *me;
	GSList *gsl=m->MacroEvents;
	ScintillaObject* sci=document_get_current()->editor->sci;
	gchar *clipboardcontents;
	gboolean bFoundAnchor=FALSE;

	scintilla_send_message(sci,SCI_BEGINUNDOACTION,0,0);

	while(gsl!=NULL)
	{
		me=gsl->data;

		/* make not if anchor has been found */
		if(me->message==SCI_SEARCHANCHOR)
			bFoundAnchor=TRUE;

		/* possibility that user edited macros might not have anchor before search */
		if((me->message==SCI_SEARCHNEXT || me->message==SCI_SEARCHPREV) &&
		   bFoundAnchor==FALSE)
		{
			scintilla_send_message(sci,SCI_SEARCHANCHOR,0,0);
			bFoundAnchor=TRUE;
		}

		/* search might use clipboard to look for: check & hanndle */
		if((me->message==SCI_SEARCHNEXT || me->message==SCI_SEARCHPREV) &&
		   ((gchar*)me->lparam)==NULL)
		{
			clipboardcontents=gtk_clipboard_wait_for_text(gtk_clipboard_get(
			                  GDK_SELECTION_CLIPBOARD));
			/* ensure there is something in the clipboard */
			if(clipboardcontents==NULL)
			{
				dialogs_show_msgbox(GTK_MESSAGE_INFO,_("No text in clipboard!"));
				break;
			}

			scintilla_send_message(sci,me->message,me->wparam,(sptr_t)clipboardcontents);
		}
		else
			scintilla_send_message(sci,me->message,me->wparam,me->lparam);

		/* move to next macro event */
		gsl=g_slist_next(gsl);
	}

	scintilla_send_message(sci,SCI_ENDUNDOACTION,0,0);
}


/* convert string so that it can be saved as text in a comma separated text entry in an ini file
 * resultant string needs to be freed after use
*/
static gchar * MakeStringSaveable(gchar *s)
{
	gchar *cTemp;
	gchar **pszBits;

	/* use GLib routine to do most of the work
	 * (sort out all special characters, and non-ASCII characters)
	*/
	cTemp=g_strescape(s,"");
	/* now make sure that all instances of ',' are replaced by octal version so that can use ',' as
	 * a separator
	 * first break the string up at the commas
	*/
	pszBits=g_strsplit(cTemp,",",0);
	/* can now free up cTemp */
	g_free(cTemp);
	/* now combine bits together with octal verion of comma in-between */
	cTemp=g_strjoinv("\\054",pszBits);
	/* free up memory used by bits */
	g_strfreev(pszBits);

	return cTemp;
}


/* create a macro event from an array of stings. This command may move past more than one array
 * entry if the macro event details require it
*/
static MacroEvent * GetMacroEventFromString(gchar **s,gint *k)
{
	MacroEvent *me;

	me=g_new0(MacroEvent,1);
	/* get event number */
	me->message=strtoll(s[(*k)++],NULL,10);

	/* default to 0 */
	me->wparam=0;

	/* now handle lparam if required */
	switch(me->message)
	{
		case SCI_SEARCHNEXT:
		case SCI_SEARCHPREV:
			/* get text */
			me->lparam=(sptr_t)(g_strcompress(s[(*k)++]));
			/* if text is empty string replace with NULL to signify use clipboard */
			if((*((gchar*)(me->lparam)))==0)
			{
				g_free((gchar*)me->lparam);
				me->lparam=(sptr_t)NULL;
			}

			/* get search flags */
			me->wparam=strtoll(s[(*k)++],NULL,10);
			break;
		case SCI_REPLACESEL:
			/* get text */
			me->lparam=(sptr_t)(g_strcompress(s[(*k)++]));
			break;
		/* default handler for messages without extra data */
		default:
			me->lparam=0;
			break;
	}

	/* return MacroEvent object */
	return me;
}


/* convert a Macro event to a string that can be savable in a comma separated ini file entry
 * resultant string needs to be freed afterwards
*/
static gchar *MacroEventToString(MacroEvent *me)
{
	gchar *szMacroNumber;
	gchar *szNumberAndData;
	gchar *pTemp;

	/* save off macro code */
	szMacroNumber=g_strdup_printf("%i",me->message);

	/* now handle lparam if required */
	switch(me->message)
	{
		case SCI_SEARCHNEXT:
		case SCI_SEARCHPREV:
			/* check if string is NULL */
			if(((gchar*)(me->lparam))==NULL)
			{
				/* now merge code and data */
				szNumberAndData=g_strdup_printf("%s,,%lu",szMacroNumber,me->wparam);
				/* free memory */
				g_free(szMacroNumber);
				return szNumberAndData;
			}

			/* first get string reprisentation of data */
			pTemp=MakeStringSaveable((gchar*)(me->lparam));
			/* now merge code and data */
			szNumberAndData=g_strdup_printf("%s,%s,%lu",szMacroNumber,pTemp,me->wparam);
			/* free memory */
			g_free(szMacroNumber);
			g_free(pTemp);
			return szNumberAndData;

		case SCI_REPLACESEL:
			/* first get string reprisentation of data */
			pTemp=MakeStringSaveable((gchar*)(me->lparam));
			/* now merge code and data */
			szNumberAndData=g_strdup_printf("%s,%s",szMacroNumber,pTemp);
			/* free memory */
			g_free(szMacroNumber);
			g_free(pTemp);
			return szNumberAndData;

		/* default handler for messages without extra data */
		default:
			return szMacroNumber;
	}
}


/* Is there a document open in the editor */
static gboolean DocumentPresent(void)
{
  return (document_get_current()!=NULL);
}


/* check editor notifications and remember editor events */
static gboolean Notification_Handler(GObject *obj,GeanyEditor *ed,SCNotification *nt,gpointer ud)
{
	MacroEvent *me;
	gint i;

	/* ignore non macro recording messages */
	if(nt->nmhdr.code!=SCN_MACRORECORD)
		return FALSE;

	/* probably overkill as should not recieve SCN_MACRORECORD messages unless recording
	 * macros
	*/
	if(RecordingMacro==NULL)
		return FALSE;

	/* check to see if it's a code we're happy to deal with */
	for(i=0;;i++)
	{
		/* break if code we can deal with */
		if(nt->message==MacroDetails[i].message)
			break;

		/* got to end of codes without match: unrecognised */
		if(MacroDetails[i].description==NULL)
		{
			dialogs_show_msgbox(GTK_MESSAGE_INFO,_("Unrecognised message\n%i %i %i"),nt->message,
			                    (gint)(nt->wParam),(gint)(nt->lParam));
			return FALSE;
		}

	}
	me=g_new0(MacroEvent,1);
	me->message=nt->message;
	me->wparam=nt->wParam;
	/* Special handling for text in lparam */
	me->lparam=(me->message==SCI_SEARCHNEXT ||
	            me->message==SCI_SEARCHPREV ||
	            me->message==SCI_REPLACESEL)
		?((sptr_t) g_strdup((gchar *)(nt->lParam))) : nt->lParam;

	/* more efficient to create reverse list and reverse it at the end */
	RecordingMacro->MacroEvents=g_slist_prepend(RecordingMacro->MacroEvents,me);

	return FALSE;
}


/* convert GTK <Alt><Control><Shift>lowercase char to standard Alt+Ctrl+Shift+uppercase char
 * format resultant string needs to be freed
*/
static gchar *GetPretyKeyName(guint keyval,guint state)
{
	gboolean bAlt,bCtrl,bShift;
	const gchar *cTemp;
	gchar *cName;
	gchar *cPretyName;

	/* get basic keypress description */
	cName=gtk_accelerator_name(keyval,state);

	/* cName now holds the name but in the <Control><Shift>h format.
	* Pretify to Ctrl+Shift+H like in menus
	*/
	bAlt=((gchar*)g_strrstr(cName,"<Alt>")!=NULL);
	bCtrl=((gchar*)g_strrstr(cName,"<Control>")!=NULL);
	bShift=((gchar*)g_strrstr(cName,"<Shift>")!=NULL);

	/* find out where the modifyer key description ends. */
	cTemp=g_strrstr(cName,">");
	/* if there are no modifyers then point to start of string */
	if(cTemp==NULL)
		cTemp=cName;
	else
		cTemp++;

	/* put string back together but pretier */
	cPretyName=g_strdup_printf("%s%s%s%c%s",(bShift?"Shift+":""),(bCtrl?"Ctrl+":""),
							(bAlt?"Alt+":""),g_ascii_toupper(cTemp[0]),
							&(g_ascii_strdown(cTemp,-1)[1]));

	/* tidy up */
	g_free(cName);

	/* return pretified name */
	return cPretyName;
}


/* save settings (preferences, and macro data) */
static void SaveSettings(void)
{
	GKeyFile *config = NULL;
	gchar *config_file = NULL;
	gchar *data;
	gchar *cKey;
	gchar *pcTemp;
	gint i,k;
	GSList *gsl=mList;
	GSList *gslTemp;
	gchar **pszMacroStrings;
	Macro *m;

	/* create new config from default settings */
	config=g_key_file_new();

	/* now set settings */
	g_key_file_set_boolean(config,"Settings","Save_Macros",bSaveMacros);
	g_key_file_set_boolean(config,"Settings","Question_Macro_Overwrite",bQueryOverwriteMacros);

	/* now save macros */
	if(bSaveMacros==TRUE)
	{
		i=0;

		/* iterate through macros and save them */
		while(gsl!=NULL)
		{
			m=(Macro*)(gsl->data);
			cKey=g_strdup_printf("A%d",i);

			/* save macro name */
			pcTemp=MakeStringSaveable(m->name);
			g_key_file_set_string(config,"Macros",cKey,pcTemp);
			g_free(pcTemp);
			/* save trigger data */
			cKey[0]='B';
			g_key_file_set_integer(config,"Macros",cKey,m->keyval);
			cKey[0]='C';
			g_key_file_set_integer(config,"Macros",cKey,m->state);
			/* convert macros to saveable format
			* first generate list of all macrodetails
			*/
			pszMacroStrings=(gchar **)
				(g_malloc(sizeof(gchar *)*(g_slist_length(m->MacroEvents)+1)));
			gslTemp=m->MacroEvents;
			k=0;
			while(gslTemp!=NULL)
			{
				pszMacroStrings[k++]=MacroEventToString((MacroEvent*)(gslTemp->data));
				gslTemp=g_slist_next(gslTemp);
			}

			/* null terminate array for g_strfreev to work */
			pszMacroStrings[k]=NULL;
			/* now transfer to single string */
			pcTemp=g_strjoinv(",",pszMacroStrings);
			/* save data */
			cKey[0]='D';
			g_key_file_set_string(config,"Macros",cKey,pcTemp);
			/* free up memory */
			g_free(pcTemp);
			g_strfreev(pszMacroStrings);
			g_free(cKey);

			/* move to next macro */
			i++;
			gsl=g_slist_next(gsl);
		}
	}

	/* turn config into data */
	data=g_key_file_to_data(config,NULL,NULL);

	/* calculate setting directory name */
	config_file=g_build_filename(geany->app->configdir,"plugins","Geany_Macros",NULL);
	/* ensure directory exists */
	g_mkdir_with_parents(config_file,0755);

	/* make config_file hold name of settings file */
	SETPTR(config_file,g_build_filename(config_file,"settings.conf",NULL));

	/* write data */
	utils_write_file(config_file, data);

	/* free memory */
	g_free(config_file);
	g_key_file_free(config);
	g_free(data);

	/* Macros have now been saved */
	bMacrosHaveChanged=FALSE;
}


/* load settings (preferences, file data, and macro data) */
static void LoadSettings(void)
{
	gchar *pcTemp;
	gchar *pcKey;
	gint i,k;
	gchar *config_file=NULL;
	GKeyFile *config=NULL;
	Macro *m;
	gchar **pcMacroCommands;

	/* Make config_file hold directory name of settings file */
	config_file=g_build_filename(geany->app->configdir,"plugins","Geany_Macros",NULL);
	/* ensure directory exists */
	g_mkdir_with_parents(config_file,0755);

	/* make config_file hold name of settings file */
	SETPTR(config_file,g_build_filename(config_file,"settings.conf",NULL));

	/* either load settings file, or create one from default */
	config=g_key_file_new();
	if(!g_key_file_load_from_file(config,config_file, G_KEY_FILE_KEEP_COMMENTS,NULL))
		g_key_file_load_from_data(config,default_config,sizeof(default_config),
								G_KEY_FILE_KEEP_COMMENTS,NULL);

	/* extract settings */
	bQueryOverwriteMacros=utils_get_setting_boolean(config,"Settings",
	                                                "Question_Macro_Overwrite",FALSE);
	bSaveMacros=utils_get_setting_boolean(config,"Settings","Save_Macros",FALSE);

	/* extract macros */
	i=0;
	while(TRUE)
	{
		pcKey=g_strdup_printf("A%d",i);
		i++;
		/* get macro name */
		pcTemp=(gchar*)(utils_get_setting_string(config,"Macros",pcKey,NULL));
		/* if null then have reached end of macros */
		if(pcTemp==NULL)
		{
			g_free(pcKey);
			break;
		}

		m=CreateMacro();
		m->name=pcTemp;
		/* load triggers */
		pcKey[0]='B';
		m->keyval=utils_get_setting_integer(config,"Macros",pcKey,0);
		pcKey[0]='C';
		m->state=utils_get_setting_integer(config,"Macros",pcKey,0);
		/* Load macro list */
		pcKey[0]='D';
		pcTemp=(gchar*)(utils_get_setting_string(config,"Macros",pcKey,NULL));
		g_free(pcKey);
		/* break into individual macro data */
		pcMacroCommands=g_strsplit(pcTemp,",",0);
		/* can now free up pcTemp */
		g_free(pcTemp);
		/* now go through macro data generating macros */
		for(k=0,m->MacroEvents=NULL;pcMacroCommands[k]!=NULL;)
			m->MacroEvents=g_slist_prepend(m->MacroEvents,
			                               GetMacroEventFromString(pcMacroCommands,
		                                       &k));

		/* list created in reverse as more efficient, now turn it around */
		m->MacroEvents=g_slist_reverse(m->MacroEvents);
		/* macro now complete, add it to the list */
		AddMacroToList(m);
		/* free up memory used by pcMacroCommands */
		g_strfreev(pcMacroCommands);
	}

	/* free memory */
	g_free(config_file);
	g_key_file_free(config);
}


/* handle button presses in the preferences dialog box */
static void on_configure_response(GtkDialog *dialog, gint response, gpointer user_data)
{
	gboolean bSettingsChanged;
	GtkCheckButton *cb1,*cb2;

	if(response!=GTK_RESPONSE_OK && response!=GTK_RESPONSE_APPLY)
		return;

	/* retreive pointers to check boxes */
	cb1=(GtkCheckButton*)(g_object_get_data(G_OBJECT(dialog),"GeanyMacros_cb1"));
	cb2=(GtkCheckButton*)(g_object_get_data(G_OBJECT(dialog),"GeanyMacros_cb2"));

	/* first see if settings are going to change */
	bSettingsChanged=(bSaveMacros!=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb1)));
	bSettingsChanged|=(bQueryOverwriteMacros!=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb2)));

	/* set new settings settings */
	bSaveMacros=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb1));
	bQueryOverwriteMacros=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb2));

	/* now save new settings if they have changed */
	if(bSettingsChanged)
		SaveSettings();
}


/* return a widget containing settings for plugin that can be changed */
GtkWidget *plugin_configure(GtkDialog *dialog)
{
	GtkWidget *vbox;
	GtkWidget *cb1,*cb2;

	vbox=gtk_vbox_new(FALSE, 6);

	cb1=gtk_check_button_new_with_label(_("Save Macros when close Geany"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb1),bSaveMacros);
	gtk_box_pack_start(GTK_BOX(vbox),cb1,FALSE,FALSE,2);
	/* save pointer to check_button */
	g_object_set_data(G_OBJECT(dialog),"GeanyMacros_cb1",cb1);

	cb2=gtk_check_button_new_with_label(_("Ask before replacing existing Macros"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb2),bQueryOverwriteMacros);
	gtk_box_pack_start(GTK_BOX(vbox),cb2,FALSE,FALSE,2);
	/* save pointer to check_button */
	g_object_set_data(G_OBJECT(dialog),"GeanyMacros_cb2",cb2);

	gtk_widget_show_all(vbox);

	g_signal_connect(dialog,"response",G_CALLBACK(on_configure_response),NULL);

	return vbox;
}


/* display help box */
void plugin_help(void)
{
	GtkWidget *dialog,*label,*scroll;
	gchar *cText;

	/* create dialog box */
	dialog=gtk_dialog_new_with_buttons(_("Geany Macros help"),
		GTK_WINDOW(geany->main_widgets->window),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_STOCK_OK,GTK_RESPONSE_ACCEPT,
		NULL);

	/* setup help text */
	cText=g_strconcat(
_("This Plugin implements Macros in Geany.\n\n"),
_("This plugin allows you to record and use your own macros. "),
_("These are sequences of actions that can then be repeated with a single key combination. "),
_("So if you had dozens of lines where you wanted to delete the last 2 characters, you could simpl\
y start recording, press End, Backspace, Backspace, down line and then stop recording. "),
_("Then simply trigger the macro and it would automatically edit the line and move to the next. "),
_("Select Record Macro from the Tools menu and you will be prompted with a dialog box. "),
_("You need to specify a key combination that isn't being used, and a name for the macro to help y\
ou identify it. "),
_("Then press Record. "),
_("What you do in the editor is then recorded until you select Stop Recording Macro from the Tools\
 menu. "),
_("Simply pressing the specified key combination will re-run the macro. "),
_("To edit the macros you have, select Edit Macro from the Tools menu. "),
_("You can select a macro and delete it, or re-record it. "),
_("You can also click on a macro's name and change it, or the key combination and re-define that a\
ssuming that it's not already in use. "),
_("Selecting the edit option allows you to view all the individual elements that make up the macro\
. "),
_("You can select a different command for each element, move them, add new elements, delete element\
s, or if it's replace/insert, you can edit the text that replaces the selected text, or is inserte\
d.\n\n"),

_("The only thing to bear in mind is that undo and redo actions are not recorded, and won't be rep\
layed when the macro is re-run.\n\n"),

_("You can alter the default behaviour of this plugin by selecting Plugin Manager under the Tools \
menu, selecting this plugin, and clicking Preferences. "),
_("You can change:\n"),
_("Save Macros when close Geany - If this is selected then Geany will save any recorded macros and\
 reload them for use the next time you open Geany, if not they will be lost when Geany is closed.\
\n"),
_("Ask before replacing existing Macros - If this is selected then if you try recording a macro o\
ver an existing one it will check before over-writing it, giving you the option of trying a differ\
ent name or key trigger combination, otherwise it will simply erase any existing macros with the s\
ame name, or the same key trigger combination."),
NULL);

	/* create label */
	label=gtk_label_new(cText);
	gtk_label_set_line_wrap(GTK_LABEL(label),TRUE);
	gtk_widget_show(label);

	/* create scrolled window to display label */
	scroll=gtk_scrolled_window_new(NULL,NULL);
	gtk_scrolled_window_set_policy((GtkScrolledWindow*)scroll,GTK_POLICY_NEVER,
								GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport((GtkScrolledWindow*)scroll,label);

	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),scroll);
	gtk_widget_show(scroll);

	/* set dialog size (leave width default) */
	gtk_widget_set_size_request(dialog,-1,300);

	/* display the dialog */
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);

	/* free memory */
	g_free(cText);
}


/* handle key press
 * used to see if macro is being triggered and to control numbered bookmarks
*/
static gboolean Key_Released_CallBack(GtkWidget *widget, GdkEventKey *ev, gpointer data)
{
	Macro *m;

	m=FindMacroByKey(ev->keyval,ev->state);

	/* if it's a macro trigger then run macro */
	if(m!=NULL)
	{
		ReplayMacro(m);
/* ?is this needed */
/*    g_signal_stop_emission_by_name((GObject *)widget,"key-release-event"); */
		return TRUE;
	}

	return FALSE;
}


/* return TRUE if this key combination is not bing used by anything else (and so can be used by
 * this plugin as a macro trigger)
*/
static gboolean UseableAccel(guint key,guint mod)
{
	gint i;
	guint u,t;
	GSList *gsl;

	/* check if in use by accelerator groups */
	gsl=gtk_accel_groups_from_object((GObject*)geany->main_widgets->window);
	/* loop through all the accelerator groups until we either find one that matches the key (k!=0)
	 * or we don't (k==0)
	*/
	for(u=0;u<g_slist_length(gsl);u++)
	{
		gtk_accel_group_query((GtkAccelGroup*)((g_slist_nth(gsl,u))->data),key,mod,&t);
		if(t!=0)
			return FALSE; /* combination in use so don't accept as macro trigger */
	}

	/* now check to see if numbered bookmark key is atempted
	 *
	 * control and number pressed
	*/
	if(mod==4)
	{
		i=((gint)key)-'0';
		if(i>=0 && i<=9)
			return FALSE;
	}

	/* control+shift+number */
	if(mod==5)
	{
		/* could use hardware keycode instead of keyvals but if unable to get keyode then don't
		 * have a logical default to fall back on
		*/
		for(i=0;i<10;i++) if((gint)key==iShiftNumbers[i])
			return FALSE;
	}

	/* there are a lot of unmodified keys, and shift+key combinations in use by the editor.
	 * Rather than list them all individually that the editor uses, ban all except the
	 * shift+function keys
	*/
	if(mod==1 || mod==0)
	{
		if(key<GDK_F1 || key>GDK_F35)
			return FALSE;
	}

	/* there are a several control keys that could be pressed but should not be used on their own
	 * check for them
	*/
	if(key==GDK_Shift_L || key==GDK_Shift_R || key==GDK_Control_L || key==GDK_Control_R ||
		key==GDK_Caps_Lock || key==GDK_Shift_Lock || key==GDK_Meta_L || key==GDK_Meta_R ||
		key==GDK_Alt_L || key==GDK_Alt_R || key==GDK_Super_L || key==GDK_Super_R ||
		key==GDK_Hyper_L || key==GDK_Hyper_R)
		return FALSE;

	/* ctrl+M is used by normal bookmarks */
	if(mod==4 && key=='m')
		return FALSE;

	/* now should have a valid keyval/state combination */
	return TRUE;
}


/* handle changes to an Entry dialog so that it handles accelerator key combinations */
static gboolean Entry_Key_Pressed_CallBack(GtkWidget *widget, GdkEventKey *ev, gpointer data)
{
	gchar *cName;

	/* make sure that tab is handled by regular handler to ensure can tab through entry boxes and
	 * buttons
	*/
	if((ev->state==0 || ev->state==1) && ev->keyval==GDK_Tab)
		return FALSE;

	/* first see if key combination is valid for use in macros */
	if(UseableAccel(ev->keyval,ev->state)==FALSE)
		return TRUE;

	cName=GetPretyKeyName(ev->keyval,ev->state);
	/* set text in entry */
	gtk_entry_set_text((GtkEntry*)widget,cName);

	/* tidy up memory */
	g_free(cName);

	/* make note of keys pressed */
	RecordingMacro->keyval=ev->keyval;
	RecordingMacro->state=ev->state;

	return TRUE;
}


/* display dialog and handle the gathering of data prior to recording a macro */
static gboolean InitializeMacroRecord(void)
{
	GtkWidget *dialog,*gtke,*gtke2,*hbox,*gtkl;
	gint iReply=GTK_RESPONSE_OK;
	Macro *m;
	gboolean bReplaceName,bReplaceTrigger;

	/* ensure have empty recording macro */
	FreeMacro(RecordingMacro);
	RecordingMacro=CreateMacro();
	/* set with default values */
	RecordingMacro->keyval=0;
	RecordingMacro->state=0;

	/* create dialog box */
	dialog=gtk_dialog_new_with_buttons(_("Record Macro"),
		GTK_WINDOW(geany->main_widgets->window),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		_("Record"),GTK_RESPONSE_OK,
		_("Cancel"),GTK_RESPONSE_CANCEL,
		NULL);

	/* create box to hold macro trigger entry box and label */
	hbox=gtk_hbox_new(FALSE,0);
	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),hbox);
	gtk_widget_show(hbox);

	gtkl=gtk_label_new(_("Macro Trigger:"));
	gtk_box_pack_start(GTK_BOX(hbox),gtkl,FALSE,FALSE,2);
	gtk_widget_show(gtkl);

	gtke=gtk_entry_new();
	g_signal_connect(gtke,"key-press-event",G_CALLBACK(Entry_Key_Pressed_CallBack),NULL);
	gtk_box_pack_start(GTK_BOX(hbox),gtke,FALSE,FALSE,2);
	gtk_widget_show(gtke);

	/* create box to hold macro name entry box, and label */
	hbox=gtk_hbox_new(FALSE,0);
	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),hbox);
	gtk_widget_show(hbox);

	gtkl=gtk_label_new(_("Macro Name:"));
	gtk_box_pack_start(GTK_BOX(hbox),gtkl,FALSE,FALSE,2);
	gtk_widget_show(gtkl);

	gtke2=gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox),gtke2,FALSE,FALSE,2);
	gtk_widget_show(gtke2);

	/* run dialog, and wait for box to be canceled or for user to press record.
	 * Check to make sure you're not over-writing existing macro trigger or name
	*/
	while(iReply==GTK_RESPONSE_OK)
	{
		iReply=gtk_dialog_run(GTK_DIALOG(dialog));

		if(iReply==GTK_RESPONSE_OK)
		{
			/* first check if any trigger has been defined, and don't go further until we have a
			 * trigger
			*/
			if(RecordingMacro->keyval==0 && RecordingMacro->state==0)
			{
				dialogs_show_msgbox(GTK_MESSAGE_INFO,("You must define a key trigger combination"));
				continue;
			}

			bReplaceName=FALSE;
			bReplaceTrigger=FALSE;
			m=FindMacroByName((gchar*)gtk_entry_get_text((GtkEntry*)gtke2));
			/* check if macro name already exists */
			if(m!=NULL)
			{
				if(bQueryOverwriteMacros==FALSE)
					bReplaceName=TRUE;
				else
					bReplaceName=dialogs_show_question(
						_("Macro name \"%s\"\n is already in use.\nReplace?"),
						gtk_entry_get_text((GtkEntry*)gtke2));

				/* don't want to replace so loop back to allow user to change or cancel */
				if(bReplaceName==FALSE)
					continue;
			}

			/* check if trigger key combination is already used */
			m=FindMacroByKey(RecordingMacro->keyval,RecordingMacro->state);
			if(m!=NULL)
			{
				if(bQueryOverwriteMacros==FALSE)
					bReplaceTrigger=TRUE;
				else
					bReplaceTrigger=dialogs_show_question(
						_("Macro trigger \"%s\"\n is already in use.\nReplace?"),
						gtk_entry_get_text((GtkEntry*)gtke));

				/* don't want to replace so loop back to allow user to change or cancel */
				if(bReplaceTrigger==FALSE)
					continue;
			}

			/* remove old macros. By now will definately want to replace either */
			if(bReplaceName==TRUE)
			{
				m=FindMacroByName((gchar*)gtk_entry_get_text((GtkEntry*)gtke2));
				RemoveMacroFromList(m);
				FreeMacro(m);
			}

			if(bReplaceTrigger==TRUE)
			{
				m=FindMacroByKey(RecordingMacro->keyval,RecordingMacro->state);
				RemoveMacroFromList(m);
				FreeMacro(m);
			}

			/* record macro name, trigger keys already recorded */
			RecordingMacro->name=g_strdup((gchar*)gtk_entry_get_text((GtkEntry*)gtke2));
			/* break out of loop */
			break;
		}
	}

	/* tidy up */
	gtk_widget_destroy(dialog);

	/* clear macro details if not going to record a macro */
	if(iReply!=GTK_RESPONSE_OK)
		RecordingMacro=FreeMacro(RecordingMacro);

	return (iReply==GTK_RESPONSE_OK);
}


/* function to start the macro recording process */
static void StartRecordingMacro(void)
{
	/* start recording process, but quit if error, or user cancels */
	if(!InitializeMacroRecord())
		return;

	/* start actual recording */
	scintilla_send_message(document_get_current()->editor->sci,SCI_STARTRECORD,0,0);
	gtk_widget_hide(Record_Macro_menu_item);
	gtk_widget_show(Stop_Record_Macro_menu_item);
}


/* function to finish recording a macro */
static void StopRecordingMacro(void)
{
	scintilla_send_message(document_get_current()->editor->sci,SCI_STOPRECORD,0,0);
	/* Recorded in reverse as more efficient */
	RecordingMacro->MacroEvents=g_slist_reverse(RecordingMacro->MacroEvents);
	/* add macro to list */
	AddMacroToList(RecordingMacro);
	/* set ready to record new macro (don't free as macro has been saved in macrolist) */
	RecordingMacro=NULL;
	gtk_widget_show(Record_Macro_menu_item);
	gtk_widget_hide(Stop_Record_Macro_menu_item);

	/* Macros have been changed */
	bMacrosHaveChanged=TRUE;
}


/* check to see if we are recording a macro and stop it if we are */
static void on_document_close(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
	if(RecordingMacro!=NULL)
		StopRecordingMacro();
}


PluginCallback plugin_callbacks[] =
{
	{ "editor-notify", (GCallback) &Notification_Handler, FALSE, NULL },
	{ "document-close", (GCallback) &on_document_close, FALSE, NULL },
	{ NULL, NULL, FALSE, NULL }
};


/* handle starting and stopping macro recording */
static void DoMacroRecording(GtkMenuItem *menuitem, gpointer gdata)
{
	/* can't record if in an empty editor */
	if(!DocumentPresent())
		return;

	if(RecordingMacro==NULL)
		StartRecordingMacro();
	else
		StopRecordingMacro();
}


/* handle a change in a macro name in the edit macro dialog */
static void Name_Render_Edited_CallBack(GtkCellRendererText *cell,gchar *iter_id,gchar *new_text,
                                        gpointer data)
{
	GtkTreeView *treeview=(GtkTreeView *)data;
	GtkTreeModel *model;
	GtkTreeIter iter;
	Macro *m,*mTemp;
	GSList *gsl=mList;

	/* Get the iterator */
	model=gtk_tree_view_get_model(treeview);
	gtk_tree_model_get_iter_from_string(model,&iter,iter_id);

	/* get Macro for this line */
	gtk_tree_model_get(model,&iter,2,&m,-1);

	/* return if line is uneditable */
	if(m==NULL)
		return;

	/* now check that no other macro is using this name */
	while(gsl!=NULL)
	{
		mTemp=(Macro*)(gsl->data);
		if(mTemp!=m && strcmp(new_text,mTemp->name)==0)
			return;

		gsl=g_slist_next(gsl);
	}

	/* set new name */
	m->name=g_strdup(new_text);

	/* Update the model */
	gtk_list_store_set(GTK_LIST_STORE(model),&iter,0,new_text,-1);

	bMacrosHaveChanged=TRUE;
}


/* handle a change in macro trigger accelerator key in the edit macro dialog */
static void Accel_Render_Edited_CallBack(GtkCellRendererAccel *cell,gchar *iter_id,guint key,
                                         GdkModifierType mods,guint keycode,gpointer data)
{
	gchar *cTemp;
	GtkTreeView *treeview=(GtkTreeView *)data;
	GtkTreeModel *model;
	GtkTreeIter iter;
	Macro *m,*mTemp;
	GSList *gsl=mList;

	/* check if is useable accelerator */
	if(UseableAccel(key,mods)==FALSE)
		return;

	/* Get the iterator */
	model=gtk_tree_view_get_model(treeview);
	gtk_tree_model_get_iter_from_string(model,&iter,iter_id);

	/* get Macro for this line */
	gtk_tree_model_get(model,&iter,2,&m,-1);

	/* return if line is uneditable */
	if(m==NULL)
		return;

	/* now check that no other macro is using this key combination */
	while(gsl!=NULL)
	{
		mTemp=(Macro*)(gsl->data);
		if(mTemp!=m && mTemp->keyval==key && mTemp->state==mods)
			return;

		gsl=g_slist_next(gsl);
	}

	/* set new trigger values */
	m->keyval=key;
	m->state=mods;

	/* Update the model */
	cTemp=GetPretyKeyName(key,mods);
	gtk_list_store_set(GTK_LIST_STORE(model),&iter,1,cTemp,-1);
	g_free(cTemp);

	bMacrosHaveChanged=TRUE;
}


/* Get Search Description string with search text and flags at end*/
static gchar * GetSearchDescription(gint message,gchar *text,gint flags)
{
	return g_strdup_printf(_("Search %s, looking for %s%s%s.%s%s%s%s%s"),
		message==SCI_SEARCHNEXT?"forewards":"backwards",
		text==NULL?"":"\"",
		text==NULL?"clipboard contents":text,
		text==NULL?"":"\"",
		(flags&SCFIND_MATCHCASE)==SCFIND_MATCHCASE?" Match case.":"",
		(flags&SCFIND_WHOLEWORD)==SCFIND_WHOLEWORD?" Match whole word.":"",
		(flags&SCFIND_WORDSTART)==SCFIND_WORDSTART?" Match start of word.":"",
		(flags&SCFIND_REGEXP)==SCFIND_REGEXP?" Search by Regular Expression.":"",
		(flags&SCFIND_POSIX)==SCFIND_POSIX?" Regular Expression is POSIX.":"");
}


/* handle button presses in the preferences dialog box */
static void on_search_toggle(GtkToggleButton *cb,gpointer user_data)
{
	GtkEntry *gtke;
	GtkLabel *gtkl;
	gboolean bUseClipboard;

	/* retreive pointers to entry & label */
	gtke=(GtkEntry*)(g_object_get_data(G_OBJECT(cb),"GeanyMacros_e"));
	gtkl=(GtkLabel*)(g_object_get_data(G_OBJECT(cb),"GeanyMacros_l"));

	/* find out what we're searching for */
	bUseClipboard=gtk_toggle_button_get_active(cb);

	/* set entry & label depending on if we're looking for text or not */
	gtk_widget_set_sensitive((GtkWidget*)gtke,!bUseClipboard);
	gtk_widget_set_sensitive((GtkWidget*)gtkl,!bUseClipboard);
}


/* Handle editing of options for search */
static void EditSearchOptions(GtkTreeModel *model,GtkTreeIter *iter)
{
	GtkWidget *dialog,*gtke,*hbox,*gtkl;
	gchar *cTemp,*cData,*cText,*cTemp2;
	gint iReply=GTK_RESPONSE_OK,i;
	GtkWidget *vbox,*gtkcb;
	GtkWidget *cbA,*cbB,*cbC,*cbD,*cbE,*cbF;
	MacroDetailEntry *mde;
	gulong flags;

	/* get MacroDetail and data for this line */
	gtk_tree_model_get(model,iter,2,&mde,3,&cData,-1);

	/* make cText point to search text */
	cText=strchr(cData,',');
	cText++;

	/* get search flags */
	flags=strtoll(cData,NULL,10);

	/* create dialog box */
	dialog=gtk_dialog_new_with_buttons(_("Search Options:"),
	                                   GTK_WINDOW(geany->main_widgets->window),
	                                   GTK_DIALOG_DESTROY_WITH_PARENT,
	                                   _("_Ok"),GTK_RESPONSE_OK,
	                                   _("_Cancel"),GTK_RESPONSE_CANCEL,
	                                   NULL);

	/* create box to hold widgets */
	vbox=gtk_vbox_new(FALSE, 6);
	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),vbox);
	gtk_widget_show(vbox);

	/* create combobox to hold search direction */
	gtkcb=gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gtkcb),_("Search Forwards"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gtkcb),_("Search Backwards"));
	gtk_combo_box_set_active(GTK_COMBO_BOX(gtkcb),(mde->message==SCI_SEARCHNEXT)?0:1);
	gtk_box_pack_start(GTK_BOX(vbox),gtkcb,FALSE,FALSE,2);
	gtk_widget_show(gtkcb);

	/* create checkbox to check for search options */
	cbA=gtk_check_button_new_with_label(_("Search for contents of clipboard"));
	/* if search text is empty then to search for clipboard contents */
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cbA),(*cText)==0);
	gtk_box_pack_start(GTK_BOX(vbox),cbA,FALSE,FALSE,2);
	/* ensure we monitor for change in this button */
	g_signal_connect(cbA,"toggled",G_CALLBACK(on_search_toggle),dialog);
	gtk_widget_show(cbA);


	/* create box to hold search text entry box, and label */
	hbox=gtk_hbox_new(FALSE,0);
	gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,2);
	gtk_widget_show(hbox);

	gtkl=gtk_label_new(_("Search for:"));
	gtk_box_pack_start(GTK_BOX(hbox),gtkl,FALSE,FALSE,2);
	gtk_widget_show(gtkl);
	/* save pointer to label */
	g_object_set_data(G_OBJECT(cbA),"GeanyMacros_l",gtkl);
	gtk_widget_set_sensitive((GtkWidget*)gtkl,(*cText)!=0);

	gtke=gtk_entry_new();
	if((*cText)!=0)
		gtk_entry_set_text(GTK_ENTRY(gtke),cText);

	gtk_box_pack_start(GTK_BOX(hbox),gtke,FALSE,FALSE,2);
	gtk_widget_show(gtke);
	/* save pointer to entry */
	g_object_set_data(G_OBJECT(cbA),"GeanyMacros_e",gtke);
	gtk_widget_set_sensitive((GtkWidget*)gtke,(*cText)!=0);

	/* create checkbox to check for search options */
	cbB=gtk_check_button_new_with_label(_("Search is case sensitive"));
	/* if search text is empty then to search for clipboard contents */
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cbB),(flags&SCFIND_MATCHCASE)==SCFIND_MATCHCASE);
	gtk_box_pack_start(GTK_BOX(vbox),cbB,FALSE,FALSE,2);

	cbC=gtk_check_button_new_with_label(_("Search for whole word"));
	/* if search text is empty then to search for clipboard contents */
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cbC),(flags&SCFIND_WHOLEWORD)==SCFIND_WHOLEWORD);
	gtk_box_pack_start(GTK_BOX(vbox),cbC,FALSE,FALSE,2);

	cbD=gtk_check_button_new_with_label(_("Search for start of word"));
	/* if search text is empty then to search for clipboard contents */
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cbD),(flags&SCFIND_WORDSTART)==SCFIND_WORDSTART);
	gtk_box_pack_start(GTK_BOX(vbox),cbD,FALSE,FALSE,2);

	cbE=gtk_check_button_new_with_label(_("Search text is regular expression"));
	/* if search text is empty then to search for clipboard contents */
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cbE),(flags&SCFIND_REGEXP)==SCFIND_REGEXP);
	gtk_box_pack_start(GTK_BOX(vbox),cbE,FALSE,FALSE,2);

	cbF=gtk_check_button_new_with_label(_("Search text is POSIX compatible"));
	/* if search text is empty then to search for clipboard contents */
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cbF),(flags&SCFIND_POSIX)==SCFIND_POSIX);
	gtk_box_pack_start(GTK_BOX(vbox),cbF,FALSE,FALSE,2);

	gtk_widget_show_all(vbox);

	while(iReply==GTK_RESPONSE_OK)
	{
		iReply=gtk_dialog_run(GTK_DIALOG(dialog));

		if(iReply==GTK_RESPONSE_OK)
		{
			/* handle change in options */

			/* check search direction 0=foreward, 1=backwards */
			iReply=gtk_combo_box_get_active((GtkComboBox*)gtkcb);

			/* calculate macro detail of relavent detail */
			i=0;
			while(MacroDetails[i].message!=SCI_SEARCHNEXT) i++;
			mde=(MacroDetailEntry *)(&MacroDetails[i+iReply]);

			/* calculate flags */
			flags=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cbB))?SCFIND_MATCHCASE:0;
			flags|=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cbC))?SCFIND_WHOLEWORD:0;
			flags|=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cbD))?SCFIND_WORDSTART:0;
			flags|=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cbE))?SCFIND_REGEXP:0;
			flags|=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cbF))?SCFIND_POSIX:0;

			/* get search string or NULL if using clipboard */
			cText=(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cbA)))?
			           NULL:(gchar*)gtk_entry_get_text((GtkEntry*)(gtke));

			/* get new data */
			cData=g_strdup_printf("%lu,%s",flags,(cText==NULL)?"":cText);

			/* get new text */
			cText=GetSearchDescription(mde->message,cText,flags);

			/* get old data for this line */
			gtk_tree_model_get(model,iter,0,&cTemp,3,&cTemp2,-1);

			/* set text and macro detail */
			gtk_list_store_set(GTK_LIST_STORE(model),iter,0,cText,2,mde,3,cData,-1);

			/* free up old text */
			g_free(cTemp);
			g_free(cTemp2);

			/* break out of loop */
			break;
		}

	}

	/* tidy up */
	gtk_widget_destroy(dialog);
}


/* Handle editing of text for SCI_REPLACESEL */
static void EditSCIREPLACESELText(GtkTreeModel *model,GtkTreeIter *iter)
{
	GtkWidget *dialog,*gtke,*hbox,*gtkl;
	gchar *cTemp,*cTemp2;
	gint iReply=GTK_RESPONSE_OK;

	/* create dialog box */
	dialog=gtk_dialog_new_with_buttons(_("Edit Insert/Replace Text"),
	                                   GTK_WINDOW(geany->main_widgets->window),
	                                   GTK_DIALOG_DESTROY_WITH_PARENT,
	                                   _("_Ok"),GTK_RESPONSE_OK,
	                                   _("_Cancel"),GTK_RESPONSE_CANCEL,
	                                   NULL);

	/* create box to hold macro name entry box, and label */
	hbox=gtk_hbox_new(FALSE,0);
	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),hbox);
	gtk_widget_show(hbox);

	gtkl=gtk_label_new(_("Text:"));
	gtk_box_pack_start(GTK_BOX(hbox),gtkl,FALSE,FALSE,2);
	gtk_widget_show(gtkl);

	gtke=gtk_entry_new();
	gtk_tree_model_get(model,iter,3,&cTemp,-1);
	if(cTemp!=NULL)
		gtk_entry_set_text(GTK_ENTRY(gtke),cTemp);

	gtk_box_pack_start(GTK_BOX(hbox),gtke,FALSE,FALSE,2);
	gtk_widget_show(gtke);

	while(iReply==GTK_RESPONSE_OK)
	{
		iReply=gtk_dialog_run(GTK_DIALOG(dialog));

		if(iReply==GTK_RESPONSE_OK)
		{
			/* handle change in text */

			/* first free old text */
			gtk_tree_model_get(model,iter,3,&cTemp,-1);
			g_free(cTemp);

			/* get new text */
			cTemp=g_strdup((gchar*)gtk_entry_get_text((GtkEntry*)(gtke)));
			cTemp2=g_strdup_printf(_("Insert/replace with \"%s\""),cTemp);

			/* set text */
			gtk_list_store_set(GTK_LIST_STORE(model),iter,0,cTemp2,3,cTemp,-1);

			g_free(cTemp2);

			/* break out of loop */
			break;
		}

	}

	/* tidy up */
	gtk_widget_destroy(dialog);
}


/* handle change in macro event in macro editor */
static void combo_edited(GtkCellRendererText *cell,gchar *iter_id,gchar *new_text,gpointer data)
{
	GtkTreeView *treeview=(GtkTreeView *)data;
	GtkTreeModel *model;
	GtkTreeIter iter;
	MacroDetailEntry *mde;
	gint i;
	gchar *cTemp,*cTemp2;
	gboolean bNeedButtonUpdate=FALSE;

	/* find MacroDetails that has the setting of new setting */
	i=0;
	while(strcmp(_(MacroDetails[i].description),new_text)!=0)
		i++;

	/* Get the iterator for treeview*/
	model=gtk_tree_view_get_model(treeview);
	gtk_tree_model_get_iter_from_string(model,&iter,iter_id);

	/* get MacroDetail and any additional string for this line */
	gtk_tree_model_get(model,&iter,0,&cTemp2,2,&mde,3,&cTemp,-1);

	/* handle freeing of string if needed */
	g_free(cTemp);
	if(mde->message==SCI_REPLACESEL ||
	   mde->message==SCI_SEARCHNEXT ||
	   mde->message==SCI_SEARCHPREV)
	{
		g_free(cTemp2);
		bNeedButtonUpdate=TRUE;
	}

	/* see what text will have to change into */
	cTemp2=NULL;
	if(MacroDetails[i].message==SCI_REPLACESEL)
	{
		cTemp=g_strdup_printf(_("Insert/replace with \"\""));
		bNeedButtonUpdate=TRUE;
	}
	else if(MacroDetails[i].message==SCI_SEARCHNEXT ||
	   MacroDetails[i].message==SCI_SEARCHPREV)
	{
		cTemp=GetSearchDescription(MacroDetails[i].message,NULL,0);
		cTemp2=g_strdup("0,");
		bNeedButtonUpdate=TRUE;
	}
	else
		cTemp=g_strdup(_(MacroDetails[i].description));

	/* Update the model */
	gtk_list_store_set(GTK_LIST_STORE(model),&iter,0,cTemp,2,&(MacroDetails[i]),3,cTemp2,-1);

	g_free(cTemp);

	/* check if changing to or from SCI_REPLACESEL and enable/disable edit button as needed */
	if(bNeedButtonUpdate)
		g_signal_emit_by_name(gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview)),"changed",
		                      G_TYPE_NONE);
}


/* handle change in selection in DoEditMacro list */
static void DoEditMacroElementsSelectionChanged(GtkTreeSelection *selection,gpointer data)
{
	GtkTreeModel *model;
	GtkTreeIter iter,iter2;
	MacroDetailEntry *mde;
	GtkWidget *button;
	GtkDialog *dialog;
	GtkTreePath *tpTemp;

	dialog=GTK_DIALOG(data);

	/* check if have item selected & adjust buttons acordingly */
	if(gtk_tree_selection_get_selected(selection,&model,&iter))
	{
		/* get details about this node */
		gtk_tree_model_get(GTK_TREE_MODEL(model),&iter,2,&mde,-1);

		/* find delete button & enable it*/
		button=(GtkWidget*)(g_object_get_data(G_OBJECT(dialog),"GeanyMacros_bD"));
		gtk_widget_set_sensitive(button,TRUE);

		/* find edit text button & enable it if looking at a SCI_REPLACESEL item*/
		button=(GtkWidget*)(g_object_get_data(G_OBJECT(dialog),"GeanyMacros_bC"));
		gtk_widget_set_sensitive(button,mde->message==SCI_REPLACESEL ||
		                                mde->message==SCI_SEARCHNEXT ||
		                                mde->message==SCI_SEARCHPREV);

		/* get copy of iteraton */
		iter2=iter;
		/* if can move to next node then it's not the last. use to set Move down */
		button=(GtkWidget*)(g_object_get_data(G_OBJECT(dialog),"GeanyMacros_bB"));
		gtk_widget_set_sensitive(button,gtk_tree_model_iter_next(GTK_TREE_MODEL(model),&iter2));

		/* find Move up button & enable/disable it */
		button=(GtkWidget*)(g_object_get_data(G_OBJECT(dialog),"GeanyMacros_bA"));
		/* get the path of the current selected line */
		tpTemp=gtk_tree_model_get_path(GTK_TREE_MODEL(model),&iter);
		/* if has previous then can be moved up a line */
		gtk_widget_set_sensitive(button,gtk_tree_path_prev(tpTemp));
		/* free Tree path */
		gtk_tree_path_free(tpTemp);
	}
	/* no item selected */
	else
	{
		/* find delete button & diable it*/
		button=(GtkWidget*)(g_object_get_data(G_OBJECT(dialog),"GeanyMacros_bD"));
		gtk_widget_set_sensitive(button,FALSE);

		/* find edit text button & diable it*/
		button=(GtkWidget*)(g_object_get_data(G_OBJECT(dialog),"GeanyMacros_bC"));
		gtk_widget_set_sensitive(button,FALSE);

		/* find Move up button & diable it*/
		button=(GtkWidget*)(g_object_get_data(G_OBJECT(dialog),"GeanyMacros_bA"));
		gtk_widget_set_sensitive(button,FALSE);

		/* find Move Down button & diable it*/
		button=(GtkWidget*)(g_object_get_data(G_OBJECT(dialog),"GeanyMacros_bB"));
		gtk_widget_set_sensitive(button,FALSE);
	}

}


/* edit individual existing macro */
static void EditMacroElements(Macro *m)
{
	GtkWidget *table,*dialog,*button;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkTreeSelection *selection;
	GtkTreeIter iter,iterNew;
	GtkListStore *ls;
	GtkTreePath *gtkPath;
	gint i;
	GSList *gsl=m->MacroEvents;
	gchar *cTitle,*cTemp,*cTemp2;
	MacroEvent *me;
	GtkListStore *lsCombo;
	MacroDetailEntry *mde;
	gboolean bHaveIter;

	/* create dialog box */
	cTitle=g_strdup_printf(_("Edit: %s"),m->name);
	dialog=gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(dialog),cTitle);
	gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(geany->main_widgets->window));
	gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog),TRUE);

	/* create store to hold table data (2nd column holds number of macro value)
	*/
	ls=gtk_list_store_new(4,G_TYPE_STRING,G_TYPE_UINT,G_TYPE_POINTER,G_TYPE_POINTER);

	while(gsl!=NULL)
	{
 		me=(MacroEvent*)(gsl->data);
		i=0;
		while(MacroDetails[i].description!=NULL)
		{
			if(MacroDetails[i].message==me->message) break;
			i++;
		}

		gtk_list_store_append(ls,&iter);  /*  Acquire an iterator */
		/* set text, pointer to macro detail, and any ascociated string */
		cTemp2=NULL;
		if(me->message==SCI_REPLACESEL)
		{
			cTemp=g_strdup_printf(_("Insert/replace with \"%s\""),
			                      (gchar*)(me->lparam));
			cTemp2=g_strdup((gchar*)(me->lparam));
		}
		else if(MacroDetails[i].message==SCI_SEARCHNEXT ||
		        MacroDetails[i].message==SCI_SEARCHPREV)
		{
			cTemp=GetSearchDescription(MacroDetails[i].message,(gchar*)(me->lparam),
			                           me->wparam);
			cTemp2=g_strdup_printf("%lu,%s",me->wparam,((gchar*)(me->lparam)==NULL)?
			                       "":((gchar*)(me->lparam)));
		}
		else
			cTemp=g_strdup(_(MacroDetails[i].description));

		gtk_list_store_set(ls,&iter,0,cTemp,2,&(MacroDetails[i]),3,cTemp2,-1);
		gsl=g_slist_next(gsl);

		g_free(cTemp);
	}

	/* create list store for combo renderer */
	lsCombo=gtk_list_store_new(2,G_TYPE_STRING,G_TYPE_POINTER);
	i=0;
	while(MacroDetails[i].description!=NULL)
	{
		gtk_list_store_append(lsCombo,&iter);
		gtk_list_store_set(lsCombo,&iter,0,_(MacroDetails[i].description),1,
		                   &(MacroDetails[i]),-1);
		i++;
	}

	/* create combo renderer */
	renderer=gtk_cell_renderer_combo_new();

	/* add list store */
	g_object_set(renderer,"model",lsCombo,"editable",TRUE,"has-entry",FALSE,NULL);

	/* create table */
	table=gtk_tree_view_new_with_model(GTK_TREE_MODEL(ls));
	gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(table),GTK_TREE_VIEW_GRID_LINES_BOTH);

	/* add column */
	column=gtk_tree_view_column_new_with_attributes(_("Event"),renderer,"text",0,"text-column"
	                                                ,1,NULL);
	g_signal_connect(renderer,"edited",G_CALLBACK(combo_edited),table);
	gtk_tree_view_append_column(GTK_TREE_VIEW(table),column);

	/* set selection mode */
	gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(table)),
	                            GTK_SELECTION_SINGLE);

	/* add table to dialog */
	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),table);
/*	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),table,FALSE,FALSE,2);*/
	gtk_widget_show(table);

	/* add buttons */
	button=gtk_dialog_add_button(GTK_DIALOG(dialog),_("Move _Up"),GEANY_MACRO_BUTTON_UP);
	g_object_set_data(G_OBJECT(dialog),"GeanyMacros_bA",button);
	button=gtk_dialog_add_button(GTK_DIALOG(dialog),_("Move Do_wn"),GEANY_MACRO_BUTTON_DOWN);
	g_object_set_data(G_OBJECT(dialog),"GeanyMacros_bB",button);
	gtk_dialog_add_button(GTK_DIALOG(dialog),_("New _Above"),GEANY_MACRO_BUTTON_ABOVE);
	gtk_dialog_add_button(GTK_DIALOG(dialog),_("New _Below"),GEANY_MACRO_BUTTON_BELOW);
	button=gtk_dialog_add_button(GTK_DIALOG(dialog),_("_Edit"),GEANY_MACRO_BUTTON_EDIT);
	g_object_set_data(G_OBJECT(dialog),"GeanyMacros_bC",button);
	button=gtk_dialog_add_button(GTK_DIALOG(dialog),_("_Delete"),GEANY_MACRO_BUTTON_DELETE);
	g_object_set_data(G_OBJECT(dialog),"GeanyMacros_bD",button);
	gtk_dialog_add_button(GTK_DIALOG(dialog),_("_Ok"),GEANY_MACRO_BUTTON_APPLY);
	gtk_dialog_add_button(GTK_DIALOG(dialog),_("_Cancel"),GEANY_MACRO_BUTTON_CANCEL);

	/* listen for changes in selection */
	selection=gtk_tree_view_get_selection(GTK_TREE_VIEW(table));
	g_signal_connect(G_OBJECT(selection),"changed",
	                 G_CALLBACK(DoEditMacroElementsSelectionChanged),dialog);

	/* call callback: this will set buttons acordingly */
	DoEditMacroElementsSelectionChanged(selection,dialog);

	i=GTK_RESPONSE_REJECT;
	while(i!=GEANY_MACRO_BUTTON_CANCEL)
	{
		/* wait for button to be pressed */
		i=gtk_dialog_run(GTK_DIALOG(dialog));

		/* exit if cancel pressed */
		if(i==GEANY_MACRO_BUTTON_CANCEL)
			break;

		/* check if line has been selected */
		if(gtk_tree_selection_get_selected(selection,NULL,&iter))
		{
			/* handle delete element */
			if(i==GEANY_MACRO_BUTTON_DELETE)
			{
				/* see if need to free non-static string */
				gtk_tree_model_get(GTK_TREE_MODEL(ls),&iter,0,&cTemp,2,&mde,3,&cTemp2,-1);
				if(mde->message==SCI_REPLACESEL ||
				   mde->message==SCI_SEARCHNEXT ||
				   mde->message==SCI_SEARCHPREV)
				{
					g_free(cTemp);
					g_free(cTemp2);
				}

				/* remove element */
				gtk_list_store_remove(ls,&iter);

				/* call callback: this will update buttons acordingly */
				DoEditMacroElementsSelectionChanged(selection,dialog);
			}

			/* handle moving element up before element previous to it */
			if(i==GEANY_MACRO_BUTTON_UP)
			{
				/* get path of selected iteration */
				gtkPath=gtk_tree_model_get_path(GTK_TREE_MODEL(ls),&iter);

				/* move path to previous node & if sucessful try swapping */
				if(gtk_tree_path_prev(gtkPath)==TRUE)
				{
					gtk_tree_model_get_iter(GTK_TREE_MODEL(ls),&iterNew,gtkPath);
					gtk_list_store_swap(ls,&iter,&iterNew);
				}

				/* free path of selected iteration */
				gtk_tree_path_free(gtkPath);

				/* call callback: this will update buttons acordingly */
				DoEditMacroElementsSelectionChanged(selection,dialog);
			}

			/* handle move element to after element after it */
			if(i==GEANY_MACRO_BUTTON_DOWN)
			{
				/* create copy of curent position */
				iterNew=iter;

				/* if there is a node after the current node then swap */
				if(gtk_tree_model_iter_next(GTK_TREE_MODEL(ls),&iterNew)==TRUE)
					gtk_list_store_swap(ls,&iter,&iterNew);

				/* call callback: this will update buttons acordingly */
				DoEditMacroElementsSelectionChanged(selection,dialog);
			}

			/* handle insert new element before curent element */
			if(i==GEANY_MACRO_BUTTON_ABOVE)
			{
				gtk_list_store_append(ls,&iterNew);
				gtk_list_store_set(ls,&iterNew,0,_(MacroDetails[0].description),2,&(MacroDetails[0]),3,NULL,
				                   -1);
				gtk_list_store_move_before(ls,&iterNew,&iter);

				/* call callback: this will update buttons acordingly */
				DoEditMacroElementsSelectionChanged(selection,dialog);
			}

			/* handle insert new element after curent element */
			if(i==GEANY_MACRO_BUTTON_BELOW)
			{
				gtk_list_store_append(ls,&iterNew);
				gtk_list_store_set(ls,&iterNew,0,_(MacroDetails[0].description),2,&(MacroDetails[0]),3,NULL,
				                   -1);
				gtk_list_store_move_after(ls,&iterNew,&iter);

				/* call callback: this will update buttons acordingly */
				DoEditMacroElementsSelectionChanged(selection,dialog);
			}

			/* handle editing SCI_REPLACESEL text */
			if(i==GEANY_MACRO_BUTTON_EDIT)
			{
				gtk_tree_model_get(GTK_TREE_MODEL(ls),&iter,2,&mde,-1);
				if(mde->message==SCI_REPLACESEL)
					EditSCIREPLACESELText(GTK_TREE_MODEL(ls),&iter);
				else if(mde->message==SCI_SEARCHNEXT || mde->message==SCI_SEARCHPREV)
					EditSearchOptions(GTK_TREE_MODEL(ls),&iter);

			}

		} /* end of commands that require line to be selected */
		/* if no elements to insert above or below,just insert new element */
		else if((i==GEANY_MACRO_BUTTON_ABOVE || i==GEANY_MACRO_BUTTON_BELOW) &&
		        gtk_tree_model_iter_n_children(GTK_TREE_MODEL(ls),NULL)==0)
		{
			gtk_list_store_append(ls,&iterNew);
			gtk_list_store_set(ls,&iterNew,0,_(MacroDetails[0].description),2,&(MacroDetails[0]),3,NULL,-1);

			/* call callback: this will update buttons acordingly */
			DoEditMacroElementsSelectionChanged(selection,dialog);
		}


		/* user has hit apply button. check for changes & implement */
		if(i==GEANY_MACRO_BUTTON_APPLY)
		{
			/* clear old macro */
			m->MacroEvents=ClearMacroList(m->MacroEvents);

			/* go through list adding macro events */
			bHaveIter=gtk_tree_model_get_iter_first(GTK_TREE_MODEL(ls),&iter);
			while(bHaveIter)
			{
				/* get Macro event for this line */
				gtk_tree_model_get(GTK_TREE_MODEL(ls),&iter,2,&mde,3,&cTemp,-1);

				/* create new macro event */
				me=g_new0(MacroEvent,1);

				me->message=mde->message;
				me->lparam=0;
				me->wparam=0;

				/* Special handling for text inserting, duplicate inserted string */
				if(me->message==SCI_REPLACESEL)
					me->lparam=(sptr_t)((cTemp!=NULL)?g_strdup(cTemp):g_strdup(""));

				/* Special handling for search */
				if(me->message==SCI_SEARCHNEXT || me->message==SCI_SEARCHPREV)
				{
					cTemp2=strchr(cTemp,',');
					cTemp2++;

					me->lparam=(sptr_t)(((*cTemp2)==0)?NULL:g_strdup(cTemp2));
					me->wparam=strtoll(cTemp,NULL,10);
				}

				/* more efficient to create reverse list and reverse it at the end */
				m->MacroEvents=g_slist_prepend(m->MacroEvents,me);

				/* get next event */
				bHaveIter=gtk_tree_model_iter_next(GTK_TREE_MODEL(ls),&iter);
			}

			/* was more efficient to record in reverse direction, so now reverse */
			m->MacroEvents=g_slist_reverse(m->MacroEvents);

			break;
		}

	} /* end look responding to dialog buttons */

	/* clear any memory allocated for strings */
	bHaveIter=gtk_tree_model_get_iter_first(GTK_TREE_MODEL(ls),&iter);
	while(bHaveIter)
	{
		/* get Macro event for this line */
		gtk_tree_model_get(GTK_TREE_MODEL(ls),&iter,0,&cTemp2,2,&mde,3,&cTemp,-1);

		/* free any non-static text */
		g_free((void*)(cTemp));
		if(mde->message==SCI_REPLACESEL || mde->message==SCI_SEARCHNEXT ||
		   mde->message==SCI_SEARCHPREV)
			g_free(cTemp2);

		/* get next event */
		bHaveIter=gtk_tree_model_iter_next(GTK_TREE_MODEL(ls),&iter);
	}

	/* tidy up */
	gtk_widget_destroy(dialog);

	/* free memory */
	g_free(cTitle);
}


/* handle change in selection in DoEditMacro list */
static void DoEditMacroSelectionChanged(GtkTreeSelection *selection,gpointer data)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkWidget *button;
	GtkDialog *dialog;
	gboolean bHasItemSelected;

	dialog=GTK_DIALOG(data);

	/* find out if anything has been selected */
	bHasItemSelected=gtk_tree_selection_get_selected(selection,&model,&iter);

	/* now set button sensitive or not depending if there is something for them to act on */
	button=(GtkWidget*)(g_object_get_data(G_OBJECT(dialog),"GeanyMacros_bA"));
	gtk_widget_set_sensitive(button,bHasItemSelected);
	button=(GtkWidget*)(g_object_get_data(G_OBJECT(dialog),"GeanyMacros_bB"));
	gtk_widget_set_sensitive(button,bHasItemSelected);
	button=(GtkWidget*)(g_object_get_data(G_OBJECT(dialog),"GeanyMacros_bC"));
	gtk_widget_set_sensitive(button,bHasItemSelected);
}


/* do editing of existing macros */
static void DoEditMacro(GtkMenuItem *menuitem, gpointer gdata)
{
	GtkWidget *table,*dialog,*button;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GtkListStore *ls;
	gint i;
	GSList *gsl=mList;
	Macro *m;
	gchar *cTemp;

	/* create dialog box */
	dialog=gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(dialog),_("Edit Macros"));
	gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(geany->main_widgets->window));
	gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog),TRUE);

	/* create store to hold table data (3rd column holds pointer to macro or NULL if not editable)
	*/
	ls=gtk_list_store_new(3,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_POINTER);

	/* add data to store either empty line, or list of macros */
	if(gsl==NULL)
	{
		gtk_list_store_append(ls,&iter);  /* Acquire an iterator  */
		gtk_list_store_set(ls,&iter,0,"",1,"",2,NULL,-1);
	}

	while(gsl!=NULL)
	{
		gtk_list_store_append(ls,&iter);  /*  Acquire an iterator */
		m=(Macro*)(gsl->data);
		cTemp=GetPretyKeyName(m->keyval,m->state);
		gtk_list_store_set(ls,&iter,0,m->name,1,cTemp,2,m,-1);
		g_free(cTemp);
		gsl=g_slist_next(gsl);
	}

	/* create table */
	table=gtk_tree_view_new_with_model(GTK_TREE_MODEL(ls));
	gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(table),GTK_TREE_VIEW_GRID_LINES_BOTH);

	/* add columns */
	renderer=gtk_cell_renderer_text_new();
	column=gtk_tree_view_column_new_with_attributes(_("Macro Name"),renderer,"text",0,NULL);
	g_signal_connect(renderer,"edited",G_CALLBACK(Name_Render_Edited_CallBack),table);
	g_object_set(renderer,"editable",TRUE,NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(table),column);

	renderer= gtk_cell_renderer_accel_new();
	column=gtk_tree_view_column_new_with_attributes(_("Key Trigger"),renderer,"text",1,NULL);
	g_signal_connect(renderer,"accel-edited",G_CALLBACK(Accel_Render_Edited_CallBack),table);
	g_object_set(renderer,"editable",TRUE,NULL);

	gtk_tree_view_append_column(GTK_TREE_VIEW(table),column);

	/* set selection mode */
	gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(table)),
	                            GTK_SELECTION_SINGLE);

	/* add table to dialog */
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),table,FALSE,FALSE,2);
	gtk_widget_show(table);

	/* add buttons */
	button=gtk_dialog_add_button(GTK_DIALOG(dialog),_("_Re-Record"),GEANY_MACRO_BUTTON_RERECORD);
	g_object_set_data(G_OBJECT(dialog),"GeanyMacros_bA",button);
	button=gtk_dialog_add_button(GTK_DIALOG(dialog),_("_Edit"),GEANY_MACRO_BUTTON_EDIT);
	g_object_set_data(G_OBJECT(dialog),"GeanyMacros_bB",button);
	button=gtk_dialog_add_button(GTK_DIALOG(dialog),_("_Delete"),GEANY_MACRO_BUTTON_DELETE);
	g_object_set_data(G_OBJECT(dialog),"GeanyMacros_bC",button);
	gtk_dialog_add_button(GTK_DIALOG(dialog),_("_Ok"),GEANY_MACRO_BUTTON_CANCEL);

	/* listen for changes in selection */
	selection=gtk_tree_view_get_selection(GTK_TREE_VIEW(table));
	g_signal_connect(G_OBJECT(selection),"changed",G_CALLBACK(DoEditMacroSelectionChanged),dialog);

	/* call callback: this will set buttons acordingly */
	DoEditMacroSelectionChanged(selection,dialog);

	i=GTK_RESPONSE_REJECT;
	while(i==GEANY_MACRO_BUTTON_RERECORD || i==GEANY_MACRO_BUTTON_EDIT ||
	      i==GEANY_MACRO_BUTTON_DELETE || i==GTK_RESPONSE_REJECT)
	{
		/* wait for button to be pressed */
		i=gtk_dialog_run(GTK_DIALOG(dialog));

		/* exit if not doing any action */
		if(i!=GEANY_MACRO_BUTTON_RERECORD && i!=GEANY_MACRO_BUTTON_EDIT && i!=GEANY_MACRO_BUTTON_DELETE)
			break;

		/* check if line has been selected */
		if(gtk_tree_selection_get_selected(selection,NULL,&iter))
		{
			/* get macro name */
			gtk_tree_model_get(GTK_TREE_MODEL(ls),&iter,2,&m,-1);
			/* handle delete macro */
			if(i==GEANY_MACRO_BUTTON_DELETE && m)
			{
				/* remove from table */
				gtk_list_store_remove(GTK_LIST_STORE(ls),&iter);
				/* remove macro */
				RemoveMacroFromList(m);
				FreeMacro(m);
				/* Signal that macros have changed (and need to be saved) */
				bMacrosHaveChanged=TRUE;

				/* call callback: this will update buttons acordingly */
				DoEditMacroElementsSelectionChanged(selection,dialog);
			}

			/* handle re-record macro */
			if(i==GEANY_MACRO_BUTTON_RERECORD && m && DocumentPresent())
			{
				/* ensure have empty recording macro */
				FreeMacro(RecordingMacro);
				RecordingMacro=CreateMacro();
				/* set values */
				RecordingMacro->keyval=m->keyval;
				RecordingMacro->state=m->state;
				RecordingMacro->name=g_strdup(m->name);
				/* remove existing macro (so newly recorded one takes it's place) */
				RemoveMacroFromList(m);
				FreeMacro(m);
				/* start actual recording */
				scintilla_send_message(document_get_current()->editor->sci,SCI_STARTRECORD,0,0);
				gtk_widget_hide(Record_Macro_menu_item);
				gtk_widget_show(Stop_Record_Macro_menu_item);

				break;
			}

			/* handle edit macro */
			if(i==GEANY_MACRO_BUTTON_EDIT && m)
			{
				EditMacroElements(m);
				/* Signal that macros have changed (and need to be saved) */
				bMacrosHaveChanged=TRUE;
			}
		}

	}

	gtk_widget_destroy(dialog);
}


/* set up this plugin */
void plugin_init(GeanyData *data)
{
	gint i,k,iResults=0;
	GdkKeymapKey *gdkkmkResults;
	GdkKeymap *gdkKeyMap=gdk_keymap_get_default();

	/* Load settings */
	LoadSettings();

	/* Calculate what shift '0' to '9 will be (£ is above 3 on uk keyboard, but it's # or ~ on us
	 * keyboard.)
	 * there must be an easier way than this of working this out, but I've not figured it out.
	 * This is needed to play nicely with the Geany Numbered Bookmarks plugin
	*/

	/* go through '0' to '9', work out hardware keycode, then find out what shift+this keycode
	 * results in
	*/
	for(i=0;i<10;i++)
	{
		/* Get keymapkey data for number key */
		k=gdk_keymap_get_entries_for_keyval(gdkKeyMap,'0'+i,&gdkkmkResults,&iResults);
		/* error retrieving hardware keycode, so leave as standard uk character for shift + number */
		if(k==0)
			continue;

		/* unsure, just in case it does return 0 results but reserve memory  */
		if(iResults==0)
		{
			g_free(gdkkmkResults);
			continue;
		}

		/* now use k to indicate GdkKeymapKey we're after */
		k=0; /* default if only one hit found */
		if(iResults>1)
			/* cycle through results if more than one matches */
			for(k=0;k<iResults;k++)
				/* have found number without using shift, ctrl, Alt etc, so shold be it. */
				if(gdkkmkResults[k].level==0)
					break;

		/* error figuring out which keycode to use so default to standard uk */
		if(k==iResults)
		{
			g_free(gdkkmkResults);
			continue;
		}

		/* set shift pressed */
		gdkkmkResults[k].level=1;
		/* now get keycode for shift + number */
		iResults=gdk_keymap_lookup_key(gdkKeyMap,&(gdkkmkResults[k]));
		/* if valid keycode, enter into list of shift + numbers */
		if(iResults!=0)
			iShiftNumbers[i]=iResults;

		/* free resources */
		g_free(gdkkmkResults);
	}

	/* add record macro menu entry */
	Record_Macro_menu_item=gtk_menu_item_new_with_mnemonic(_("Record _Macro"));
	gtk_widget_show(Record_Macro_menu_item);
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu),Record_Macro_menu_item);
	g_signal_connect(Record_Macro_menu_item,"activate",G_CALLBACK(DoMacroRecording),NULL);

	/* add stop record macromenu entry */
	Stop_Record_Macro_menu_item=gtk_menu_item_new_with_mnemonic(_("Stop Recording _Macro"));
	gtk_widget_hide(Stop_Record_Macro_menu_item);
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu),Stop_Record_Macro_menu_item);
	g_signal_connect(Stop_Record_Macro_menu_item,"activate",G_CALLBACK(DoMacroRecording),NULL);

	/* add Edit Macro menu entry */
	Edit_Macro_menu_item=gtk_menu_item_new_with_mnemonic(_("_Edit Macros"));
	gtk_widget_show(Edit_Macro_menu_item);
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu),Edit_Macro_menu_item);
	g_signal_connect(Edit_Macro_menu_item,"activate",G_CALLBACK(DoEditMacro),NULL);

	/* set key press monitor handle */
	key_release_signal_id=g_signal_connect(geany->main_widgets->window,"key-release-event",
										G_CALLBACK(Key_Released_CallBack),NULL);
}


/* clean up on exiting this plugin */
void plugin_cleanup(void)
{
	/* if macros have changed then save off */
	if(bMacrosHaveChanged==TRUE && bSaveMacros==TRUE)
		SaveSettings();

	/* uncouple keypress monitor */
	g_signal_handler_disconnect(geany->main_widgets->window,key_release_signal_id);

	/* clear menu entries */
	gtk_widget_destroy(Record_Macro_menu_item);
	gtk_widget_destroy(Stop_Record_Macro_menu_item);
	gtk_widget_destroy(Edit_Macro_menu_item);

	/* Clear any macros that are recording */
	RecordingMacro=FreeMacro(RecordingMacro);

	/* clean up memory used by macros */
	ClearAllMacros();
}
