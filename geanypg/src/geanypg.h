//      geanypg.h
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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <locale.h>

#include <geanyplugin.h>
#include <Scintilla.h>
#include <gpgme.h>

#define SIZE 32

enum
{
    READ = 0,
    WRITE = 1
};

typedef struct
{
    gpgme_ctx_t ctx;
    gpgme_key_t * key_array;
    unsigned long nkeys;
    gpgme_key_t * skey_array;
    unsigned long nskeys;
} encrypt_data;

extern GeanyPlugin     *geany_plugin;
extern GeanyData       *geany_data;
extern GeanyFunctions  *geany_functions;

// auxiliary functions (aux.c)
void geanypg_init_ed(encrypt_data * ed);
int geanypg_get_keys(encrypt_data * ed);
int geanypg_get_secret_keys(encrypt_data * ed);
void geanypg_release_keys(encrypt_data * ed);
void geanypg_load_buffer(gpgme_data_t * buffer);
void geanypg_write_file(FILE * file);

// some more auxiliary functions (verify_aux.c)
void geanypg_handle_signatures(encrypt_data * ed);
void geanypg_check_sig(encrypt_data * ed, gpgme_signature_t sig);

// dialogs
int geanypg_encrypt_selection_dialog(encrypt_data * ed, gpgme_key_t ** selected, int * sign);
int geanypg_sign_selection_dialog(encrypt_data * ed);
gpgme_error_t geanypg_show_err_msg(gpgme_error_t err);

// callback functions
void geanypg_encrypt_cb(GtkMenuItem * menuitem, gpointer user_data);
void geanypg_sign_cb(GtkMenuItem * menuitem, gpointer user_data);
void geanypg_decrypt_cb(GtkMenuItem * menuitem, gpointer user_data);
void geanypg_verify_cb(GtkMenuItem * menuitem, gpointer user_data);

// pinentry callback
gpgme_error_t geanypg_passphrase_cb(void *hook,
                                    const char *uid_hint,
                                    const char *passphrase_info,
                                    int prev_was_bad ,
                                    int fd);
