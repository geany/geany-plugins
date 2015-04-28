/* MoreMarkup plugin. */

#include <gtk/gtk.h>
#include <glib-object.h>

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include "geanyplugin.h"
#include <string.h>
#include <gdk/gdk.h>
#include "marker.h"
#include "moremarkup.h"

PLUGIN_VERSION_CHECK(GEANY_API_VERSION)
PLUGIN_SET_INFO(_("MoreMarkup"), _("Provides More Markup"),
    "0.1", _("Pik"))

GeanyData *geany_data; 
GeanyFunctions  *geany_functions;
GeanyPlugin     *geany_plugin;

typedef struct _MoreMarkupPrivate MoreMarkupPrivate; 

#define MORE_MARKUP_GET_PRIVATE(obj)    (G_TYPE_INSTANCE_GET_PRIVATE((obj), MORE_MARKUP_TYPE, MoreMarkupPrivate))

struct _MoreMarkup {
    GObject parent;
};

struct _MoreMarkupClass {
    GObjectClass parent_class;
};

struct _MoreMarkupPrivate {
    gboolean plugin_active;
    gboolean visible;
    gint indic_numbers_table[512]; 
    gint64 documents_table[512];
    GtkToolItem *toolbar_more_markup_button;
    GtkWidget *more_markup_bar;
    GList *active_marker_labels;
    GHashTable * g_hash_table_new;
};

/* Keybindings */ 
/* TODO */

/* state */
enum
{
    PROP_0, 
    PROP_PLUGIN_ACTIVE
};

/* Default Behaviour on Mark */
enum {
    MM_DEFAULT_CLEAR,
    MM_DEFAULT_NEXT_IF_COLOR,
    MM_DEFAULT_NEXT_COLOR_AUTO,
};

static void more_markup_finalize (GObject *object);
static void more_markup_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec); 

G_DEFINE_TYPE(MoreMarkup, more_markup, G_TYPE_OBJECT) 

MoreMarkup *mdock;
MoreMarkupPrefs markup_prefs; 
GtkWidget *plugin_menu_item;

static void more_markup_class_init(MoreMarkupClass *cls) {
    g_warning("class init");
    GObjectClass *g_object_class;
    g_object_class = G_OBJECT_CLASS(cls);
    g_object_class->finalize = more_markup_finalize; 
    g_object_class->set_property = more_markup_set_property; 
    
    g_type_class_add_private(cls, sizeof(MoreMarkupPrivate));
    
    g_object_class_install_property(g_object_class, 
    PROP_PLUGIN_ACTIVE, 
    g_param_spec_boolean(
    "plugin-active", 
    "plugin-active", 
    "Whether to show a docked toolbar for search", 
    FALSE, 
    G_PARAM_WRITABLE));
}

static void more_markup_finalize(GObject *object) {
    g_warning("more_markup_finalize");
    MoreMarkupPrivate *priv = MORE_MARKUP_GET_PRIVATE(object);
    struct MarkupLabel *markup_label, *temp;
    GList *link = priv->active_marker_labels;
    if (priv->toolbar_more_markup_button != NULL) {
        gtk_widget_destroy(GTK_WIDGET(priv->toolbar_more_markup_button));
    }
    if (priv->more_markup_bar != NULL) {
        gtk_widget_destroy(GTK_WIDGET(priv->more_markup_bar));
    }
    if (link != NULL) {
        do {
            markup_label = link->data; 
            gtk_widget_destroy(markup_label->label_box);
            g_free(markup_label);
            link = g_list_delete_link(priv->active_marker_labels, link);
        } while (link != NULL);
    }
    if (link != NULL) {
        g_critical("Failed to free all extra markup_labels");
    }
    /* Clean marker data */
    marker_data_finalize();
    G_OBJECT_CLASS(more_markup_parent_class)->finalize(object);
}

static void on_refresh(G_GNUC_UNUSED guint key_id) {
    g_warning("refreshed");
}
static void on_menu_show(G_GNUC_UNUSED guint key_id) {
    g_warning("menu_show");
}

static gboolean label_unmark_cb(GtkWidget *label_box, GdkEventButton * event) {
    MoreMarkup *self = mdock;
    MoreMarkupPrivate *priv = MORE_MARKUP_GET_PRIVATE(self);
	GeanyDocument *doc = document_get_current();
    GList *link = priv->active_marker_labels;
    struct MarkupLabel *markup_label; 
	if (doc == NULL) {
		return TRUE; 
	}
    if (link == NULL) {
        g_critical("We got a zombie label");
        return TRUE;
    }
    do { 
        markup_label = link->data;
        if (markup_label->label_box == label_box) {
            g_warning("label_unmark_cb, labelbox %i, data->lpstrText s", label_box);
            marker_indicator_clear(markup_label->doc->editor->sci, markup_label->indic_number);
            gtk_widget_destroy(markup_label->label_box);
            //g_free(markup_label->text);
            g_free(markup_label);
            priv->active_marker_labels = g_list_delete_link(priv->active_marker_labels, link);
            return TRUE;
        }
    } while ((link = g_list_next(link)) != NULL);
    g_warning("Label not found");
    return TRUE;
}

void clear_markup_label(gint indic_number) {
    struct MarkupLabel *markup_label;
    MoreMarkup *self = mdock;
    MoreMarkupPrivate *priv = MORE_MARKUP_GET_PRIVATE(self);
    GList *link = priv->active_marker_labels;
    GeanyDocument *doc = document_get_current();
    if (doc == NULL || link == NULL) {
		return;
	}
    do {
        markup_label = link->data;
        if ((markup_label->indic_number == indic_number) && (doc == markup_label->doc)) {
            gtk_widget_destroy(markup_label->label_box);
            //g_free(markup_label->text);
            g_free(markup_label);
            priv->active_marker_labels = g_list_delete_link(priv->active_marker_labels, link);
            return;
        }
    } while((link = g_list_next(link)) != NULL);
    return;
}

static gboolean markup_clear_all_cb(GtkWidget *clear_all_button) {
    struct MarkupLabel *markup_label; 
    MoreMarkup *self = mdock;
    MoreMarkupPrivate *priv = MORE_MARKUP_GET_PRIVATE(self);
    GList *link, *temp;
    link = priv->active_marker_labels;
    GeanyDocument *doc = document_get_current();
    if (doc== NULL ) {
        return TRUE;
    }
    while (link != NULL) {
        markup_label = link->data; 
        if (doc == markup_label->doc) {
            marker_indicator_clear(markup_label->doc->editor->sci, markup_label->indic_number);
            gtk_widget_destroy(markup_label->label_box);
            //g_free(markup_label->text);
            g_free(markup_label);
            //g_warning("link next: %i", link->next);
            temp = link;
            link = link->next; 
            priv->active_marker_labels = g_list_delete_link(priv->active_marker_labels, temp);
        }
        else {
            link = link->next;
        }
    }
    return TRUE;
}

static void markup_add_label(void) {
    struct MarkupLabel *markup_label = g_malloc(sizeof(struct MarkupLabel));
    MarkerMatchInfo *info = get_last_marker_info();
    MoreMarkup *self = mdock;
    MoreMarkupPrivate *priv = MORE_MARKUP_GET_PRIVATE(self);
    if (info != NULL) {
        /* Duplicate the info since marker only stores the most recently marked entry */
        g_warning("add_mark_up_label -- info: %lli, %s", info, info->lpstrText); 
        GtkWidget *frame = gtk_frame_new (NULL); 
        GtkWidget *label = gtk_label_new((gchar*)info->lpstrText);
        GtkToolItem *item = gtk_tool_item_new();
        GtkWidget *label_box = gtk_event_box_new();
        
        gtk_widget_set_events(GTK_WIDGET(label_box), GDK_BUTTON_PRESS_MASK);
        gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
        gtk_container_add(GTK_CONTAINER(frame), GTK_WIDGET(label));
        gtk_container_add(GTK_CONTAINER(label_box), GTK_WIDGET(frame));
        gtk_container_add(GTK_CONTAINER(item), GTK_WIDGET(label_box));
        gtk_container_add(GTK_CONTAINER(priv->more_markup_bar), GTK_WIDGET(item));
        gtk_widget_show_all(GTK_WIDGET(item));
        
        markup_label->label_box = label_box;
        markup_label->indic_number = info->indic_number;
        markup_label->indic_style = info->indic_style;
        markup_label->sci_color = info->sci_color;
        markup_label->doc = document_get_current();
        //markup_label->text = g_strdup(info->lpstrText);
        
        priv->active_marker_labels = g_list_append(priv->active_marker_labels, markup_label);
        g_signal_connect(G_OBJECT(label_box), "button-press-event", G_CALLBACK(label_unmark_cb), NULL);
    }
    else {
        g_critical("last_info was NULL");
    }
}

static gboolean markup_set_cb(GtkWidget *mark_button, gint response, gpointer user_data) {
    g_warning("marker_set_cb--- found mdock %i", mdock);
    MarkerMatchInfo *info;
    MoreMarkup *self = mdock;
    gchar *entry_text;
    GtkWidget *color_button;
    GdkColor *color = malloc(sizeof(GdkColor));
    GtkTreeIter   iter;
	GtkTreeModel *model;
    GList *link;
    gint indic_number, indic_style, sci_color;
    
    /* Get scintilla indicator */
    GtkComboBox *combo = GTK_COMBO_BOX(g_object_get_data(user_data, "indicator"));
    if (gtk_combo_box_get_active_iter( combo, &iter )) {
        model = gtk_combo_box_get_model(combo);
        gtk_tree_model_get( model, &iter, 0, &indic_style, -1 );
    }
    
    /* get Gdk color */
    color_button = g_object_get_data(user_data, "color_button");
    gtk_color_button_get_color (GTK_COLOR_BUTTON(color_button), color);
    g_warning("Got a color(rgb): %x %x %x", color->red, color->blue, color->green);
    
    /* get entry text */
    entry_text = g_strdup(gtk_entry_get_text(GTK_ENTRY(user_data))); 

    /* Get indicator number */
    indic_number = -1;
    if (markup_prefs.default_on_mark == MM_DEFAULT_NEXT_IF_COLOR) {
        GeanyDocument *doc = document_get_current();
        guint i;
        struct MarkupLabel *markup_label;
        gint sci_color = _GDK_COLOR_TO_SCI_COLOR(color);
        MoreMarkupPrivate *priv = MORE_MARKUP_GET_PRIVATE(self);
        link = priv->active_marker_labels;
        if (link != NULL) {
            do {
                    markup_label = link->data; 
                    //g_warning("markuplabel->sci_color %i  sci_color  %i", markup_label->sci_color, sci_color);
                   /*
                    if (strcmp(markup_label->text, entry_text) != 0) {
                        continue;
                    }*/
                    if (markup_label->sci_color != sci_color) {
                        continue;
                    }
                    else if (markup_label->doc != doc) {
                        continue;
                    }
                    else if (markup_label->indic_style != indic_style) {
                        continue;
                    }
                    else {
                        indic_number = markup_label->indic_number;
                        break;
                    }
            } while ((link = g_list_next(link)) != NULL);
            /* Move this stuff */
            if (indic_number < 0) { 
                for (i=0; i<512; i++) {
                    if (priv->documents_table[i] == (gint64)doc) {
                        indic_number = priv->indic_numbers_table[i]++;
                        break;
                    }
                }
                if (indic_number < 0) {
                    for (i=0; i<512; i++) {
                        if (priv->documents_table[i] == 0) {
                            priv->documents_table[i] = (gint64)doc;
                            indic_number = priv->indic_numbers_table[i]++;
                            break;
                        }
                    } 
                }
            }
        }
        else {
            for (i=0; i<512; i++) {
                if (priv->documents_table[i] == 0) {
                    priv->documents_table[i] = (gint64)doc;
                    indic_number = priv->indic_numbers_table[i]++; 
                    break;
                }
            }
        }
    }
    else if (markup_prefs.default_on_mark == MM_DEFAULT_CLEAR) {
        indic_number = indic_style;
    }
    g_warning("indic set %i", indic_number);
    /* Call set function */
    gint count = on_marker_set(entry_text, indic_number, indic_style, color);
    g_warning("marker_set_cb -- got count %i", count);

    g_free(entry_text);
    gdk_color_free(color);
    
    /* If current indicator was cleared */
    if (count != -1) { 
        info = get_last_marker_info();
        clear_markup_label(info->indic_number);
        /* If text was marked */
        if (count > 0) {
            markup_add_label();
        }
    }
    return TRUE;
}

static GtkWidget *create_more_markup_bar(MoreMarkupPrefs *prefs) {
   
    GtkWidget *toolbar, *indicator_menu, *color_button;
    GtkToolItem *mark, *clear_all;
    GtkEntry *entry;
	GtkToolItem *entry_item, *indicator_item, *color_item;
    
    /* initialize toolbar */
    toolbar = gtk_toolbar_new();
    gtk_toolbar_set_icon_size(GTK_TOOLBAR(toolbar), GTK_ICON_SIZE_MENU);
    gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
    
    /* Mark and Clear-All buttons */
    mark = gtk_tool_button_new (NULL, "mark");
    clear_all= gtk_tool_button_new (NULL, "clear all");
    
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(mark), -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(clear_all), -1);
    
    /* Text Entry */
    entry = GTK_ENTRY(gtk_entry_new_with_max_length(120));
    entry_item = gtk_tool_item_new();
    gtk_container_add(GTK_CONTAINER(entry_item), GTK_WIDGET(entry));
    gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(entry_item));
    
    /* Color Chooser for indicator color */
    color_item = gtk_tool_item_new();
    color_button = gtk_color_button_new();
    //I think gdk_color_parse does not allocate memory? docs unclear
    GdkColor color;
    gdk_color_parse(markup_prefs.default_color, &color);
    gtk_color_button_set_color (GTK_COLOR_BUTTON(color_button), &color);
    
    gtk_container_add(GTK_CONTAINER(color_item), GTK_WIDGET(color_button));
    gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(color_item));
    
    /* Initialize combo-box for setting the Scintilla Indicator Type */
    GtkListStore *store = gtk_list_store_new (2, G_TYPE_INT, G_TYPE_STRING);
    GtkTreePath *path;
    GtkTreeIter iter;
    gint i =0;
    GtkCellRenderer *cell;
    gtk_list_store_insert_with_values(store, &iter, -1, 0, INDIC_PLAIN, 1, "Plain", -1);
    gtk_list_store_insert_with_values(store, &iter, -1, 0, INDIC_SQUIGGLE, 1, "Squiggle", -1);
    gtk_list_store_insert_with_values(store, &iter, -1, 0, INDIC_TT, 1, "TT", -1);
    gtk_list_store_insert_with_values(store, &iter, -1, 0, INDIC_DIAGONAL, 1, "Diagonal", -1);
    gtk_list_store_insert_with_values(store, &iter, -1, 0, INDIC_STRIKE, 1, "Strike", -1);
    gtk_list_store_insert_with_values(store, &iter, -1, 0, INDIC_ROUNDBOX, 1, "RoundBox", -1);
    
    indicator_menu = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));  
    indicator_item  = gtk_tool_item_new();
    /* for now */
    gtk_combo_box_set_active_iter (GTK_COMBO_BOX(indicator_menu), &iter);
    /* Create cell renderer. */
    cell = gtk_cell_renderer_text_new();
    /* Pack it to the combo box. */
    gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( indicator_menu ), cell, TRUE );
    /* Connect renderer to data source */
    gtk_cell_layout_set_attributes( GTK_CELL_LAYOUT( indicator_menu ), cell, "text", 1, NULL );
    
    gtk_container_add(GTK_CONTAINER(indicator_item), GTK_WIDGET(indicator_menu));
    gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(indicator_item));
    g_object_unref(G_OBJECT(store));
    gtk_widget_show_all(indicator_menu);
    
    /* Set context for entry */ 
    g_object_set_data(G_OBJECT(entry), "color_button", color_button);
    g_object_set_data(G_OBJECT(entry), "indicator", indicator_menu);
    
    /* Connect Signals */ 
    g_signal_connect(G_OBJECT(mark), "clicked", G_CALLBACK(markup_set_cb), entry);
    g_signal_connect(G_OBJECT(clear_all), "clicked", G_CALLBACK(markup_clear_all_cb), NULL);
    
    return toolbar;
}

static void more_markup_toggle_cb(GtkWidget *btn) {
    MoreMarkup *self = mdock;
    GtkWidget *vbox = ui_lookup_widget(geany_data->main_widgets->window, "vbox1");
    g_warning("is searchdock %i", IS_MORE_MARKUP(self));
    MoreMarkupPrivate *priv = MORE_MARKUP_GET_PRIVATE(self); 
    priv->visible = !priv->visible;
    g_warning("real search dock visible %i, more_markup_bar %lli, toolbar_item %lli", priv->visible, priv->more_markup_bar, priv->toolbar_more_markup_button);
    if (priv->more_markup_bar != NULL) {
        if (priv->visible) {
            gtk_widget_show_all(priv->more_markup_bar);
        /*   gtk_box_pack_start(GTK_BOX(vbox), priv->more_markup_bar, FALSE, FALSE, 1);
            gtk_box_reorder_child(GTK_BOX(vbox), priv->more_markup_bar, 1); */
        }
        else {
            gtk_widget_hide(priv->more_markup_bar);
        }
    }
}

/*
GtkWidget * more_markup_bar_new (void) {
    GtkBuilder *builder = gtk_builder_new();
    GError *error = NULL; 
    if (! gtk_builder_add_from_file(builder, PKGDATADIR"/searchdock/searchdockbar.ui", &error)) {
        g_critical(_("Failed to load UI definition, please check your installation, error message: %s"), error->message); 
        g_error_free(error);
        g_object_unref(builder); 
        return NULL;
    }
    GtkWidget *mdock_bar = GTK_WIDGET(gtk_builder_get_object(builder, "more_markup_frame")); 
    g_object_unref(builder); 
    return mdock_bar; 
} */

static void more_markup_toolbar_update(MoreMarkup *self) {
    MoreMarkupPrivate *priv = MORE_MARKUP_GET_PRIVATE(self);
    if (! priv->plugin_active) {
        if (priv->toolbar_more_markup_button != NULL) {
            gtk_widget_hide(GTK_WIDGET(priv->toolbar_more_markup_button));
        }
    }
    else {
        if (priv->toolbar_more_markup_button == NULL) {
            priv->toolbar_more_markup_button = gtk_tool_button_new(NULL, "More Markup"); 
            plugin_add_toolbar_item(geany_plugin, priv->toolbar_more_markup_button);
            ui_add_document_sensitive(GTK_WIDGET(priv->toolbar_more_markup_button));
            g_signal_connect(GTK_WIDGET(priv->toolbar_more_markup_button), "clicked", G_CALLBACK(more_markup_toggle_cb), self);
        }
        gtk_widget_show(GTK_WIDGET(priv->toolbar_more_markup_button));
    }
}

static void more_markup_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
    MoreMarkupPrivate *priv = MORE_MARKUP_GET_PRIVATE(object);
    switch (prop_id)
    {
        case PROP_PLUGIN_ACTIVE:
            g_warning("more_markup_set_property: PluginActive");
            priv->plugin_active = g_value_get_boolean(value);
            more_markup_toolbar_update(MORE_MARKUP(object));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

 /*
static void toggle_status(MoreMarkup *self, gpointer data) {
    MoreMarkupPrivate *priv = MORE_MARKUP_GET_PRIVATE(self);
    gboolean status =  gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(plugin_menu_item), "activate")));
    g_object_set(self, "plugin-active", status, NULL);
}*/

static void on_document_activate(GObject *geany_object, GeanyDocument *doc, MoreMarkup *self) {
    MoreMarkupPrivate *priv = MORE_MARKUP_GET_PRIVATE(self);
    struct MarkupLabel *markup_label; 
    GList *link = priv->active_marker_labels; 
    if (link == NULL) return;
    do {
        markup_label = link->data;
        if (markup_label->doc == doc) {
            gtk_widget_show(markup_label->label_box); 
        }
        else {
            gtk_widget_hide(markup_label->label_box);
        }
    } while((link = g_list_next(link)) != NULL);
}

static void on_document_close(GObject *geany_object, GeanyDocument *doc, MoreMarkup *self) {
    MoreMarkupPrivate *priv = MORE_MARKUP_GET_PRIVATE(self);
    struct MarkupLabel *markup_label; 
    GList *link = priv->active_marker_labels; 
    guint i;
    if (link == NULL) return;
    do {
        markup_label = link->data;
        if (markup_label->doc == doc) {
            gtk_widget_destroy(markup_label->label_box); 
            //g_free(markup_label->text);
            g_free(markup_label);
            priv->active_marker_labels = g_list_delete_link(priv->active_marker_labels, link);
        }
    } while((link = g_list_next(link)) != NULL);
    for (i=0; i< 512; i++) {
        if (priv->documents_table[i] == (gint64)doc) {
            priv->documents_table[i] = 0;
            priv->indic_numbers_table[i] = 0;
            break;
        }
    }
}

static void more_markup_connect_signals(MoreMarkup *self) {
    plugin_signal_connect(geany_plugin, NULL, "document-activate", TRUE, G_CALLBACK(on_document_activate), self);
	plugin_signal_connect(geany_plugin, NULL, "document-close", TRUE, G_CALLBACK(on_document_close), self);
}

/* For now */
/* Add to configure */
static void more_markup_prefs_init(MoreMarkupPrefs *prefs) {
    prefs->default_color = "#FFA700";
    prefs->default_indicator = 8;
    prefs->default_on_mark = MM_DEFAULT_CLEAR;
}

/* Created on plugin_active callback */
static void more_markup_init(MoreMarkup *self)
{
    MoreMarkupPrivate *priv = MORE_MARKUP_GET_PRIVATE(self);
    more_markup_prefs_init(&markup_prefs);
    marker_data_init();
    priv->toolbar_more_markup_button = NULL;
    priv->visible = FALSE; 
    priv->more_markup_bar = create_more_markup_bar(&markup_prefs);
    memset(priv->indic_numbers_table, 0, 512);
    memset(priv->documents_table, 0, 512);
    //priv->indic_numbers_table = g_hash_table_new(g_int_hash, g_int_equal);
    /* Make it bigger than any default scintilla style */
    more_markup_connect_signals(self);
    GtkWidget *vbox = ui_lookup_widget(geany_data->main_widgets->window, "vbox1");
    gtk_box_pack_start(GTK_BOX(vbox), priv->more_markup_bar, FALSE, FALSE, 1);
    gtk_box_reorder_child(GTK_BOX(vbox), priv->more_markup_bar, 2);
}


MoreMarkup *more_markup_new(gboolean plugin_active) {
    return g_object_new(MORE_MARKUP_TYPE, "plugin-active", plugin_active, NULL);
}

static void on_configure_response(GtkDialog *dialog, gint response, GtkComboBox *combo) {
    GtkTreeIter   iter;
	GtkTreeModel *model;
	
	if( gtk_combo_box_get_active_iter(combo, &iter)) {
		model = gtk_combo_box_get_model( combo );
		gtk_tree_model_get( model, &iter, 0, &markup_prefs.default_on_mark, -1 );
	}
}

GtkWidget *plugin_configure(GtkDialog *dialog)
{
    GtkWidget *label, *vbox, *configure_combo;

    /* example configuration dialog */
    vbox = gtk_vbox_new(FALSE, 6);

    /* add a label and a text entry to the dialog */
    label = gtk_label_new(_("Set Default Behaviour on Mark:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_container_add(GTK_CONTAINER(vbox), label);

    GtkListStore *store = gtk_list_store_new (2, G_TYPE_INT, G_TYPE_STRING);
    GtkTreePath *path;
    GtkTreeIter iter;
    GtkCellRenderer *cell;
    gtk_list_store_insert_with_values(store, &iter, -1, 0, MM_DEFAULT_CLEAR, 1, "Clear other markers of the same type by default", -1);
    gtk_list_store_insert_with_values(store, &iter, -1, 0, MM_DEFAULT_NEXT_IF_COLOR, 1, "Set a new marker of the same type if a different color is selected", -1);
    /* TODO */
    //gtk_list_store_insert_with_values(store, &iter, -1, 0, MM_DEFAULT_NEXT_COLOR_AUTO, 1, "Autoincrement in the color and set a new marker ", -1);
    
    configure_combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));  
    /* for now */
    gtk_combo_box_set_active (GTK_COMBO_BOX(configure_combo), 0);
    /* Create cell renderer. */
    cell = gtk_cell_renderer_text_new();
    /* Pack it to the combo box. */
    gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( configure_combo ), cell, TRUE );
    /* Connect renderer to data source */
    gtk_cell_layout_set_attributes( GTK_CELL_LAYOUT( configure_combo ), cell, "text", 1, NULL );
    
    GtkWidget *align = 	gtk_alignment_new(0,0,1,0);
    gtk_container_add(GTK_CONTAINER(align), GTK_WIDGET(configure_combo));
    gtk_container_add(GTK_CONTAINER(vbox), GTK_WIDGET(align));
    gtk_widget_show_all(vbox);

    /* Connect a callback for when the user clicks a dialog button */
    g_signal_connect(dialog, "response", G_CALLBACK(on_configure_response), configure_combo);
    return vbox;
}

void plugin_init(GeanyData *data) { 
    mdock = more_markup_new(TRUE); //plugin-active, visible
    plugin_module_make_resident(geany_plugin);
}

void plugin_cleanup(void) {
    g_warning("searchdock cleanup");
    g_object_unref(mdock);
}







