/*      helper_functions.c
 *
 *      Copyright 2011 Hans Alves <alves.h88@gmail.com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */


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
    /* initialize idx to 0 */
    unsigned long idx = 0;
    /* allocate array of size 1N */
    gpgme_key_t * key;
    ed->key_array = (gpgme_key_t*) malloc(SIZE * sizeof(gpgme_key_t));
    err = gpgme_op_keylist_start(ed->ctx, NULL, 0);
    while (!err)
    {
        key = ed->key_array + idx;
        err = gpgme_op_keylist_next(ed->ctx, key);
        if (err)
            break;
        if ((*key)->revoked  || /* key cannot be used */
            (*key)->expired  ||
            (*key)->disabled ||
            (*key)->invalid)
           gpgme_key_unref(*key);
        else /* key is valid */
            ++idx;
        if (idx >= size)
        {
            size += SIZE;
            ed->key_array = (gpgme_key_t*) realloc(ed->key_array, size * sizeof(gpgme_key_t));
        }
    }
    ed->nkeys = idx;
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
    /* initialize idx to 0 */
    unsigned long idx = 0;
    /* allocate array of size 1N */
    gpgme_key_t * key;
    ed->skey_array = (gpgme_key_t*) malloc(SIZE * sizeof(gpgme_key_t));
    err = gpgme_op_keylist_start(ed->ctx, NULL, 1);
    while (!err)
    {
        key = ed->skey_array + idx;
        err = gpgme_op_keylist_next(ed->ctx, key);
        if (err)
            break;
        if ((*key)->revoked  || /* key cannot be used */
            (*key)->expired  ||
            (*key)->disabled ||
            (*key)->invalid)
           gpgme_key_unref(*key);
        else /* key is valid */
            ++idx;
        if (idx >= size)
        {
            size += SIZE;
            ed->skey_array = (gpgme_key_t*) realloc(ed->skey_array, size * sizeof(gpgme_key_t));
        }
    }
    ed->nskeys = idx;
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
    /* gpgme_data_new_from_mem(buffer, text, size, 0); */
    GeanyDocument * doc = document_get_current();
    char * data;
    if (sci_has_selection(doc->editor->sci))
    {
        data = sci_get_selection_contents(doc->editor->sci);
    }
    else
    {
        data = sci_get_contents(doc->editor->sci, -1);
    }
    gpgme_data_new_from_mem(buffer, data, strlen(data), 1);
    free(data);
    gpgme_data_set_encoding(*buffer, GPGME_DATA_ENCODING_BINARY);
}

void geanypg_write_file(FILE * file)
{
#define BUFSIZE 2048
    unsigned long size;
    char buffer[BUFSIZE] = {0};
    GeanyDocument * doc = document_get_current();
    sci_start_undo_action(doc->editor->sci);
    if (sci_has_selection(doc->editor->sci))
    {   /* replace selected text
         * clear selection, cursor should be at the end or beginneng of the selection */
        scintilla_send_message(doc->editor->sci, SCI_REPLACESEL, 0, (sptr_t)"");
        while ((size = fread(buffer, 1, BUFSIZE, file)))
            /* add at the cursor */
            scintilla_send_message(doc->editor->sci, SCI_ADDTEXT, (uptr_t) size, (sptr_t) buffer);
    }
    else
    {   /* replace complete document */
        scintilla_send_message(doc->editor->sci, SCI_CLEARALL, 0, 0);
        while ((size = fread(buffer, 1, BUFSIZE, file)))
            scintilla_send_message(doc->editor->sci, SCI_APPENDTEXT, (uptr_t) size, (sptr_t) buffer);
    }
    sci_end_undo_action(doc->editor->sci);
#undef BUFSIZE
}
