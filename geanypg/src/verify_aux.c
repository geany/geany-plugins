/*      verify_aux.c
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


static void geanypg_get_keys_with_fp(encrypt_data * ed, char * buffer)
{
    unsigned long idx, found = 0;
    char empty_string = '\0';
    for (idx = 0; idx < ed->nkeys && ! found; ++idx)
    {
        gpgme_subkey_t sub = ed->key_array[idx]->subkeys;
        while (sub && !found)
        {
            if (sub->fpr && !strncmp(sub->fpr, buffer, 40))
            {

                char * name = (ed->key_array[idx]->uids && ed->key_array[idx]->uids->name)
                               ?
                               ed->key_array[idx]->uids->name
                               :
                               &empty_string;
                char * email = (ed->key_array[idx]->uids && ed->key_array[idx]->uids->email)
                                ?
                                ed->key_array[idx]->uids->email
                                :
                                &empty_string;
                if (strlen(name) + strlen(email) < 500)
                    sprintf(buffer, "%s <%s>", name, email);
                else
                {
                    char tmp[62] = {0};
                    strncpy(tmp, buffer, 41);
                    sprintf(buffer, "%s %s", _("a key with fingerprint"), tmp);
                }
                found = 1;
            }
            sub = sub->next;
        }
    }
}

const char * geanypg_validity(gpgme_validity_t validity)
{
    switch (validity)
    {
        case GPGME_VALIDITY_UNKNOWN:  return _("unknown");
        case GPGME_VALIDITY_UNDEFINED:return _("undefined");
        case GPGME_VALIDITY_NEVER:    return _("never");
        case GPGME_VALIDITY_MARGINAL: return _("marginal");
        case GPGME_VALIDITY_FULL:     return _("full");
        case GPGME_VALIDITY_ULTIMATE: return _("ultimate");
        default : return _("[bad validity value]");
    }
    return _("[bad validity value]");
}

static char * geanypg_summary(gpgme_sigsum_t summary, char * buffer)
{ /* buffer should be more than 105 bytes long */
  if (summary & GPGME_SIGSUM_VALID)       strcat(buffer, _(" valid"));
  if (summary & GPGME_SIGSUM_GREEN)       strcat(buffer, _(" green"));
  if (summary & GPGME_SIGSUM_RED)         strcat(buffer, _(" red"));
  if (summary & GPGME_SIGSUM_KEY_REVOKED) strcat(buffer, _(" revoked"));
  if (summary & GPGME_SIGSUM_KEY_EXPIRED) strcat(buffer, _(" key-expired"));
  if (summary & GPGME_SIGSUM_SIG_EXPIRED) strcat(buffer, _(" sig-expired"));
  if (summary & GPGME_SIGSUM_KEY_MISSING) strcat(buffer, _(" key-missing"));
  if (summary & GPGME_SIGSUM_CRL_MISSING) strcat(buffer, _(" crl-missing"));
  if (summary & GPGME_SIGSUM_CRL_TOO_OLD) strcat(buffer, _(" crl-too-old"));
  if (summary & GPGME_SIGSUM_BAD_POLICY)  strcat(buffer, _(" bad-policy"));
  if (summary & GPGME_SIGSUM_SYS_ERROR)   strcat(buffer, _(" sys-error"));
  return buffer;
}

static gchar * geanypg_result(gpgme_signature_t sig)
{
    char summary[128] = {0};
    const char * pubkey = gpgme_pubkey_algo_name(sig->pubkey_algo);
    const char * hash = gpgme_hash_algo_name(sig->hash_algo);
    char created[64] = {0};
    char expires[64] = {0};
    if (sig->timestamp)
        strncpy(created, ctime((time_t*)&sig->timestamp), sizeof(created) - 1);
    else
        strcpy(created, _("Unknown\n"));

    if (sig->exp_timestamp)
        strncpy(expires, ctime((time_t*)&sig->exp_timestamp), sizeof(expires) - 1);
    else
        strcpy(expires, _("Unknown\n"));

    return g_strdup_printf(
        _("status ....: %s\n"
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
          "notations .: %s\n"),
        gpgme_strerror(sig->status),
        geanypg_summary(sig->summary, summary),
        sig->fpr ? sig->fpr : _("[None]"),
        created,
        expires,
        geanypg_validity(sig->validity),
        gpgme_strerror(sig->status),
        pubkey ? pubkey : _("Unknown"),
        hash ? hash : _("Unknown"),
        sig->pka_address ? sig->pka_address : _("[None]"),
        sig->pka_trust == 0 ? _("n/a") : sig->pka_trust == 1 ? _("bad") : sig->pka_trust == 2 ? _("okay"): _("RFU"),
        sig->wrong_key_usage ? _(" wrong-key-usage") : "", sig->chain_model ? _(" chain-model") : "",
        sig->notations ? _("yes") : _("no"));
}
void geanypg_check_sig(encrypt_data * ed, gpgme_signature_t sig)
{
    GtkWidget * dialog;
    char buffer[512] = {0};
    gchar * result;
    strncpy(buffer, sig->fpr, 40);
    buffer[40] = 0;
    geanypg_get_keys_with_fp(ed, buffer);
    result = geanypg_result(sig);

    dialog = gtk_message_dialog_new_with_markup(GTK_WINDOW(geany->main_widgets->window),
                                                GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                GTK_MESSAGE_INFO,
                                                GTK_BUTTONS_OK,
                                                "%s %s\n<tt>%s</tt>",
                                                _("Found a signature from"),
                                                buffer,
                                                result);
    gtk_window_set_title(GTK_WINDOW(dialog), _("Signature"));

    gtk_dialog_run(GTK_DIALOG(dialog));
    g_free(result);
    gtk_widget_destroy(dialog);
}

void geanypg_handle_signatures(encrypt_data * ed, int need_error)
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
    if (!verified && need_error)
    {
        fprintf(stderr, "GeanyPG: %s\n", _("Could not find verification results"));
        dialogs_show_msgbox(GTK_MESSAGE_ERROR, _("Error, could not find verification results"));
    }
}
