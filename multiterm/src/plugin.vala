
using Gtk;
using Geany;
using MultiTerm;

public Plugin		geany_plugin;
public Data		geany_data;
public Functions	geany_functions;

public MultiTerm.Notebook nb;
private Alignment align;


public int plugin_version_check(int abi_version)
{
	return Plugin.version_check(abi_version, 185);
}

public void plugin_set_info(Plugin.Info info)
{
	info.set("MultiTerm",
			 "Multi-tabbed virtual terminal emulator.",
			 "0.1", "Matthew Brush <mbrush@leftclick.ca>");
}

static string init_config_file()
{
	string config_dir = Path.build_filename(geany_data.app.config_dir,
                            "plugins", "multiterm");

    DirUtils.create_with_parents(config_dir, 0755);

	string config_file = Path.build_filename(config_dir, "multiterm.conf");

	try
	{
		if (!FileUtils.test(config_file, FileTest.EXISTS | FileTest.IS_REGULAR))
			FileUtils.set_contents(config_file, MultiTerm.default_config);
	}
	catch (FileError err)
	{
		warning("Unable to write default config file: %s", err.message);
	}

	return config_file;
}

public void plugin_init(Geany.Data data)
{
	geany_plugin.module_make_resident();


	align = new Alignment(0.5f, 0.5f, 1.0f, 1.0f);

	nb = new MultiTerm.Notebook(init_config_file());

	align.add(nb as Gtk.Notebook);

	data.main_widgets.message_window_notebook.append_page(
			align, new Label("MultiTerm"));

	align.show_all();

	//align.set_size_request(100,100); 	// for whatever reason, makes
	//									// it size properly

	Gtk.Notebook mwnb = (Gtk.Notebook)data.main_widgets.message_window_notebook;
	mwnb.set_current_page(mwnb.page_num(align));
}

public void plugin_cleanup ()
{
	align.destroy();
}
