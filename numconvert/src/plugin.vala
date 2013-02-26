/*
 * plugin.vala - This file is part of the Geany NumConvert plugin
 *
 * Copyright (c) 2013 Thomas Martitz <kugel@rockbox.org>.
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
using NumConvert;

public Plugin       geany_plugin;
public Data         geany_data;
public Functions    geany_functions;

/* Widgets to clean up when the plugin is unloaded */
private Gtk.MenuItem    plugin_item;

/* Geany calls this to determine min. required API/ABI version */
public int plugin_version_check(int abi_version)
{
    return Plugin.version_check(abi_version, 185);
}

/* Geany calls this to get some info about the plugin */
public void plugin_set_info(Plugin.Info info)
{
    info.set("NumConvert",
             "Number Conversion plugin",
             "0.1", "Thomas Martitz <kugel@rockbox.org>");
}

/* Geany will call this when the plugin is loaded */
public void plugin_init(Geany.Data data)
{
    unowned MenuShell       editor_menu;
    Gtk.MenuItem    item;
    ContextMenu menu;


    /* Needed for GObject type system not to freak out about
     * unregistering and re-registering new types */
    geany_plugin.module_make_resident();

    editor_menu = data.main_widgets.editor_menu;
    
    menu = new ContextMenu();
    item = new Gtk.MenuItem.with_label("Convert number");
    item.show();
    item.set_submenu(menu);
    item.activate.connect(() => menu.on_acivate(item));
    editor_menu.append(item);

    plugin_item = item;
}

/* Geany will call this when the plugin is unloaded, so be sure to
 * free/destroy anything needed here */
public void plugin_cleanup ()
{
    geany_data.main_widgets.editor_menu.remove(plugin_item);
    plugin_item.destroy();
}
