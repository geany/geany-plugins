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
				warning("Unable to save config file %s: %s", _filename, err.message);
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
				warning("Unable to load config file %s: %s", _filename, err.message);
			}
			catch (FileError err)
			{
				warning("Unable to load config file %s: %s", _filename, err.message);
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
