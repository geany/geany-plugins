#include <geanyplugin.h>
#include <stdlib.h>
#include <string.h>
GeanyPlugin     *geany_plugin;
GeanyData       *geany_data;
GeanyFunctions  *geany_functions;
GeanyDocument   *yof;
PLUGIN_VERSION_CHECK(211)

PLUGIN_SET_INFO("SPOJ Submissions", "Tool to submit spoj problem submissions from geany itself!",
                "1.0", "Mayank Jha <mayank25080562@gmail.com>");
static GtkWidget *ii;
static GtkWidget *window;
static GtkWidget *frame;
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
GList *glist=NULL;

static GtkWidget *submit_button;
static GtkWidget *save_button;

static GtkWidget *table;

static void item_activate_cb(GtkMenuItem *menuitem, gpointer gdata)
{
   gtk_widget_show_all(window);
   // dialogs_show_msgbox(GTK_MESSAGE_INFO, "Hello World");
}
void spoj_preferences_load_doit(void)
{gchar bufa[30],bufb[30],bufc[30];
int thenum;
GtkWidget *widget;
FILE *f=fopen("/home/mj/code/datafile.txt","r"); /*the path where the data is stored like username,password etc.*/
fscanf(f,"%s %s %s %d",bufa,bufb,bufc,&thenum);
gtk_entry_set_text(GTK_ENTRY(user_entry),(const gchar*)(bufa));
gtk_entry_set_text(GTK_ENTRY(pwd_entry),(const gchar*)(bufb));
gtk_entry_set_text(GTK_ENTRY(probid_entry),(const gchar*)(bufc));;
gtk_combo_box_set_active(GTK_COMBO_BOX(language_box),thenum);
}

static void spoj_save_doit(GtkWidget *save_button, gpointer gdata)
{
FILE *da=fopen("/home/mj/code/datafile.txt","w");/*the path where the data is stored like username,password etc.*/
fputs(gtk_entry_get_text(GTK_ENTRY(user_entry)),da);
fputc(' ',da);
fputs(gtk_entry_get_text(GTK_ENTRY(pwd_entry)),da);
fputc(' ',da);
fputs(gtk_entry_get_text(GTK_ENTRY(probid_entry)),da);
fputc(' ',da);
fprintf(da,"%d",gtk_combo_box_get_active(GTK_COMBO_BOX(language_box)));
//fputs(gtk_entry_get_text(GTK_ENTRY(ui_lookup_widget(ui_widgets.prefs_dialog, "passwordentry1"))),da);
//fputc(' ',da);
//puts(gtk_entry_get_text(GTK_ENTRY(ui_lookup_widget(ui_widgets.prefs_dialog, "problemid_entry1"))),da);
//fputc(' ',da);
//fprintf(da,"%d",gtk_combo_box_get_active(GTK_COMBO_BOX(ui_lookup_widget(ui_widgets.prefs_dialog, "language_box2"))));
}


static void spoj_submit_online_doit(GtkWidget *submit_button, gpointer gdata)
{gchar buffer[50];
yof=document_get_current(); 
gchar *a,*b,*c,*d;
gchar *lang;
lang=(gchar*)malloc(sizeof(gchar)*3);
a=(gchar*)malloc(sizeof(gchar)*20);
b=(gchar*)malloc(sizeof(gchar)*20);
c=(gchar*)malloc(sizeof(gchar)*20);
d=(gchar*)malloc(sizeof(gchar)*20);
int i;
gchar *expression=(gchar*)malloc(sizeof(gchar)*1000);
a=yof->file_name;
//msgwin_compiler_add_string(COLOR_BLACK,"fuck yeah!");


strcpy(b,gtk_entry_get_text(GTK_ENTRY(user_entry)));
strcpy(c,gtk_entry_get_text(GTK_ENTRY(pwd_entry)));
strcpy(d,gtk_entry_get_text(GTK_ENTRY(probid_entry)));
i=gtk_combo_box_get_active(GTK_COMBO_BOX(language_box));
msgwin_compiler_add_string(COLOR_BLACK,c);
switch(i)
{
case 1:{strcpy(lang,"11");break;}//c
case 2:{strcpy(lang,"41");break;}//c++
case 3:{strcpy(lang,"10");break;}//python
case 4:{strcpy(lang,"12");break;}//brainfuck
}
//msgwin_compiler_add_string(COLOR_BLACK,lang);
expression=strcpy(expression,"python /home/mj/code/y.py ");
//the path where the python script which does the submission
//is present 
strcat(expression,b);
strcat(expression," ");
strcat(expression,c);
strcat(expression," ");
strcat(expression,a);
strcat(expression," ");
strcat(expression,lang);
strcat(expression," ");
strcat(expression,d);

FILE* in;
extern FILE *popen();
if(!(in = popen((const gchar*)(expression), "r")))
    {return;}
 
    while(fgets(buffer, 50, in) != NULL) {
    
    		msgwin_compiler_add_string(COLOR_BLACK,(buffer));}
pclose(in);
 		

}

void plugin_init(GeanyData *data)
{
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
  gtk_window_set_default_size(GTK_WINDOW(window), 100,100);
  gtk_window_set_title (GTK_WINDOW (window), "SPOJ Plugin");

    main_menu_item = gtk_menu_item_new_with_mnemonic("Hello Wor");
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
    gtk_widget_destroy(main_menu_item);
}
