/*
 * notebook.vala - This file is part of the Geany MultiTerm plugin
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
using Gdk;
using Pango;
using Vte;

namespace MultiTerm
{
	public class Notebook : Gtk.Notebook
	{
		private Button add_button;
		public Config cfg;
		private ContextMenu? context_menu;

		private void on_tab_label_close_clicked(int tab_num)
		{
			if (this.get_n_pages() > 1)
				this.remove_terminal(tab_num);
		}

		private void on_show_tabs_activate(bool show_tabs)
		{
			this.show_tabs = show_tabs;
			cfg.show_tabs = show_tabs;
		}

		private bool on_next_tab_activate()
		{
			int n_tabs = this.get_n_pages();
			int current = this.get_current_page();

			if (current < (n_tabs - 1))
			{
				current++;
				this.set_current_page(current);
			}

			return (current < (n_tabs - 1)) ? true : false;
		}

		private bool on_previous_tab_activate()
		{
			int current = this.get_current_page();

			if (current > 0)
			{
				current--;
				this.set_current_page(current);
			}

			return (current > 0) ? true : false;
		}

		private void on_new_shell_activate(ShellConfig cfg)
		{
			add_terminal(cfg);
		}

		private void on_new_window_activate()
		{
			Pid pid;
			string[] args = { cfg.external_terminal, null };

			try
			{
				if (Process.spawn_async(null, args, null, SpawnFlags.SEARCH_PATH, null, out pid))
					debug("Started external terminal '%s' with pid of '%d'", args[0], pid);
			}
			catch (SpawnError err)
			{
				warning(_("Unable to launch external terminal: %s").printf(err.message));
			}
		}

		private void on_move_to_location(string location)
		{
			Container frame = this.get_parent() as Container;
			Container parent = frame.get_parent() as Container;
			Gtk.Notebook new_nb;

			parent.remove(frame);

			if (location == "msgwin")
			{
				new_nb = this.get_data<Notebook>("msgwin_notebook");
				new_nb.append_page(frame, this.get_data<Label>("label"));
			}
			else
			{
				new_nb = this.get_data<Notebook>("sidebar_notebook");
				new_nb.append_page(frame, this.get_data<Label>("label"));
			}

			new_nb.set_current_page(new_nb.page_num(frame));
			cfg.location = location;
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
			warning(_("Unable to locate default shell in configuration file"));
		}

		private bool on_terminal_right_click_event(EventButton event)
		{
			if (context_menu == null)
			{
				context_menu = new ContextMenu(cfg);
				context_menu.show_tabs_activate.connect(on_show_tabs_activate);
				context_menu.next_tab_activate.connect(on_next_tab_activate);
				context_menu.previous_tab_activate.connect(on_previous_tab_activate);
				context_menu.new_shell_activate.connect(on_new_shell_activate);
				context_menu.new_window_activate.connect(on_new_window_activate);
				context_menu.move_to_location_activate.connect(on_move_to_location);
			}
			context_menu.popup(null, null, null, event.button, event.time);
			return true;
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
			term.right_click_event.connect(on_terminal_right_click_event);

			this.append_page(term, label);
			this.set_tab_reorderable(term, true);
			/* TODO: this is deprecated, try and figure out alternative
			 * from GtkNotebook docs. */
			this.set_tab_label_packing(term, true, true, PackType.END);
			this.scrollable = true;

			return term;
		}

		public void remove_terminal(int tab_num)
		{
			this.remove_page(tab_num);
		}

		public class Notebook(string config_filename)
		{
			Gtk.Image img;
			RcStyle style;

			cfg = new Config(config_filename);

			style = new RcStyle();
			style.xthickness = 0;
			style.ythickness = 0;

			img = new Gtk.Image.from_stock(Gtk.Stock.ADD, Gtk.IconSize.MENU);

			add_button = new Button();
			add_button.modify_style(style);
			add_button.relief = ReliefStyle.NONE;
			add_button.focus_on_click = false;
			add_button.set_border_width(2);
			add_button.set_tooltip_text(_("New terminal"));
			add_button.add(img);
			add_button.clicked.connect(on_add_button_clicked);
			add_button.show_all();
			add_button.style_set.connect(on_add_button_style_set);

			/* TODO: make this button show a list to select which shell
			 * to open */
			//this.set_action_widget(add_button, PackType.END);

			this.show_tabs = cfg.show_tabs;

			foreach (ShellConfig sh in cfg.shell_configs)
			{
				Terminal term = add_terminal(sh);
				term.right_click_event.connect(on_terminal_right_click_event);
			}
		}
	}
}
