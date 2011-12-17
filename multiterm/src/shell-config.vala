
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
