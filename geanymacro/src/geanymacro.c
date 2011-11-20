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
	/* I'm leaving wparam commented out as this may prove useful if it's used by a Scintilla
	 * message that's recorded in a macro that I'm not aware of yet
	*/
	/*	gulong wparam; */
	glong lparam;
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

GeanyPlugin     *geany_plugin;
GeanyData       *geany_data;
GeanyFunctions  *geany_functions;

PLUGIN_VERSION_CHECK(147)

PLUGIN_SET_INFO(_("Macros"),_("Macros for Geany"),
                "0.1","William Fraser <william.fraser@virgin.net>");

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

	while(gslTemp!=NULL)
	{
		me=gslTemp->data;
		/* check to see if it's a message that has string attached, and free it if so */
		if(me->message==SCI_REPLACESEL)
			g_free((void*)(me->lparam));

		g_free((void*)(gslTemp->data));
		gslTemp=g_slist_next(gslTemp);
	}

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

	scintilla_send_message(sci,SCI_BEGINUNDOACTION,0,0);

	while(gsl!=NULL)
	{
		me=gsl->data;
/* may be needed if come across any scintilla messages that use wparam */
/*        scintilla_send_message(sci,me->message,me->wparam,me->lparam); */
		scintilla_send_message(sci,me->message,0,me->lparam);
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

	/* now handle lparam if required */
	switch(me->message)
	{
		case SCI_REPLACESEL:
			/* get text */
			me->lparam=(glong)(g_strcompress(s[(*k)++]));
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


/* check editor notifications and remember editor events */
static gboolean Notification_Handler(GObject *obj, GeanyEditor *editor, SCNotification *nt,
                                     gpointer user_data)
{
  MacroEvent *me;

	/* ignore non macro recording messages */
	if(nt->nmhdr.code!=SCN_MACRORECORD)
		return FALSE;

	/* probably overkill as should not recieve SCN_MACRORECORD messages unless recording macros */
	if(RecordingMacro==NULL)
		return FALSE;

	/* check to see if it's a code we're happy to deal with */
	switch(nt->message)
	{
		case SCI_REPLACESEL:
		case SCI_CUT:
		case SCI_COPY:
		case SCI_PASTE:
		case SCI_CLEAR:
		case SCI_LINEDOWN:
		case SCI_LINEDOWNEXTEND:
		case SCI_LINEUP:
		case SCI_LINEUPEXTEND:
		case SCI_CHARLEFT:
		case SCI_CHARLEFTEXTEND:
		case SCI_CHARRIGHT:
		case SCI_CHARRIGHTEXTEND:
		case SCI_WORDLEFT:
		case SCI_WORDLEFTEXTEND:
		case SCI_WORDRIGHT:
		case SCI_WORDRIGHTEXTEND:
		case SCI_HOME:
		case SCI_HOMEEXTEND:
		case SCI_LINEEND:
		case SCI_LINEENDEXTEND:
		case SCI_DOCUMENTSTART:
		case SCI_DOCUMENTSTARTEXTEND:
		case SCI_DOCUMENTEND:
		case SCI_DOCUMENTENDEXTEND:
		case SCI_PAGEUP:
		case SCI_PAGEUPEXTEND:
		case SCI_PAGEDOWN:
		case SCI_PAGEDOWNEXTEND:
		case SCI_EDITTOGGLEOVERTYPE:
		case SCI_CANCEL:
		case SCI_DELETEBACK:
		case SCI_TAB:
		case SCI_BACKTAB:
		case SCI_NEWLINE:
		case SCI_FORMFEED:
		case SCI_VCHOME:
		case SCI_VCHOMEEXTEND:
		case SCI_ZOOMIN:
		case SCI_ZOOMOUT:
		case SCI_DELWORDLEFT:
		case SCI_DELWORDRIGHT:
		case SCI_LINECUT:
		case SCI_LINEDELETE:
		case SCI_LINETRANSPOSE:
		case SCI_LOWERCASE:
		case SCI_UPPERCASE:
		case SCI_LINESCROLLDOWN:
		case SCI_LINESCROLLUP:
		case SCI_DELETEBACKNOTLINE:
		case SCI_HOMEDISPLAY:
		case SCI_HOMEDISPLAYEXTEND:
		case SCI_LINEENDDISPLAY:
		case SCI_LINEENDDISPLAYEXTEND:
			break;
		default:
			dialogs_show_msgbox(GTK_MESSAGE_INFO,_("Unrecognised message\n%i %i %i"),nt->message,
			                    (gint)(nt->wParam),(gint)(nt->lParam));
			return FALSE;
	}
	me=g_new0(MacroEvent,1);
	me->message=nt->message;
/*    me->wparam=nt->wParam; */
	/* Special handling for text inserting, duplicate inserted string */
	me->lparam=(me->message==SCI_REPLACESEL)?((glong) g_strdup((gchar *)(nt->lParam))) : nt->lParam;

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
	gchar *cTemp;
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
	setptr(config_file,g_build_filename(config_file,"settings.conf",NULL));

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
	setptr(config_file,g_build_filename(config_file,"settings.conf",NULL));

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
			m->MacroEvents=g_slist_prepend(m->MacroEvents,GetMacroEventFromString(pcMacroCommands,
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


PluginCallback plugin_callbacks[] =
{
	{ "editor-notify", (GCallback) &Notification_Handler, FALSE, NULL },
	{ NULL, NULL, FALSE, NULL }
};


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

	cb2=gtk_check_button_new_with_label(_("Ask before replaceing existing Macros"));
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

	/* create dialog box */
  dialog=gtk_dialog_new_with_buttons(_("Geany Macros help"),
        GTK_WINDOW(geany->main_widgets->window),
        GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_STOCK_OK,GTK_RESPONSE_ACCEPT,
        NULL);

	/* create label */
	label=gtk_label_new(
		_("This Plugin implements Macros in Geany.\n\n"
		"This plugin allows you to record and use your own macros. These are sequences of \
actions that can then be repeated with a single key combination. So if you had dozens of lines \
where you wanted to delete the last 2 characters, you could simple start recording, press End, \
Backspace, Backspace, down line and then stop recording. Then simply trigger the macro and it \
would automatically edit the line and move to the next. Select Record Macro from the Tools menu \
and you will be prompted with a dialog box. You need to specify a key combination that isn't being\
 used, and a name for the macro to help you identify it. Then press Record. What you do in the \
editor is then recorded until you select Stop Recording Macro from the Tools menu. Simply pressing\
 the specified key combination will re-run the macro. To edit the macros you have select Edit \
Macro from the Tools menu. You can select a macro and delete it, or re-record it. You can also \
click on a macro's name and change it, or the key combination and re-define that assuming that it's\
 not already in use.\n\n"
		"You can alter the default behaviour of this plugin by selecting Plugin Manager under the \
Tools menu, selecting this plugin, and cliking Preferences. You can change:\nSave Macros when \
close Geany - If this is selected then Geany will save any recorded macros and reload them for use\
 the next time you open Geany, if not they will be lost when Geany is closed.\nAsk before \
replaceing existing Macros - If this is selected then if you try recording a macro over an \
existing one it will check before over-writing it, giving you the option of trying a different \
name or key trigger combination, otherwise it will simply erase any existing macros with the same \
name, or the same key trigger combination."));
	gtk_label_set_line_wrap(GTK_LABEL(label),TRUE);
	gtk_widget_show(label);

	/* create scrolled window to display label */
	scroll=gtk_scrolled_window_new(NULL,NULL);
	gtk_scrolled_window_set_policy((GtkScrolledWindow*)scroll,GTK_POLICY_NEVER,
								GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport((GtkScrolledWindow*)scroll,label);

	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox),scroll);
	gtk_widget_show(scroll);

	/* set dialog size (leave width default) */
	gtk_widget_set_size_request(dialog,-1,300);

	/* display the dialog */
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
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
	gint i,k;
	guint u,t;
	GSList *gsl;

	/* check if in use by accelerator groups */
	gsl=gtk_accel_groups_from_object((GObject*)geany->main_widgets->window);
	/* loop through all the accelerator groups until we either find one that matches the key (k!=0)
	 * or we don't (k==0)
	*/
	for(u=0,k=0;u<g_slist_length(gsl);u++)
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
	dialog=gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(dialog),_("Record Macro"));

	/* create buttons */
	gtk_dialog_add_button(GTK_DIALOG(dialog),_("Record"),GTK_RESPONSE_OK);
	gtk_dialog_add_button(GTK_DIALOG(dialog),_("Cancel"),GTK_RESPONSE_CANCEL);

	/* create box to hold macro trigger entry box and label */
	hbox=gtk_hbox_new(FALSE,0);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox),hbox);
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
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox),hbox);
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
				dialogs_show_msgbox(GTK_MESSAGE_INFO,
									_("You must define a key trigger combination"));
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


/* handle starting and stopping macro recording */
static void DoMacroRecording(GtkMenuItem *menuitem, gpointer gdata)
{
	if(RecordingMacro==NULL)
	{
		/* start recording process, but quit if error, or user cancels */
		if(!InitializeMacroRecord())
			return;

		/* start actual recording */
		scintilla_send_message(document_get_current()->editor->sci,SCI_STARTRECORD,0,0);
    gtk_widget_hide(Record_Macro_menu_item);
    gtk_widget_show(Stop_Record_Macro_menu_item);
	}
	else {
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

/* only allow render edit if GTK high enough version */
#if GTK_CHECK_VERSION(2,10,0)
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
#endif


/* do editing of existing macros */
static void DoEditMacro(GtkMenuItem *menuitem, gpointer gdata)
{
	GtkWidget *table,*dialog;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GtkListStore *ls;
	gint i;
	GSList *gsl=mList;
	Macro *m;
	gchar *cTemp;
	gboolean bEditable;

	/* create dialog box */
	dialog=gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(dialog),_("Edit Macros"));

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
/* only allow grid lines if GTK high enough version (cosmetic so no biggie if absent)*/
#if GTK_CHECK_VERSION(2,10,0)
	gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(table),GTK_TREE_VIEW_GRID_LINES_BOTH);
#endif

	/* add columns */
	renderer=gtk_cell_renderer_text_new();
	column=gtk_tree_view_column_new_with_attributes(_("Macro Name"),renderer,"text",0,NULL);
	g_signal_connect(renderer,"edited",G_CALLBACK(Name_Render_Edited_CallBack),table);
	g_object_set(renderer,"editable",TRUE,NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(table),column);

	renderer= gtk_cell_renderer_accel_new();
	column=gtk_tree_view_column_new_with_attributes(_("Key Trigger"),renderer,"text",1,NULL);
/* only allow render edit if GTK high enough version. Shame to loose this function, but not the
 * end of the world. I may time permitting write my own custom renderer
*/
#if GTK_CHECK_VERSION(2,10,0)
	g_signal_connect(renderer,"accel-edited",G_CALLBACK(Accel_Render_Edited_CallBack),table);
	g_object_set(renderer,"editable",TRUE,NULL);
#else
	g_object_set(renderer,"editable",FALSE,NULL);
#endif

	gtk_tree_view_append_column(GTK_TREE_VIEW(table),column);

	/* set selection mode */
	gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(table)),
								GTK_SELECTION_SINGLE);

	/* add table to dialog */
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox),table);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),table,FALSE,FALSE,2);
	gtk_widget_show(table);

	/* add buttons */
	gtk_dialog_add_button(GTK_DIALOG(dialog),_("Re-Record"),GTK_RESPONSE_OK);
	gtk_dialog_add_button(GTK_DIALOG(dialog),_("Delete"),GTK_RESPONSE_REJECT);
	gtk_dialog_add_button(GTK_DIALOG(dialog),_("Cancel"),GTK_RESPONSE_CANCEL);

	i=GTK_RESPONSE_REJECT;
	while(i==GTK_RESPONSE_REJECT)
	{
		/* wait for button to be pressed */
		i=gtk_dialog_run(GTK_DIALOG(dialog));

		/* exit if not doing any action */
		if(i!=GTK_RESPONSE_OK && i!=GTK_RESPONSE_REJECT)
			break;

		/* find out what's been selected */
		selection=gtk_tree_view_get_selection(GTK_TREE_VIEW(table));
		/* check if line has been selected */
		if(gtk_tree_selection_get_selected(selection,NULL,&iter))
		{
			/* get macro name */
			gtk_tree_model_get(GTK_TREE_MODEL(ls),&iter,0,&cTemp,2,&bEditable,-1);
			/* handle delete macro */
			if(i==GTK_RESPONSE_REJECT && bEditable)
			{
				/* remove from table */
				gtk_list_store_remove(GTK_LIST_STORE(ls),&iter);
				/* remove macro */
				m=FindMacroByName(cTemp);
				RemoveMacroFromList(m);
				FreeMacro(m);
				/* Signal that macros have changed (and need to be saved) */
				bMacrosHaveChanged=TRUE;
			}

			/* handle re-record macro */
			if(i==GTK_RESPONSE_OK && bEditable)
			{
				m=FindMacroByName(cTemp);
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
			}

			/* free memory */
			g_free(cTemp);
		}

	}

	gtk_widget_destroy(dialog);
}


/* set up this plugin */
void plugin_init(GeanyData *data)
{
	gint i,k,iResults=0;
	GdkKeymapKey *gdkkmkResults;

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
		k=gdk_keymap_get_entries_for_keyval(NULL,'0'+i,&gdkkmkResults,&iResults);
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
		iResults=gdk_keymap_lookup_key(NULL,&(gdkkmkResults[k]));
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
