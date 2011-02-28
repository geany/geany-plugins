//      encrypt_cb.c
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

void geanypg_encrypt(encrypt_data * ed, gpgme_key_t * recp, int sign)
{   // FACTORIZE
    gpgme_data_t plain, cipher;
    gpgme_error_t err;
    FILE * tempfile;
    tempfile = tmpfile();
    if (!(tempfile))
    {
        fprintf(stderr, "GEANYPG: couldn't create tempfile: %s.\n", strerror(errno));
        return ;
    }
    gpgme_data_new_from_stream(&cipher, tempfile);
    gpgme_data_set_encoding(cipher, GPGME_DATA_ENCODING_ARMOR);

    geanypg_load_buffer(&plain);

    // do the actual encryption
    if (sign)
        err = gpgme_op_encrypt_sign(ed->ctx, recp, 0, plain, cipher);
    else
        err = gpgme_op_encrypt(ed->ctx, recp, 0, plain, cipher);
    if (err != GPG_ERR_NO_ERROR && gpgme_err_code(err) != GPG_ERR_CANCELED)
        geanypg_show_err_msg(err);
    else
    {
        rewind(tempfile);
        geanypg_write_file(tempfile);
    }

    fclose(tempfile);
    // release buffers
    gpgme_data_release(plain);
    gpgme_data_release(cipher);
}

void geanypg_encrypt_cb(GtkMenuItem * menuitem, gpointer user_data)
{
    int sign;
    encrypt_data ed;
    geanypg_init_ed(&ed);
    gpgme_error_t err = gpgme_new(&ed.ctx);
    if (err && geanypg_show_err_msg(err))
        return;
    gpgme_set_armor(ed.ctx, 1);
    gpgme_set_passphrase_cb(ed.ctx, geanypg_passphrase_cb, NULL);
    if (geanypg_get_keys(&ed) && geanypg_get_secret_keys(&ed))
    {
        gpgme_key_t * recp = NULL;
        if (geanypg_encrypt_selection_dialog(&ed, &recp, &sign))
        {
            if (*recp)
                geanypg_encrypt(&ed, recp, sign);
            else if (dialogs_show_question("No recipients were selected,\nuse symetric cipher?"))
                geanypg_encrypt(&ed, NULL, sign);
        }
        if (recp)
            free(recp);
    }
    geanypg_release_keys(&ed);
    gpgme_release(ed.ctx);
}
