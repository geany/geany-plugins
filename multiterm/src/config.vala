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

		public List<ShellConfig> shell_configs { get { return _shell_configs; } }
	}
}
