/*
 * context-menu-all.vala - This file is part of the Geany NumConvert plugin
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

namespace NumConvert
{
    public class ContextMenuAll : Gtk.Menu
    {
        public void on_acivate(Gtk.MenuItem item)
        {
            if (Document.get_current() != null)
            {
                unowned Sci sci = Document.get_current().editor.scintilla;
                string sel = sci.get_selection_contents();
                
                if (sel != null && sel != "")
                {
                    NumberParser p = new NumberParser(sel);
                    this.foreach((w) =>
                    {
                        if (w is ConvertMenuItem)
                        {
                            ((ConvertMenuItem) w).generate_label(p.result);
                        }
                    });
                }
            }
        }

        private void add_separator()
        {
            SeparatorMenuItem item = new SeparatorMenuItem();
            append(item);
            item.show();
        }

        private void add_convitem(IConverter conv)
        {
            ConvertMenuItem item = new ConvertMenuItem(conv);
            item.activate.connect(() =>
            {
                unowned Sci sci = Document.get_current().editor.scintilla;
                if (sci.has_selection())
                {
                    sci.replace_selection(item.get_converted());
                }
            });
            append(item);
            item.show();
        }

        public ContextMenuAll()
        {
            add_convitem(new CHexConverter());
            add_convitem(new COctConverter());
            add_convitem(new CBinConverter());
            add_convitem(new CBitFieldConverter());
            add_separator();
            add_convitem(new VhdlHexConverter());
            add_convitem(new VhdlOctConverter());
            add_convitem(new VhdlBinConverter());
            add_convitem(new VhdlBitFieldConverter());
            for(int i = 2; i <= 16; i++)
                add_convitem(new VhdlGenericConverter(i));
        }
    }
}
