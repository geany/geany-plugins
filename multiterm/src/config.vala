
namespace MultiTerm
{

	public struct ShellConfig
	{
		public string section;
		public string name;
		public string? command;
		public bool track_title;
		public Config? cfg;
	}

	public class Config
	{
		private KeyFile kf;
		private string _filename;

		public Config(string filename)
		{
			_filename = filename;
			try
			{
				kf = new KeyFile();
				kf.load_from_file(_filename, KeyFileFlags.KEEP_COMMENTS | KeyFileFlags.KEEP_TRANSLATIONS);
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

		public void store()
		{
			string data = kf.to_data();
			try
			{
				FileUtils.set_contents(_filename, data);
			}
			catch (FileError err)
			{
				warning("Unable to save config file %s: %s", _filename, err.message);
			}
		}

		public string background_color
		{
			owned get
			{
				try
				{
					return kf.get_string("general", "bg_color");
				}
				catch (KeyFileError err)
				{
					warning("Error reading config value: %s", err.message);
					return "#ffffff";
				}
			}
			set { kf.set_string("general", "bg_color", value); }
		}

		public string foreground_color
		{
			owned get
			{
				try
				{
					return kf.get_string("general", "fg_color");
				}
				catch (KeyFileError err)
				{
					warning("Error reading config value: %s", err.message);
					return "#000000";
				}
			}
			set { kf.set_string("general", "fg_color", value); }
		}

		public string font
		{
			owned get
			{
				try
				{
					return kf.get_string("general", "font");
				}
				catch (KeyFileError err)
				{
					warning("Error reading config value: %s", err.message);
					return "Monospace 9";
				}
			}
			set { kf.set_string("general", "font", value); }
		}

		public string filename
		{
			get { return _filename; }
		}

		public List<ShellConfig?> shell_configs
		{
			owned get
			{
				List<ShellConfig?> list = new List<ShellConfig?>();
				string[] sections = kf.get_groups();

				for (int i=0; i < sections.length; i++)
				{
					if (sections[i].has_prefix("shell="))
					{
						try
						{
							ShellConfig sh = ShellConfig();
							sh.cfg = this;
							sh.section = sections[i];
							sh.name = kf.get_string(sections[i], "name");
							sh.command = kf.get_string(sections[i], "command");
							sh.track_title = kf.get_boolean(sections[i], "track_title");

							list.append(sh);
						}
						catch (KeyFileError err)
						{
							warning("Error reading config value: %s", err.message);
						}
					}
				}

				return list;
			}
		}

	}
}
