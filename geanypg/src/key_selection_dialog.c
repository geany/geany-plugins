//      key_selection_dialog.c
//
//      Copyright 2011 Hans Alves <alves.h88@gmail.com>
//
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU General Public License as published by
//      the Free Software Foundation; either version 2 of the License, or
//      (at your option) any later version.
//
//      This program is distributed in the hope that it will be useful,
//      but WITHOUT ANY WARRANTY; without even the implied warranty of
//      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//      GNU General Public License for more details.
//
//      You should have received a copy of the GNU General Public License
//      along with this program; if not, write to the Free Software
//      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
//      MA 02110-1301, USA.

// something about premature optimization, it still needs to be done

#include "geanypg.h"

enum
{
    TOGGLE_COLUMN,
    RECIPIENT_COLUMN,
    KEYID_COLUMN,
    N_COLUMNS
};

typedef struct
{
    GtkListStore * store;
    gint column;
} listdata;

static void geanypg_toggled_cb(GtkCellRendererToggle * cell_renderer,
                        gchar * path,
                        listdata * udata)
{
    GtkTreeIter iter;
    gboolean value;
    if (!udata) return;
    if (gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(udata->store), &iter, path))
    {
        gtk_tree_model_get(GTK_TREE_MODEL(udata->store), &iter, udata->column, &value, -1);
        value = !value;
        gtk_list_store_set(udata->store, &iter, udata->column, value, -1);
    }
}

static GtkListStore * geanypg_makelist(gpgme_key_t * key_array, unsigned long nkeys, int addnone)
{
    GtkTreeIter iter;
    unsigned long index;
    GtkListStore * list = gtk_list_store_new(N_COLUMNS, G_TYPE_BOOLEAN, G_TYPE_STRING,  G_TYPE_STRING);
    if (addnone)
    {
        gtk_list_store_append(list, &iter);
        gtk_list_store_set(list, &iter,
                           TOGGLE_COLUMN, FALSE,
                           RECIPIENT_COLUMN, "None",
                           KEYID_COLUMN, "",
                           -1);
    }
    for (index = 0; index < nkeys; ++index)
    {
        char * name = (key_array[index]->uids && key_array[index]->uids->name) ? key_array[index]->uids->name : "";
        char * email = (key_array[index]->uids && key_array[index]->uids->email) ? key_array[index]->uids->email : "";
        char buffer[strlen(name) + strlen(email) + 7];
        sprintf(buffer, "%s    <%s>", name, email);
        gtk_list_store_append(list, &iter);
        gtk_list_store_set(list, &iter,
                           TOGGLE_COLUMN, FALSE,
                           RECIPIENT_COLUMN, buffer,
                           KEYID_COLUMN, key_array[index]->subkeys->keyid,
                           -1);
    }
    return list;
}

static GtkWidget * geanypg_combobox(GtkListStore * list)
{
    GtkWidget * combobox = gtk_combo_box_new_with_model(GTK_TREE_MODEL(list));
    GtkCellRenderer * cell1 = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combobox), cell1, FALSE);
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(combobox), cell1,
                                  "text", RECIPIENT_COLUMN);
    return combobox;
}

static GtkWidget * geanypg_listview(GtkListStore * list, listdata * data)
{
    GtkTreeViewColumn * column;
    GtkCellRenderer * togglerenderer, * textrenderer;
    GtkWidget * listview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list));
    // checkbox column
    togglerenderer = gtk_cell_renderer_toggle_new();
    g_signal_connect(G_OBJECT(togglerenderer), "toggled", G_CALLBACK(geanypg_toggled_cb), NULL);
    column = gtk_tree_view_column_new_with_attributes("?",
                                                     togglerenderer,
                                                     "active", TOGGLE_COLUMN,
                                                     NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(listview), column);
    data->store = list;
    data->column = TOGGLE_COLUMN;
    g_signal_connect(G_OBJECT(togglerenderer), "toggled", G_CALLBACK(geanypg_toggled_cb), (gpointer) data);
    // recipient column
    textrenderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("recipient",
                                                      textrenderer,
                                                      "text", RECIPIENT_COLUMN,
                                                      NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(listview), column);
    // keyid column
    column = gtk_tree_view_column_new_with_attributes("keyid",
                                                      textrenderer,
                                                      "text", KEYID_COLUMN,
                                                      NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(listview), column);
    return listview;
}

int geanypg_encrypt_selection_dialog(encrypt_data * ed, gpgme_key_t ** selected, int * sign)
{
    GtkWidget * dialog = gtk_dialog_new();
    unsigned long index, sindex, capacity;
    int response;
    GtkWidget * contentarea, * listview, * scrollwin, * combobox;
    GtkTreeIter iter;
    listdata data;
    gboolean active;

    *sign = 0;

    GtkListStore * list = geanypg_makelist(ed->key_array, ed->nkeys, 0);
    listview = geanypg_listview(list, &data);
    scrollwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrollwin),
                        listview);
    gtk_widget_set_size_request(scrollwin, 500, 160);
    combobox = geanypg_combobox(geanypg_makelist(ed->skey_array, ed->nskeys, 1));


    contentarea = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_box_pack_start(GTK_BOX(contentarea), gtk_label_new("Please select any recipients"), FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(contentarea), scrollwin, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(contentarea), gtk_label_new("Sign the message as:"), FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(contentarea), combobox, FALSE, FALSE, 0);


    // add ok and cancel buttons
    gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_OK, GTK_RESPONSE_OK);
    gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);

    gtk_window_set_title(GTK_WINDOW(dialog), "Select recipients");
    gtk_widget_show_all(dialog);
    // make sure dialog is destroyed when user responds
    response = gtk_dialog_run(GTK_DIALOG(dialog));
    if (response == GTK_RESPONSE_CANCEL)
    {
        gtk_widget_destroy(dialog);
        return 0;
    }
    index = gtk_combo_box_get_active(GTK_COMBO_BOX(combobox));
    if (index && index <= ed->nskeys)
    {
        *sign = 1;
        gpgme_signers_add(ed->ctx, ed->skey_array[index - 1]); // -1 because the first option is `None'
    }
    // try to loop all the keys in the list
    // if they are active (the user checked the checkbox in front of the key)
    // add it to the selected array, finaly make sure that the array
    // is NULL terminated
    if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(list), &iter))
    {
        capacity = SIZE;
        *selected = (gpgme_key_t*) malloc(SIZE * sizeof(gpgme_key_t));
        index = 0;
        sindex = 0;
        gtk_tree_model_get(GTK_TREE_MODEL(list), &iter, TOGGLE_COLUMN, &active, -1);
        if (active)
                (*selected)[sindex++] = ed->key_array[index];

        while (gtk_tree_model_iter_next(GTK_TREE_MODEL(list), &iter))
        {
            ++index;
            gtk_tree_model_get(GTK_TREE_MODEL(list), &iter, TOGGLE_COLUMN, &active, -1);
            if (active)
                (*selected)[sindex++] = ed->key_array[index];
            if (sindex >= capacity - 1)
            {
                capacity += SIZE;
                *selected = (gpgme_key_t*) realloc(*selected, capacity * sizeof(gpgme_key_t));
            }
        }
        (*selected)[sindex] = NULL;
    }
    else
    {
        gtk_widget_destroy(dialog);
        return 0;
    }

    gtk_widget_destroy(dialog);
    return 1;
}

int geanypg_sign_selection_dialog(encrypt_data * ed)
{
    GtkWidget * dialog = gtk_dialog_new();
    unsigned long index;
    int response;
    GtkWidget * contentarea = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget * combobox = geanypg_combobox(
                                geanypg_makelist(ed->skey_array, ed->nskeys, 0));

    gtk_box_pack_start(GTK_BOX(contentarea), gtk_label_new("Choose a key to sign with:"), FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(contentarea), combobox, TRUE, TRUE, 0);

    // add ok and cancel buttons
    gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_OK, GTK_RESPONSE_OK);
    gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);

    gtk_widget_show_all(dialog);
    gtk_window_set_title(GTK_WINDOW(dialog), "Select signer");
    // make sure dialog is destroyed when user responds
    response = gtk_dialog_run(GTK_DIALOG(dialog));
    if (response == GTK_RESPONSE_CANCEL)
    {
        gtk_widget_destroy(dialog);
        return 0;
    }
    index = gtk_combo_box_get_active(GTK_COMBO_BOX(combobox));
    gpgme_signers_clear(ed->ctx);
    if (index < ed->nskeys)
        gpgme_signers_add(ed->ctx, ed->skey_array[index]);

    gtk_widget_destroy(dialog);
    return 1;
}
