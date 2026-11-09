/*
 * geanylinemove.c — Geany plugin: move lines/selections up or down
 *
 * Binds Ctrl+Shift+Up / Ctrl+Shift+Down to Scintilla's native
 * SCI_MOVESELECTEDLINESUP / SCI_MOVESELECTEDLINESDOWN messages.
 * Works on single lines and multi-line selections.
 *
 * Copyright 2026 teknopaul
 * License: GPL-2.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <gdk/gdkkeysyms.h>
#include <geanyplugin.h>

GeanyPlugin  *geany_plugin;
GeanyData    *geany_data;

PLUGIN_VERSION_CHECK(224)
PLUGIN_SET_INFO("Line Move",
                "Move the current line or selection up/down with Ctrl+Shift+Up/Down.",
                VERSION,
                "teknopaul")

typedef enum {
    KB_MOVE_LINE_UP,
    KB_MOVE_LINE_DOWN,
    KB_COUNT
} KeyBinding;

static void kb_move_up(G_GNUC_UNUSED guint key_id)
{
    GeanyDocument *doc = document_get_current();
    if (doc)
        scintilla_send_message(doc->editor->sci, SCI_MOVESELECTEDLINESUP, 0, 0);
}

static void kb_move_down(G_GNUC_UNUSED guint key_id)
{
    GeanyDocument *doc = document_get_current();
    if (doc)
        scintilla_send_message(doc->editor->sci, SCI_MOVESELECTEDLINESDOWN, 0, 0);
}

void plugin_init(G_GNUC_UNUSED GeanyData *data)
{
    GeanyKeyGroup *kg = plugin_set_key_group(geany_plugin, "linemove", KB_COUNT, NULL);
    keybindings_set_item(kg, KB_MOVE_LINE_UP, kb_move_up,
                         GDK_KEY_Up, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                         "move_line_up", "Move line/selection up", NULL);
    keybindings_set_item(kg, KB_MOVE_LINE_DOWN, kb_move_down,
                         GDK_KEY_Down, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                         "move_line_down", "Move line/selection down", NULL);
}

void plugin_cleanup(void)
{
}
