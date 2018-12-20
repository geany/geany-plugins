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


#include "config.h"
#include "geanyplugin.h"
#include "utils.h"
#include "Scintilla.h"
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <glib/gstdio.h>

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

/* define structures used in this plugin */
typedef struct FileData
{
	gchar *pcFileName;    /* holds filename */
	gint iBookmark[10];   /* holds bookmark lines or -1 for not set */
	gint iBookmarkMarkerUsed[10]; /*holds which marker (2-24) is used for this bookmark */
	gint iBookmarkLinePos[10]; /* holds position of cursor in line */
	gchar *pcFolding;     /* holds which folds are open and which not */
	gint LastChangedTime; /* time file was last changed by this editor */
	gchar *pcBookmarks;   /* holds non-numbered bookmarks */
	struct FileData * NextNode;
} FileData;


GeanyPlugin     *geany_plugin;
GeanyData       *geany_data;

PLUGIN_VERSION_CHECK(224)

PLUGIN_SET_TRANSLATABLE_INFO(LOCALEDIR, GETTEXT_PACKAGE,
                             "Numbered Bookmarks",
                             _("Numbered Bookmarks for Geany"),
                             "1.0",
                             "William Fraser <william.fraser@virgin.net>")

/* Plugin user alterable settings */
static gboolean bCenterWhenGotoBookmark=TRUE;
static gboolean bRememberFolds=TRUE;
static gint PositionInLine=0;
static gint WhereToSaveFileDetails=0;
static gchar *FileDetailsSuffix; /* initialised when settings loaded */
static gboolean bRememberBookmarks=TRUE;

/* internal variables */
static gint iShiftNumbers[]={41,33,34,163,36,37,94,38,42,40};
static FileData *fdKnownFilesSettings=NULL;
static gulong key_release_signal_id;

/* default config file */
const gchar default_config[] =
	"[Settings]\n"
	"Center_When_Goto_Bookmark = true\n"
	"Remember_Folds = true\n"
	"Position_In_Line = 0\n"
	"Remember_Bookmarks = true\n"
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
			fdKnownFilesSettings->pcBookmarks=NULL;
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
				fdTemp->NextNode->pcBookmarks=NULL;
				fdTemp->NextNode->NextNode=NULL;
			}
			return fdTemp->NextNode;

		}
		fdTemp=fdTemp->NextNode;

	}
}


/* save individual file details. return TRUE if saved, FALSE if doesn't need to be saved */
static gboolean SaveIndividualSetting(GKeyFile *gkf,FileData *fd,gint iNumber,gchar *Filename)
{
	gchar *cKey;
	gchar szMarkers[1000];
	gchar *pszMarkers;
	gint i;

	/* first check if any bookmarks or folds for this file */
	/* if not can skip it */
	for(i=0;i<10;i++)
		if(fd->iBookmark[i]!=-1) break;
	/* i==10 if no markers set */

	/* skip if no folding data or markers */
	if(i==10 && (bRememberFolds==FALSE || fd->pcFolding==NULL) &&
	   (bRememberBookmarks==FALSE || fd->pcBookmarks==NULL))
		return FALSE;

	/* now save file data */
	if(iNumber==-1)
		cKey=g_strdup("A");
	else
		cKey=g_strdup_printf("A%d",iNumber);

	/* save filename */
	if(Filename!=NULL)
		g_key_file_set_string(gkf,"FileData",cKey,Filename);

	/* save folding data */
	cKey[0]='B';
	if(fd->pcFolding!=NULL && bRememberFolds==TRUE)
		g_key_file_set_string(gkf,"FileData",cKey,fd->pcFolding);

	/* save last saved time */
	cKey[0]='C';
	if(fd->LastChangedTime!=-1)
		g_key_file_set_integer(gkf,"FileData",cKey,fd->LastChangedTime);

	/* save bookmarks */
	cKey[0]='D';
	pszMarkers=szMarkers;
	pszMarkers[0]=0;
	for(i=0;i<10;i++)
	{
		if(fd->iBookmark[i]!=-1)
		{
			sprintf(pszMarkers,"%d",fd->iBookmark[i]);
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
	/* only save markers if have any set. Will contain 9 commas only if none set */
	if(szMarkers[9]!=0)
		g_key_file_set_string(gkf,"FileData",cKey,szMarkers);

	/* save positions in bookmarked lines */
	cKey[0]='E';
	pszMarkers=szMarkers;
	pszMarkers[0]=0;
	for(i=0;i<10;i++)
	{
		if(fd->iBookmark[i]!=-1)
		{
			sprintf(pszMarkers,"%d",fd->iBookmarkLinePos[i]);
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
	/* only save positions of markers if set. Will contain 9 commas only if none set */
	if(szMarkers[9]!=0)
		g_key_file_set_string(gkf,"FileData",cKey,szMarkers);

	/* save non-numbered bookmarks */
	cKey[0]='F';
	if(fd->pcBookmarks!=NULL && bRememberBookmarks==TRUE)
		g_key_file_set_string(gkf,"FileData",cKey,fd->pcBookmarks);

	g_free(cKey);

	return TRUE;
}


/* save settings (preferences, file data such as fold states, marker positions) */
static void SaveSettings(gchar *filename)
{
	GKeyFile *config=NULL;
	gchar *config_file=NULL,*config_dir=NULL;
	gchar *data;
	FileData* fdTemp=fdKnownFilesSettings;
	gint i=0;

	/* create new config from default settings */
	config=g_key_file_new();

	/* now set settings */
	g_key_file_set_boolean(config,"Settings","Center_When_Goto_Bookmark",bCenterWhenGotoBookmark);
	g_key_file_set_boolean(config,"Settings","Remember_Folds",bRememberFolds);
	g_key_file_set_integer(config,"Settings","Position_In_Line",PositionInLine);
	g_key_file_set_integer(config,"Settings","Where_To_Save_File_Details",WhereToSaveFileDetails);
	g_key_file_set_boolean(config,"Settings","Remember_Bookmarks",bRememberBookmarks);
	if(FileDetailsSuffix!=NULL)
		g_key_file_set_string(config,"Settings","File_Details_Suffix",FileDetailsSuffix);

	/* now save file data */
	while(fdTemp!=NULL)
	{
		/* if this entry has data needing saveing then save it and increment the counter */
		if(SaveIndividualSetting(config,fdTemp,i,fdTemp->pcFileName))
			i++;

		fdTemp=fdTemp->NextNode;
	}

	/* turn config into data */
	data=g_key_file_to_data(config,NULL,NULL);

	/* calculate setting directory name */
	config_dir=g_build_filename(geany->app->configdir,"plugins","Geany_Numbered_Bookmarks",NULL);
	/* ensure directory exists */
	g_mkdir_with_parents(config_dir,0755);

	/* make config_file hold name of settings file */
	config_file=g_build_filename(config_dir,"settings.conf",NULL);

	/* write data */
	utils_write_file(config_file,data);

	/* free memory */
	g_free(config_dir);
	g_free(config_file);
	g_key_file_free(config);
	g_free(data);

	/* now consider if not purely saving file settings to main settings file */
	/* return if not saving data with file */
	if(filename==NULL || WhereToSaveFileDetails==0)
		return;

	/* setup keyfile to hold values */
	config=g_key_file_new();

	/* get pointer to data we're saving */
	fdTemp=GetFileData(filename);

	/* calculate settings filename */
	config_file=g_strdup_printf("%s%s",filename,FileDetailsSuffix);

	/* if nothing to save then delete any old data */
	if(SaveIndividualSetting(config,fdTemp,-1,NULL)==FALSE)
		g_remove(config_file);
	/* otherwise save the data */
	else
	{
		/* turn config into data */
		data=g_key_file_to_data(config,NULL,NULL);
		/* write data */
		utils_write_file(config_file,data);

		g_free(data);
	}

	/* free memory */
	g_free(config_file);
	g_key_file_free(config);
}


/* load individual file details. return TRUE if data there, FALSE if there isn't */
static gboolean LoadIndividualSetting(GKeyFile *gkf,gint iNumber,gchar *Filename)
{
	gchar *pcKey=NULL;
	gchar *pcTemp;
	gchar *pcTemp2;
	gint l;
	FileData *fd=NULL;

	/* if loading from local file then no fiilename in file and no number in key*/
	if(iNumber==-1)
	{
		/* get structure to hold filedetails */
		fd=GetFileData(Filename);

		/* create key */
		pcKey=g_strdup("A");
	}
	/* if loading from central file then need to extract filename from A key */
	else
	{
		pcKey=g_strdup_printf("A%d",iNumber);

		/* get filename */
		pcTemp=(gchar*)(utils_get_setting_string(gkf,"FileData",pcKey,NULL));
		/* if null then have reached end of files */
		if(pcTemp==NULL)
		{
			g_free(pcKey);
			return FALSE;
		}

		fd=GetFileData(pcTemp);
		g_free(pcTemp);
	}

	/* get folding data */
	pcKey[0]='B';
	if(bRememberFolds==TRUE)
		fd->pcFolding=(gchar*)(utils_get_setting_string(gkf,"FileData",pcKey,NULL));
	else
		fd->pcFolding=NULL;

	/* load last saved time */
	pcKey[0]='C';
	fd->LastChangedTime=utils_get_setting_integer(gkf,"FileData",pcKey,-1);
	/* get bookmarks */
	pcKey[0]='D';
	pcTemp=(gchar*)(utils_get_setting_string(gkf,"FileData",pcKey,NULL));
	/* pcTemp contains comma seperated numbers (or blank for -1) */
	pcTemp2=pcTemp;
	if(pcTemp!=NULL) for(l=0;l<10;l++)
	{
		/* Bookmark entries are initialized to -1, so only need to parse non-empty slots */
		if(pcTemp2[0]!=',' && pcTemp2[0]!=0)
		{
			fd->iBookmark[l]=strtoll(pcTemp2,NULL,10);
			while(pcTemp2[0]!=0 && pcTemp2[0]!=',')
				pcTemp2++;
		}

		pcTemp2++;
	}
	g_free(pcTemp);

	/* get position in bookmarked lines */
	pcKey[0]='E';
	pcTemp=(gchar*)(utils_get_setting_string(gkf,"FileData",pcKey,NULL));
	/* pcTemp contains comma seperated numbers (or blank for -1) */
	pcTemp2=pcTemp;
	if(pcTemp!=NULL) for(l=0;l<10;l++)
	{
		/* Bookmark entries are initialized to -1, so only need to parse non-empty slots */
		if(pcTemp2[0]!=',' && pcTemp2[0]!=0)
		{
			fd->iBookmarkLinePos[l]=strtoll(pcTemp2,NULL,10);
			while(pcTemp2[0]!=0 && pcTemp2[0]!=',')
				pcTemp2++;
		}

		pcTemp2++;
	}

	/* get non-numbered bookmarks */
	pcKey[0]='F';
	if(bRememberBookmarks==TRUE)
		fd->pcBookmarks=(gchar*)(utils_get_setting_string(gkf,"FileData",pcKey,NULL));
	else
		fd->pcBookmarks=NULL;

	/* free used memory */
	g_free(pcTemp);
	g_free(pcKey);

	return TRUE;
}


/* load settings (preferences, file data, and macro data) */
static void LoadSettings(void)
{
	gint i;
	gchar *config_file=NULL;
	gchar *config_dir=NULL;
	GKeyFile *config=NULL;

	/* Make config_dir hold directory name of settings file */
	config_dir=g_build_filename(geany->app->configdir,"plugins","Geany_Numbered_Bookmarks",NULL);
	/* ensure directory exists */
	g_mkdir_with_parents(config_dir,0755);

	/* make config_file hold name of settings file */
	config_file=g_build_filename(config_dir,"settings.conf",NULL);

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
	WhereToSaveFileDetails=utils_get_setting_integer(config,"Settings",
	                                                 "Where_To_Save_File_Details",0);
	bRememberBookmarks=utils_get_setting_boolean(config,"Settings","Remember_Bookmarks",FALSE);
	FileDetailsSuffix=utils_get_setting_string(config,"Settings","File_Details_Suffix",
	                                           ".gnbs.conf");

	/* extract data about files */
	i=0;
	while(LoadIndividualSetting(config,i,NULL))
		i++;

	/* free memory */
	g_free(config_dir);
	g_free(config_file);
	g_key_file_free(config);
}


/* try to load localy saved file details */
static void LoadLocalFileDetails(gchar *filename)
{
	gchar *config_file=NULL;
	GKeyFile *config=NULL;

	/* calculate settings filename */
	config_file=g_strdup_printf("%s%s",filename,FileDetailsSuffix);

	/* create keyfile to hold data */
	config=g_key_file_new();

	/* if can load settings file then extract the info */
	if(g_key_file_load_from_file(config,config_file,G_KEY_FILE_KEEP_COMMENTS,NULL))
	{
		/* load file details */
		LoadIndividualSetting(config,-1,filename);
	}

	/* free memory */
	g_free(config_file);
	g_key_file_free(config);
}


/* Get markers for editor. If not set then initiate */
static guint32 * GetMarkersUsed(ScintillaObject* sci)
{
	guint32 *markers;

	/*fetch pointer to markers */
	markers=(guint32*)(g_object_get_data(G_OBJECT(sci),"Geany_Numbered_Bookmarks_Used"));

	/* if initialised then return these */
	if(markers!=NULL)
		return markers;

	/* initialise markers as none initialised */
	markers=g_malloc(sizeof(guint32));

	/* if failed to allocate space return NULL */
	if(markers==NULL)
		return NULL;

	/*initiate markers */
	(*markers)=0;

	/* save record of which markers are being used */
	g_object_set_data(G_OBJECT(sci),"Geany_Numbered_Bookmarks_Used",(gpointer)markers);

	return markers;
}


/* get next free marker number */
static gint NextFreeMarker(GeanyDocument* doc)
{
	gint i,l,m,k;
	guint32 *markers;
	FileData *fd;
	ScintillaObject *sci=doc->editor->sci;

	markers=GetMarkersUsed(sci);

	/* fail if can't allocate space for markers */
	if(markers==NULL)
		return -1;

	/* markers 0 and 1 reserved for bookmarks & errors, 25 onwards for folds */
	/* find first free marker after last defined marker. Will ensure that new marker */
	/* is displayed over any previously set markers */
	for(i=24,m=-1;i>1;i--)
	{
		l=scintilla_send_message(sci,SCI_MARKERSYMBOLDEFINED,i,0);

		if(l==SC_MARK_CIRCLE || l==SC_MARK_AVAILABLE)
		{
			/* if reached start of user-defined markers then return this */
			if(i==2)
				return 2;

			/* found empty marker so make note of it */
			m=i;
			/* keep looking for 1st unused marker after last used marker */
			continue;
		}

		/* found marker */

		/* if not a numbered bookmark then ignore it */
		if(((*markers)&(1<<i))==0)
			continue;

		/* if have found an empty marker higher then return this */
		if(m!=-1)
			return m;

		/* no empty markers above last used */

		/* first see if there are any unused markers */
		while(i>1)
		{
			l=scintilla_send_message(sci,SCI_MARKERSYMBOLDEFINED,i,0);
			if(l==SC_MARK_CIRCLE || l==SC_MARK_AVAILABLE)
				break;

			i--;
		}

		/* no empty markers available so return -1 */
		if(i==1)
			return -1;

		/* there are empty markers available. Break out of for loop & make them available */
		break;
	}

	/* compact used numbered markers */
	/* move through markers moving numbered bookmarks ones from i to m */
	for(m=2,i=2;i<25;i++)
	{
		/* don't move marker unless it's a numbered bookmark marker */
		if(((*markers)&(1<<i))==0)
			continue;

		/* find unused marker */
		l=scintilla_send_message(sci,SCI_MARKERSYMBOLDEFINED,m,0);
		while((l!=SC_MARK_CIRCLE && l!=SC_MARK_AVAILABLE) && m<i)
		{
			m++;
			l=scintilla_send_message(sci,SCI_MARKERSYMBOLDEFINED,m,0);
		}

		/* if can't move marker forward then don't */
		if(m==i)
			continue;

		/* move marker from i to m */
		/* first make note of line number of marker */
		l=scintilla_send_message(sci,SCI_MARKERNEXT,0,1<<i);
		/* remove old marker */
		scintilla_send_message(sci,SCI_MARKERDELETEALL,i,0);
		scintilla_send_message(sci,SCI_MARKERDEFINE,i,SC_MARK_AVAILABLE);

		/* find bookmark number, put in k */
		fd=GetFileData(doc->file_name);
		for(k=0;k<10;k++)
			if(fd->iBookmarkMarkerUsed[k]==i)
				break;

		/* insert new marker */
		scintilla_send_message(sci,SCI_MARKERDEFINEPIXMAP,m,(glong)(aszMarkerImages[k]));
		scintilla_send_message(sci,SCI_MARKERADD,l,m);

		/* update markers record */
		(*markers)-=1<<i;
		(*markers)|=1<<m;

		/* update record of which bookmark uses which marker */
		fd->iBookmarkMarkerUsed[k]=m;
	}

	/* save record of which markers are being used */
	g_object_set_data(G_OBJECT(sci),"Geany_Numbered_Bookmarks_Used",(gpointer)markers);


	/* m should point to last used marker. Next free marker should lie after this */
	/* find free marker & return it */
	for(;m<25;m++)
	{
		l=scintilla_send_message(sci,SCI_MARKERSYMBOLDEFINED,m,0);
		if(l==SC_MARK_CIRCLE || l==SC_MARK_AVAILABLE)
			return m;
	}

	/* no empty markers available so return -1 */
	/* in theory shouldn't get here but leave just in case my logic is flawed */
	return -1;
}


static void SetMarker(GeanyDocument* doc,gint bookmarkNumber,gint markerNumber,gint line)
{
	guint32 *markers;
	FileData *fd;
	ScintillaObject *sci=doc->editor->sci;

	/* insert new marker */
	scintilla_send_message(sci,SCI_MARKERDEFINEPIXMAP,markerNumber,
	                       (glong)(aszMarkerImages[bookmarkNumber]));
	scintilla_send_message(sci,SCI_MARKERADD,line,markerNumber);

	/* update record of which bookmark uses which marker */
	fd=GetFileData(doc->file_name);
	fd->iBookmarkMarkerUsed[bookmarkNumber]=markerNumber;

	/* update record of which markers are being used */
	markers=GetMarkersUsed(sci);
	(*markers)|=1<<markerNumber;
	g_object_set_data(G_OBJECT(sci),"Geany_Numbered_Bookmarks_Used",(gpointer)markers);
}


static void DeleteMarker(GeanyDocument* doc,gint bookmarkNumber,gint markerNumber)
{
	guint32 *markers;
	ScintillaObject *sci=doc->editor->sci;

	/* remove marker */
	scintilla_send_message(sci,SCI_MARKERDELETEALL,markerNumber,0);
	scintilla_send_message(sci,SCI_MARKERDEFINE,markerNumber,SC_MARK_AVAILABLE);

	/* update record of which markers are being used */
	markers=GetMarkersUsed(sci);
	(*markers)-=1<<markerNumber;
	g_object_set_data(G_OBJECT(sci),"Geany_Numbered_Bookmarks_Used",(gpointer)markers);
}


static void ApplyBookmarks(GeanyDocument* doc,FileData *fd)
{
	gint i,iLineCount,m;
	GtkWidget *dialog;
	ScintillaObject* sci=doc->editor->sci;

	iLineCount=scintilla_send_message(sci,SCI_GETLINECOUNT,0,0);

	for(i=0;i<10;i++)
		if(fd->iBookmark[i]!=-1 && fd->iBookmark[i]<iLineCount)
		{
			m=NextFreeMarker(doc);
			/* if run out of markers report this */
			if(m==-1)
			{
				dialog=gtk_message_dialog_new(GTK_WINDOW(geany->main_widgets->window),
				                              GTK_DIALOG_DESTROY_WITH_PARENT,
				                              GTK_MESSAGE_ERROR,GTK_BUTTONS_NONE,
_("Unable to apply all markers to '%s' as all being used."),
				                              doc->file_name);
				gtk_dialog_add_button(GTK_DIALOG(dialog),_("_Okay"),GTK_RESPONSE_OK);
				gtk_dialog_run(GTK_DIALOG(dialog));
				gtk_widget_destroy(dialog);
				return;
			}

			/* otherwise ok to set marker */
			SetMarker(doc,i,m,fd->iBookmark[i]);
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
	gchar *pcTemp;
	/* keep compiler happy & initialise iBits: will logically be initiated anyway */
	gint iBits=0,iFlags,iBitCounter;

	/* if saving details in file alongside file we're editing then load it up */
	if(WhereToSaveFileDetails==1)
		LoadLocalFileDetails(doc->file_name);

	/* check to see if file has changed since geany last saved it */
	fd=GetFileData(doc->file_name);
	if(stat(doc->file_name,&sBuf)==0 && fd!=NULL && fd->LastChangedTime!=-1 &&
	   fd->LastChangedTime!=sBuf.st_mtime)
	{
		/* notify user that file has been changed */
		dialog=gtk_message_dialog_new(GTK_WINDOW(geany->main_widgets->window),
		                              GTK_DIALOG_DESTROY_WITH_PARENT,GTK_MESSAGE_ERROR,
		                              GTK_BUTTONS_NONE,
_("'%s' has been edited since it was last saved by geany. Marker positions may be unreliable and w\
ill not be loaded.\nPress Ignore to try an load markers anyway."),doc->file_name);
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
			ApplyBookmarks(doc,fd);

			/* get fold settings if present and want to use them */
			if(fd->pcFolding!=NULL && bRememberFolds==TRUE)
			{
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
			}

			/* get non-numbered bookmark settings if present and want to use them */
			if(fd->pcBookmarks!=NULL && bRememberBookmarks==TRUE)
			{
				pcTemp=fd->pcBookmarks;
				while(pcTemp[0]!=0)
				{
					/* get next linenumber */
					i=strtoll(pcTemp,NULL,16);
					/* set bookmark */
					scintilla_send_message(sci,SCI_MARKERADD,i,1);
					/* move to next linenumber */
					while(pcTemp[0]!=0 && pcTemp[0]!=',')
						pcTemp++;

					if(pcTemp[0]==',')
						pcTemp++;

				}

			}

			break;
		/* file has changed since Geany last saved but, try to load bookmarks anyway */
		case GTK_RESPONSE_REJECT:
			ApplyBookmarks(doc,fd);
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
	FileData *fd;
	gint i,iLineCount,iFlags,iBitCounter=0;
	ScintillaObject* sci=doc->editor->sci;
	struct stat sBuf;
	GByteArray *gbaFoldData=NULL;
	guint8 guiFold=0;
	gboolean bHasClosedFold=FALSE,bHasBookmark=FALSE;
	gchar szLine[20];

	/* update markerpos */
	fd=GetFileData(doc->file_name);
	for(i=0;i<10;i++)
		fd->iBookmark[i]=scintilla_send_message(sci,SCI_MARKERNEXT,0,
		                                        1<<(fd->iBookmarkMarkerUsed[i]));

	/* save fold state */
	if(bRememberFolds==TRUE)
	{
		gbaFoldData=g_byte_array_sized_new(1000);

		iLineCount=scintilla_send_message(sci,SCI_GETLINECOUNT,0,0);
		/* go through each line */
		for(i=0;i<iLineCount;i++)
		{
			iFlags=scintilla_send_message(sci,SCI_GETFOLDLEVEL,i,0);
			/* ignore line if not a folding point */
			if((iFlags & SC_FOLDLEVELHEADERFLAG)==0)
				continue;

			iFlags=scintilla_send_message(sci,SCI_GETFOLDEXPANDED,i,0);
			/* make note of if any folds closed or not */
			bHasClosedFold|=((iFlags&1)==0);
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

		/* transfer data to text string if have closed fold. Default will leave them open*/
		fd->pcFolding=(!bHasClosedFold)?NULL:g_strndup((gchar*)(gbaFoldData->data),
	                                               gbaFoldData->len);

		/* free byte array space */
		g_byte_array_free(gbaFoldData,TRUE);
	}
	else
		fd->pcFolding=NULL;

	/* now save off bookmarks */
	if(bRememberBookmarks==TRUE)
	{
		gbaFoldData=g_byte_array_sized_new(1000);

		i=0;
		while((i=scintilla_send_message(sci,SCI_MARKERNEXT,i+1,2))!=-1)
		{
			g_sprintf(szLine,"%s%X",bHasBookmark?",":"",i);
			g_byte_array_append(gbaFoldData,(guint8*)szLine,strlen(szLine));

			/* will be data in byte array to save */
			bHasBookmark=TRUE;
		}

		/* transfer data to text string */
		fd->pcBookmarks=(!bHasBookmark)?NULL:g_strndup((gchar*)(gbaFoldData->data),
		                 gbaFoldData->len);

		/* free byte array space */
		g_byte_array_free(gbaFoldData,TRUE);
	}
	else
		fd->pcBookmarks=NULL;



	/* make note of time last saved */
	if(stat(doc->file_name,&sBuf)==0)
		fd->LastChangedTime=sBuf.st_mtime;

	/* save settings */
	SaveSettings(doc->file_name);
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
	GtkCheckButton *cb1,*cb2,*cb3;
	GtkComboBox *gtkcb1,*gtkcb2;

	if(response!=GTK_RESPONSE_OK && response!=GTK_RESPONSE_APPLY)
		return;

	/* retreive pointers to widgets */
	cb1=(GtkCheckButton*)(g_object_get_data(G_OBJECT(dialog),"Geany_Numbered_Bookmarks_cb1"));
	cb2=(GtkCheckButton*)(g_object_get_data(G_OBJECT(dialog),"Geany_Numbered_Bookmarks_cb2"));
	gtkcb1=(GtkComboBox*)(g_object_get_data(G_OBJECT(dialog),"Geany_Numbered_Bookmarks_cb3"));
	gtkcb2=(GtkComboBox*)(g_object_get_data(G_OBJECT(dialog),"Geany_Numbered_Bookmarks_cb4"));
	cb3=(GtkCheckButton*)(g_object_get_data(G_OBJECT(dialog),"Geany_Numbered_Bookmarks_cb5"));

	/* first see if settings are going to change */
	bSettingsHaveChanged=(bRememberFolds!=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb1)));
	bSettingsHaveChanged|=(bCenterWhenGotoBookmark!=gtk_toggle_button_get_active(
	                       GTK_TOGGLE_BUTTON(cb2)));
	bSettingsHaveChanged|=(gtk_combo_box_get_active(gtkcb1)!=PositionInLine);
	bSettingsHaveChanged|=(gtk_combo_box_get_active(gtkcb2)!=WhereToSaveFileDetails);
	bSettingsHaveChanged|=(bRememberBookmarks!=gtk_toggle_button_get_active(
	                       GTK_TOGGLE_BUTTON(cb3)));

	/* set new settings settings */
	bRememberFolds=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb1));
	bCenterWhenGotoBookmark=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb2));
	PositionInLine=gtk_combo_box_get_active(gtkcb1);
	WhereToSaveFileDetails=gtk_combo_box_get_active(gtkcb2);
	bRememberBookmarks=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb3));

	/* now save new settings if they have changed */
	if(bSettingsHaveChanged)
		SaveSettings(NULL);
}


/* return a widget containing settings for plugin that can be changed */
GtkWidget *plugin_configure(GtkDialog *dialog)
{
	GtkWidget *vbox;
	GtkWidget *gtkw;

	vbox=gtk_vbox_new(FALSE, 6);

	gtkw=gtk_check_button_new_with_label(_("remember fold state"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtkw),bRememberFolds);
	gtk_box_pack_start(GTK_BOX(vbox),gtkw,FALSE,FALSE,2);
	/* save pointer to check_button */
	g_object_set_data(G_OBJECT(dialog),"Geany_Numbered_Bookmarks_cb1",gtkw);

	gtkw=gtk_check_button_new_with_label(_("Center view when goto bookmark"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtkw),bCenterWhenGotoBookmark);
	gtk_box_pack_start(GTK_BOX(vbox),gtkw,FALSE,FALSE,2);
	/* save pointer to check_button */
	g_object_set_data(G_OBJECT(dialog),"Geany_Numbered_Bookmarks_cb2",gtkw);

	gtkw=gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gtkw),_("Move to start of line"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gtkw),_("Move to remembered position in line"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gtkw),_("Move to position in current line"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gtkw),_("Move to End of line"));
	gtk_combo_box_set_active(GTK_COMBO_BOX(gtkw),PositionInLine);
	gtk_box_pack_start(GTK_BOX(vbox),gtkw,FALSE,FALSE,2);
	g_object_set_data(G_OBJECT(dialog),"Geany_Numbered_Bookmarks_cb3",gtkw);

	gtkw=gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gtkw),_("Save file settings with program settings"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gtkw),_("Save file settings to filename with suffix"));
	gtk_combo_box_set_active(GTK_COMBO_BOX(gtkw),WhereToSaveFileDetails);
	gtk_box_pack_start(GTK_BOX(vbox),gtkw,FALSE,FALSE,2);
	g_object_set_data(G_OBJECT(dialog),"Geany_Numbered_Bookmarks_cb4",gtkw);

	gtkw=gtk_check_button_new_with_label(_("remember normal Bookmarks"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtkw),bRememberBookmarks);
	gtk_box_pack_start(GTK_BOX(vbox),gtkw,FALSE,FALSE,2);
	/* save pointer to check_button */
	g_object_set_data(G_OBJECT(dialog),"Geany_Numbered_Bookmarks_cb5",gtkw);

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
_("This Plugin implements Numbered Bookmarks in Geany, as well as remembering the state of folds, \
and positions of standard non-numbered bookmarks when a file is saved.\n\n"
"It allows you to use up to 10 numbered bookmarks. To set a numbered bookmark press Ctrl+Shift+a n\
umber from 0 to 9. You will see a marker appear next to the line number. If you press Ctrl+Shift+a \
number on a line that already has that bookmark number then it removes the bookmark, otherwise it \
will move the bookmark there if it was set on a different line, or create it if it had not already\
 been set. Only the most recently set bookmark on a line will be shown, but you can have more than\
 one bookmark per line. To move to a previously set bookmark, press Ctrl+a number from 0 to 9."));
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
}


/* goto numbered bookmark */
static void GotoBookMark(GeanyDocument* doc, gint iBookMark)
{
	gint iLine,iLinesVisible,iLineCount,iPosition,iEndOfLine;
	ScintillaObject* sci=doc->editor->sci;
	FileData *fd;

	fd=GetFileData(doc->file_name);

	iLine=scintilla_send_message(sci,SCI_MARKERNEXT,0,1<<(fd->iBookmarkMarkerUsed[iBookMark]));

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
static void SetBookMark(GeanyDocument *doc, gint iBookMark)
{
	gint iNewLine,iOldLine,iPosInLine,m;
	ScintillaObject* sci=doc->editor->sci;
	FileData *fd=GetFileData(doc->file_name);
	GtkWidget *dialog;

	/* see if already such a bookmark present */
	iOldLine=scintilla_send_message(sci,SCI_MARKERNEXT,0,1<<(fd->iBookmarkMarkerUsed[iBookMark]));
	iNewLine=GetLine(sci);
	iPosInLine=scintilla_send_message(sci,SCI_GETCURRENTPOS,0,0)-
	           scintilla_send_message(sci,SCI_POSITIONFROMLINE,iNewLine,0);

	/* if no marker then simply add one to current line */
	if(iOldLine==-1)
	{
		m=NextFreeMarker(doc);
		/* if run out of markers report this */
		if(m==-1)
		{
			dialog=gtk_message_dialog_new(GTK_WINDOW(geany->main_widgets->window),
			                              GTK_DIALOG_DESTROY_WITH_PARENT,
			                              GTK_MESSAGE_ERROR,GTK_BUTTONS_NONE,
			                              _("Unable to apply markers as all being used."));
			gtk_dialog_add_button(GTK_DIALOG(dialog),_("_Okay"),GTK_RESPONSE_OK);
			gtk_dialog_run(GTK_DIALOG(dialog));
			gtk_widget_destroy(dialog);
			return;
		}
		/* otherwise ok to set marker */
		SetMarker(doc,iBookMark,m,iNewLine);
		fd->iBookmarkLinePos[iBookMark]=iPosInLine;
	}
	/* else if have to remove marker from current line */
	else if(iOldLine==iNewLine)
	{
		DeleteMarker(doc,iBookMark,fd->iBookmarkMarkerUsed[iBookMark]);
	}
	/* else have to move it to current line */
	else
	{
		/* go through the process of removing and adding the marker so the marker will */
		/* have the highest value and be on top */

		/* remove old marker */
		DeleteMarker(doc,iBookMark,fd->iBookmarkMarkerUsed[iBookMark]);
		/* add new marker if moving marker */
		m=NextFreeMarker(doc);
		/* don't bother checking for failure to find marker as have just released one so */
		/* there should be one free */
		SetMarker(doc,iBookMark,m,iNewLine);
		fd->iBookmarkLinePos[iBookMark]=iPosInLine;
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

		GotoBookMark(doc, i);
		return TRUE;
	}
	/* control+shift+number */
	if(ev->state==5) {
		/* could use hardware keycode instead of keyvals but if unable to get keyode then don't
		 * have logical default to fall back on
		*/
		for(i=0;i<10;i++) if((gint)(ev->keyval)==iShiftNumbers[i])
		{
			SetBookMark(doc, i);
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
	GdkKeymap *gdkKeyMap=gdk_keymap_get_default();

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
	FileData *fdTemp=fdKnownFilesSettings;
	FileData *fdTemp2;
	guint32 *markers;

	/* uncouple keypress monitor */
	g_signal_handler_disconnect(geany->main_widgets->window,key_release_signal_id);

	/* go through all documents removing markers (?needed) */
	for(i=0;i<GEANY(documents_array)->len;i++)
		if(documents[i]->is_valid) {
			sci=documents[i]->editor->sci;
			markers=g_object_steal_data(G_OBJECT(sci), "Geany_Numbered_Bookmarks_Used");
			if(!markers)
				continue;
			for(k=2;k<25;k++)
				if(((*markers)&(1<<k))!=0)
					scintilla_send_message(sci,SCI_MARKERDELETEALL,k,0);

			g_free(markers);
		}

	/* Clear memory used to hold file details */
	while(fdTemp!=NULL)
	{
		/* free filename */
		g_free(fdTemp->pcFileName);
		/* free folding & bookmark information if present */
		g_free(fdTemp->pcFolding);
		g_free(fdTemp->pcBookmarks);

		fdTemp2=fdTemp->NextNode;
		/* free memory block  */
		g_free(fdTemp);
		fdTemp=fdTemp2;
	}
	fdKnownFilesSettings = NULL;

	/* free memory used for settings */
	g_free(FileDetailsSuffix);
}
