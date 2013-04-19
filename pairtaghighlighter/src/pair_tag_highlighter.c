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

/* If to set indicator >8, highlighting will be of grey color.
 * Light grey line highlighter covers higher values of indicator. */
#define INDICATOR_TAGMATCH 0
#define MAX_TAG_NAME 64

/* These items are set by Geany before plugin_init() is called. */
GeanyPlugin     *geany_plugin;
GeanyData       *geany_data;
GeanyFunctions  *geany_functions;

static ScintillaObject *sci;

/* Is needed for clearing highlighting after moving cursor out
 * from the tag */
static gint highlightedBrackets[] = {0, 0, 0, 0};

PLUGIN_VERSION_CHECK(211)

PLUGIN_SET_TRANSLATABLE_INFO(LOCALEDIR, GETTEXT_PACKAGE, _("Pair Tag Highlighter"),
                            _("Finds and highlights matching opening/closing HTML tag"),
                            "1.0", "Volodymyr Kononenko <vm@kononenko.ws>")


/* Searches tag brackets.
 * direction variable shows sets search direction:
 * TRUE  - to the right
 * FALSE - to the left
 * from the current cursor position to the start of the line.
 */
static gint findBracket(gint position, gint endOfSearchPos,
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
            if(charAtCurPosition == searchedBracket)
            {
                foundBracket = pos;
                break;
            }
            if(charAtCurPosition == breakBracket)
                break;
        }
    }
    else
    {
        /* search to the left */
        for(pos=position-1; pos>=endOfSearchPos; pos--)
        {
            gchar charAtCurPosition = sci_get_char_at(sci, pos);
            if(charAtCurPosition == searchedBracket)
            {
                foundBracket = pos;
                break;
            }
            if(charAtCurPosition == breakBracket)
                break;
        }
    }

    return foundBracket;
}


static void highlight_tag(gint openingBracket, gint closingBracket)
{
    scintilla_send_message(sci, SCI_SETINDICATORCURRENT, INDICATOR_TAGMATCH, 0);
    scintilla_send_message(sci, SCI_INDICSETSTYLE,
                            INDICATOR_TAGMATCH, INDIC_ROUNDBOX);
    scintilla_send_message(sci, SCI_INDICSETFORE, 0, 0x00d000); /* green */
    scintilla_send_message(sci, SCI_INDICSETALPHA, INDICATOR_TAGMATCH, 60);
    scintilla_send_message(sci, SCI_INDICATORFILLRANGE,
                            openingBracket, closingBracket-openingBracket+1);
}


static void clear_previous_highlighting(gint rangeStart, gint rangeEnd)
{
    scintilla_send_message(sci, SCI_SETINDICATORCURRENT, INDICATOR_TAGMATCH, 0);
    scintilla_send_message(sci, SCI_INDICATORCLEARRANGE, rangeStart, rangeEnd+1);
}


static gboolean is_tag_self_closing(gint closingBracket)
{
    gboolean isTagSelfClosing = FALSE;
    gchar charBeforeBracket = sci_get_char_at(sci, closingBracket-1);

    if('/' == charBeforeBracket)
        isTagSelfClosing = TRUE;
    return isTagSelfClosing;
}


static gboolean is_tag_opening(gint openingBracket)
{
    gboolean isTagOpening = TRUE;
    gchar charAfterBracket = sci_get_char_at(sci, openingBracket+1);

    if('/' == charAfterBracket)
        isTagOpening = FALSE;
    return isTagOpening;
}


static void get_tag_name(gint openingBracket, gint closingBracket,
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


static void findMatchingOpeningTag(gchar *tagName, gint openingBracket)
{
    gint pos;
    gint openingTagsCount = 0;
    gint closingTagsCount = 1;

    for(pos=openingBracket; pos>0; pos--)
    {
        /* are we inside tag? */
        gint lineNumber = sci_get_line_from_position(sci, pos);
        gint lineStart = sci_get_position_from_line(sci, lineNumber);
        gint matchingOpeningBracket = findBracket(pos, lineStart, '<', '\0', FALSE);
        gint matchingClosingBracket = findBracket(pos, lineStart, '>', '\0', FALSE);

        if(-1 != matchingOpeningBracket && -1 != matchingClosingBracket
            && (matchingClosingBracket > matchingOpeningBracket))
        {
            /* we are inside of some tag. Let us check what tag*/
            gchar matchingTagName[MAX_TAG_NAME];
            gboolean isMatchingTagOpening = is_tag_opening(matchingOpeningBracket);
            get_tag_name(matchingOpeningBracket, matchingClosingBracket,
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
            highlight_tag(matchingOpeningBracket, matchingClosingBracket);
            highlightedBrackets[2] = matchingOpeningBracket;
            highlightedBrackets[3] = matchingClosingBracket;
            break;
        }
    }
}


static void findMatchingClosingTag(gchar *tagName, gint closingBracket)
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
        gint matchingOpeningBracket = findBracket(pos, endOfDocument, '<', '\0', TRUE);
        gint matchingClosingBracket = findBracket(pos, endOfDocument, '>', '\0', TRUE);

        if(-1 != matchingOpeningBracket && -1 != matchingClosingBracket
            && (matchingClosingBracket > matchingOpeningBracket))
        {
            /* we are inside of some tag. Let us check what tag*/
            gchar matchingTagName[64];
            gboolean isMatchingTagOpening = is_tag_opening(matchingOpeningBracket);
            get_tag_name(matchingOpeningBracket, matchingClosingBracket,
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
        /* Speed up search: if findBracket returns -1, that means end of line
         * is reached. There is no need to go through the same positions again.
         * Jump to the end of line */
        else if(-1 == matchingOpeningBracket || -1 == matchingClosingBracket)
        {
            pos = lineEnd;
            continue;
        }
        if(openingTagsCount == closingTagsCount)
        {
            /* matching tag is found */
            highlight_tag(matchingOpeningBracket, matchingClosingBracket);
            highlightedBrackets[2] = matchingOpeningBracket;
            highlightedBrackets[3] = matchingClosingBracket;
            break;
        }
    }
}


static void findMatchingTag(gint openingBracket, gint closingBracket)
{
    gchar tagName[MAX_TAG_NAME];
    gboolean isTagOpening = is_tag_opening(openingBracket);
    get_tag_name(openingBracket, closingBracket, tagName, isTagOpening);

    if(TRUE == isTagOpening)
        findMatchingClosingTag(tagName, closingBracket);
    else
        findMatchingOpeningTag(tagName, openingBracket);
}


static void run_tag_highlighter(void)
{
    gint position = sci_get_current_position(sci);
    gint lineNumber = sci_get_current_line(sci);
    gint lineStart = sci_get_position_from_line(sci, lineNumber);
    gint lineEnd = sci_get_line_end_position(sci, lineNumber);
    gint openingBracket = findBracket(position, lineStart, '<', '>', FALSE);
    gint closingBracket = findBracket(position, lineEnd, '>', '<', TRUE);
    int i;

    if(-1 == openingBracket || -1 == closingBracket)
    {
        clear_previous_highlighting(highlightedBrackets[0], highlightedBrackets[1]);
        clear_previous_highlighting(highlightedBrackets[2], highlightedBrackets[3]);
        for(i=0; i<3; i++)
            highlightedBrackets[i] = 0;
        return;
    }

    /* If the cursor jumps from one tag into another, clear
     * previous highlighted tags*/
    if(openingBracket != highlightedBrackets[0] ||
        closingBracket != highlightedBrackets[1])
    {
        clear_previous_highlighting(highlightedBrackets[0], highlightedBrackets[1]);
        clear_previous_highlighting(highlightedBrackets[2], highlightedBrackets[3]);
    }

    highlightedBrackets[0] = openingBracket;
    highlightedBrackets[1] = closingBracket;

    /* Highlight current tag. Matching tag will be highlighted from
     * findMatchingTag() functiong */
    highlight_tag(openingBracket, closingBracket);

    /* Find matching tag only if a tag is not self-closing */
    if(FALSE == is_tag_self_closing(closingBracket))
        findMatchingTag(openingBracket, closingBracket);
}


/* Notification handler for editor-notify */
static gboolean on_editor_notify(GObject *obj, GeanyEditor *editor,
                                SCNotification *nt, gpointer user_data)
{
    gint lexer;

    /* setting global sci variable to be available in other functions */
    sci = editor->sci;

    lexer = sci_get_lexer(sci);
    if(lexer != SCLEX_HTML)
    {
        return FALSE;
    }

    /* nmhdr is a structure containing information about the event */
    switch (nt->nmhdr.code)
    {
        case SCN_UPDATEUI:
            run_tag_highlighter();
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
    clear_previous_highlighting(highlightedBrackets[0], highlightedBrackets[1]);
    clear_previous_highlighting(highlightedBrackets[2], highlightedBrackets[3]);
}
