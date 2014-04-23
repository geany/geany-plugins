/*
 * defconf.vala - This file is part of the Geany MultiTerm plugin
 *
 * Copyright (c) 2012 Matthew Brush <matt@geany.org>.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

/*
 * This file contains the default configuration which will be written
 * to the user's config dir if it doesn't exist.
 *
 * Try to make all defaults apply from here rather than hardcoded in
 * other source files.
 */

namespace MultiTerm
{
	public const string default_config =
"""########################################################################
# MultiTerm Configuration File                                         #
#======================================================================#
#                                                                      #
# You can configure the behaviour of the MultiTerm plugin by adjusting #
# the values in this file.  Lines beginning with a # are considered    #
# comments and are left as is.  Group/section names go in [] and keys  #
# and values are separated by an = symbol.  Where more than one value  #
# can be supplied, as in a list, separate the values with the ; symbol.#
#                                                                      #
# Groups/section names beginning with 'shell=' denote a type of        #
# terminal/shell that can be opened.  The name of the shell follows    #
# the = symbol. Each shell can have it's own specific VTE              #
# configuration and can run it's own child command.  As an example, if #
# you wanted to use the Python interpreter shell instead of the        #
# default shell, specify 'command=python'.                             #
#                                                                      #
# Keys/values commented out or empty values will cause MultiTerm to    #
# use default values.                                                  #
#                                                                      #
########################################################################

#=======================================================================
# General Settings
#=======================================================================
[general]

# Where to put the multiterm notebook in the Geany user interface
# one of: sidebar, msgwin
location=msgwin

# Make tabs take up as much space as is available
full_width_tabs=true

# Allow reordering of tabs
reorderable_tabs=true

# When there is only one tab left in the notebook, hide the tabs area
hide_tabs_on_last=true

# When launching an external terminal from the MultiTerm context menu,
# use this terminal.  Good choices include xterm, gnome-terminal,
# xfce4-terminal or konsole.
external_terminal=xterm

# Save which shells were open when Geany closes and restore their
# tabs when it restarts.
save_tabs=false

# Whether to show tabs or not
show_tabs=true

bg_color=#ffffff
fg_color=#000000
font=Monospace 9


#=======================================================================
# Default Shell
#=======================================================================
[shell=default]

# This will be the tabs default tab label and won't change unless
# track_title is set to true.
name=Default Shell

# This is the command to fork in the VTE, leave blank for default shell
command=

# Make the tab's label track the VTE title
track_title=true

# Background color, foreground color and font for the VTE
bg_color=#ffffff
fg_color=#000000
font=Monospace 9

# Whether to allow bold fonts in the VTE
#allow_bold=true

# Whether to beep when the child outputs a bell sequence.
#audible_bell=true

# Controls whether the cursor blinks or not, one of:
#   system (or blank), on, off
#cursor_blink_mode=system

# Controls the shape of the VTE cursor, one of:
#   block, ibeam, underline
#cursor_shape=block

# Controls how erasing characters is handled, one of:
#   auto, ascii_backspace, ascii_delete, delete_sequence, tty
#backspace_binding=auto

# Whether to hide the mouse pointer on key press if it's in the
# terminal window
#pointer_autohide=false

# Scroll to the prompt at the bottom of the scrollback buffer on key
# press
#scroll_on_keystroke=true

# Scroll to the bottom of the scrollback buffer when the child sends
# output
#scroll_on_output=false

# The number of lines to keep in the scrollback buffer
#scrollback_lines=512

# Whether the terminal will present a visible bell when the child
# sends a bell sequence.  The terminal will clear itself to the
# default foreground color and then repaint itself.
#visible_bell=false

# When the user double-clicks to start selection, the terminal will
# extend the selection on word boundaries. It will treat characters
# the word-chars characters as parts of words, and all other
# characters as word separators. Ranges of characters can be
# specified by separating them with a hyphen.
word_chars=-A-Za-z0-9,./\\?%&#:_


#=======================================================================
# Other Shells
#=======================================================================

# You can define additional shells just like the default shell but
# using other commands and/or settings.

#[shell=python]
#name=Python Shell
#command=python
#track_title=false

#[shell=irb]
#name=Ruby Shell
#command=irb
#track_title=false
""";
}
