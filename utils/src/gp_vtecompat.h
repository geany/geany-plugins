/*
 * Copyright 2017 LarsGit223
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/* Compatibility macros to support different VTE versions */

#ifndef GP_VTECOMPAT_H
#define GP_VTECOMPAT_H

G_BEGIN_DECLS

/* Replace call to vte_terminal_copy_clipboard() with a call to
   vte_terminal_copy_clipboard_format starting from version 0.50 */
#if VTE_CHECK_VERSION(0, 50, 0)
#define vte_terminal_copy_clipboard(terminal) \
        vte_terminal_copy_clipboard_format(terminal, VTE_FORMAT_TEXT)
#endif

/* Version info for VTE is incomplete so we use all the macros below
   simply if GTK3 is used. */
#if GTK_CHECK_VERSION(3, 0, 0)
/* Remove vte_terminal_set_emulation() starting from 0.26.2 version */
#define vte_terminal_set_emulation(vte, emulation)

/* Replace call to vte_terminal_set_font_from_string() with a call to
   gp_vtecompat_set_font_from_string() starting from version 0.26.2 */
#define vte_terminal_set_font_from_string(vte, font) \
        gp_vtecompat_set_font_from_string(vte, font)

/* Replace call to vte_pty_new_foreign() with a call to
   vte_pty_new_foreign_sync() starting from version 0.26.2 */
#define vte_pty_new_foreign(pty, error) \
        vte_pty_new_foreign_sync(pty, NULL, error)

/* Replace call to vte_terminal_set_pty_object() with a call to
   vte_terminal_set_pty() starting from version 0.26.2 */
#define vte_terminal_set_pty_object(terminal, pty) \
        vte_terminal_set_pty(terminal, pty)

void gp_vtecompat_set_font_from_string(VteTerminal *vte, char *font);
#endif

G_END_DECLS

#endif /* GP_VTECOMPAT_H */

