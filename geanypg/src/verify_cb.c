//      verify_cb.c
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

#include "geanypg.h"

char * geanypg_choose_sig()
{
    int response;
    char * file = NULL;
    GtkWidget * dialog = gtk_file_chooser_dialog_new("Open a signature file",
                                                     GTK_WINDOW(geany->main_widgets->window),
                                                     GTK_FILE_CHOOSER_ACTION_OPEN,
                                                     GTK_STOCK_OPEN, GTK_RESPONSE_OK,
                                                     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                     NULL);
    gtk_widget_show_all(dialog);
    response = gtk_dialog_run(GTK_DIALOG(dialog));
    if (response == GTK_RESPONSE_OK)
        file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    gtk_widget_destroy(dialog);
    return file;
}

void geanypg_verify(encrypt_data * ed, char * signame)
{
    gpgme_data_t sig, text;
    gpgme_error_t err;
    FILE * sigfile = fopen(signame, "r");
    gpgme_data_new_from_stream(&sig, sigfile);
    geanypg_load_buffer(&text);

    err = gpgme_op_verify(ed->ctx, sig, text, NULL);

    if (err != GPG_ERR_NO_ERROR)
        geanypg_show_err_msg(err);
    else
        geanypg_handle_signatures(ed);

    gpgme_data_release(sig);
    gpgme_data_release(text);
    fclose(sigfile);
}

void geanypg_verify_cb(GtkMenuItem * menuitem, gpointer user_data)
{
    char * sigfile = NULL;
    encrypt_data ed;
    geanypg_init_ed(&ed);
    gpgme_error_t err = gpgme_new(&ed.ctx);
    if (err && geanypg_show_err_msg(err))
        return;
    gpgme_set_protocol(ed.ctx, GPGME_PROTOCOL_OpenPGP);
    gpgme_set_passphrase_cb(ed.ctx, geanypg_passphrase_cb, NULL);
    if (geanypg_get_keys(&ed) && geanypg_get_secret_keys(&ed))
    {
        sigfile = geanypg_choose_sig();
        if (sigfile)
        {
            geanypg_verify(&ed, sigfile);
            g_free(sigfile);
        }
    }
    geanypg_release_keys(&ed);
    gpgme_release(ed.ctx);
}
