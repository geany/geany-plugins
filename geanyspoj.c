#include <geanyplugin.h>
#include <stdlib.h>
#include <string.h>
GeanyPlugin     *geany_plugin;
GeanyData       *geany_data;
GeanyFunctions  *geany_functions;
GeanyDocument 	*soln_doc;
PLUGIN_VERSION_CHECK(211)

PLUGIN_SET_INFO("SPOJ Submissions", "Tool to submit spoj problem submissions from geany itself!",
                "1.0", "Mayank Jha <mayank25080562@gmail.com>");

static GtkWidget *window;
static GtkWidget *main_menu_item = NULL;
static GtkWidget *user_label;
static GtkWidget *pwd_label;
static GtkWidget *probid_label;
static GtkWidget *language_label;
static GtkWidget *server_label;

static GtkWidget *user_entry;
static GtkWidget *pwd_entry;
static GtkWidget *probid_entry;

static GtkWidget *language_box;
static GtkWidget *server_box;
static GtkComboBox *combo_box;


static GtkWidget *submit_button;
static GtkWidget *save_button;

static GtkWidget *table;

static void item_activate_cb(GtkMenuItem *menuitem, gpointer gdata)
{
   gtk_widget_show_all(window);
}
void spoj_preferences_load_doit(void)
{
	gchar doc_fn[40],user_ent[30],pwd_ent[30],probid_ent[30];
int active_choice;

FILE *f=fopen(g_build_path(G_DIR_SEPARATOR_S, geany->app->configdir, "plugins", "spojplugin",
    "datafile.txt", NULL),"r"); /*the path where the data is stored like username,password etc.*/
if(!f)
{

fscanf(f,"%s %s %s %d",doc_fn,user_ent,pwd_ent,&active_choice);
gtk_entry_set_text(GTK_ENTRY(user_entry),(doc_fn));
gtk_entry_set_text(GTK_ENTRY(pwd_entry),(user_ent));
gtk_entry_set_text(GTK_ENTRY(probid_entry),(probid_ent));;
gtk_combo_box_set_active(GTK_COMBO_BOX(language_box),active_choice);

}
else
{
//print an error dialog box and then 
return;	
}
}

static void spoj_save_doit(GtkWidget *save_button, gpointer gdata)
{
FILE *da=fopen(g_build_path(G_DIR_SEPARATOR_S, geany->app->configdir, "plugins", "spojplugin",
    "datafile.txt", NULL) ,"w");/*the path where the data is stored like username,password etc.*/
if(!da)
{
fprintf(da, "%s %s %s %d", 
  gtk_entry_get_text(GTK_ENTRY(user_entry)),
  gtk_entry_get_text(GTK_ENTRY(pwd_entry)),
  gtk_entry_get_text(GTK_ENTRY(probid_entry)),
  gtk_combo_box_get_active(GTK_COMBO_BOX(language_box)));

fclose(da); 
}
else
{
	dialogs_show_msgbox(GTK_MESSAGE_INFO, "Data File could not be opened!");
	//print an error mesage and then 
	return;
}
}


static void spoj_submit_online_doit(GtkWidget *submit_button, gpointer gdata)
{
gchar buffer[50];
soln_doc=document_get_current(); 
if(!soln_doc)
{
gchar *doc_fn, *user_ent, *pwd_ent, *probid_ent,*lang;

lang=malloc(sizeof(gchar)*3);

int active_choice;
//gchar *expression=malloc(sizeof(gchar)*1000);
doc_fn=soln_doc->file_name;


user_ent=g_strdup(gtk_entry_get_text(GTK_ENTRY(user_entry)));
pwd_ent=g_strdup(gtk_entry_get_text(GTK_ENTRY(pwd_entry)));
probid_ent=g_strdup(gtk_entry_get_text(GTK_ENTRY(probid_entry)));


active_choice=gtk_combo_box_get_active(GTK_COMBO_BOX(language_box));

switch(active_choice)
{
case 1:{strcpy(lang,"11");break;}//c
case 2:{strcpy(lang,"41");break;}//c++
case 3:{strcpy(lang,"10");break;}//python
case 4:{strcpy(lang,"12");break;}//brainfuck
};
//presently built only for probids c,c++,python and brainfuck will extend it later

GString *expression=gstring_new("python ");
g_string_append(expression,g_build_path(G_DIR_SEPARATOR_S, geany->app->configdir, "plugins", "spojplugin",
    "y.py", NULL));
g_string_append(expression,user_ent);
g_string_append(expression," ");
g_string_append(expression,pwd_ent);
g_string_append(expression," ");
g_string_append(expression,doc_fn);
g_string_append(expression," ");
g_string_append(expression,lang);
g_string_append(expression," ");
g_string_append(expression,probid_ent);

FILE* in=popen(expression->str, "r");
if(!in )
    {//display a dialog box showing the error
		dialogs_show_msgbox(GTK_MESSAGE_INFO, "Terminal Pipe could not be opened!");
		return;
	}
 
    while(fgets(buffer, 50, in) != NULL) {
    
    		msgwin_compiler_add_string(COLOR_BLACK,(buffer));}
pclose(in);

//freeing all the pointers
g_free(doc_fn);
g_free(user_ent);
g_free(pwd_ent);
g_free(probid_ent);
g_free(lang);
}
else 
{
	dialogs_show_msgbox(GTK_MESSAGE_INFO, "Solution file could not be found!");
//print a error dialog box
return;
}
}

void plugin_init(GeanyData *data)
{
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
  gtk_window_set_default_size(GTK_WINDOW(window), 100,100);
  gtk_window_set_title (GTK_WINDOW (window), "SPOJ Plugin");

    main_menu_item = gtk_menu_item_new_with_mnemonic("SPOJ");
    gtk_widget_show(main_menu_item);
    gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu),
        main_menu_item);


    
    //Creating the labels 
    
    user_label = gtk_label_new ("User Name : ");
    pwd_label= gtk_label_new ("Password : ");
    probid_label= gtk_label_new ("Problem Id : ");
    server_label= gtk_label_new ("Server : ");
    language_label= gtk_label_new ("Language : ");
    
	//Creating user entries
	user_entry=gtk_entry_new();
	pwd_entry=gtk_entry_new();
	probid_entry=gtk_entry_new();
	
	//Creating the combo boxes
	language_box=gtk_combo_box_new_text();
	gtk_combo_box_append_text(GTK_COMBO_BOX(language_box),"C");
	gtk_combo_box_append_text(GTK_COMBO_BOX(language_box),"C++");
	gtk_combo_box_append_text(GTK_COMBO_BOX(language_box),"Brainfuck");
	gtk_combo_box_append_text(GTK_COMBO_BOX(language_box),"Python");
	server_box=gtk_combo_box_new_text();
	gtk_combo_box_append_text(GTK_COMBO_BOX(server_box),"SPOJ");
	gtk_combo_box_append_text(GTK_COMBO_BOX(server_box),"CodeChef");
	//Creating the buttons
	submit_button=gtk_button_new_with_label ("Submit!");
	save_button=gtk_button_new_with_label ("Save!");
	  g_signal_connect (submit_button, "clicked", 
                      G_CALLBACK (spoj_submit_online_doit),
                      NULL);
	  g_signal_connect (save_button, "clicked", 
                      G_CALLBACK (spoj_save_doit),
                      NULL);
     //creating a table
			table=gtk_table_new (2, 8, TRUE);
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
    g_signal_connect(main_menu_item, "activate",G_CALLBACK(item_activate_cb), NULL);
	spoj_preferences_load_doit();
}

void plugin_cleanup(void)
{	
    gtk_widget_destroy(window);
    gtk_widget_destroy(main_menu_item);
}
