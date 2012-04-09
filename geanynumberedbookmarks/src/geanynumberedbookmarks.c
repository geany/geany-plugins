/*
 * This code is supplied as is, and is used at your own risk.
 * The GNU GPL version 2 rules apply to this code (see http://fsf.org/>
 * You can alter it, and pass it on as you want.
 * If you alter it, or pass it on, the only restriction is that this disclamour and licence be
 * left intact
 *
 * william.fraser@virgin.net
 * 2010-11-01
*/


#include "geanyplugin.h"
#include "utils.h"
#include "Scintilla.h"
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

static const gint base64_char_to_int[]=
{
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255, 62,255,255,255, 63,
   52, 53, 54, 55, 56, 57, 58, 59, 60, 61,255,255,255,  0,255,255,
  255,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
   15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,255,255,255,255,255,
  255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
   41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,255,255,255,255,255
};

static const gchar base64_int_to_char[]=
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* offset for marker numbers used - to bypass markers used by normal bookmarksetc */
#define BOOKMARK_BASE 10

/* define structures used in this plugin */
typedef struct FileData
{
	gchar *pcFileName;    /* holds filename */
	gint iBookmark[10];   /* holds bookmark lines or -1 for not set */
	gint iBookmarkLinePos[10]; /* holds position of cursor in line */
	gchar *pcFolding;     /* holds which folds are open and which not */
	gint LastChangedTime; /* time file was last changed by this editor */
	struct FileData * NextNode;
} FileData;

/* structure to hold list of Scintilla objects that have had icons set for bookmarks */
typedef struct SCIPOINTERHOLDER
{
	ScintillaObject* sci;
	struct SCIPOINTERHOLDER * NextNode;
} SCIPOINTERHOLDER;

GeanyPlugin     *geany_plugin;
GeanyData       *geany_data;
GeanyFunctions  *geany_functions;

PLUGIN_VERSION_CHECK(147)

PLUGIN_SET_INFO("Numbered Bookmarks",_("Numbered Bookmarks for Geany"),
                "0.1","William Fraser <william.fraser@virgin.net>");

/* Plugin user alterable settings */
static gboolean bCenterWhenGotoBookmark=TRUE;
static gboolean bRememberFolds=TRUE;
static gint PositionInLine=0;

/* internal variables */
static gint iShiftNumbers[]={41,33,34,163,36,37,94,38,42,40};
static SCIPOINTERHOLDER *sciList=NULL;
static FileData *fdKnownFilesSettings=NULL;
static gulong key_release_signal_id;

/* default config file */
const gchar default_config[] =
	"[Settings]\n"
	"Center_When_Goto_Bookmark = true\n"
	"Remember_Folds = true\n"
	"Position_In_Line = 0\n"
	"[FileData]";

/* Definitions for bookmark images */
static const gchar * aszMarkerImage0[] =
{
	"17 14 3 1", /* width height colours characters-per-pixel */
	". c None",
	"B c #000000",
	"* c #FFFF00",
	"...BBBBBBBBBB....",
	"..B**********B...",
	".B****BBBB****B..",
	"B****BB**BB****B.",
	"B****B****B****B.",
	"B****B****B****B.",
	"B****B*BB*B****B.",
	"B****B****B****B.",
	"B****B****B****B.",
	"B****B****B****B.",
	"B****BB**BB****B.",
	".B****BBBB****B..",
	"..B**********B...",
	"...BBBBBBBBBB...."
};
static const gchar * aszMarkerImage1[] =
{
	"17 14 3 1", /* width height colours characters-per-pixel */
	". c None",
	"B c #000000",
	"* c #FFFF00",
	"...BBBBBBBBBB....",
	"..B**********B...",
	".B*****BB*****B..",
	"B*****BBB******B.",
	"B*******B******B.",
	"B*******B******B.",
	"B*******B******B.",
	"B*******B******B.",
	"B*******B******B.",
	"B*******B******B.",
	"B*******B******B.",
	".B****BBBBB***B..",
	"..B**********B...",
	"...BBBBBBBBBB...."
};
static const gchar * aszMarkerImage2[] =
{
	"17 14 3 1", /* width height colours characters-per-pixel */
	". c None",
	"B c #000000",
	"* c #FFFF00",
	"...BBBBBBBBBB....",
	"..B**********B...",
	".B****BBBB****B..",
	"B****BB**BB****B.",
	"B*********B****B.",
	"B*********B****B.",
	"B********BB****B.",
	"B*******BB*****B.",
	"B******BB******B.",
	"B*****BB*******B.",
	"B****BB********B.",
	".B***BBBBBB***B..",
	"..B**********B...",
	"...BBBBBBBBBB...."
};
static const gchar * aszMarkerImage3[] =
{
	"17 14 3 1", /* width height colours characters-per-pixel */
	". c None",
	"B c #000000",
	"* c #FFFF00",
	"...BBBBBBBBBB....",
	"..B**********B...",
	".B****BBBB****B..",
	"B****BB**BB****B.",
	"B*********B****B.",
	"B********BB****B.",
	"B*****BBBB*****B.",
	"B********BB****B.",
	"B*********B****B.",
	"B*********B****B.",
	"B****BB**BB****B.",
	".B****BBBB****B..",
	"..B**********B...",
	"...BBBBBBBBBB...."
};
static const gchar * aszMarkerImage4[] =
{
	"17 14 3 1", /* width height colours characters-per-pixel */
	". c None",
	"B c #000000",
	"* c #FFFF00",
	"...BBBBBBBBBB....",
	"..B**********B...",
	".B******BB****B..",
	"B*******BB*****B.",
	"B******B*B*****B.",
	"B******B*B*****B.",
	"B*****B**B*****B.",
	"B*****B**B*****B.",
	"B****BBBBBB****B.",
	"B********B*****B.",
	"B********B*****B.",
	".B*******B****B..",
	"..B**********B...",
	"...BBBBBBBBBB...."
};
static const gchar * aszMarkerImage5[] =
{
	"17 14 3 1", /* width height colours characters-per-pixel */
	". c None",
	"B c #000000",
	"* c #FFFF00",
	"...BBBBBBBBBB....",
	"..B**********B...",
	".B***BBBBBB***B..",
	"B****B*********B.",
	"B****B*********B.",
	"B****B*********B.",
	"B****BBBBB*****B.",
	"B********BB****B.",
	"B*********B****B.",
	"B*********B****B.",
	"B****BB**BB****B.",
	".B****BBBB****B..",
	"..B**********B...",
	"...BBBBBBBBBB...."
};
static const gchar * aszMarkerImage6[] =
{
	"17 14 3 1", /* width height colours characters-per-pixel */
	". c None",
	"B c #000000",
	"* c #FFFF00",
	"...BBBBBBBBBB....",
	"..B**********B...",
	".B*****BBBB***B..",
	"B*****BB*******B.",
	"B****BB********B.",
	"B****B*********B.",
	"B****BBBBB*****B.",
	"B****BB**BB****B.",
	"B****B****B****B.",
	"B****B****B****B.",
	"B****BB**BB****B.",
	".B****BBBB****B..",
	"..B**********B...",
	"...BBBBBBBBBB...."
};
static const gchar * aszMarkerImage7[] =
{
	"17 14 3 1", /* width height colours characters-per-pixel */
	". c None",
	"B c #000000",
	"* c #FFFF00",
	"...BBBBBBBBBB....",
	"..B**********B...",
	".B***BBBBBB***B..",
	"B*********B****B.",
	"B********B*****B.",
	"B*******B******B.",
	"B*******B******B.",
	"B******B*******B.",
	"B*****B********B.",
	"B*****B********B.",
	"B****B*********B.",
	".B***B********B..",
	"..B**********B...",
	"...BBBBBBBBBB...."
};
static const gchar * aszMarkerImage8[] =
{
	"17 14 3 1", /* width height colours characters-per-pixel */
	". c None",
	"B c #000000",
	"* c #FFFF00",
	"...BBBBBBBBBB....",
	"..B**********B...",
	".B****BBBB****B..",
	"B****BB**BB****B.",
	"B****B****B****B.",
	"B****BB**BB****B.",
	"B*****BBBB*****B.",
	"B****BB**BB****B.",
	"B****B****B****B.",
	"B****B****B****B.",
	"B****BB**BB****B.",
	".B****BBBB****B..",
	"..B**********B...",
	"...BBBBBBBBBB...."
};
static const gchar * aszMarkerImage9[] =
{
	"17 14 3 1", /* width height colours characters-per-pixel */
	". c None",
	"B c #000000",
	"* c #FFFF00",
	"...BBBBBBBBBB....",
	"..B**********B...",
	".B****BBBB****B..",
	"B****BB**BB****B.",
	"B****B****B****B.",
	"B****BB**BB****B.",
	"B*****BBBBB****B.",
	"B*********B****B.",
	"B*********B****B.",
	"B********BB****B.",
	"B*******BB*****B.",
	".B***BBBB*****B..",
	"..B**********B...",
	"...BBBBBBBBBB...."
};

static const gchar ** aszMarkerImages[]=
{
	aszMarkerImage0,aszMarkerImage1,aszMarkerImage2,aszMarkerImage3,aszMarkerImage4,
	aszMarkerImage5,aszMarkerImage6,aszMarkerImage7,aszMarkerImage8,aszMarkerImage9
};


/* return a FileData structure for a file
 * if not come across this file before then create one, otherwise return existing structure with
 * data in it
 * returns NULL on error
*/
static FileData * GetFileData(gchar *pcFileName)
{
	FileData *fdTemp=fdKnownFilesSettings;
	gint i;

	/* First handle if main pointer doesn't point to anything */
	if(fdTemp==NULL)
	{
		if((fdKnownFilesSettings=(FileData*)(g_malloc(sizeof *fdTemp)))!=NULL)
		{
			fdKnownFilesSettings->pcFileName=g_strdup(pcFileName);
			for(i=0;i<10;i++)
				fdKnownFilesSettings->iBookmark[i]=-1;

			/* don't need to initiate iBookmarkLinePos */
			fdKnownFilesSettings->pcFolding=NULL;
			fdKnownFilesSettings->LastChangedTime=-1;
			fdKnownFilesSettings->NextNode=NULL;
		}
		return fdKnownFilesSettings;
	}

	/* move through chain to the correct entry or the end of a chain */
	while(TRUE)
	{
		/* if have found relavent FileData, then exit */
		if(utils_str_equal(pcFileName,fdTemp->pcFileName)==TRUE)
			return fdTemp;

		/* if end of chain, then add new entry, and return it. */
		if(fdTemp->NextNode==NULL)
		{
			if((fdTemp->NextNode=(FileData*)(g_malloc(sizeof *fdTemp)))!=NULL)
			{
				fdTemp->NextNode->pcFileName=g_strdup(pcFileName);
				for(i=0;i<10;i++)
					fdTemp->NextNode->iBookmark[i]=-1;

				/* don't need to initiate iBookmarkLinePos */
				fdTemp->NextNode->pcFolding=NULL;
				fdTemp->NextNode->LastChangedTime=-1;
				fdTemp->NextNode->NextNode=NULL;
			}
			return fdTemp->NextNode;

		}
		fdTemp=fdTemp->NextNode;

	}
}


/* save settings (preferences, file data such as fold states, marker positions) */
static void SaveSettings(void)
{
	GKeyFile *config = NULL;
	gchar *config_file = NULL;
	gchar *data;
	FileData* fdTemp=fdKnownFilesSettings;
	gchar *cKey;
	gchar szMarkers[1000];
	gchar *pszMarkers;
	gint i,iFiles=0;

	/* create new config from default settings */
	config=g_key_file_new();

	/* now set settings */
	g_key_file_set_boolean(config,"Settings","Center_When_Goto_Bookmark",bCenterWhenGotoBookmark);
	g_key_file_set_boolean(config,"Settings","Remember_Folds",bRememberFolds);
	g_key_file_set_integer(config,"Settings","Position_In_Line",PositionInLine);

	/* now save file data */
	while(fdTemp!=NULL)
	{
		cKey=g_strdup_printf("A%d",iFiles);
		/* save filename */
		g_key_file_set_string(config,"FileData",cKey,fdTemp->pcFileName);
		/* save folding data */
		cKey[0]='B';
		if(NZV(fdTemp->pcFolding))
			g_key_file_set_string(config,"FileData",cKey,fdTemp->pcFolding);

		/* save last saved time */
		cKey[0]='C';
		g_key_file_set_integer(config,"FileData",cKey,fdTemp->LastChangedTime);
		/* save bookmarks */
		cKey[0]='D';
		pszMarkers=szMarkers;
		pszMarkers[0]=0;
		for(i=0;i<10;i++)
		{
			if(fdTemp->iBookmark[i]!=-1)
			{
				sprintf(pszMarkers,"%d",fdTemp->iBookmark[i]);
				while(pszMarkers[0]!=0)
					pszMarkers++;
			}

			pszMarkers[0]=',';
			pszMarkers[1]=0;
			pszMarkers++;
		}
		/* don't need a ',' after last position (have '\0' instead) */
		pszMarkers--;
		pszMarkers[0]=0;
		g_key_file_set_string(config,"FileData",cKey,szMarkers);

		/* save positions in bookmarked lines */
		cKey[0]='E';
		pszMarkers=szMarkers;
		pszMarkers[0]=0;
		for(i=0;i<10;i++)
		{
			if(fdTemp->iBookmark[i]!=-1)
			{
				sprintf(pszMarkers,"%d",fdTemp->iBookmarkLinePos[i]);
				while(pszMarkers[0]!=0)
					pszMarkers++;
			}

			pszMarkers[0]=',';
			pszMarkers[1]=0;
			pszMarkers++;
		}
		/* don't need a ',' after last position (have '\0' instead) */
		pszMarkers--;
		pszMarkers[0]=0;
		g_key_file_set_string(config,"FileData",cKey,szMarkers);

		g_free(cKey);

		/* point to next FileData entry or NULL if end of chain */
		iFiles++;
		fdTemp=fdTemp->NextNode;
	}

	/* turn config into data */
	data=g_key_file_to_data(config,NULL,NULL);

	/* calculate setting directory name */
	config_file=g_build_filename(geany->app->configdir,"plugins","Geany_Numbered_Bookmarks",NULL);
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
}


/* load settings (preferences, file data, and macro data) */
static void LoadSettings(void)
{
	gchar *pcTemp;
	gchar *pcTemp2;
	gchar *pcKey;
	gint i,l;
	gchar *config_file=NULL;
	GKeyFile *config=NULL;
	FileData *fdTemp;

	/* Make config_file hold directory name of settings file */
	config_file=g_build_filename(geany->app->configdir,"plugins","Geany_Numbered_Bookmarks",NULL);
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
	bCenterWhenGotoBookmark=utils_get_setting_boolean(config,"Settings",
	                        "Center_When_Goto_Bookmark",FALSE);
	bRememberFolds=utils_get_setting_boolean(config,"Settings","Remember_Folds",FALSE);
	PositionInLine=utils_get_setting_integer(config,"Settings","Position_In_Line",0);

	/* extract data about files */
	i=0;
	while(TRUE)
	{
		pcKey=g_strdup_printf("A%d",i);
		i++;
		/* get filename */
		pcTemp=(gchar*)(utils_get_setting_string(config,"FileData",pcKey,NULL));
		/* if null then have reached end of files */
		if(pcTemp==NULL)
		{
			g_free(pcKey);
			break;
		}

		fdTemp=GetFileData(pcTemp);
		/* get folding data */
		pcKey[0]='B';
		fdTemp->pcFolding=(gchar*)(utils_get_setting_string(config,"FileData",pcKey,NULL));
		/* load last saved time */
		pcKey[0]='C';
		fdTemp->LastChangedTime=utils_get_setting_integer(config,"FileData",pcKey,-1);
		/* get bookmarks */
		pcKey[0]='D';
		pcTemp=(gchar*)(utils_get_setting_string(config,"FileData",pcKey,NULL));
		/* pcTemp contains comma seperated numbers (or blank for -1) */
		pcTemp2=pcTemp;
		if(pcTemp!=NULL) for(l=0;l<10;l++)
		{
			/* Bookmark entries are initialized to -1, so only need to parse non-empty slots */
			if(pcTemp2[0]!=',' && pcTemp2[0]!=0)
			{
				fdTemp->iBookmark[l]=strtoll(pcTemp2,NULL,10);
				while(pcTemp2[0]!=0 && pcTemp2[0]!=',')
					pcTemp2++;
			}

			pcTemp2++;
		}
		g_free(pcTemp);

		/* get position in bookmarked lines */
		pcKey[0]='E';
		pcTemp=(gchar*)(utils_get_setting_string(config,"FileData",pcKey,NULL));
		g_free(pcKey);
		/* pcTemp contains comma seperated numbers (or blank for -1) */
		pcTemp2=pcTemp;
		if(pcTemp!=NULL) for(l=0;l<10;l++)
		{
			/* Bookmark entries are initialized to -1, so only need to parse non-empty slots */
			if(pcTemp2[0]!=',' && pcTemp2[0]!=0)
			{
				fdTemp->iBookmarkLinePos[l]=strtoll(pcTemp2,NULL,10);
				while(pcTemp2[0]!=0 && pcTemp2[0]!=',')
					pcTemp2++;
			}

			pcTemp2++;
		}

		g_free(pcTemp);
	}

	/* free memory */
	g_free(config_file);
	g_key_file_free(config);
}


/* Define Markers for an editor */
static void DefineMarkers(ScintillaObject* sci)
{
	gint i;
	for(i=0;i<10;i++)
		scintilla_send_message(sci,SCI_MARKERDEFINEPIXMAP,i+BOOKMARK_BASE,
		                       (glong)(aszMarkerImages[i]));
}


/* Make sure that have setup markers if not come across this particular editor
 * Keep track of editors we've encountered and set markers for
*/
static void CheckEditorSetup(void)
{
	SCIPOINTERHOLDER *sciTemp;
	ScintillaObject* sci=document_get_current()->editor->sci;

	/* no recorded editors so make note of editor */
	if(sciList==NULL)
	{
		if((sciList=(SCIPOINTERHOLDER *)(g_malloc(sizeof *sciTemp)))!=NULL)
		{
			sciList->sci=sci;
			sciList->NextNode=NULL;
			DefineMarkers(sci);
		}

		return;
	}

	sciTemp=sciList;
	while(sciTemp->NextNode!=NULL)
	{
		/* if have come across this editor before, then it will have had it's markers set and we
		 * don't need to do any more
		*/
		if(sciTemp->sci==sci)
			return;

		sciTemp=sciTemp->NextNode;
	}
	/* if have come across this editor before, then it will have had it's markers set and we don't
	 * need to do any more
	*/
	if(sciTemp->sci==sci)
		return;

	/* not come across this editor */
	if((sciTemp->NextNode=g_malloc(sizeof *sciTemp->NextNode))!=NULL)
	{
		sciTemp->NextNode->sci=sci;
		sciTemp->NextNode->NextNode=NULL;
		DefineMarkers(sci);
	}
}


/* handler for when a document has been opened
 * this checks to see if a document has been altered since it was last saved in geany (as plugin
 * data may then be out of date for file)
 * It then applies file settings
*/
static void on_document_open(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
	FileData *fd;
	gint i,l=GTK_RESPONSE_ACCEPT,iLineCount;
	ScintillaObject* sci=doc->editor->sci;
	struct stat sBuf;
	GtkWidget *dialog;
	gchar *cFoldData=NULL;
	/* keep compiler happy & initialise iBits: will logically be initiated anyway */
	gint iBits=0,iFlags,iBitCounter;

	/* ensure have markers set */
	CheckEditorSetup();
	DefineMarkers(sci);

	/* check to see if file has changed since geany last saved it */
	fd=GetFileData(doc->file_name);
	if(stat(doc->file_name,&sBuf)==0 && fd!=NULL && fd->LastChangedTime!=-1 &&
	   fd->LastChangedTime!=sBuf.st_mtime)
	{
		/* notify user that file has been changed */
		dialog=gtk_message_dialog_new(GTK_WINDOW(geany->main_widgets->window),
		                              GTK_DIALOG_DESTROY_WITH_PARENT,GTK_MESSAGE_ERROR,
		                              GTK_BUTTONS_NONE,
		                              _("'%s' has been edited since it was last saved by g\
		                                eany. Marker positions may be unreliable and will \
		                                not be loaded.\nPress Ignore to try an load marker\
		                                s anyway."),doc->file_name);
		gtk_dialog_add_button(GTK_DIALOG(dialog),_("_Okay"),GTK_RESPONSE_OK);
		gtk_dialog_add_button(GTK_DIALOG(dialog),_("_Ignore"),GTK_RESPONSE_REJECT);
		l=gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
	}

	switch(l)
	{
		/* file not changed since Geany last saved it so saved settings should be fine */
		case GTK_RESPONSE_ACCEPT:
			/* now set markers */
			for(i=0;i<10;i++)
				if(fd->iBookmark[i]!=-1)
					scintilla_send_message(sci,SCI_MARKERADD,fd->iBookmark[i],i+BOOKMARK_BASE);

			/* get fold settings if present and want to use them */
			if(fd->pcFolding==NULL || bRememberFolds==FALSE)
				break;

			cFoldData=fd->pcFolding;

			/* first ensure fold positions exist */
			scintilla_send_message(sci,SCI_COLOURISE,0,-1);

			iLineCount=scintilla_send_message(sci,SCI_GETLINECOUNT,0,0);

			/* go through lines setting fold status */
			for(i=0,iBitCounter=6;i<iLineCount;i++)
			{
				iFlags=scintilla_send_message(sci,SCI_GETFOLDLEVEL,i,0);
				/* ignore non-folding lines */
				if((iFlags & SC_FOLDLEVELHEADERFLAG)==0)
					continue;

				/* get next 6 fold states if needed */
				if(iBitCounter==6)
				{
					iBitCounter=0;
					iBits=base64_char_to_int[(gint)(*cFoldData)];
					cFoldData++;
				}

				/* set fold if needed */
				if(((iBits>>iBitCounter)&1)==0)
					scintilla_send_message(sci,SCI_TOGGLEFOLD,i,0);

				/* increment counter */
				iBitCounter++;
			}

			break;
		/* file has changed since Geany last saved but, try to load bookmarks anyway */
		case GTK_RESPONSE_REJECT:
			iLineCount=scintilla_send_message(sci,SCI_GETLINECOUNT,0,0);
			for(i=0;i<10;i++)
				if(fd->iBookmark[i]!=-1 && fd->iBookmark[i]<iLineCount)
					scintilla_send_message(sci,SCI_MARKERADD,fd->iBookmark[i],i);

			break;
		default: /* default - don't try to set markers */
			break;
	}
}


/* handler for when a document has been saved
 * This saves off fold state, and marker positions for the file
*/
static void on_document_save(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
	FileData *fdTemp;
	gint i,iLineCount,iFlags,iBitCounter=0;
	ScintillaObject* sci=doc->editor->sci;
	struct stat sBuf;
	GByteArray *gbaFoldData=g_byte_array_sized_new(1000);
	guint8 guiFold=0;

	/* update markerpos */
	fdTemp=GetFileData(doc->file_name);
	for(i=0;i<10;i++)
		fdTemp->iBookmark[i]=scintilla_send_message(sci,SCI_MARKERNEXT,0,1<<(i+BOOKMARK_BASE));

	/* update fold state */
	iLineCount=scintilla_send_message(sci,SCI_GETLINECOUNT,0,0);
	/* go through each line */
	for(i=0;i<iLineCount;i++)
	{
		iFlags=scintilla_send_message(sci,SCI_GETFOLDLEVEL,i,0);
		/* ignore line if not a folding point */
		if((iFlags & SC_FOLDLEVELHEADERFLAG)==0)
			continue;

		iFlags=scintilla_send_message(sci,SCI_GETFOLDEXPANDED,i,0);
		/* remember if folded or not */
		guiFold|=(iFlags&1)<<iBitCounter;
		iBitCounter++;
		if(iBitCounter<6)
			continue;

		/* if have 6 bits then store these */
		iBitCounter=0;
		guiFold=(guint8)base64_int_to_char[guiFold];
		g_byte_array_append(gbaFoldData,&guiFold,1);
		guiFold=0;
	}

	/* flush buffer */
	if(iBitCounter!=0)
	{
		guiFold=(guint8)base64_int_to_char[guiFold];
		g_byte_array_append(gbaFoldData,&guiFold,1);
	}

	/* transfer data to text string */
	fdTemp->pcFolding=g_strndup((gchar*)(gbaFoldData->data),gbaFoldData->len);

	/* free byte array space */
	g_byte_array_free(gbaFoldData,TRUE);

	/* make note of time last saved */
	if(stat(doc->file_name,&sBuf)==0)
		fdTemp->LastChangedTime=sBuf.st_mtime;

	/* save settings */
	SaveSettings();
}


PluginCallback plugin_callbacks[] =
{
	{ "document-open", (GCallback) &on_document_open, FALSE, NULL },
	{ "document-save", (GCallback) &on_document_save, FALSE, NULL },
	{ NULL, NULL, FALSE, NULL }
};


/* returns current line number */
static gint GetLine(ScintillaObject* sci)
{
	return scintilla_send_message(sci,SCI_LINEFROMPOSITION,
	                              scintilla_send_message(sci,SCI_GETCURRENTPOS,10,0),0);
}


/* handle button presses in the preferences dialog box */
static void on_configure_response(GtkDialog *dialog, gint response, gpointer user_data)
{
	gboolean bSettingsHaveChanged;
	GtkCheckButton *cb1,*cb2;
	GtkComboBox *gtkcb;

	if(response!=GTK_RESPONSE_OK && response!=GTK_RESPONSE_APPLY)
		return;

	/* retreive pointers to check boxes */
	cb1=(GtkCheckButton*)(g_object_get_data(G_OBJECT(dialog),"Geany_Numbered_Bookmarks_cb1"));
	cb2=(GtkCheckButton*)(g_object_get_data(G_OBJECT(dialog),"Geany_Numbered_Bookmarks_cb2"));
	gtkcb=(GtkComboBox*)(g_object_get_data(G_OBJECT(dialog),"Geany_Numbered_Bookmarks_cb3"));

	/* first see if settings are going to change */
	bSettingsHaveChanged=(bRememberFolds!=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb1)));
	bSettingsHaveChanged|=(bCenterWhenGotoBookmark!=gtk_toggle_button_get_active(
	                       GTK_TOGGLE_BUTTON(cb2)));
	bSettingsHaveChanged|=(gtk_combo_box_get_active(gtkcb)!=PositionInLine);

	/* set new settings settings */
	bRememberFolds=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb1));
	bCenterWhenGotoBookmark=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb2));
	PositionInLine=gtk_combo_box_get_active(gtkcb);

	/* now save new settings if they have changed */
	if(bSettingsHaveChanged)
		SaveSettings();
}


/* return a widget containing settings for plugin that can be changed */
GtkWidget *plugin_configure(GtkDialog *dialog)
{
	GtkWidget *vbox;
	GtkWidget *cb1,*cb2,*gtkcb;

	vbox=gtk_vbox_new(FALSE, 6);

	cb1=gtk_check_button_new_with_label(_("remember fold state"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb1),bRememberFolds);
	gtk_box_pack_start(GTK_BOX(vbox),cb1,FALSE,FALSE,2);
	/* save pointer to check_button */
	g_object_set_data(G_OBJECT(dialog),"Geany_Numbered_Bookmarks_cb1",cb1);

	cb2=gtk_check_button_new_with_label(_("Center view when goto bookmark"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb2),bCenterWhenGotoBookmark);
	gtk_box_pack_start(GTK_BOX(vbox),cb2,FALSE,FALSE,2);
	/* save pointer to check_button */
	g_object_set_data(G_OBJECT(dialog),"Geany_Numbered_Bookmarks_cb2",cb2);

	gtkcb=gtk_combo_box_new_text();
	gtk_combo_box_append_text((GtkComboBox*)gtkcb,_("Move to start of line"));
	gtk_combo_box_append_text((GtkComboBox*)gtkcb,_("Move to remembered position in line"));
	gtk_combo_box_append_text((GtkComboBox*)gtkcb,_("Move to position in current line"));
	gtk_combo_box_append_text((GtkComboBox*)gtkcb,_("Move to End of line"));
	gtk_combo_box_set_active((GtkComboBox*)gtkcb,PositionInLine);
	gtk_box_pack_start(GTK_BOX(vbox),gtkcb,FALSE,FALSE,2);
	g_object_set_data(G_OBJECT(dialog),"Geany_Numbered_Bookmarks_cb3",gtkcb);

	gtk_widget_show_all(vbox);

	g_signal_connect(dialog,"response",G_CALLBACK(on_configure_response),NULL);

	return vbox;
}


/* display help box */
void plugin_help(void)
{
	GtkWidget *dialog,*label,*scroll;

	/* create dialog box */
	dialog=gtk_dialog_new_with_buttons(_("Numbered Bookmarks help"),
	                                   GTK_WINDOW(geany->main_widgets->window),
	                                   GTK_DIALOG_DESTROY_WITH_PARENT,
	                                   GTK_STOCK_OK,GTK_RESPONSE_ACCEPT,NULL);

	/* create label */
	label=gtk_label_new(
		_("This Plugin implements Numbered Bookmarks in Geany.\n\n"
		"It allows you to use 10 numbered bookmarks. Normaly if you had more than one \
bookmark, you would have to cycle through them until you reached the one you wanted. With this \
plugin you can go straight to the bookmark that you want with a single key combination. To set a \
numbered bookmark press Ctrl+Shift+a number from 0 to 9. You will see a marker apear next to the \
line number. If you press Ctrl+Shift+a number on a line that already has that bookmark number then\
 it removes the bookmark, otherwise it will move the bookmark there if it was set on a different \
line, or create it if it had not already been set. Only the bookmark with the highest number on a \
line will be shown, but you can have more than one bookmark per line. This plugin does not \
interfer with regular bookmarks. When a file is saved, Geany will remember the numbered bookmarks \
and make sure that they are set the next time you open the file.\n\n"
		"You can alter the default behaviour of this plugin by selecting Plugin Manager under the \
Tools menu, selecting this plugin, and cliking Preferences. You can change:\nRemember fold state -\
 if this is set then this plugin will remember the state of any folds along with the numbered \
bookmarks and set them when the file is next loaded.\nCenter view when goto bookmark - If this is \
set it will try to make sure that the numbered bookmark that you are going to is in the center of \
the screen, otherwise it will simply be on the screen somewhere."));
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


/* goto numbered bookmark */
static void GotoBookMark(gint iBookMark)
{
	gint iLine,iLinesVisible,iLineCount,iPosition,iEndOfLine;
	ScintillaObject* sci=document_get_current()->editor->sci;
	FileData *fd;

	iLine=scintilla_send_message(sci,SCI_MARKERNEXT,0,1<<(iBookMark+BOOKMARK_BASE));

	/* ignore if no marker placed for requested bookmark */
	if(iLine==-1)
		return;

	/* calculate position we're moving to */
	/* first get position of start of line we're after */
	iPosition=scintilla_send_message(sci,SCI_POSITIONFROMLINE,iLine,0);
	/* adjust for where in line we want to be */
	iEndOfLine=scintilla_send_message(sci,SCI_GETLINEENDPOSITION,iLine,0);
	switch(PositionInLine)
	{
		/* start of line */
		case 0:
			break;
		/* remembered line position */
		case 1:
			fd=GetFileData(document_get_current()->file_name);
			iPosition+=fd->iBookmarkLinePos[iBookMark];
			if(iPosition>iEndOfLine)
				iPosition=iEndOfLine;

			break;
		/* try to go to position in current line */
		case 2:
			iPosition+=scintilla_send_message(sci,SCI_GETCURRENTPOS,0,0)-
	           	           scintilla_send_message(sci,SCI_POSITIONFROMLINE,GetLine(sci),0);
			if(iPosition>iEndOfLine)
				iPosition=iEndOfLine;

			break;
		/* goto end of line */
		case 3:
			iPosition=iEndOfLine;
			break;
	}

	/* move to bookmark */
	scintilla_send_message(sci,SCI_GOTOPOS,iPosition,0);

	/* finnished unless centering on line */
	if(bCenterWhenGotoBookmark==FALSE)
		return;

	/* try and center bookmark on screen */
	iLinesVisible=scintilla_send_message(sci,SCI_LINESONSCREEN,0,0);
	iLineCount=scintilla_send_message(sci,SCI_GETLINECOUNT,0,0);
	iLine-=iLinesVisible/2;
	/* make sure view is not beyond start or end of document */
	if(iLine+iLinesVisible>iLineCount)
		iLine=iLineCount-iLinesVisible;
		
	if(iLine<0)
		iLine=0;

	scintilla_send_message(sci,SCI_SETFIRSTVISIBLELINE,iLine,0);
}


/* set (or remove) numbered bookmark */
static void SetBookMark(gint iBookMark)
{
	gint iNewLine,iOldLine,iPosInLine;
	ScintillaObject* sci=document_get_current()->editor->sci;
	FileData *fd;

	/* see if already such a bookmark present */
	iOldLine=scintilla_send_message(sci,SCI_MARKERNEXT,0,1<<(iBookMark+BOOKMARK_BASE));
	iNewLine=GetLine(sci);
	iPosInLine=scintilla_send_message(sci,SCI_GETCURRENTPOS,0,0)-
	           scintilla_send_message(sci,SCI_POSITIONFROMLINE,iNewLine,0);

	/* if no marker then simply add one to current line */
	if(iOldLine==-1)
	{
		CheckEditorSetup();
		scintilla_send_message(sci,SCI_MARKERADD,iNewLine,iBookMark+BOOKMARK_BASE);
		fd=GetFileData(document_get_current()->file_name);
		fd->iBookmarkLinePos[iBookMark]=iPosInLine;
	}
	/* else either have to remove marker from current line, or move it to current line */
	else
	{
		/* remove old marker */
		scintilla_send_message(sci,SCI_MARKERDELETEALL,iBookMark+BOOKMARK_BASE,0);
		/* add new marker if moving marker */
		if(iOldLine!=iNewLine)
		{
			scintilla_send_message(sci,SCI_MARKERADD,iNewLine,iBookMark+BOOKMARK_BASE);
			fd=GetFileData(document_get_current()->file_name);
			fd->iBookmarkLinePos[iBookMark]=iPosInLine;
		}
	}
}


/* handle key press
 * used to see if macro is being triggered and to control numbered bookmarks
*/
static gboolean Key_Released_CallBack(GtkWidget *widget, GdkEventKey *ev, gpointer data)
{
	GeanyDocument *doc;
	gint i;

	doc=document_get_current();
	if(doc==NULL)
		return FALSE;

	if(ev->type!=GDK_KEY_RELEASE)
		return FALSE;

	/* control and number pressed */
	if(ev->state==4)
	{
		i=((gint)(ev->keyval))-'0';
		if(i<0 || i>9)
			return FALSE;

		GotoBookMark(i);
		return TRUE;
	}
	/* control+shift+number */
	if(ev->state==5) {
		/* could use hardware keycode instead of keyvals but if unable to get keyode then don't
		 * have logical default to fall back on
		*/
		for(i=0;i<10;i++) if((gint)(ev->keyval)==iShiftNumbers[i])
		{
			SetBookMark(i);
			return TRUE;
		}

	}

	return FALSE;
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

	/* set key press monitor handle */
	key_release_signal_id=g_signal_connect(geany->main_widgets->window,"key-release-event",
	                                       G_CALLBACK(Key_Released_CallBack),NULL);
}


/* clean up on exiting this plugin */
void plugin_cleanup(void)
{
	gint k;
	guint i;
	ScintillaObject* sci;
	SCIPOINTERHOLDER *sciTemp;
	SCIPOINTERHOLDER *sciNext;
	FileData *fdTemp=fdKnownFilesSettings;
	FileData *fdTemp2;

	/* uncouple keypress monitor */
	g_signal_handler_disconnect(geany->main_widgets->window,key_release_signal_id);

	/* go through all documents removing markers (?needed) */
	for(i=0;i<GEANY(documents_array)->len;i++)
		if(documents[i]->is_valid) {
			sci=documents[i]->editor->sci;
			for(k=0;k<9;k++)
				scintilla_send_message(sci,SCI_MARKERDELETEALL,BOOKMARK_BASE+k,0);

		}

	/* Clear memory used for list of editors */
	sciTemp=sciList;
	while(sciTemp!=NULL)
	{
		sciNext=sciTemp->NextNode;
		free(sciTemp);
		sciTemp=sciNext;
	}

	/* Clear memory used to hold file details */
	while(fdTemp!=NULL)
	{
		/* free filename */
		g_free(fdTemp->pcFileName);
		/* free folding information if present */
		if(fdTemp->pcFolding!=NULL)
			g_free(fdTemp->pcFolding);

		fdTemp2=fdTemp->NextNode;
		/* free memory block  */
		g_free(fdTemp);
		fdTemp=fdTemp2;
	}
}
