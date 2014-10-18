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

#define INDICATOR_TAGMATCH 9
#define MAX_TAG_NAME 64

#define MATCHING_PAIR_COLOR     0x00ff00    /* green */
#define NONMATCHING_PAIR_COLOR  0xff0000    /* red */
#define EMPTY_TAG_COLOR         0xffff00    /* yellow */

/* These items are set by Geany before plugin_init() is called. */
GeanyPlugin     *geany_plugin;
GeanyData       *geany_data;
GeanyFunctions  *geany_functions;

/* Is needed for clearing highlighting after moving cursor out
 * from the tag */
static gint highlightedBrackets[] = {0, 0, 0, 0};

PLUGIN_VERSION_CHECK(211)

PLUGIN_SET_TRANSLATABLE_INFO(LOCALEDIR, GETTEXT_PACKAGE, _("Pair Tag Highlighter"),
                            _("Finds and highlights matching opening/closing HTML tag"),
                            "1.1", "Volodymyr Kononenko <vm@kononenko.ws>")


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
    scintilla_send_message(sci, SCI_INDICATORCLEARRANGE, rangeStart, rangeEnd+1);
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


static void get_tag_name(ScintillaObject *sci, gint openingBracket, gint closingBracket,
                    gchar tagName[], gboolean isTagOpening)
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
    sci_get_text_range(sci, nameStart, nameEnd-1, tagName);
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
        gint matchingOpeningBracket = findBracket(sci, pos, lineStart, '<', '\0', FALSE);
        gint matchingClosingBracket = findBracket(sci, pos, lineStart, '>', '\0', FALSE);

        if(-1 != matchingOpeningBracket && -1 != matchingClosingBracket
            && (matchingClosingBracket > matchingOpeningBracket))
        {
            /* we are inside of some tag. Let us check what tag*/
            gchar matchingTagName[MAX_TAG_NAME];
            gboolean isMatchingTagOpening = is_tag_opening(sci, matchingOpeningBracket);
            get_tag_name(sci, matchingOpeningBracket, matchingClosingBracket,
                            matchingTagName, isMatchingTagOpening);
            if(strcmp(tagName, matchingTagName) == 0)
            {
                if(TRUE == isMatchingTagOpening)
                    openingTagsCount++;
                else
                    closingTagsCount++;
            }
            pos = matchingOpeningBracket+1;
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
        gint lineNumber = sci_get_line_from_position(sci, pos);
        gint lineEnd = sci_get_line_end_position(sci, lineNumber);
        gint matchingOpeningBracket = findBracket(sci, pos, lineEnd, '<', '\0', TRUE);
        gint matchingClosingBracket = findBracket(sci, pos, lineEnd, '>', '\0', TRUE);

        if(-1 != matchingOpeningBracket && -1 != matchingClosingBracket
            && (matchingClosingBracket > matchingOpeningBracket))
        {
            /* we are inside of some tag. Let us check what tag*/
            gchar matchingTagName[64];
            gboolean isMatchingTagOpening = is_tag_opening(sci, matchingOpeningBracket);
            get_tag_name(sci, matchingOpeningBracket, matchingClosingBracket,
                            matchingTagName, isMatchingTagOpening);
            if(strcmp(tagName, matchingTagName) == 0)
            {
                if(TRUE == isMatchingTagOpening)
                    openingTagsCount++;
                else
                    closingTagsCount++;
            }
            pos = matchingClosingBracket;
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
    gchar tagName[MAX_TAG_NAME];
    gboolean isTagOpening = is_tag_opening(sci, openingBracket);

    get_tag_name(sci, openingBracket, closingBracket, tagName, isTagOpening);

    if(is_tag_self_closing(sci, closingBracket) || is_tag_empty(tagName)) {
        highlight_tag(sci, openingBracket, closingBracket, EMPTY_TAG_COLOR);
    } else {
        if(isTagOpening)
            findMatchingClosingTag(sci, tagName, closingBracket);
        else
            findMatchingOpeningTag(sci, tagName, openingBracket);
    }
}


static void run_tag_highlighter(ScintillaObject *sci)
{
    gint position = sci_get_current_position(sci);
    gint lineNumber = sci_get_current_line(sci);
    gint lineStart = sci_get_position_from_line(sci, lineNumber);
    gint lineEnd = sci_get_line_end_position(sci, lineNumber);
    gint openingBracket = findBracket(sci, position, lineStart, '<', '>', FALSE);
    gint closingBracket = findBracket(sci, position, lineEnd, '>', '<', TRUE);
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
    if((lexer != SCLEX_HTML) && (lexer != SCLEX_XML))
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


PluginCallback plugin_callbacks[] =
{
    { "editor-notify", (GCallback) &on_editor_notify, FALSE, NULL },
    { NULL, NULL, FALSE, NULL }
};


void plugin_init(GeanyData *data)
{
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
