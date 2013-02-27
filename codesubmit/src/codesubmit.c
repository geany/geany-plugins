/*
* codesubmit - submit your code online at SPOJ,Codechef etc..
*
* Copyright 2012 Mayank "Enrix835" Trotta <enrico.trt@gmail.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
* MA 02110-1301, USA.
*/

#include <geanyplugin.h>
#include <stdlib.h>
#include <string.h>
GeanyPlugin     *geany_plugin;
GeanyData       *geany_data;
GeanyFunctions  *geany_functions;
GeanyDocument   *soln_doc;
PLUGIN_VERSION_CHECK(211)

PLUGIN_SET_INFO("Online Code Submissions", "Tool to submit coding problem submissions from geany itself!",
                "1.0", "Mayank Jha <mayank25080562@gmail.com>");

static GtkWidget *window;
static GtkWidget *tools_menu_item=NULL;
static GtkWidget *user_entry;
static GtkWidget *pwd_entry;
static GtkWidget *probid_entry;
static GtkWidget *language_box;
static GtkWidget *server_box;
static GtkComboBox *combo_box;
static GtkWidget *submit_button;
static GtkWidget *save_button;


void spoj_preferences_load_doit(void)
{
gchar doc_fn[30],user_ent[30],pwd_ent[30],probid_ent[30];
int active_choice;
FILE *f=fopen(g_build_path(G_DIR_SEPARATOR_S, geany->app->configdir, "plugins","datafile.conf", NULL),"r"); 
/*the path where the data is stored like username,password etc.*/
	if(f)
	{

		fscanf(f,"%s %s %s %d",doc_fn,user_ent,pwd_ent,&active_choice);
		gtk_entry_set_text(GTK_ENTRY(user_entry),(doc_fn));
		gtk_entry_set_text(GTK_ENTRY(pwd_entry),(user_ent));
		gtk_entry_set_text(GTK_ENTRY(probid_entry),(probid_ent));;
		gtk_combo_box_set_active(GTK_COMBO_BOX(language_box),active_choice);

	}
	else
	{
		return;	
	}
fclose(f);
}


static void spoj_save_doit(GtkWidget *save_button, gpointer gdata)
{

FILE *da=fopen(g_build_path(G_DIR_SEPARATOR_S, geany->app->configdir, "plugins","datafile.conf", NULL),"w");
/*the path where the data is stored like username,password etc.*/
	if(da!=NULL)
	{
	  if(strlen(gtk_entry_get_text(GTK_ENTRY(user_entry)))>30||
		 strlen(gtk_entry_get_text(GTK_ENTRY(pwd_entry)))>30||
		 strlen(gtk_entry_get_text(GTK_ENTRY(probid_entry)))>30
		 )dialogs_show_msgbox(GTK_MESSAGE_INFO, "Input parameters don't seem to be valid! Reenter values");
	  else
	  {		
		  fprintf(da, "%s %s %s %d", 
		  gtk_entry_get_text(GTK_ENTRY(user_entry)),
		  gtk_entry_get_text(GTK_ENTRY(pwd_entry)),
		  gtk_entry_get_text(GTK_ENTRY(probid_entry)),
		  gtk_combo_box_get_active(GTK_COMBO_BOX(language_box))); 
      }	
	}

	else
	{
		/*print an error mesage and then*/
		dialogs_show_msgbox(GTK_MESSAGE_INFO, "Data File could not be opened!");
		return;
	}
fclose(da);

}



static void spoj_submit_online_doit(GtkWidget *submit_button, gpointer gdata)
{
gchar buffer[50];
gchar *doc_fn, *user_ent, *pwd_ent, *probid_ent,*lang,*expression;
soln_doc=document_get_current();
int active_choice;

	if(soln_doc)
	{
		lang=malloc(sizeof(gchar)*3);
		doc_fn=g_strdup(soln_doc->file_name);
		user_ent=g_strdup(gtk_entry_get_text(GTK_ENTRY(user_entry)));
		pwd_ent=g_strdup(gtk_entry_get_text(GTK_ENTRY(pwd_entry)));
		probid_ent=g_strdup(gtk_entry_get_text(GTK_ENTRY(probid_entry)));
		active_choice=gtk_combo_box_get_active(GTK_COMBO_BOX(language_box));
		
		switch(active_choice)
		{
		case 0:{strcpy(lang,"11");break;}//c
		case 1:{strcpy(lang,"41");break;}//c++
		case 2:{strcpy(lang,"10");break;}//python
		case 3:{strcpy(lang,"12");break;}//brainfuck
		};
    
		/*presently built only for probids c,c++,python and brainfuck
		* to extend it to include other languages like JAVA etc.*/
	
		GString *expression=g_string_new("python ");
		g_string_append(expression,g_build_path(G_DIR_SEPARATOR_S, geany->app->configdir, "plugins","y.py", NULL);
		g_string_append(expression," ");
		g_string_append(expression,user_ent);
		g_string_append(expression," ");
		g_string_append(expression,pwd_ent);
		g_string_append(expression," ");
		g_string_append(expression,doc_fn);
		g_string_append(expression," ");
		g_string_append(expression,lang);
		g_string_append(expression," ");
		g_string_append(expression,probid_ent);
		g_string_append(expression," || (echo \"0\"&&  exit 1)");
		printf("%s\n",expression->str);
		FILE *temp;
		FILE* in=popen(expression->str, "r");
			if(in==NULL)
			{	/*display a dialog box showing the error*/
				dialogs_show_msgbox(GTK_MESSAGE_INFO, "Terminal Pipe could not be opened!");
				return;
			}
			temp=in;
			if(getc(temp)=='0'){dialogs_show_msgbox(GTK_MESSAGE_INFO,
			 "Bash Script failed!\n*No internet or wrong URL\n*or invalid entries");}
			else
			{
				while(fgets(buffer, 50, in) != NULL) 
				{	
					if(strcmp(buffer,"0")==0)
					msgwin_compiler_add_string(COLOR_BLACK,(buffer));
				}
			}
		pclose(in);

		/*freeing all the pointers*/
		g_free(doc_fn);
		g_free(user_ent);
		g_free(pwd_ent);
		g_free(probid_ent);
		g_free(lang);
		g_free(expression->str);
	}

	else 
	{
		/*print an error dialog box*/
		dialogs_show_msgbox(GTK_MESSAGE_INFO, "Solution file could not be found!");
		return;
	}

}




static void menu_item_activate_cb(GtkMenuItem *menuitem, gpointer gdata)
{
  	/*window setting*/
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_default_size(GTK_WINDOW(window), 200,40);
	gtk_window_set_title (GTK_WINDOW (window), "CodeSubmit Plugin");

    
    /*Creating the labels*/ 
	GtkWidget *user_label = gtk_label_new ("User Name : ");
    GtkWidget *pwd_label= gtk_label_new ("Password : ");
    GtkWidget *probid_label= gtk_label_new ("Problem Id : ");
    GtkWidget *server_label= gtk_label_new ("Server : ");
    GtkWidget *language_label= gtk_label_new ("Language : ");
    
	/*Creating user entries*/
	user_entry=gtk_entry_new();
	pwd_entry=gtk_entry_new();
	probid_entry=gtk_entry_new();
	
	/*Creating the combo boxes*/
	language_box=gtk_combo_box_new_text();
	gtk_combo_box_append_text(GTK_COMBO_BOX(language_box),"C");
	gtk_combo_box_append_text(GTK_COMBO_BOX(language_box),"C++");
	gtk_combo_box_append_text(GTK_COMBO_BOX(language_box),"Brainfuck");
	gtk_combo_box_append_text(GTK_COMBO_BOX(language_box),"Python");
	server_box=gtk_combo_box_new_text();
	gtk_combo_box_append_text(GTK_COMBO_BOX(server_box),"SPOJ");
	gtk_combo_box_append_text(GTK_COMBO_BOX(server_box),"CodeChef");
	
	/*Creating the buttons*/
	submit_button=gtk_button_new_with_label ("Submit!");
	save_button=gtk_button_new_with_label ("Save!");
    
    /*creating the table*/	
	GtkWidget *	table=gtk_table_new (2, 2, TRUE);
	gtk_table_attach_defaults (GTK_TABLE(table),user_label, 0, 1, 0, 1);
	gtk_table_attach_defaults (GTK_TABLE(table),user_entry, 1, 2, 0, 1);
	gtk_table_attach_defaults (GTK_TABLE(table),pwd_label, 0, 1, 1, 2);
	gtk_table_attach_defaults (GTK_TABLE(table),pwd_entry, 1, 2, 1, 2);
	gtk_table_attach_defaults (GTK_TABLE(table),probid_label, 0, 1, 2, 3);
	gtk_table_attach_defaults (GTK_TABLE(table),probid_entry, 1, 2, 2, 3);
	gtk_table_attach_defaults (GTK_TABLE(table),server_label, 0, 1, 3, 4);
	gtk_table_attach_defaults (GTK_TABLE(table),server_box, 1, 2, 3, 4);
	gtk_table_attach_defaults (GTK_TABLE(table),language_label, 0, 1, 4, 5);
	gtk_table_attach_defaults (GTK_TABLE(table),language_box, 1, 2, 4, 5);
	gtk_table_attach_defaults (GTK_TABLE(table),submit_button, 0, 1, 5, 6);
	gtk_table_attach_defaults (GTK_TABLE(table),save_button, 1, 2, 5, 6);
    gtk_container_add (GTK_CONTAINER (window), table);
    
    /*connecting signals to the button and tools menu option*/
	g_signal_connect (submit_button, "clicked",G_CALLBACK (spoj_submit_online_doit),NULL);
	g_signal_connect (save_button, "clicked",G_CALLBACK (spoj_save_doit),NULL);
   	spoj_preferences_load_doit();
	gtk_widget_show_all(window);

}




void plugin_init(GeanyData *data)
{	

    /*adding option in the Tools Menu*/
	GtkWidget *demo_item;
    tools_menu_item = gtk_menu_item_new_with_mnemonic(_("_CodeSubmit"));
    gtk_widget_show(tools_menu_item);
    gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu),tools_menu_item);
    g_signal_connect(tools_menu_item, "activate",G_CALLBACK(menu_item_activate_cb), NULL);
    ui_add_document_sensitive(tools_menu_item); 

	/*naive way of generating the python script and saving it in config directory*/
	
	FILE *p=fopen(g_build_path(G_DIR_SEPARATOR_S, geany->app->configdir, "plugins","y.py", NULL),"w");
	/*encoding of the python script*/
	fprintf(p,"import httplib\nimport urllib2\nimport re\nfrom poster.encode ");
	fprintf(p,"import multipart_encode\nfrom poster.streaminghttp import register");
	fprintf(p,"_openers\nimport sys\nimport BeautifulSoup\nregister_openers()\n");
	fprintf(p,"datagen, headers = multipart_encode({'login_user':sys.argv[1],'pa");
	fprintf(p,"ssword':sys.argv[2],\"subm_file\": open(sys.argv[3], \"rb\"),'lang':s");
	fprintf(p,"ys.argv[4],'problemcode':sys.argv[5]})\nurl=\"http://www.spoj.com/submit/");
	fprintf(p,"complete/\"\nrequest = urllib2.Request(url, datagen, headers)\nm = re.search");
	fprintf(p,"(r\'\"newSubmissionId\" value=\"(");
	fprintf(p,"\\");
	fprintf(p,"d+)\"/>\', urllib2.urlopen(request).read())\nsubmid=m.group(1)\nsubmid=str");
	fprintf(p,"(submid)\nstats=\"statusres_\"+submid\nmemory=\"statusmem_\"+submid\ntime=\"s");
	fprintf(p,"tatustime_\"+submid\ndata=urllib2.urlopen(\"https://www.spoj.com/status/\").read");
	fprintf(p,"()\nsoup = BeautifulSoup.BeautifulSoup(data)\ntim=soup.find(\"td\", {\"id\":time}");
	fprintf(p,")\nmem=soup.find(\"td\",{\"id\":memory})\nsta=soup.find(\"td\",{\"id\":stats})\nfina");
	fprintf(p,"l=\"Submission id: \"+submid+\"Status: \"+str(sta)+\"Time: \"+str(tim)+\"Memory: \"+");
	fprintf(p,"str(mem)\nsoup = BeautifulSoup.BeautifulSoup(final)\ngy=\'\'\nfor node in soup.findAll");
	fprintf(p,"(\'td\'):\n\tgy+=\'\'.join(node.findAll(text=True))\ny=str(gy)\ny=y.replace(\"google_ad");
	fprintf(p,"_section_start(weight=ignore) \",\"\")\ny=y.replace(\"google_ad_section_end\",\"\")\ny=y.");
	fprintf(p,"replace(\"\\n\",\"\")\ny=y.replace(\"  		\",\"\")\ny=y.replace(\"  		    	\",\"\")\nprint y\n");
	fclose(p);
/*The actual python script
 * 
 * import httplib
import urllib2
import re#import error exception
from poster.encode import multipart_encode#import error exception
from poster.streaminghttp import register_openers#import error exception
import sys
import BeautifulSoup#import error exception
register_openers()
datagen, headers = multipart_encode({'login_user':sys.argv[1],'password':sys.argv[2],"subm_file": open(sys.argv[3], "rb"),'lang':sys.argv[4],'problemcode':sys.argv[5]})
url="http://www.spoj.com/submit/complete/"
request = urllib2.Request(url, datagen, headers)
#opening url exception
m = re.search(r'"newSubmissionId" value="(\d+)"/>', urllib2.urlopen(request).read())
submid=m.group(1)
submid=str(submid)
stats="statusres_"+submid
memory="statusmem_"+submid
time="statustime_"+submid
data=urllib2.urlopen("https://www.spoj.com/status/").read()
#opening url exception
soup = BeautifulSoup.BeautifulSoup(data)
tim=soup.find("td",{"id":time})
mem=soup.find("td",{"id":memory})
sta=soup.find("td",{"id":stats})
final="Submission id: "+submid+"Status: "+str(sta)+"Time: "+str(tim)+"Memory: "+str(mem)
soup = BeautifulSoup.BeautifulSoup(final)
gy=''
for node in soup.findAll('td'):
    gy+=''.join(node.findAll(text=True))
y=str(gy)
y=y.replace("google_ad_section_start(weight=ignore) ","")
y=y.replace("google_ad_section_end","")
y=y.replace("\n","")
y=y.replace("  		","")
y=y.replace("  		    	","")
print y
*/


}


void plugin_cleanup(void)
{	
    gtk_widget_destroy(tools_menu_item);
	
}
