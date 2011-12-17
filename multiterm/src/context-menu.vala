using Gtk;

namespace MultiTerm
{
	public class ContextMenu : Menu
	{
		public signal void new_shell_activate(ShellConfig sh);
		public signal void new_window_activate();
		public signal void copy_activate();
		public signal void paste_activate();
		public signal void show_tabs_activate();
		public signal void preferences_activate();

		public void add_separator()
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

			foreach (ShellConfig sh in cfg.shell_configs)
			{
				item = new MenuItem.with_label(sh.name);
				menu.append(item);
				item.show();
			}

			item = new MenuItem.with_label("Open Window");
			item.activate.connect(() => new_window_activate());
			this.append(item);
			item.show();

			add_separator();

			image_item = new ImageMenuItem.from_stock(Gtk.Stock.COPY, null);
			image_item.activate.connect(() => copy_activate());
			this.append(image_item);
			image_item.show();

			image_item = new ImageMenuItem.from_stock(Gtk.Stock.PASTE, null);
			image_item.activate.connect(() => paste_activate());
			this.append(image_item);
			image_item.show();

			add_separator();

			check_item = new CheckMenuItem.with_label("Show Tabs");
			check_item.activate.connect(() => show_tabs_activate());
			this.append(check_item);
			check_item.show();

			add_separator();

			image_item = new ImageMenuItem.from_stock(Gtk.Stock.PREFERENCES, null);
			image_item.activate.connect(() => preferences_activate());
			this.append(image_item);
			image_item.show();
		}
	}
}
