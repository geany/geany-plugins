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
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <gdk/gdk.h>
#include "Scintilla.h"  /* for the SCNotification struct */
#include "SciLexer.h"

#define INDICATOR_TAGMATCH 9
#define MAX_TAG_NAME 64
#define HIGHLIGHT_ALPHA_MIN 0
#define HIGHLIGHT_ALPHA_MAX 255
#define HIGHLIGHT_ALPHA_DEFAULT 60

/* Tag types */
enum {
  MATCHING_PAIR,
  NONMATCHING_PAIR,
  EMPTY_TAG,
  TAG_TYPE_COUNT
};

/* Colors for tag highlighting */
const gint HIGHLIGHT_COLOR_DEFAULTS[TAG_TYPE_COUNT] = {0x00ff00, 0xff0000, 0xffff00};
static gint highlightColor_G[TAG_TYPE_COUNT];
static glong highlightAlpha_G = HIGHLIGHT_ALPHA_DEFAULT;

/* Keyboard Shortcut */
enum {
  KB_MATCH_TAG,
  KB_SELECT_TAG,
  KB_COUNT
};

/* Preference dialog objects */
typedef struct PrefDialogObjects PrefDialogObjects;
struct PrefDialogObjects {
  GtkWidget *main_prefs_frame;
  GtkWidget *matching_pair_color_button;
  GtkWidget *nonmatching_pair_color_button;
  GtkWidget *empty_tag_color_button;
  GObject *alpha_adjust;
  GtkWidget *defaults_button;
};

/* These items are set by Geany before plugin_init() is called. */
GeanyPlugin     *geany_plugin;
GeanyData       *geany_data;

/* Is needed for clearing highlighting after moving cursor out
 * from the tag */
static gint highlightedBrackets[] = {0, 0, 0, 0};
static GtkWidget *goto_matching_tag = NULL;
static GtkWidget *select_matching_tag = NULL;

PLUGIN_VERSION_CHECK(224)

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
    scintilla_send_message(sci, SCI_INDICSETALPHA, INDICATOR_TAGMATCH, highlightAlpha_G);
    scintilla_send_message(sci, SCI_INDICATORFILLRANGE,
                            openingBracket, closingBracket-openingBracket+1);
}


static void highlight_matching_pair(ScintillaObject *sci)
{
    highlight_tag(sci, highlightedBrackets[0], highlightedBrackets[1],
                  highlightColor_G[MATCHING_PAIR]);
    highlight_tag(sci, highlightedBrackets[2], highlightedBrackets[3],
                  highlightColor_G[MATCHING_PAIR]);
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
        gint matchingOpeningBracket = findBracket(sci, pos, lineStart, '<', '\0', FALSE);
        gint matchingClosingBracket = findBracket(sci, pos, lineStart, '>', '\0', FALSE);

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
                  highlightColor_G[NONMATCHING_PAIR]);
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
                  highlightColor_G[NONMATCHING_PAIR]);
}


static void findMatchingTag(ScintillaObject *sci, gint openingBracket, gint closingBracket)
{
    gboolean isTagOpening = is_tag_opening(sci, openingBracket);
    gchar *tagName = get_tag_name(sci, openingBracket, closingBracket, isTagOpening);

    if (!tagName)
        return;

    if(is_tag_self_closing(sci, closingBracket) || is_tag_empty(tagName)) {
        highlight_tag(sci, openingBracket, closingBracket, highlightColor_G[EMPTY_TAG]);
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
select_or_match_tag (gboolean select)
{
    gint cur_line;
    gint jump_line=-5, select_start=0, select_end=0;
    GeanyDocument *doc = document_get_current();
    if(highlightedBrackets[0] != highlightedBrackets[2]){
        cur_line = sci_get_current_position(doc->editor->sci);
        if(cur_line >= highlightedBrackets[0] && cur_line <= highlightedBrackets[1]){
            if (!select){
                jump_line = highlightedBrackets[2];
            }
        }
        else if(cur_line >= highlightedBrackets[2] && cur_line <= highlightedBrackets[3]){
            if(!select){
                jump_line = highlightedBrackets[0];
            }
        }
        if(select){
            select_end = (highlightedBrackets[0] < highlightedBrackets[2])?highlightedBrackets[3]+1:highlightedBrackets[1]+1;
            select_start = (highlightedBrackets[0] < highlightedBrackets[2])?highlightedBrackets[0]:highlightedBrackets[2];
        }
    }
    if (select){
        sci_set_selection_start(doc->editor->sci, select_start);
        sci_set_selection_end(doc->editor->sci, select_end);
    }
    else if (jump_line >= 0){
        sci_set_current_position(doc->editor->sci, jump_line, TRUE);
    }
}


static void
on_goto_matching_tag(GtkWidget *widget, gpointer user_data)
{
  select_or_match_tag(FALSE);
  return;
}
static void
on_select_matching_tag(GtkWidget *widget, gpointer user_data)
{
  select_or_match_tag(TRUE);
  return;
}
static void
on_editor_menu_popup (GObject       *object,
                       const gchar   *word,
                       gint           pos,
                       GeanyDocument *doc,
                       gpointer       user_data)
{
    
   if(DOC_VALID(doc) && (doc->file_type->id == GEANY_FILETYPES_HTML || doc->file_type->id == GEANY_FILETYPES_PHP || doc->file_type->id == GEANY_FILETYPES_XML))
    {
        gtk_widget_set_sensitive (goto_matching_tag, TRUE);
        gtk_widget_set_sensitive (select_matching_tag, TRUE);
        gtk_widget_show(select_matching_tag);
        gtk_widget_show(goto_matching_tag);
    }
    else{
        gtk_widget_set_sensitive (goto_matching_tag, FALSE);
        gtk_widget_set_sensitive (select_matching_tag, FALSE);
        gtk_widget_hide(select_matching_tag);
        gtk_widget_hide(goto_matching_tag);
    }
}

/*** Configuration file ***/

/* Returns the name of the configuration file. The string must be freed by the caller */
static gchar *get_config_filename(void)
{
  return g_build_filename(geany_data->app->configdir, "plugins",
                          PLUGIN, PLUGIN".conf", NULL);
}

/* Read a color setting from the key file. Returns -1 if not valid or can't be read */
static gint key_file_get_color(GKeyFile *key_file, const gchar *group, const gchar *key)
{
    glong value = -1;
    gchar *value_str = g_key_file_get_value(key_file, group, key, NULL);
    
    if (value_str) {
        glong temp_value;
        gchar *index = value_str;
        
        /* get to the first hex digit */
        while (*index != '\0' && !isxdigit(*index))
            index++;

        /* attempt to convert the value string to an RGB color integer */
        if (*index != '\0') {
            char *end_ptr;
            errno = 0;
            temp_value = strtol(index, &end_ptr, 16);
            if (!errno && (end_ptr != index))
                if (temp_value >= 0 && temp_value <= 0xffffff)
                    value = temp_value;
        }
    }

    return (gint)value;
}

/* Write a color setting to the key file */
static void key_file_write_color(GKeyFile *key_file, const gchar *group,
                                 const gchar *key, gint color)
{
    if (color >= 0 && color <= 0xffffff) {
        gchar value_str[8];
        g_snprintf(value_str, sizeof(value_str), "#%.6X", color);
        g_key_file_set_value(key_file, group, key, value_str);
    } else
        g_warning("key_file_write_color(): Value out of bounds %d\n", color);
}

/* Read a long integer value from the key file. Returns FALSE if there is an error
 * reading the value, TRUE otherwise
 */
static gboolean key_file_get_long(GKeyFile *key_file, const gchar *group,
                                  const gchar *key, glong *value)
{
    gboolean success = FALSE;
    gchar *value_str = g_key_file_get_value(key_file, group, key, NULL);
    
    if (value_str) {
        glong temp_value;
        gchar *end_ptr;

        /* attempt to convert the value string to an integer */
        errno = 0;
        temp_value = strtol(value_str, &end_ptr, 10);
        if (!errno && (end_ptr != value_str)) {
            *value = temp_value;
            success = TRUE;
        }
    }

    return success;
}

/* Write a long integer setting to the key file */
static void key_file_write_long(GKeyFile *key_file, const gchar *group,
                                const gchar *key, glong value)
{
    gchar *value_str;
    value_str = g_strdup_printf("%ld", value);
    g_key_file_set_value(key_file, group, key, value_str);
    g_free(value_str);
}

/* Loads the key file from the config file. Returns TRUE if successful or the config
 * file doesn't exist, FALSE otherwise
 */
static gboolean load_key_file(GKeyFile *key_file, gchar *config_file, GKeyFileFlags flags)
{
    gboolean success = TRUE;
    GError *error = NULL;
    
    if (!g_key_file_load_from_file(key_file, config_file, flags, &error)) {
        if (error->code != G_FILE_ERROR_NOENT)
            g_warning("load_key_file(): Config file error\n%s", error->message);
        g_error_free(error);        
        success = FALSE;
    }
    return success;
}

/* Load the configuration from the config file, if it exists */
static void load_config(void)
{
    GKeyFile *key_file = g_key_file_new();
    gchar *config_file = get_config_filename();
    
    if (load_key_file(key_file, config_file, G_KEY_FILE_NONE)) {
        gint color;
        glong temp_value;
        color = key_file_get_color(key_file, "color", "matching-pair");
        if (color != -1)
            highlightColor_G[MATCHING_PAIR] = color;
        else
            g_warning("load_config(): matching-pair not loaded\n");
        color = key_file_get_color(key_file, "color", "nonmatching-pair");
        if (color != -1)
            highlightColor_G[NONMATCHING_PAIR] = color;
        else
            g_warning("load_config(): nonmatching-pair not loaded\n");
        color = key_file_get_color(key_file, "color", "empty-tag");
        if (color != -1)
            highlightColor_G[EMPTY_TAG] = color;
        else
            g_warning("load_config(): empty-tag not read\n");
        if (key_file_get_long(key_file, "color", "alpha", &temp_value))
            if (temp_value >= HIGHLIGHT_ALPHA_MIN && temp_value <= HIGHLIGHT_ALPHA_MAX)
                highlightAlpha_G = temp_value;
            else
                g_warning("load_config(): alpha out of range\n");
        else
            g_warning("load_config(): alpha not loaded\n");
    }

    g_key_file_free(key_file);
    g_free(config_file);
}

/* Write the configuration to the config file */
static void write_config(void)
{
    GKeyFile *key_file = g_key_file_new();
    gchar *config_file = get_config_filename();
    gchar *dir_name = g_path_get_dirname(config_file);
    gint error_code;
    GError *error = NULL;

    load_key_file(key_file, config_file, G_KEY_FILE_KEEP_COMMENTS);
    key_file_write_color(key_file, "color", "matching-pair",
                         highlightColor_G[MATCHING_PAIR]);
    key_file_write_color(key_file, "color", "nonmatching-pair",
                         highlightColor_G[NONMATCHING_PAIR]);
    key_file_write_color(key_file, "color", "empty-tag",
                         highlightColor_G[EMPTY_TAG]);
    key_file_write_long(key_file, "color", "alpha", highlightAlpha_G);

    /* create the config file's directory, if it doesn't exist */
    error_code = utils_mkdir(dir_name, TRUE);
    if (! error_code) {
        if (! g_key_file_save_to_file(key_file, config_file, &error)) {
            g_warning("write_config(): Failed to write to file '%s'\n%s",
                       config_file, error->message);
            g_error_free(error);
        }
    } else
        g_warning("write_config(): Failed to create directory '%s'\n%s",
                   dir_name, g_strerror(error_code));

    g_key_file_free(key_file);
    g_free(dir_name);
    g_free(config_file);
}


PluginCallback plugin_callbacks[] =
{
    { "editor-notify", (GCallback) &on_editor_notify, FALSE, NULL },
    { "update-editor-menu", (GCallback) &on_editor_menu_popup, FALSE, NULL },
    { NULL, NULL, FALSE, NULL }
};


void plugin_init(GeanyData *data)
{
    GeanyKeyGroup *kb_group;
    goto_matching_tag = gtk_menu_item_new_with_label (_("Goto Matching XML Tag"));
    select_matching_tag = gtk_menu_item_new_with_label (_("Select Matching XML Tag"));
    g_signal_connect (goto_matching_tag, "activate",
                    G_CALLBACK (on_goto_matching_tag), NULL);
    g_signal_connect (select_matching_tag, "activate",
                    G_CALLBACK (on_select_matching_tag), NULL);
    gtk_container_add (GTK_CONTAINER (data->main_widgets->editor_menu),
                     goto_matching_tag);
    gtk_container_add (GTK_CONTAINER (data->main_widgets->editor_menu),
                     select_matching_tag);
    kb_group = plugin_set_key_group (geany_plugin, PLUGIN, KB_COUNT, NULL);
    keybindings_set_item (kb_group, KB_MATCH_TAG, NULL,
                        0, 0, "goto_matching_tag", _("Go To Matching Tag"), goto_matching_tag);
    keybindings_set_item (kb_group, KB_SELECT_TAG, NULL,
                        0, 0, "select_matching_tag", _("Select To Matching Tag"), select_matching_tag);
    memcpy(&highlightColor_G, &HIGHLIGHT_COLOR_DEFAULTS, sizeof(highlightColor_G));
    load_config();
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

/*** Preference dialog ***/

/* Returns the path of the plugin's data directory. The path must be freed
 * by the caller
 */
static gchar * get_data_dir_path(const gchar *filename)
{
  gchar *prefix = NULL;
  gchar *path;

#ifdef G_OS_WIN32
  prefix = g_win32_get_package_installation_directory_of_module(NULL);
#elif defined(__APPLE__)
  if (g_getenv("GEANY_PLUGINS_SHARE_PATH"))
    return g_build_filename(g_getenv("GEANY_PLUGINS_SHARE_PATH"), 
                            PLUGIN, filename, NULL);
#endif
  path = g_build_filename(prefix ? prefix : "", PLUGINDATADIR, filename, NULL);
  g_free(prefix);
  return path;
}

#if GTK_CHECK_VERSION(3,4,0)
/* Convert an RGB (0xRRGGBB) number in integer form into a GdkRGBA */
static void rgba_from_int(GdkRGBA *color, gint num)
{
    gfloat conv_factor = 1.0 / 255;
    color->red = ((num & 0xff0000) >> 16) * conv_factor;
    color->green = ((num & 0x00ff00) >> 8) * conv_factor;
    color->blue = (num & 0x0000ff) * conv_factor;
    color->alpha = 1.0;
}

/* Convert a GdkRGBA into an integer RGB value (0xRRGGBB) */
static int int_from_rbga(GdkRGBA *color)
{
    gfloat conv_factor = 255.0;
    int red = (int)(color->red * conv_factor);
    int green = (int)(color->green * conv_factor);
    int blue = (int)(color->blue * conv_factor);
    return ((red << 16) | (green << 8) | blue);
}
#else
/* Convert an RGB (0xRRGGBB) number in integer form into a GdkColor */
static void color_from_int(GdkColor *color, gint num)
{
    gint conv_factor = 65535 / 255;
    color->red = ((num & 0xff0000) >> 16) * conv_factor;
    color->green = ((num & 0x00ff00) >> 8) * conv_factor;
    color->blue = (num & 0x0000ff) * conv_factor;
}

/* Convert a GdkColor into an integer RGB value (0xRRGGBB) */
static int int_from_color(GdkColor *color)
{
    gfloat conv_factor = 255.0 / 65535.0;
    int red = (int)(color->red * conv_factor);
    int green = (int)(color->green * conv_factor);
    int blue = (int)(color->blue * conv_factor);
    return ((red << 16) | (green << 8) | blue);
}
#endif

static void free_config_dialog_objects(PrefDialogObjects *dialog_objects)
{
    g_object_unref(dialog_objects->main_prefs_frame);
    g_free(dialog_objects);
}

/* Set the color of the color button from an integer RGB value */
static void set_color_button_color(GtkWidget *color_button, gint value)
{
#if GTK_CHECK_VERSION(3,4,0)
    GdkRGBA color;
    rgba_from_int(&color, value);
    gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(color_button), &color);
#else
    GdkColor color;
    color_from_int(&color, value);
    gtk_color_button_set_color(GTK_COLOR_BUTTON(color_button), &color);
#endif
}

/* Get the color in integer RGB form of the color button */
static gint get_color_button_color(GtkWidget *color_button)
{
#if GTK_CHECK_VERSION(3,4,0)
    GdkRGBA color;
    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(color_button), &color);
    return int_from_rbga(&color);
#else
    GdkColor color;
    gtk_color_button_get_color(GTK_COLOR_BUTTON(color_button), &color);
    return int_from_color(&color);
#endif
}

/* Handle responses to the preferences dialog */
static void on_configure_response(GtkDialog *dialog, gint response, 
                                  PrefDialogObjects *dialog_objects)
{
    if (response == GTK_RESPONSE_APPLY || response == GTK_RESPONSE_OK) {
        /* update configuration values from the preferences dialog */
        highlightColor_G[MATCHING_PAIR] = 
            get_color_button_color(dialog_objects->matching_pair_color_button);
        highlightColor_G[NONMATCHING_PAIR] = 
            get_color_button_color(dialog_objects->nonmatching_pair_color_button);
        highlightColor_G[EMPTY_TAG] = 
            get_color_button_color(dialog_objects->empty_tag_color_button);
        highlightAlpha_G = 
            (gint)gtk_adjustment_get_value(GTK_ADJUSTMENT(dialog_objects->alpha_adjust));

        write_config();

        /* update the current document, if it's a markup document */
        GeanyDocument *document = document_get_current();
        if (document) {
            gint lexer;
            lexer = sci_get_lexer(document->editor->sci);
            if((lexer == SCLEX_HTML) || (lexer == SCLEX_XML) || (lexer == SCLEX_PHPSCRIPT))
                run_tag_highlighter(document->editor->sci);
        }
    }
}

/* Return the settings in the preferences dialog to the defaults */
static void on_defaults_button_pressed(GtkButton *button, PrefDialogObjects *dialog_objects)
{
    set_color_button_color(dialog_objects->matching_pair_color_button,
                           HIGHLIGHT_COLOR_DEFAULTS[MATCHING_PAIR]);
    set_color_button_color(dialog_objects->nonmatching_pair_color_button,
                           HIGHLIGHT_COLOR_DEFAULTS[NONMATCHING_PAIR]);
    set_color_button_color(dialog_objects->empty_tag_color_button,
                           HIGHLIGHT_COLOR_DEFAULTS[EMPTY_TAG]);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(dialog_objects->alpha_adjust),
                             (gdouble)HIGHLIGHT_ALPHA_DEFAULT);
}

/* Set up the preferences dialog */
GtkWidget *
plugin_configure(GtkDialog *dialog)
{
    GError *error = NULL;
    GtkWidget *prefs_frame = NULL;
    GtkBuilder *builder = gtk_builder_new();
    gchar *path = get_data_dir_path("prefs.ui");

    /* get the builder from the UI file */
    gtk_builder_set_translation_domain(builder, GETTEXT_PACKAGE);
    if (! gtk_builder_add_from_file(builder, path, &error)) {
        g_critical(_("Can't find the preferences UI file!\n%s"), error->message);
        g_error_free(error);
    } else {
        /* get the dialog objects from the builder */
        PrefDialogObjects *dialog_objects = g_malloc(sizeof(PrefDialogObjects));
        dialog_objects->main_prefs_frame = 
            GTK_WIDGET(gtk_builder_get_object(builder, "prefs-frame"));
        dialog_objects->matching_pair_color_button = 
            GTK_WIDGET(gtk_builder_get_object(builder, "matching-pair-color-button"));
        set_color_button_color(dialog_objects->matching_pair_color_button,
                               highlightColor_G[MATCHING_PAIR]);
        dialog_objects->nonmatching_pair_color_button = 
            GTK_WIDGET(gtk_builder_get_object(builder, "nonmatching-pair-color-button"));
        set_color_button_color(dialog_objects->nonmatching_pair_color_button,
                               highlightColor_G[NONMATCHING_PAIR]);
        dialog_objects->empty_tag_color_button = 
            GTK_WIDGET(gtk_builder_get_object(builder, "empty-tag-color-button"));
        set_color_button_color(dialog_objects->empty_tag_color_button,
                               highlightColor_G[EMPTY_TAG]);
        dialog_objects->alpha_adjust =
            G_OBJECT(gtk_builder_get_object(builder, "alpha-adjustment"));
        gtk_adjustment_set_value(GTK_ADJUSTMENT(dialog_objects->alpha_adjust),
                                 (gdouble)highlightAlpha_G);
        dialog_objects->defaults_button =
            GTK_WIDGET(gtk_builder_get_object(builder, "defaults-button"));

        /* increase the reference count of the dialog frame */
        prefs_frame = g_object_ref_sink(dialog_objects->main_prefs_frame);
        
        /* connect handlers */
        g_signal_connect_data(dialog, "response",
                              G_CALLBACK(on_configure_response), dialog_objects,
                              (GClosureNotify)free_config_dialog_objects, 0);
        g_signal_connect(dialog_objects->defaults_button, "clicked",
                         G_CALLBACK(on_defaults_button_pressed), dialog_objects);
        }
    
    g_free(path);
    g_object_unref(builder);
    
    return prefs_frame;
}
