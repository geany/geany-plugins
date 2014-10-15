/*
 * plugin.vala - This file is part of the Geany MultiTerm plugin
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

using Gtk;
using Geany;
using MultiTerm;

public Plugin		geany_plugin;
public Data			geany_data;
public Functions	geany_functions;

/* Widgets to clean up when the plugin is unloaded */
private List<Widget> toplevel_widgets = null;

/* Geany calls this to determine min. required API/ABI version */
public int plugin_version_check(int abi_version)
{
	return Plugin.version_check(abi_version, 185);
}

/* Geany calls this to get some info about the plugin */
public void plugin_set_info(Plugin.Info info)
{
	info.set("MultiTerm",
			 "Multi-tabbed virtual terminal emulator.",
			 "0.1", "Matthew Brush <matt@geany.org>");
}

/* Geany will call this when the plugin is loaded */
public void plugin_init(Geany.Data data)
{
	string config_file;
	string config_dir;
	Label label;
	Alignment align;
	MultiTerm.Notebook notebook;

	/* Needed for GObject type system not to freak out about
	 * unregistering and re-registering new types */
	geany_plugin.module_make_resident();

	toplevel_widgets = new List<Widget>();

	/* Initialize plugin's configuration directory/file */
	config_dir = Path.build_filename(geany_data.app.config_dir, "plugins", "multiterm");
	config_file = Path.build_filename(config_dir, "multiterm.conf");
    DirUtils.create_with_parents(config_dir, 0755);
	try
	{
		/* Create a default config file if there isn't one yet */
		if (!FileUtils.test(config_file, FileTest.EXISTS | FileTest.IS_REGULAR))
			FileUtils.set_contents(config_file, default_config);
	}
	catch (FileError err)
	{
		warning("Unable to write default config file: %s", err.message);
	}

	/* Setup the widgets */
	align = new Alignment(0.5f, 0.5f, 1.0f, 1.0f);
	notebook = new MultiTerm.Notebook(config_file);
	align.add(notebook as Gtk.Notebook);
	align.show_all();
	toplevel_widgets.append(align);

	label = new Label("MultiTerm");
	notebook.set_data<Label>("label", label);
	notebook.set_data<Gtk.Notebook>("msgwin_notebook", data.main_widgets.message_window_notebook);
	notebook.set_data<Gtk.Notebook>("sidebar_notebook", data.main_widgets.sidebar_notebook);

	if (notebook.cfg.location == "msgwin")
	{
		data.main_widgets.message_window_notebook.append_page(align, label);
		data.main_widgets.message_window_notebook.set_current_page(
			data.main_widgets.message_window_notebook.page_num(align));
	}
	else
	{
		data.main_widgets.sidebar_notebook.append_page(align, label);
		data.main_widgets.sidebar_notebook.set_current_page(
			data.main_widgets.sidebar_notebook.page_num(align));
	}


	/* Activate the new MultiTerm tab */
}

/* Geany will call this when the plugin is unloaded, so be sure to
 * free/destroy anything needed here */
public void plugin_cleanup ()
{
	foreach (Widget wid in toplevel_widgets)
		wid.destroy();
	toplevel_widgets = null;
}
