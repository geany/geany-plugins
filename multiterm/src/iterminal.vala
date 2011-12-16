using Gtk;

namespace MultiTerm
{
	public interface ITerminal : Frame
	{
		public abstract string tab_label_text { get; set; }
		public abstract string background_color { set; }
		public abstract string foreground_color { set; }
		public abstract void run_command(string command);
	}
}