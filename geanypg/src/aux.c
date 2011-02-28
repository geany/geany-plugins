//      aux.c
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

void geanypg_init_ed(encrypt_data * ed)
{
    ed->key_array = NULL;
    ed->nkeys = 0;
    ed->skey_array = NULL;
    ed->nskeys = 0;
}

int geanypg_get_keys(encrypt_data * ed)
{
    gpgme_error_t err;
    unsigned long size = SIZE;
    //initialize index to 0
    unsigned long index = 0;
    //allocate array of size 1N
    ed->key_array = (gpgme_key_t*) malloc(SIZE * sizeof(gpgme_key_t));
    err = gpgme_op_keylist_start(ed->ctx, NULL, 0);
    while (!err)
    {
        err = gpgme_op_keylist_next(ed->ctx, ed->key_array + index);
        if (err)
            break;
        ++index;
        if (index >= size)
        {
            size += SIZE;
            ed->key_array = (gpgme_key_t*) realloc(ed->key_array, size * sizeof(gpgme_key_t));
        }
    }
    ed->nkeys = index;
    if (gpg_err_code(err) != GPG_ERR_EOF)
    {
        geanypg_show_err_msg(err);
        return 0;
    }
    return 1;
}

int geanypg_get_secret_keys(encrypt_data * ed)
{
    gpgme_error_t err;
    unsigned long size = SIZE;
    //initialize index to 0
    unsigned long index = 0;
    //allocate array of size 1N
    ed->skey_array = (gpgme_key_t*) malloc(SIZE * sizeof(gpgme_key_t));
    err = gpgme_op_keylist_start(ed->ctx, NULL, 1);
    while (!err)
    {
        err = gpgme_op_keylist_next(ed->ctx, ed->skey_array + index);
        if (err)
            break;
        ++index;
        if (index >= size)
        {
            size += SIZE;
            ed->skey_array = (gpgme_key_t*) realloc(ed->skey_array, size * sizeof(gpgme_key_t));
        }
    }
    ed->nskeys = index;
    if (gpg_err_code(err) != GPG_ERR_EOF)
    {
        geanypg_show_err_msg(err);
        return 0;
    }
    return 1;
}

void geanypg_release_keys(encrypt_data * ed)
{
    gpgme_key_t * ptr;
    if (ed->key_array)
    {
        ptr = ed->key_array;
        while (ptr < ed->key_array + ed->nkeys)
            gpgme_key_unref(*(ptr++));
        free(ed->key_array);
        ed->key_array = NULL;
        ed->nkeys = 0;
    }
    if (ed->skey_array)
    {
        ptr = ed->skey_array;
        while (ptr < ed->skey_array + ed->nskeys)
            gpgme_key_unref(*(ptr++));
        free(ed->skey_array);
        ed->skey_array = NULL;
        ed->nskeys = 0;
    }
}


void geanypg_load_buffer(gpgme_data_t * buffer)
{
    //gpgme_data_new_from_mem(buffer, text, size, 0);
    GeanyDocument * doc = document_get_current();
    //SCI_GETSELECTIONSTART-SCI_GETSELECTIONEND
    char * data = NULL;
    unsigned long sstart = scintilla_send_message(doc->editor->sci, SCI_GETSELECTIONSTART, 0, 0);
    unsigned long send = scintilla_send_message(doc->editor->sci, SCI_GETSELECTIONEND, 0, 0);
    unsigned long size = 0;
    if (sstart - send)
    {
        size = scintilla_send_message(doc->editor->sci, SCI_GETSELTEXT, 0, 0);
        data = (char *) malloc(size + 1);
        scintilla_send_message(doc->editor->sci, SCI_GETSELTEXT, 0, (sptr_t)data);
        gpgme_data_new_from_mem(buffer, data, size, 1);
    }
    else
    {
        size = scintilla_send_message(doc->editor->sci, SCI_GETLENGTH, 0, 0);
        data = (char *) malloc(size + 1);
        scintilla_send_message(doc->editor->sci, SCI_GETTEXT, (uptr_t)(size + 1), (sptr_t)data);
        gpgme_data_new_from_mem(buffer, data, size, 1);
    }
    if (data) // if there is no text data may still be NULL
        free(data);
    gpgme_data_set_encoding(*buffer, GPGME_DATA_ENCODING_BINARY);
}

void geanypg_write_file(FILE * file)
{
    unsigned bufsize = 2048;
    unsigned long size;
    char buffer[bufsize];
    GeanyDocument * doc = document_get_current();
    if (abs(sci_get_selection_start(doc->editor->sci) - sci_get_selection_end(doc->editor->sci)))
    {   // replace selected text
        // clear selection, cursor should be at the end or beginneng of the selection
        scintilla_send_message(doc->editor->sci, SCI_REPLACESEL, 0, (sptr_t)"");
        while ((size = fread(buffer, 1, bufsize, file)))
            // add at the cursor
            scintilla_send_message(doc->editor->sci, SCI_ADDTEXT, (uptr_t) size, (sptr_t) buffer);
    }
    else
    {   //replace complete document
        scintilla_send_message(doc->editor->sci, SCI_CLEARALL, 0, 0);
        while ((size = fread(buffer, 1, bufsize, file)))
            scintilla_send_message(doc->editor->sci, SCI_APPENDTEXT, (uptr_t) size, (sptr_t) buffer);
    }
}
