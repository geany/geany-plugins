/*
 * tab-label.vala - This file is part of the Geany MultiTerm plugin
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

namespace MultiTerm
{
	public class TabLabel : HBox
	{
		public signal void close_clicked(int tab_num);
		
		private Button btn;
		public Label label;

		public string text
		{
			get { return label.get_label(); }
			set { label.set_label(value); }
		}

		public Button button
		{
			get { return btn; }
		}
		
		private void on_button_clicked() 
		{ 
			Terminal term = this.get_data<Terminal>("terminal");
			Notebook nb = (Notebook)term.get_parent();
			int page_num = nb.page_num(term);
			
			close_clicked(page_num);
		}
		
		private void on_button_style_set(Gtk.Style? previous_style)
		{
			int w, h;
			Gtk.icon_size_lookup_for_settings(btn.get_settings(), 
											  IconSize.MENU, 
											  out w, out h);							
			btn.set_size_request(w+2, h+2);
		}
		
		public TabLabel(string text=_("Terminal"))
		{
			Image img;
			
			GLib.Object(homogeneous: false, spacing: 2);
			this.set_border_width(0);
			
			label = new Label(text);
			label.set_alignment(0.0f, 0.5f);
			label.ellipsize = Pango.EllipsizeMode.END;
			
			this.pack_start(label, true, true, 0);
			
			img = new Image.from_stock(Gtk.Stock.CLOSE, IconSize.MENU);
			
			RcStyle style = new RcStyle();
			style.xthickness = 0;
			style.ythickness = 0;
			
			btn = new Button();
			btn.modify_style(style);
			btn.add(img);
			btn.set_tooltip_text(_("Close terminal"));
			btn.clicked.connect(on_button_clicked);
			btn.relief = ReliefStyle.NONE;
			btn.focus_on_click = false;
			btn.style_set.connect(on_button_style_set);
			
			this.pack_start(btn, false, false, 0);
		}
	}
}
