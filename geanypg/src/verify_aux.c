//      verify_aux.c
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


void geanypg_get_keys_with_fp(encrypt_data * ed, char * buffer)
{
    unsigned long index, found = 0;
    for (index = 0; index < ed->nkeys && ! found; ++index)
    {
        gpgme_subkey_t sub = ed->key_array[index]->subkeys;
        while (sub && !found)
        {
            if (sub->fpr && !strncmp(sub->fpr, buffer, 40))
            {

                char * name = (ed->key_array[index]->uids && ed->key_array[index]->uids->name) ? ed->key_array[index]->uids->name : "";
                char * email = (ed->key_array[index]->uids && ed->key_array[index]->uids->email) ? ed->key_array[index]->uids->email : "";
                if (strlen(name) + strlen(email) < 500)
                    sprintf(buffer, "%s <%s>", name, email);
                else
                {
                    char tmp[62];
                    strncpy(tmp, buffer, 41);
                    sprintf(buffer, "a key with fingerprint %s", tmp);
                }
                found = 1;
            }
            sub = sub->next;
        }
    }
}

static const char * geanypg_validity(gpgme_sigsum_t summary)
{
    switch (summary)
    {
        case GPGME_VALIDITY_UNKNOWN:  return "unknown";
        case GPGME_VALIDITY_UNDEFINED:return "undefined";
        case GPGME_VALIDITY_NEVER:    return "never";
        case GPGME_VALIDITY_MARGINAL: return "marginal";
        case GPGME_VALIDITY_FULL:     return "full";
        case GPGME_VALIDITY_ULTIMATE: return "ultimate";
        default : return "[bad validity value]";
    }
    return "[bad validity value]";
}

static char * geanypg_summary(gpgme_sigsum_t summary, char * buffer)
{ // buffer should be more than 105 bytes long
  if (summary & GPGME_SIGSUM_VALID)       strcat(buffer, " valid");
  if (summary & GPGME_SIGSUM_GREEN)       strcat(buffer, " green");
  if (summary & GPGME_SIGSUM_RED)         strcat(buffer, " red");
  if (summary & GPGME_SIGSUM_KEY_REVOKED) strcat(buffer, " revoked");
  if (summary & GPGME_SIGSUM_KEY_EXPIRED) strcat(buffer, " key-expired");
  if (summary & GPGME_SIGSUM_SIG_EXPIRED) strcat(buffer, " sig-expired");
  if (summary & GPGME_SIGSUM_KEY_MISSING) strcat(buffer, " key-missing");
  if (summary & GPGME_SIGSUM_CRL_MISSING) strcat(buffer, " crl-missing");
  if (summary & GPGME_SIGSUM_CRL_TOO_OLD) strcat(buffer, " crl-too-old");
  if (summary & GPGME_SIGSUM_BAD_POLICY)  strcat(buffer, " bad-policy");
  if (summary & GPGME_SIGSUM_SYS_ERROR)   strcat(buffer, " sys-error");
  return buffer;
}

static char * geanypg_result(gpgme_signature_t sig)
{
    char format[] =
    "status ....: %s\n"
    "summary ...:%s\n"
    "fingerprint: %s\n"
    "created ...: %s"
    "expires ...: %s"
    "validity ..: %s\n"
    "val.reason : %s\n"
    "pubkey algo: %s\n"
    "digest algo: %s\n"
    "pka address: %s\n"
    "pka trust .: %s\n"
    "other flags:%s%s\n"
    "notations .: %s\n"; // 210 characters
    char * buffer = (char *)calloc(2048, 1); // everything together probably won't be more
                                          // than 1061 characters, but we don't want to
                                          // take risks
    char summary[128];
    const char * pubkey = gpgme_pubkey_algo_name(sig->pubkey_algo);
    const char * hash = gpgme_hash_algo_name(sig->hash_algo);
    char created[64];
    char expires[64];
    if (sig->timestamp)
        strncpy(created, ctime((time_t*)&sig->timestamp), 64);
    else
        strcpy(created, "Unknown\n");

    if (sig->exp_timestamp)
        strncpy(expires, ctime((time_t*)&sig->exp_timestamp), 64);
    else
        strcpy(expires, "Unknown\n");

    memset(summary, 0, 128);
    sprintf(buffer, format,
        gpgme_strerror(sig->status),            // probably won't be more than 128
        geanypg_summary(sig->summary, summary), // max 105 characters
        sig->fpr ? sig->fpr : "[None]",         // max 40 characters
        created,                                // probably about 24 characters
        expires,                                // probably about 24 characters
        geanypg_validity(sig->validity),        // max 11 characters
        gpgme_strerror(sig->status),            // probably won't be more than 128
        pubkey ? pubkey : "Unknown",            // probably won't be more than 32
        hash ? hash : "Unknown",            // probably won't be more than 32
        sig->pka_address ? sig->pka_address : "[None]", // probably won't be more than 128
        sig->pka_trust == 0 ? "n/a" : sig->pka_trust == 1 ? "bad" : sig->pka_trust == 2 ? "okay": "RFU", // max 4 characters
        sig->wrong_key_usage ? " wrong-key-usage" : "", sig->chain_model ? " chain-model" : "",          // max 28 characters
        sig->notations ? "yes" : "no");         // max 3 characters
    return buffer;
}
void geanypg_check_sig(encrypt_data * ed, gpgme_signature_t sig)
{
    GtkWidget * dialog;
    gpgme_sigsum_t summary;
    char buffer[512];
    strncpy(buffer, sig->fpr, 40);
    buffer[40] = 0;
    geanypg_get_keys_with_fp(ed, buffer);
    summary = sig->summary;
    char * result = geanypg_result(sig);

    dialog = gtk_message_dialog_new_with_markup(GTK_WINDOW(geany->main_widgets->window),
                                                GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                GTK_MESSAGE_INFO,
                                                GTK_BUTTONS_OK,
                                                "Found a signature from %s\n<tt>%s</tt>",
                                                buffer,
                                                result);
    gtk_window_set_title(GTK_WINDOW(dialog), "Signature");

    gtk_dialog_run(GTK_DIALOG(dialog));
    free(result);
    gtk_widget_destroy(dialog);
}

void geanypg_handle_signatures(encrypt_data * ed)
{
    int verified = 0;
    gpgme_verify_result_t vres = gpgme_op_verify_result(ed->ctx);
    if (vres)
    {
        gpgme_signature_t sig = vres->signatures;
        while (sig)
        {
            geanypg_check_sig(ed, sig);
            sig = sig->next;
            verified = 1;
        }
    }
    if (!verified)
    {
        fprintf(stderr, "GEANYPG: could not find verification results\n");
        dialogs_show_msgbox(GTK_MESSAGE_ERROR, "Error, could not find verification results");
    }
}
