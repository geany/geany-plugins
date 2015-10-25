/*
 * config.vala - This file is part of the Geany MultiTerm plugin
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

	public class Config
	{
		internal KeyFile kf;
		private string _filename;
		private List<ShellConfig> _shell_configs = new List<ShellConfig>();

		public Config(string filename)
		{
			_filename = filename;
			reload();
		}

		public bool store()
		{
			string data = kf.to_data();
			try
			{
				FileUtils.set_contents(_filename, data);
				return false;
			}
			catch (FileError err)
			{
				warning(_("Unable to save config file %s: %s"), _filename, err.message);
				return true;
			}
		}

		public void store_eventually()
		{
			Idle.add(() => store());
		}

		public void reload()
		{
			try
			{
				kf = new KeyFile();

				/* Delete the old keys and groups */
				foreach (string group in kf.get_groups())
				{
					foreach (string key in kf.get_keys(group))
						kf.remove_key(group, key);
					kf.remove_group(group);
				}

				/* Load from file again */
				kf.load_from_file(_filename,
					KeyFileFlags.KEEP_COMMENTS | KeyFileFlags.KEEP_TRANSLATIONS);

				/* Reload shell configurations */
				_shell_configs = new List<ShellConfig>();
				foreach (string section in kf.get_groups())
				{
					if (section.has_prefix("shell="))
						_shell_configs.append(new ShellConfig(this, section));
				}
			}
			catch (KeyFileError err)
			{
				warning(_("Unable to load config file %s: %s"), _filename, err.message);
			}
			catch (FileError err)
			{
				warning(_("Unable to load config file %s: %s"), _filename, err.message);
			}
		}

		public string filename { get { return _filename; } }

		public bool show_tabs
		{
			get
			{
				try { return kf.get_boolean("general", "show_tabs"); }
				catch (KeyFileError err) { return true; }
			}
			set
			{
				kf.set_boolean("general", "show_tabs", value);
				store_eventually();
			}
		}

		public string external_terminal
		{
			owned get
			{
				try { return kf.get_string("general", "external_terminal"); }
				catch (KeyFileError err) { return "xterm"; }
			}
			set
			{
				kf.set_string("general", "external_terminal", value);
				store_eventually();
			}
		}

		public string location
		{
			owned get
			{
				try { return kf.get_string("general", "location"); }
				catch (KeyFileError err) { return "msgwin"; }
			}
			set
			{
				kf.set_string("general", "location", value);
				store_eventually();
			}
		}

		public List<ShellConfig> shell_configs { get { return _shell_configs; } }
	}
}
