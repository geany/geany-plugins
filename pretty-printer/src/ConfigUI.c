/**
 *   Copyright (C) 2009  Cedric Tabin
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License along
 *   with this program; if not, write to the Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "ConfigUI.h"

/*======================= FUNCTIONS ====================================================================*/

/*static GtkWidget* createTwoOptionsBox(const char* label, const char* checkBox1, const char* checkBox2, gboolean cb1Active, gboolean cb2Active, GtkWidget** option1, GtkWidget** option2);*/
static GtkWidget* createThreeOptionsBox(const char* label, const char* checkBox1, const char* checkBox2, const char* checkBox3, gboolean cb1Active, gboolean cb2Active, gboolean cb3Active, GtkWidget** option1, GtkWidget** option2, GtkWidget** option3);
static GtkWidget* createEmptyTextOptions(gboolean emptyNodeStripping, gboolean emptyNodeStrippingSpace, gboolean forceEmptyNodeSplit);
static GtkWidget* createIndentationOptions(char indentation, int count);
static GtkWidget* createLineReturnOptions(const char* lineReturn);

/*============================================ PRIVATE PROPERTIES ======================================*/

static GtkWidget* commentOneLine;
static GtkWidget* commentInline;
static GtkWidget* commentAlign;
static GtkWidget* textOneLine;
static GtkWidget* textInline;
static GtkWidget* textAlign;
static GtkWidget* cdataOneLine;
static GtkWidget* cdataInline;
static GtkWidget* cdataAlign;
static GtkWidget* emptyNodeStripping;
static GtkWidget* emptyNodeStrippingSpace;
static GtkWidget* emptyNodeSplit;
static GtkWidget* indentationChar;
static GtkWidget* indentationCount;
static GtkWidget* lineBreak;

/*============================================= PUBLIC FUNCTIONS ========================================*/

/* redeclaration of extern variable */
PrettyPrintingOptions* prettyPrintingOptions;

/* Will never be used, just here for example */
GtkWidget* createPrettyPrinterConfigUI(GtkDialog * dialog)
{
    PrettyPrintingOptions* ppo;
    GtkWidget* container;
    GtkWidget* leftBox;
    GtkWidget* rightBox;
    GtkWidget* commentOptions;
    GtkWidget* textOptions;
    GtkWidget* cdataOptions;
    GtkWidget* emptyOptions;
    GtkWidget* indentationOptions;
    GtkWidget* lineReturnOptions;

    /* default printing options */
    if (prettyPrintingOptions == NULL) { prettyPrintingOptions = createDefaultPrettyPrintingOptions(); }
    ppo = prettyPrintingOptions;

    container = gtk_hbox_new(FALSE, 10);

    leftBox = gtk_vbox_new(FALSE, 6);
    commentOptions = createThreeOptionsBox(_("Comments"), _("Put on one line"), _("Inline if possible"), _("Alignment"), ppo->oneLineComment, ppo->inlineComment, ppo->alignComment, &commentOneLine, &commentInline, &commentAlign);
    textOptions = createThreeOptionsBox(_("Text nodes"), _("Put on one line"), _("Inline if possible"), _("Alignment"), ppo->oneLineText, ppo->inlineText, ppo->alignText, &textOneLine, &textInline, &textAlign);
    cdataOptions = createThreeOptionsBox(_("CDATA"), _("Put on one line"), _("Inline if possible"), _("Alignment"), ppo->oneLineCdata, ppo->inlineCdata, ppo->alignCdata, &cdataOneLine, &cdataInline, &cdataAlign);
    emptyOptions = createEmptyTextOptions(ppo->emptyNodeStripping, ppo->emptyNodeStrippingSpace, ppo->forceEmptyNodeSplit);
    indentationOptions = createIndentationOptions(ppo->indentChar, ppo->indentLength);
    lineReturnOptions = createLineReturnOptions(ppo->newLineChars);

    gtk_box_pack_start(GTK_BOX(leftBox), commentOptions, FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(leftBox), textOptions, FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(leftBox), cdataOptions, FALSE, FALSE, 3);

    rightBox = gtk_vbox_new(FALSE, 6);
    gtk_box_pack_start(GTK_BOX(rightBox), emptyOptions, FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(rightBox), indentationOptions, FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(rightBox), lineReturnOptions, FALSE, FALSE, 3);

    gtk_box_pack_start(GTK_BOX(container), leftBox, FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(container), rightBox, FALSE, FALSE, 3);

    gtk_widget_show_all(container);
    return container;
}

static void
fetchSettingsFromConfigUI(PrettyPrintingOptions* ppo)
{
    int breakStyle;

    if (ppo == NULL) return;
    ppo->oneLineComment = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(commentOneLine));
    ppo->inlineComment = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(commentInline));
    ppo->alignComment = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(commentAlign));

    ppo->oneLineText = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(textOneLine));
    ppo->inlineText = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(textInline));
    ppo->alignText = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(textAlign));

    ppo->oneLineCdata = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cdataOneLine));
    ppo->inlineCdata = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cdataInline));
    ppo->alignCdata = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cdataAlign));

    ppo->emptyNodeStripping = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(emptyNodeStripping));
    ppo->emptyNodeStrippingSpace = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(emptyNodeStrippingSpace));
    ppo->forceEmptyNodeSplit = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(emptyNodeSplit));

    ppo->indentLength = gtk_spin_button_get_value(GTK_SPIN_BUTTON(indentationCount));
    ppo->indentChar = gtk_combo_box_get_active(GTK_COMBO_BOX(indentationChar))==0 ? '\t' : ' ';

    breakStyle = gtk_combo_box_get_active(GTK_COMBO_BOX(lineBreak));
    g_free ((gpointer)ppo->newLineChars);
    if (breakStyle == 0) ppo->newLineChars = g_strdup("\r");
    else if (breakStyle == 1) ppo->newLineChars = g_strdup("\n");
    else ppo->newLineChars = g_strdup("\r\n");
}

static gchar *
prefsToData (PrettyPrintingOptions* ppo,
             gsize* size,
             GError** error)
{
    GKeyFile *kf;
    gchar    *contents;

    kf = g_key_file_new ();

    g_key_file_set_string (kf, "pretty-printer", "newLineChars", ppo->newLineChars);
    g_key_file_set_integer (kf, "pretty-printer", "indentChar", (int)ppo->indentChar);
    g_key_file_set_integer (kf, "pretty-printer", "indentLength", ppo->indentLength);
    g_key_file_set_boolean (kf, "pretty-printer", "oneLineText", ppo->oneLineText);
    g_key_file_set_boolean (kf, "pretty-printer", "inlineText", ppo->inlineText);
    g_key_file_set_boolean (kf, "pretty-printer", "oneLineComment", ppo->oneLineComment);
    g_key_file_set_boolean (kf, "pretty-printer", "inlineComment", ppo->inlineComment);
    g_key_file_set_boolean (kf, "pretty-printer", "oneLineCdata", ppo->oneLineCdata);
    g_key_file_set_boolean (kf, "pretty-printer", "inlineCdata", ppo->inlineCdata);
    g_key_file_set_boolean (kf, "pretty-printer", "emptyNodeStripping", ppo->emptyNodeStripping);
    g_key_file_set_boolean (kf, "pretty-printer", "emptyNodeStrippingSpace", ppo->emptyNodeStrippingSpace);
    g_key_file_set_boolean (kf, "pretty-printer", "forceEmptyNodeSplit", ppo->forceEmptyNodeSplit);
    g_key_file_set_boolean (kf, "pretty-printer", "trimLeadingWhites", ppo->trimLeadingWhites);
    g_key_file_set_boolean (kf, "pretty-printer", "trimTrailingWhites", ppo->trimTrailingWhites);
    g_key_file_set_boolean (kf, "pretty-printer", "alignComment", ppo->alignComment);
    g_key_file_set_boolean (kf, "pretty-printer", "alignText", ppo->alignText);
    g_key_file_set_boolean (kf, "pretty-printer", "alignCdata", ppo->alignCdata);

    contents = g_key_file_to_data (kf, size, error);
    g_key_file_free (kf);
    return contents;
}

static gboolean
prefsFromData (PrettyPrintingOptions* ppo,
               const gchar* contents,
               gssize size,
               GError** error)
{
    GKeyFile *kf;

    g_return_val_if_fail (contents != NULL, FALSE);

    kf = g_key_file_new ();

    if (!g_key_file_load_from_data (kf, contents, size,
             G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS,
             error))
    {
        g_key_file_free (kf);
        return FALSE;
    }

    if (g_key_file_has_key (kf, "pretty-printer", "newLineChars", NULL))
    {
        g_free ((gpointer)ppo->newLineChars);
        ppo->newLineChars = g_key_file_get_string (kf, "pretty-printer", "newLineChars", error);
    }
    if (g_key_file_has_key (kf, "pretty-printer", "indentChar", NULL))
    {
        ppo->indentChar = (char)g_key_file_get_integer (kf, "pretty-printer", "indentChar", error);
    }
    if (g_key_file_has_key (kf, "pretty-printer", "indentLength", NULL))
    {
        ppo->indentLength = g_key_file_get_integer (kf, "pretty-printer", "indentLength", error);
    }
    if (g_key_file_has_key (kf, "pretty-printer", "oneLineText", NULL))
    {
        ppo->oneLineText = g_key_file_get_boolean (kf, "pretty-printer", "oneLineText", error);
    }
    if (g_key_file_has_key (kf, "pretty-printer", "inlineText", NULL))
    {
        ppo->inlineText = g_key_file_get_boolean (kf, "pretty-printer", "inlineText", error);
    }
    if (g_key_file_has_key (kf, "pretty-printer", "oneLineComment", NULL))
    {
        ppo->oneLineComment = g_key_file_get_boolean (kf, "pretty-printer", "oneLineComment", error);
    }
    if (g_key_file_has_key (kf, "pretty-printer", "inlineComment", NULL))
    {
        ppo->inlineComment = g_key_file_get_boolean (kf, "pretty-printer", "inlineComment", error);
    }
    if (g_key_file_has_key (kf, "pretty-printer", "oneLineCdata", NULL))
    {
        ppo->oneLineCdata = g_key_file_get_boolean (kf, "pretty-printer", "oneLineCdata", error);
    }
    if (g_key_file_has_key (kf, "pretty-printer", "inlineCdata", NULL))
    {
        ppo->inlineCdata = g_key_file_get_boolean (kf, "pretty-printer", "inlineCdata", error);
    }
    if (g_key_file_has_key (kf, "pretty-printer", "emptyNodeStripping", NULL))
    {
        ppo->emptyNodeStripping = g_key_file_get_boolean (kf, "pretty-printer", "emptyNodeStripping", error);
    }
    if (g_key_file_has_key (kf, "pretty-printer", "emptyNodeStrippingSpace", NULL))
    {
        ppo->emptyNodeStrippingSpace = g_key_file_get_boolean (kf, "pretty-printer", "emptyNodeStrippingSpace", error);
    }
    if (g_key_file_has_key (kf, "pretty-printer", "forceEmptyNodeSplit", NULL))
    {
        ppo->forceEmptyNodeSplit = g_key_file_get_boolean (kf, "pretty-printer", "forceEmptyNodeSplit", error);
    }
    if (g_key_file_has_key (kf, "pretty-printer", "trimLeadingWhites", NULL))
    {
        ppo->trimLeadingWhites = g_key_file_get_boolean (kf, "pretty-printer", "trimLeadingWhites", error);
    }
    if (g_key_file_has_key (kf, "pretty-printer", "trimTrailingWhites", NULL))
    {
        ppo->trimTrailingWhites = g_key_file_get_boolean (kf, "pretty-printer", "trimTrailingWhites", error);
    }
    if (g_key_file_has_key (kf, "pretty-printer", "alignComment", NULL))
    {
        ppo->alignComment = g_key_file_get_boolean (kf, "pretty-printer", "alignComment", error);
    }
    if (g_key_file_has_key (kf, "pretty-printer", "alignText", NULL))
    {
        ppo->alignText = g_key_file_get_boolean (kf, "pretty-printer", "alignText", error);
    }
    if (g_key_file_has_key (kf, "pretty-printer", "alignCdata", NULL))
    {
        ppo->alignCdata = g_key_file_get_boolean (kf, "pretty-printer", "alignCdata", error);
    }

    g_key_file_free (kf);
    return TRUE;
}

gboolean
prefsLoad (const gchar* filename,
           GError** error)
{
    PrettyPrintingOptions* ppo;
    gchar  *contents = NULL;
    gsize   size = 0;

    g_return_val_if_fail (filename != NULL, FALSE);

    /* default printing options */
    if (prettyPrintingOptions == NULL) { prettyPrintingOptions = createDefaultPrettyPrintingOptions(); }
    ppo = prettyPrintingOptions;

    if (!g_file_get_contents (filename, &contents, &size, error))
        return FALSE;
    if (!prefsFromData (ppo, contents, size, error))
    {
        g_free (contents);
        return FALSE;
    }
    g_free (contents);
    return TRUE;
}

gboolean
prefsSave (const gchar* filename,
           GError** error)
{
    PrettyPrintingOptions* ppo;
    gchar *contents = NULL;
    gsize size = 0;

    g_return_val_if_fail (filename != NULL, FALSE);
    ppo = prettyPrintingOptions;
    fetchSettingsFromConfigUI (ppo);
    contents = prefsToData (ppo, &size, error);
    if (contents == NULL)
        return FALSE;
    if (! g_file_set_contents (filename, contents, size, error))
    {
        g_free (contents);
        return FALSE;
    }
    g_free (contents);
    return TRUE;
}

gchar *
getDefaultPrefs (GError** error)
{
    gchar *contents = NULL;
    gsize size = 0;
    PrettyPrintingOptions* ppo;

    ppo = createDefaultPrettyPrintingOptions();
    g_return_val_if_fail (ppo != NULL, NULL);
    contents = prefsToData (ppo, &size, error);
    return contents;
}

/*============================================= PRIVATE FUNCTIONS =======================================*/

/*GtkWidget* createTwoOptionsBox(const char* label,
                               const char* checkBox1,
                               const char* checkBox2,
                               gboolean cb1Active,
                               gboolean cb2Active,
                               GtkWidget** option1,
                               GtkWidget** option2)
{
    GtkWidget* container = gtk_hbox_new(TRUE, 2);
    GtkWidget* rightBox = gtk_vbox_new(FALSE, 6);
    GtkWidget* leftBox = gtk_vbox_new(FALSE, 6);

    GtkWidget* lbl = gtk_label_new(label);
    GtkWidget* chb1 = gtk_check_button_new_with_label(checkBox1);
    GtkWidget* chb2 = gtk_check_button_new_with_label(checkBox2);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chb1), cb1Active);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chb2), cb2Active);

    gtk_box_pack_start(GTK_BOX(container), leftBox, FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(container), rightBox, FALSE, FALSE, 3);

    gtk_box_pack_start(GTK_BOX(leftBox), lbl, FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(rightBox), chb1, FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(rightBox), chb2, FALSE, FALSE, 3);

    *option1 = chb1;
    *option2 = chb2;

    return container;
}*/

static GtkWidget* createThreeOptionsBox(const char* label,
                                        const char* checkBox1,
                                        const char* checkBox2,
                                        const char* checkBox3,
                                        gboolean cb1Active,
                                        gboolean cb2Active,
                                        gboolean cb3Active,
                                        GtkWidget** option1,
                                        GtkWidget** option2,
                                        GtkWidget** option3)
{
    GtkWidget* container = gtk_hbox_new(TRUE, 2);
    GtkWidget* rightBox = gtk_vbox_new(FALSE, 6);
    GtkWidget* leftBox = gtk_vbox_new(FALSE, 6);

    GtkWidget* lbl = gtk_label_new(label);
    GtkWidget* chb1 = gtk_check_button_new_with_label(checkBox1);
    GtkWidget* chb2 = gtk_check_button_new_with_label(checkBox2);
    GtkWidget* chb3 = gtk_check_button_new_with_label(checkBox3);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chb1), cb1Active);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chb2), cb2Active);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chb3), cb3Active);

    gtk_box_pack_start(GTK_BOX(container), leftBox, FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(container), rightBox, FALSE, FALSE, 3);

    gtk_box_pack_start(GTK_BOX(leftBox), lbl, FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(rightBox), chb1, FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(rightBox), chb2, FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(rightBox), chb3, FALSE, FALSE, 3);

    *option1 = chb1;
    *option2 = chb2;
    *option3 = chb3;

    return container;
}

GtkWidget* createEmptyTextOptions(gboolean optEmptyNodeStripping, gboolean optEmptyNodeStrippingSpace, gboolean optForceEmptyNodeSplit)
{
    GtkWidget* container = gtk_hbox_new(FALSE, 2);
    GtkWidget* rightBox = gtk_vbox_new(FALSE, 6);
    GtkWidget* leftBox = gtk_vbox_new(FALSE, 6);

    GtkWidget* lbl = gtk_label_new(_("Empty nodes"));
    GtkWidget* chb1 = gtk_check_button_new_with_label(_("Concatenation (<x></x> to <x/>)"));
    GtkWidget* chb2 = gtk_check_button_new_with_label(_("Spacing (<x/> to <x />)"));
    GtkWidget* chb3 = gtk_check_button_new_with_label(_("Expansion (<x/> to <x></x>)"));

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chb1), optEmptyNodeStripping);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chb2), optEmptyNodeStrippingSpace);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chb3), optForceEmptyNodeSplit);

    gtk_box_pack_start(GTK_BOX(container), leftBox, FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(container), rightBox, FALSE, FALSE, 3);

    gtk_box_pack_start(GTK_BOX(leftBox), lbl, FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(rightBox), chb1, FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(rightBox), chb2, FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(rightBox), chb3, FALSE, FALSE, 3);

    emptyNodeStripping = chb1;
    emptyNodeStrippingSpace = chb2;
    emptyNodeSplit = chb3;

    return container;
}

GtkWidget* createIndentationOptions(char indentation, int count)
{
    GtkWidget* container = gtk_hbox_new(FALSE, 20);
    GtkWidget* rightBox = gtk_hbox_new(FALSE, 6);
    GtkWidget* leftBox = gtk_vbox_new(FALSE, 6);

    GtkWidget* lbl = gtk_label_new(_("Indentation"));
    GtkWidget* comboChar = gtk_combo_box_text_new();
    GtkWidget* spinIndent = gtk_spin_button_new_with_range(0, 100, 1);

    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(comboChar), _("Tab"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(comboChar), _("Space"));
    gtk_combo_box_set_active(GTK_COMBO_BOX(comboChar), (indentation == ' ') ? 1 : 0);

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinIndent), count);

    gtk_box_pack_start(GTK_BOX(leftBox), lbl, FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(rightBox), comboChar, FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(rightBox), spinIndent, FALSE, FALSE, 3);

    gtk_box_pack_start(GTK_BOX(container), leftBox, FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(container), rightBox, FALSE, FALSE, 3);

    indentationChar = comboChar;
    indentationCount = spinIndent;

    return container;
}

static GtkWidget* createLineReturnOptions(const char* lineReturn)
{
    GtkWidget* container = gtk_hbox_new(FALSE, 25);
    GtkWidget* rightBox = gtk_hbox_new(FALSE, 6);
    GtkWidget* leftBox = gtk_vbox_new(FALSE, 6);

    GtkWidget* lbl = gtk_label_new(_("Line break"));
    GtkWidget* comboChar = gtk_combo_box_text_new();

    int active = 0;

    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(comboChar), "\\r");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(comboChar), "\\n");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(comboChar), "\\r\\n");

    if (strlen(lineReturn) == 2) active = 2;
    else if (lineReturn[0] == '\n') active = 1;

    gtk_combo_box_set_active(GTK_COMBO_BOX(comboChar), active);

    gtk_box_pack_start(GTK_BOX(leftBox), lbl, FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(rightBox), comboChar, FALSE, FALSE, 3);

    gtk_box_pack_start(GTK_BOX(container), leftBox, FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(container), rightBox, FALSE, FALSE, 3);

    lineBreak = comboChar;

    return container;
}
