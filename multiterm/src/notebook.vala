using Gtk;
using Gdk;
using Pango;
using Vte;

namespace MultiTerm
{
	public class Notebook : Gtk.Notebook
	{
		private Button add_button;
		private Config cfg;
		private ContextMenu? context_menu;

		private void on_tab_label_close_clicked(int tab_num)
		{
			if (this.get_n_pages() > 1)
				this.remove_terminal(tab_num);
		}

		private void on_add_button_style_set()
		{
			int w, h;
			Gtk.icon_size_lookup_for_settings(add_button.get_settings(),
											  IconSize.MENU,
											  out w, out h);
			add_button.set_size_request(w+2, h+2);
		}

		private void on_add_button_clicked()
		{
			foreach (ShellConfig sh in cfg.shell_configs)
			{
				if (sh.section.strip() == "shell=default")
				{
					add_terminal(sh);
					return;
				}
			}
			warning("Unable to locate default shell in configuration file");
		}

		private bool on_terminal_right_click_event(EventButton event)
		{
			if (context_menu == null)
				context_menu = new ContextMenu(cfg);
			context_menu.popup(null, null, null, event.button, event.time);
			return true;
		}

		private void show_hide_notebook_tabs()
		{
			/* TODO: once the context menu is added, make this
			 * optional in the config file. */
			/*
			if (this.get_n_pages() <= 1)
				this.show_tabs = false;
			else
				this.show_tabs = true;
			*/
		}

		public Terminal add_terminal(ShellConfig cfg)
		{
			TabLabel label = new TabLabel(cfg.name);
			Terminal term = new Terminal(cfg);

			label.show_all();
			label.close_clicked.connect(on_tab_label_close_clicked);
			label.set_data<Terminal>("terminal", term);

			term.set_data<TabLabel>("label", label);
			term.show_all();

			this.append_page(term, label);

			this.set_tab_reorderable(term, true);

			/* TODO: this is deprecated, try and figure out alternative
			 * from GtkNotebook docs. */
			this.set_tab_label_packing(term, true, true, PackType.END);
			this.scrollable = true;

			show_hide_notebook_tabs();

			return term;
		}

		public void remove_terminal(int tab_num)
		{
			this.remove_page(tab_num);
			show_hide_notebook_tabs();
		}

		public class Notebook(string config_filename)
		{
			Gtk.Image img;
			RcStyle style;

			style = new RcStyle();
			style.xthickness = 0;
			style.ythickness = 0;

			img = new Gtk.Image.from_stock(Gtk.Stock.ADD, Gtk.IconSize.MENU);

			add_button = new Button();
			add_button.modify_style(style);
			add_button.relief = ReliefStyle.NONE;
			add_button.focus_on_click = false;
			add_button.set_border_width(2);
			add_button.set_tooltip_text("New terminal");
			add_button.add(img);
			add_button.clicked.connect(on_add_button_clicked);
			add_button.show_all();
			add_button.style_set.connect(on_add_button_style_set);

			this.set_action_widget(add_button, PackType.END);

			cfg = new Config(config_filename);
			//context_menu = new ContextMenu(cfg);

			foreach (ShellConfig sh in cfg.shell_configs)
			{
				Terminal term = add_terminal(sh);
				term.right_click_event.connect(on_terminal_right_click_event);
			}
		}
	}
}
