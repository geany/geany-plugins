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
	}
}
