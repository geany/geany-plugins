/*
 * shell-config.vala - This file is part of the Geany MultiTerm plugin
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

using Vte;

namespace MultiTerm
{
	public class ShellConfig
	{
		public Config _cfg;
		string _section;
		const string warning_tmpl = "Unable to read value for '%s' key: %s";

		internal Config cfg { get { return _cfg; } }
		internal KeyFile kf { get { return cfg.kf; } }

		public ShellConfig(Config cfg, string section)
		{
			_cfg = cfg;
			_section = section;
		}

		public string section { get { return _section;} }

		public string name
		{
			owned get
			{
				try { return kf.get_string(_section, "name"); }
				catch (KeyFileError err) { return "Default"; }
			}
			set
			{
				kf.set_string(_section, "name", value);
				cfg.store_eventually();
			}
		}

		public string command
		{
			owned get
			{
				try { return  kf.get_string(_section, "command"); }
				catch (KeyFileError err) { return "sh"; }
			}
			set
			{
				kf.set_string(_section, "command", value);
				cfg.store_eventually();
			}
		}

		public bool track_title
		{
			get
			{
				try { return kf.get_boolean(_section, "track_title"); }
				catch (KeyFileError err) { return true; }
			}
			set
			{
				kf.set_boolean(_section, "track_title", value);
				cfg.store_eventually();
			}
		}

		public string background_color
		{
			owned get
			{
				try { return kf.get_string(_section, "bg_color"); }
				catch (KeyFileError err) { return "#ffffff"; }
			}
			set
			{
				kf.set_string(_section, "bg_color", value);
				cfg.store_eventually();
			}
		}

		public string foreground_color
		{
			owned get
			{
				try { return kf.get_string(_section, "fg_color"); }
				catch (KeyFileError err) { return "#000000"; }
			}
			set
			{
				kf.set_string(_section, "fg_color", value);
				cfg.store_eventually();
			}
		}

		public string font
		{
			owned get
			{
				try { return kf.get_string(_section, "font"); }
				catch (KeyFileError err) { return "Monospace 9"; }
			}
			set
			{
				kf.set_string(_section, "font", value);
				cfg.store_eventually();
			}
		}

		public bool allow_bold
		{
			get
			{
				try { return kf.get_boolean(_section, "allow_bold"); }
				catch (KeyFileError err) { return true; }
			}
			set
			{
				kf.set_boolean(_section, "allow_bold", value);
				cfg.store_eventually();
			}
		}

		public bool audible_bell
		{
			get
			{
				try { return kf.get_boolean(_section, "audible_bell"); }
				catch (KeyFileError err) { return true; }
			}
			set
			{
				kf.set_boolean(_section, "audible_bell", value);
				cfg.store_eventually();
			}
		}

		public TerminalCursorBlinkMode cursor_blink_mode
		{
			get
			{
				try
				{
					string blink_mode = kf.get_string(_section, "cursor_blink_mode").down();
					if (blink_mode == "on" || blink_mode == "true")
						return TerminalCursorBlinkMode.ON;
					else if (blink_mode == "off" || blink_mode == "false")
						return TerminalCursorBlinkMode.OFF;
					else
						return TerminalCursorBlinkMode.SYSTEM;
				}
				catch (KeyFileError err) { return TerminalCursorBlinkMode.SYSTEM; }
			}
			set
			{
				switch (value)
				{
					case TerminalCursorBlinkMode.ON:
						kf.set_string(_section, "cursor_blink_mode", "on");
						break;
					case TerminalCursorBlinkMode.OFF:
						kf.set_string(_section, "cursor_blink_mode", "off");
						break;
					default:
						kf.set_string(_section, "cursor_blink_mode", "system");
						break;
				}
				cfg.store_eventually();
			}
		}

		public TerminalCursorShape cursor_shape
		{
			get
			{
				try
				{
					string shape = kf.get_string(_section, "cursor_shape").down();
					if (shape == "ibeam")
						return TerminalCursorShape.IBEAM;
					else if (shape == "underline")
						return TerminalCursorShape.UNDERLINE;
					else
						return TerminalCursorShape.BLOCK;
				}
				catch (KeyFileError err) { return TerminalCursorShape.BLOCK; }
			}
			set
			{
				switch (value)
				{
					case TerminalCursorShape.IBEAM:
						kf.set_string(_section, "cursor_shape", "ibeam");
						break;
					case TerminalCursorShape.UNDERLINE:
						kf.set_string(_section, "cursor_shape", "underline");
						break;
					default:
						kf.set_string(_section, "cursor_shape", "block");
						break;
				}
				cfg.store_eventually();
			}
		}

		public TerminalEraseBinding backspace_binding
		{
			get
			{
				try
				{
					string binding = kf.get_string(_section, "backspace_binding").down();
					if (binding == "ascii_backspace")
						return TerminalEraseBinding.ASCII_BACKSPACE;
					else if (binding == "ascii_delete")
						return TerminalEraseBinding.ASCII_DELETE;
					else if (binding == "delete_sequence")
						return TerminalEraseBinding.DELETE_SEQUENCE;
					else if (binding == "tty")
						return TerminalEraseBinding.TTY;
					else
						return TerminalEraseBinding.AUTO;
				}
				catch (KeyFileError err) { return TerminalEraseBinding.AUTO; }
			}
			set
			{
				switch (value)
				{
					case TerminalEraseBinding.ASCII_BACKSPACE:
						kf.set_string(_section, "backspace_binding", "ascii_backspace");
						break;
					case TerminalEraseBinding.ASCII_DELETE:
						kf.set_string(_section, "backspace_binding", "ascii_delete");
						break;
					case TerminalEraseBinding.DELETE_SEQUENCE:
						kf.set_string(_section, "backspace_binding", "delete_sequence");
						break;
					case TerminalEraseBinding.TTY:
						kf.set_string(_section, "backspace_binding", "tty");
						break;
					default:
						kf.set_string(_section, "backspace_binding", "auto");
						break;
				}
				cfg.store_eventually();
			}
		}

		public bool pointer_autohide
		{
			get
			{
				try { return kf.get_boolean(_section, "pointer_autohide"); }
				catch (KeyFileError err) { return false; }
			}
			set
			{
				kf.set_boolean(_section, "pointer_autohide", value);
				cfg.store_eventually();
			}
		}

		public bool scroll_on_keystroke
		{
			get
			{
				try { return kf.get_boolean(_section, "scroll_on_keystroke"); }
				catch (KeyFileError err) { return true; }
			}
			set
			{
				kf.set_boolean(_section, "scroll_on_keystroke", value);
				cfg.store_eventually();
			}
		}

		public bool scroll_on_output
		{
			get
			{
				try { return kf.get_boolean(_section, "scroll_on_output"); }
				catch (KeyFileError err) { return false; }
			}
			set
			{
				kf.set_boolean(_section, "scroll_on_output", value);
				cfg.store_eventually();
			}
		}

		public int scrollback_lines
		{
			get
			{
				try { return kf.get_integer(_section, "scrollback_lines"); }
				catch (KeyFileError err) { return 512; }
			}
			set
			{
				kf.set_integer(_section, "scrollback_lines", value);
				cfg.store_eventually();
			}
		}

		public bool visible_bell
		{
			get
			{
				try { return kf.get_boolean(_section, "visible_bell"); }
				catch (KeyFileError err) { return false; }
			}
			set
			{
				kf.set_boolean(_section, "visible_bell", value);
				cfg.store_eventually();
			}
		}

		public string word_chars
		{
			owned get
			{
				try { return kf.get_string(_section, "word_chars"); }
				catch (KeyFileError err) { return ""; }
			}
			set
			{
				kf.set_string(_section, "word_chars", value);
				cfg.store_eventually();
			}
		}
	}
}
