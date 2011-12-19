using Gtk;

namespace MultiTerm
{
	public class ContextMenu : Menu
	{
		public signal void new_shell_activate(ShellConfig sh);
		public signal void new_window_activate();
		public signal void copy_activate();
		public signal void paste_activate();
		public signal void show_tabs_activate(bool show_tabs);
		public signal void preferences_activate();
		public signal bool next_tab_activate();
		public signal bool previous_tab_activate();
		public signal void move_to_location_activate(string location);

		private void on_show_tabs_activate(CheckMenuItem item)
		{
			show_tabs_activate(item.active);
		}

		private void on_next_previous_tab_activate(MenuItem item, bool next)
		{
			item.sensitive = next ? next_tab_activate() : previous_tab_activate();
		}

		private void on_move_to_location(MenuItem item)
		{
			if (item.get_data<bool>("location_is_msgwin"))
			{
				item.set_label("Move to message window");
				item.set_data<bool>("location_is_msgwin", false);
				move_to_location_activate("sidebar");
			}
			else
			{
				item.set_label("Move to sidebar");
				item.set_data<bool>("location_is_msgwin", true);
				move_to_location_activate("msgwin");
			}
		}

		private void add_separator()
		{
			SeparatorMenuItem item = new SeparatorMenuItem();
			this.append(item);
			item.show();
		}

		public ContextMenu(Config? cfg)
		{
			Menu menu;
			MenuItem item;
			ImageMenuItem image_item;
			CheckMenuItem check_item;

			menu = new Menu();
			menu.show();

			item = new MenuItem.with_label("Open Tab");
			item.set_submenu(menu);
			item.show();
			this.append(item);

			uint len = cfg.shell_configs.length();
			for (uint i = 0; i < len; i++)
			{
				ShellConfig sh = cfg.shell_configs.nth_data(i);
				item = new MenuItem.with_label(sh.name);
				item.activate.connect(() => new_shell_activate(sh));
				menu.append(item);
				item.show();
			}

			item = new MenuItem.with_label("Open Window");
			item.activate.connect(() => new_window_activate());
			this.append(item);
			item.show();

			add_separator();

			item = new MenuItem.with_label("Next tab");
			item.activate.connect(() => on_next_previous_tab_activate(item, true));
			//this.append(item);
			//item.show();

			item = new MenuItem.with_label("Previous tab");
			item.activate.connect(() => on_next_previous_tab_activate(item, false));
			//this.append(item);
			//item.show();

			//add_separator();

			image_item = new ImageMenuItem.from_stock(Gtk.Stock.COPY, null);
			image_item.activate.connect(() => copy_activate());
			//this.append(image_item);
			//image_item.show();

			image_item = new ImageMenuItem.from_stock(Gtk.Stock.PASTE, null);
			image_item.activate.connect(() => paste_activate());
			//this.append(image_item);
			//image_item.show();

			//add_separator();

			check_item = new CheckMenuItem.with_label("Show Tabs");
			check_item.active = cfg.show_tabs;
			check_item.activate.connect(() => on_show_tabs_activate(check_item));
			this.append(check_item);
			check_item.show();

			if (cfg.location == "msgwin")
			{
				item = new MenuItem.with_label("Move to sidebar");
				item.set_data<bool>("location_is_msgwin", true);
			}
			else
			{
				item = new MenuItem.with_label("Move to message window");
				item.set_data<bool>("location_is_msgwin", false);
			}
			item.activate.connect(() => on_move_to_location(item));
			this.append(item);
			item.show();

			//add_separator();

			image_item = new ImageMenuItem.from_stock(Gtk.Stock.PREFERENCES, null);
			image_item.activate.connect(() => preferences_activate());
			//this.append(image_item);
			//image_item.show();
		}
	}
}
