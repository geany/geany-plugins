using Gtk;
using Pango;
using Vte;

namespace MultiTerm
{
	public class Notebook : Gtk.Notebook
	{
		private Button add_button;
		private Config cfg;

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
			add_terminal(null);
		}

		public void add_terminal(ShellConfig? cfg=null)
		{
			TabLabel label;
			Terminal term;

            if (cfg != null)
            {
                term = new Terminal(cfg);
                label = new TabLabel(cfg.name);
            }
            else
            {
                term = new Terminal();
                label = new TabLabel();
            }

			label.show_all();
			label.close_clicked.connect(on_tab_label_close_clicked);
			label.set_data<Terminal>("terminal", term);

			term.set_data<TabLabel>("label", label);
			term.show_all();

			this.append_page(term, label);

			this.set_tab_reorderable(term, true);

			/* TODO: check if this is depracated */
			this.set_tab_label_packing(term, true, true, PackType.END);
		}

		public void remove_terminal(int tab_num)
		{
			this.remove_page(tab_num);
		}

		public class Notebook(string config_filename)
		{
			Image img;
			RcStyle style;
			List<ShellConfig?> list;
			uint len;

			style = new RcStyle();
			style.xthickness = 0;
			style.ythickness = 0;

			add_button = new Button();
			add_button.modify_style(style);
			add_button.relief = ReliefStyle.NONE;
			add_button.focus_on_click = false;
			add_button.set_border_width(2);
			add_button.set_tooltip_text("New terminal");
			img = new Image.from_stock(Gtk.Stock.ADD, Gtk.IconSize.MENU);
			add_button.add(img);
			add_button.clicked.connect(on_add_button_clicked);
			add_button.show_all();
			add_button.style_set.connect(on_add_button_style_set);
			this.set_action_widget(add_button, PackType.END);

			cfg = new Config(config_filename);
			list = cfg.shell_configs;
			len = list.length();
			for (int i=0; i < len; i++)
			{
				ShellConfig sh = list.nth_data(i);
				add_terminal(sh);
			}
		}
	}
}
