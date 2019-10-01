/*
 * Pair Tag Highlighter
 *
 * highlights matching opening/closing HTML tags
 *
 * Author:  Volodymyr Kononenko aka kvm
 * Email:   vm@kononenko.ws
 *
 */

#include "config.h"
#include <geanyplugin.h>
#include <string.h>
#include "Scintilla.h"  /* for the SCNotification struct */
#include "SciLexer.h"

#define DEFAULT_SEARCH_RANGE 1024

#define INDICATOR_TAGMATCH 9
#define MAX_TAG_NAME 64

#define MATCHING_PAIR_COLOR     0x00ff00    /* green */
#define NONMATCHING_PAIR_COLOR  0xff0000    /* red */
#define EMPTY_TAG_COLOR         0xffff00    /* yellow */

/* Keyboard Shortcut */
enum {
  KB_MATCH_TAG,
  KB_COUNT
};

/* These items are set by Geany before plugin_init() is called. */
GeanyPlugin     *geany_plugin;
GeanyData       *geany_data;

/* Is needed for clearing highlighting after moving cursor out
 * from the tag */
static gint highlightedBrackets[] = {0, 0, 0, 0};

typedef struct
{
    guint64      search_range;
    GtkWidget    *label;
    GtkWidget    *spin;
}ConfigData;

static ConfigData    config_data;

PLUGIN_VERSION_CHECK(224)

PLUGIN_SET_TRANSLATABLE_INFO(LOCALEDIR, GETTEXT_PACKAGE, _("Pair Tag Highlighter"),
                            _("Finds and highlights matching opening/closing HTML tag"),
                            "1.1", "Volodymyr Kononenko <vm@kononenko.ws>")

/* Determine max. position to search to from the current position. */
static gint getsearchlimit(ScintillaObject *sci, gint pos, gint max, gboolean direction)
{
    gint endofsearch, length;

    if (direction == TRUE)
    {
        endofsearch = pos + max;
        length = sci_get_length(sci);
        if (endofsearch > length)
        {
            endofsearch = length - 1;
        }
    }
    else
    {
        endofsearch = pos - max;
        if (endofsearch < 0)
        {
            endofsearch = 0;
        }
    }
    return endofsearch;
}


/* Searches tag brackets.
 * direction variable shows sets search direction:
 * TRUE  - to the right
 * FALSE - to the left
 * from the current cursor position to the start of the line.
 */
static gint findBracket(ScintillaObject *sci, gint position, gint endOfSearchPos,
                        gchar searchedBracket, gchar breakBracket, gboolean direction)
{
    gint foundBracket = -1;
    gint pos;

    if(TRUE == direction)
    {
        /* search to the right */
        for(pos=position; pos<=endOfSearchPos; pos++)
        {
            gchar charAtCurPosition = sci_get_char_at(sci, pos);
            gchar charAtPrevPosition = sci_get_char_at(sci, pos-1);
            gchar charAtNextPosition = sci_get_char_at(sci, pos+1);

            if(charAtCurPosition == searchedBracket) {
                if ('>' == searchedBracket) {
                    if (('-' == charAtPrevPosition) || ('?' == charAtPrevPosition))
                        continue;
                } else if ('<' == searchedBracket) {
                    if ('?' == charAtNextPosition)
                        continue;
                }
                foundBracket = pos;
                break;
            } else if(charAtCurPosition == breakBracket) {
                if ('<' == breakBracket) {
                    if ('?' == charAtNextPosition)
                        continue;
                }
                break;
            }
        }
    }
    else
    {
        /* search to the left */
        for(pos=position-1; pos>=endOfSearchPos; pos--)
        {
            gchar charAtCurPosition = sci_get_char_at(sci, pos);
            gchar charAtPrevPosition = sci_get_char_at(sci, pos+1);
            gchar charAtNextPosition = sci_get_char_at(sci, pos-1);

            if(charAtCurPosition == searchedBracket)
            {
                if ('<' == searchedBracket) {
                    if ('?' == charAtPrevPosition)
                        continue;
                } else if ('>' == searchedBracket) {
                    if (('-' == charAtNextPosition) || ('?' == charAtNextPosition))
                        continue;
                }
                foundBracket = pos;
                break;
            } else if(charAtCurPosition == breakBracket) {
                if ('>' == breakBracket) {
                    if (('-' == charAtNextPosition) || ('?' == charAtNextPosition))
                        continue;
                }
                break;
            }
        }
    }

    return foundBracket;
}


static gint rgb2bgr(gint color)
{
    guint r, g, b;

    r = color >> 16;
    g = (0x00ff00 & color) >> 8;
    b = (0x0000ff & color);

    color = (r | (g << 8) | (b << 16));

    return color;
}


static void highlight_tag(ScintillaObject *sci, gint openingBracket,
                          gint closingBracket, gint color)
{
    scintilla_send_message(sci, SCI_SETINDICATORCURRENT, INDICATOR_TAGMATCH, 0);
    scintilla_send_message(sci, SCI_INDICSETSTYLE,
                            INDICATOR_TAGMATCH, INDIC_ROUNDBOX);
    scintilla_send_message(sci, SCI_INDICSETFORE, INDICATOR_TAGMATCH, rgb2bgr(color));
    scintilla_send_message(sci, SCI_INDICSETALPHA, INDICATOR_TAGMATCH, 60);
    scintilla_send_message(sci, SCI_INDICATORFILLRANGE,
                            openingBracket, closingBracket-openingBracket+1);
}


static void highlight_matching_pair(ScintillaObject *sci)
{
    highlight_tag(sci, highlightedBrackets[0], highlightedBrackets[1],
                  MATCHING_PAIR_COLOR);
    highlight_tag(sci, highlightedBrackets[2], highlightedBrackets[3],
                  MATCHING_PAIR_COLOR);
}


static void clear_previous_highlighting(ScintillaObject *sci, gint rangeStart, gint rangeEnd)
{
    scintilla_send_message(sci, SCI_SETINDICATORCURRENT, INDICATOR_TAGMATCH, 0);
    scintilla_send_message(sci, SCI_INDICATORCLEARRANGE, rangeStart, rangeEnd-rangeStart+1);
}


static gboolean is_tag_self_closing(ScintillaObject *sci, gint closingBracket)
{
    gboolean isTagSelfClosing = FALSE;
    gchar charBeforeBracket = sci_get_char_at(sci, closingBracket-1);

    if('/' == charBeforeBracket)
        isTagSelfClosing = TRUE;
    return isTagSelfClosing;
}


static gboolean is_tag_empty(gchar *tagName)
{
    const char *emptyTags[] = {"area", "base", "br", "col", "embed",
                         "hr", "img", "input", "keygen", "link", "meta",
                         "param", "source", "track", "wbr", "!DOCTYPE"};
    unsigned int i;

    g_return_val_if_fail(tagName != NULL, FALSE);

    for(i=0; i<(sizeof(emptyTags)/sizeof(emptyTags[0])); i++)
    {
        if(strcmp(tagName, emptyTags[i]) == 0)
            return TRUE;
    }

    return FALSE;
}


static gboolean is_tag_opening(ScintillaObject *sci, gint openingBracket)
{
    gboolean isTagOpening = TRUE;
    gchar charAfterBracket = sci_get_char_at(sci, openingBracket+1);

    if('/' == charAfterBracket)
        isTagOpening = FALSE;
    return isTagOpening;
}


static gchar *get_tag_name(ScintillaObject *sci, gint openingBracket, gint closingBracket,
                    gboolean isTagOpening)
{
    gint nameStart = openingBracket + (TRUE == isTagOpening ? 1 : 2);
    gint nameEnd = nameStart;
    gchar charAtCurPosition = sci_get_char_at(sci, nameStart);

    while(' ' != charAtCurPosition && '>' != charAtCurPosition &&
        '\t' != charAtCurPosition && '\r' != charAtCurPosition && '\n' != charAtCurPosition)
    {
        charAtCurPosition = sci_get_char_at(sci, nameEnd);
        nameEnd++;
        if(nameEnd-nameStart > MAX_TAG_NAME)
            break;
    }
    return nameEnd > nameStart ? sci_get_contents_range(sci, nameStart, nameEnd-1) : NULL;
}


static void findMatchingOpeningTag(ScintillaObject *sci, gchar *tagName, gint openingBracket)
{
    gint pos;
    gint openingTagsCount = 0;
    gint closingTagsCount = 1;

    for(pos=openingBracket; pos>0; pos--)
    {
        /* are we inside tag? */
        gint lineNumber = sci_get_line_from_position(sci, pos);
        gint lineStart = sci_get_position_from_line(sci, lineNumber);
        gint leftlimit = getsearchlimit(sci, pos, config_data.search_range, FALSE);
        gint matchingOpeningBracket = findBracket(sci, pos, leftlimit, '<', '\0', FALSE);
        gint matchingClosingBracket = findBracket(sci, pos, leftlimit, '>', '\0', FALSE);

        if(-1 != matchingOpeningBracket && -1 != matchingClosingBracket
            && (matchingClosingBracket > matchingOpeningBracket))
        {
            /* we are inside of some tag. Let us check what tag*/
            gboolean isMatchingTagOpening = is_tag_opening(sci, matchingOpeningBracket);
            gchar *matchingTagName = get_tag_name(sci, matchingOpeningBracket,
                                                  matchingClosingBracket,
                                                  isMatchingTagOpening);
            if(matchingTagName && strcmp(tagName, matchingTagName) == 0)
            {
                if(TRUE == isMatchingTagOpening)
                    openingTagsCount++;
                else
                    closingTagsCount++;
            }
            pos = matchingOpeningBracket+1;
            g_free(matchingTagName);
        }
        /* Speed up search: if findBracket returns -1, that means start of line
         * is reached. There is no need to go through the same positions again.
         * Jump to the start of line */
        else if(-1 == matchingOpeningBracket || -1 == matchingClosingBracket)
        {
            pos = lineStart;
            continue;
        }
        if(openingTagsCount == closingTagsCount)
        {
            /* matching tag is found */
            highlightedBrackets[2] = matchingOpeningBracket;
            highlightedBrackets[3] = matchingClosingBracket;
            highlight_matching_pair(sci);
            return;
        }
    }
    highlight_tag(sci, highlightedBrackets[0], highlightedBrackets[1],
                  NONMATCHING_PAIR_COLOR);
}


static void findMatchingClosingTag(ScintillaObject *sci, gchar *tagName, gint closingBracket)
{
    gint pos;
    gint linesInDocument = sci_get_line_count(sci);
    gint endOfDocument = sci_get_position_from_line(sci, linesInDocument);
    gint openingTagsCount = 1;
    gint closingTagsCount = 0;

    for(pos=closingBracket; pos<endOfDocument; pos++)
    {
        /* are we inside tag? */
        gint rightlimit = getsearchlimit(sci, pos, config_data.search_range, TRUE);
        gint matchingOpeningBracket = findBracket(sci, pos, rightlimit, '<', '\0', TRUE);
        gint matchingClosingBracket = findBracket(sci, pos, rightlimit, '>', '\0', TRUE);

        if(-1 != matchingOpeningBracket && -1 != matchingClosingBracket
            && (matchingClosingBracket > matchingOpeningBracket))
        {
            /* we are inside of some tag. Let us check what tag*/
            gboolean isMatchingTagOpening = is_tag_opening(sci, matchingOpeningBracket);
            gchar *matchingTagName = get_tag_name(sci, matchingOpeningBracket,
                                                  matchingClosingBracket,
                                                  isMatchingTagOpening);
            if(matchingTagName && strcmp(tagName, matchingTagName) == 0)
            {
                if(TRUE == isMatchingTagOpening)
                    openingTagsCount++;
                else
                    closingTagsCount++;
            }
            pos = matchingClosingBracket;
            g_free(matchingTagName);
        }

        if(openingTagsCount == closingTagsCount)
        {
            /* matching tag is found */
            highlightedBrackets[2] = matchingOpeningBracket;
            highlightedBrackets[3] = matchingClosingBracket;
            highlight_matching_pair(sci);
            return;
        }
    }
    highlight_tag(sci, highlightedBrackets[0], highlightedBrackets[1],
                  NONMATCHING_PAIR_COLOR);
}


static void findMatchingTag(ScintillaObject *sci, gint openingBracket, gint closingBracket)
{
    gboolean isTagOpening = is_tag_opening(sci, openingBracket);
    gchar *tagName = get_tag_name(sci, openingBracket, closingBracket, isTagOpening);

    if (!tagName)
        return;

    if(is_tag_self_closing(sci, closingBracket) || is_tag_empty(tagName)) {
        highlight_tag(sci, openingBracket, closingBracket, EMPTY_TAG_COLOR);
    } else {
        if(isTagOpening)
            findMatchingClosingTag(sci, tagName, closingBracket);
        else
            findMatchingOpeningTag(sci, tagName, openingBracket);
    }

    g_free(tagName);
}


static void run_tag_highlighter(ScintillaObject *sci)
{
    gint position = sci_get_current_position(sci);
    gint leftlimit = getsearchlimit(sci, position, config_data.search_range, FALSE);
    gint rightlimit = getsearchlimit(sci, position, config_data.search_range, TRUE);
    gint openingBracket = findBracket(sci, position, leftlimit, '<', '>', FALSE);
    gint closingBracket = findBracket(sci, position, rightlimit, '>', '<', TRUE);
    int i;

    if(-1 == openingBracket || -1 == closingBracket)
    {
        clear_previous_highlighting(sci, highlightedBrackets[0], highlightedBrackets[1]);
        clear_previous_highlighting(sci, highlightedBrackets[2], highlightedBrackets[3]);
        for(i=0; i<3; i++)
            highlightedBrackets[i] = 0;
        return;
    }

    /* If the cursor jumps from one tag into another, clear
     * previous highlighted tags*/
    if(openingBracket != highlightedBrackets[0] ||
        closingBracket != highlightedBrackets[1])
    {
        clear_previous_highlighting(sci, highlightedBrackets[0], highlightedBrackets[1]);
        clear_previous_highlighting(sci, highlightedBrackets[2], highlightedBrackets[3]);
    }

    /* Don't run search on empty brackets <> */
    if (closingBracket - openingBracket > 1) {
        highlightedBrackets[0] = openingBracket;
        highlightedBrackets[1] = closingBracket;

        findMatchingTag(sci, openingBracket, closingBracket);
    }
}


/* Notification handler for editor-notify */
static gboolean on_editor_notify(GObject *obj, GeanyEditor *editor,
                                SCNotification *nt, gpointer user_data)
{
    gint lexer;

    lexer = sci_get_lexer(editor->sci);
    if((lexer != SCLEX_HTML) && (lexer != SCLEX_XML) && (lexer != SCLEX_PHPSCRIPT))
    {
        return FALSE;
    }

    /* nmhdr is a structure containing information about the event */
    switch (nt->nmhdr.code)
    {
        case SCN_UPDATEUI:
            run_tag_highlighter(editor->sci);
            break;
    }

    /* returning FALSE to allow Geany processing the event */
    return FALSE;
}


static void
on_kb_goto_matching_tag (guint key_id)
{
    gint cur_line;
    gint jump_line = 0;
    if(highlightedBrackets[0] != highlightedBrackets[2] && highlightedBrackets[0] != 0){
        GeanyDocument *doc = document_get_current();
        cur_line = sci_get_current_position(doc->editor->sci);
        if(cur_line >= highlightedBrackets[0] && cur_line <= highlightedBrackets[1]){
            jump_line = highlightedBrackets[2];
        }
        else if(cur_line >= highlightedBrackets[2] && cur_line <= highlightedBrackets[3]){
            jump_line = highlightedBrackets[0];
        }
        if(jump_line != 0){
            sci_set_current_position(doc->editor->sci, jump_line, TRUE);
        }
    }
}


/* Configure the preferences GUI and callbacks */
static GtkWidget *create_config_ui(void)
{
    GtkWidget *vbox, *hbox;

    vbox = gtk_vbox_new(FALSE, 0);
    hbox = gtk_hbox_new(FALSE, 0);

    config_data.label = gtk_label_new(_("Search range for brackets:"));
    gtk_box_pack_start(GTK_BOX(hbox), config_data.label, FALSE, FALSE, 3);

    config_data.spin = gtk_spin_button_new_with_range(64, 1024*100, 64);
    ui_entry_add_clear_icon(GTK_ENTRY(config_data.spin));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(config_data.spin), config_data.search_range);
    gtk_box_pack_start(GTK_BOX(hbox), config_data.spin, TRUE, TRUE, 3);

    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 3);
    gtk_widget_show_all(vbox);

    return vbox;
}

static gchar *get_config_filename(void)
{
    return g_strconcat(geany->app->configdir,
        G_DIR_SEPARATOR_S, "plugins", G_DIR_SEPARATOR_S,
        "pairtaghighlighter", G_DIR_SEPARATOR_S, "general.conf", NULL);
}


/* handle button presses in the preferences dialog box */
void write_config(void)
{
    GKeyFile *config = g_key_file_new();
    gchar *config_file = get_config_filename();
    gchar *config_dir = g_path_get_dirname(config_file);
    gchar *data;

    /* Grabbing options that has been set */
    config_data.search_range = gtk_spin_button_get_value(GTK_SPIN_BUTTON(config_data.spin));

    /* Write preference to file */
    g_key_file_load_from_file(config, config_file, G_KEY_FILE_NONE, NULL);

    g_key_file_set_uint64(config, "general", "search_range",
        config_data.search_range);

    if (!g_file_test(config_dir, G_FILE_TEST_IS_DIR)
        && utils_mkdir(config_dir, TRUE) != 0)
    {
        dialogs_show_msgbox(GTK_MESSAGE_ERROR,
            _("Plugin configuration directory could not be created."));
    }
    else
    {
        /* write config to file */
        data = g_key_file_to_data(config, NULL, NULL);
        utils_write_file(config_file, data);
        g_free(data);
    }

    g_free(config_dir);
    g_free(config_file);
    g_key_file_free(config);
}


void read_config(void)
{
    gchar *config_file = get_config_filename();
    GKeyFile *config = g_key_file_new();

    /* load preferences from file */
    g_key_file_load_from_file(config, config_file, G_KEY_FILE_NONE, NULL);

    config_data.search_range = g_key_file_get_uint64(config,
        "general", "search_range", NULL);
    if (config_data.search_range == 0)
    {
        config_data.search_range = DEFAULT_SEARCH_RANGE;
    }

    g_free(config_file);
    g_key_file_free(config);
}


static void configure_response_cb(GtkDialog *dialog, gint response, gpointer user_data)
{
    if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY)
    {
        write_config ();
    }
}

PluginCallback plugin_callbacks[] =
{
    { "editor-notify", (GCallback) &on_editor_notify, FALSE, NULL },
    { NULL, NULL, FALSE, NULL }
};


void plugin_init(GeanyData *data)
{
    GeanyKeyGroup *group;
    group = plugin_set_key_group (geany_plugin, "Pair Tag Highlighter", KB_COUNT, NULL);
    keybindings_set_item (group, KB_MATCH_TAG, on_kb_goto_matching_tag,
                        0, 0, "goto_matching_tag", _("Go To Matching Tag"), NULL);
    read_config();
}


void plugin_cleanup(void)
{
    GeanyDocument *doc = document_get_current();

    if (doc)
    {
        clear_previous_highlighting(doc->editor->sci, highlightedBrackets[0], highlightedBrackets[1]);
        clear_previous_highlighting(doc->editor->sci, highlightedBrackets[2], highlightedBrackets[3]);
    }
}


GtkWidget *plugin_configure(GtkDialog *dialog)
{
  GtkWidget *panel;
  panel = create_config_ui();
  g_signal_connect(dialog, "response", G_CALLBACK(configure_response_cb), NULL);
  return panel;
}
