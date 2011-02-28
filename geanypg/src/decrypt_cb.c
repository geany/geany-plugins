//      decrypt_cb.c
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

void geanypg_decrypt_verify(encrypt_data * ed)
{
    gpgme_data_t plain, cipher;
    gpgme_error_t err;
    FILE * tempfile;

    tempfile = tmpfile();
    if (!(tempfile))
    {
        fprintf(stderr, "GEANYPG: couldn't create tempfile: %s.\n", strerror(errno));
        return ;
    }
    gpgme_data_new_from_stream(&plain, tempfile);

    geanypg_load_buffer(&cipher);

    err = gpgme_op_decrypt_verify(ed->ctx, cipher, plain);
    if (gpgme_err_code(err) == GPG_ERR_NO_DATA) // no encription, but maybe signatures
    {
        // maybe reaload cipher
        gpgme_data_release(cipher);
        geanypg_load_buffer(&cipher);
        rewind(tempfile);
        err = gpgme_op_verify(ed->ctx, cipher, NULL, plain);
    }
    if (err != GPG_ERR_NO_ERROR)
        geanypg_show_err_msg(err);
    else
    {
        rewind(tempfile);
        geanypg_write_file(tempfile);
        geanypg_handle_signatures(ed);
    }

    fclose(tempfile);
    // release buffers
    gpgme_data_release(cipher);
    gpgme_data_release(plain);
}

void geanypg_decrypt_cb(GtkMenuItem * menuitem, gpointer user_data)
{
    encrypt_data ed;
    geanypg_init_ed(&ed);
    gpgme_error_t err = gpgme_new(&ed.ctx);
    if (err && geanypg_show_err_msg(err))
        return;
    gpgme_set_protocol(ed.ctx, GPGME_PROTOCOL_OpenPGP);
    gpgme_set_passphrase_cb(ed.ctx, geanypg_passphrase_cb, NULL);
    if (geanypg_get_keys(&ed) && geanypg_get_secret_keys(&ed))
        geanypg_decrypt_verify(&ed);
    geanypg_release_keys(&ed);
    gpgme_release(ed.ctx);
}
