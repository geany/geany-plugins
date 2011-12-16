using Gtk;
using Vte;

namespace MultiTerm
{
	public class Terminal : Frame
	{
		public Vte.Terminal terminal;
		private ShellConfig sh;

		private void on_window_title_changed()
		{
			tab_label_text = terminal.window_title;
		}

		public string tab_label_text
		{
			get
			{
				TabLabel label = this.get_data<TabLabel>("label");
				return label.text;
			}
			set
			{
				TabLabel label = this.get_data<TabLabel>("label");
				label.text = value;
			}
		}

		public string background_color
		{
			set
			{
				Gdk.Color color = Gdk.Color();
				Gdk.Colormap.get_system().alloc_color(color, true, true);
				Gdk.Color.parse(value, out color);
				terminal.set_color_background(color);
			}
		}

		public string foreground_color
		{
			set
			{
				Gdk.Color color = Gdk.Color();
				Gdk.Colormap.get_system().alloc_color(color, true, true);
				Gdk.Color.parse(value, out color);
				terminal.set_color_foreground(color);
			}
		}

		public void run_command(string command)
		{
			/* TODO: add wrapper for fork_command_full() since this
			 * function is deprecated */
			terminal.fork_command(command, null, null, null, true, true, true);
		}

		private void on_vte_realize()
		{
			if (sh.cfg != null)
			{
				background_color = sh.cfg.background_color;
				foreground_color = sh.cfg.foreground_color;
			}
			else
			{
				background_color = "#ffffff";
				foreground_color = "#000000";
			}
		}

		private void on_child_exited()
		{
			terminal.fork_command(this.sh.command, null, null, null, true, true, true);
		}

		public void send_command(string command)
		{
			terminal.feed_child("%s\n".printf(command), -1);
		}

		public Terminal(ShellConfig? sh=null)
		{
			VScrollbar vsb;
			HBox hbox;

			if (sh == null)
			{
				this.sh = ShellConfig();
				this.sh.section = "default";
				this.sh.name = "Default Terminal";
				this.sh.track_title = true;
				this.sh.command = null;
				this.sh.cfg = null;
			}
			else
            {
				this.sh = sh;
                if (this.sh.command == "")
                    this.sh.command = null;
            }

			terminal = new Vte.Terminal();
			terminal.set_size_request(100, 100); // stupid
			terminal.show_all();

			vsb = new VScrollbar(terminal.get_adjustment());
			hbox = new HBox(false, 0);

			hbox.pack_start(terminal, true, true, 0);
			hbox.pack_start(vsb, false, false, 0);

			this.add(hbox);

			if (this.sh.track_title)
				terminal.window_title_changed.connect(on_window_title_changed);

			terminal.child_exited.connect(on_child_exited);

			if (this.sh.cfg != null)
				terminal.set_font_from_string_full(this.sh.cfg.font, TerminalAntiAlias.FORCE_ENABLE);
			else
				terminal.set_font_from_string_full("Monospace 9", TerminalAntiAlias.FORCE_ENABLE);

			terminal.realize.connect(on_vte_realize); /* colors can only be set on realize (lame) */

			/* TODO: add wrapper for fork_command_full() since this
			 * function is deprecated */
			terminal.fork_command(this.sh.command, null, null, null, true, true, true);
		}

	}
}
