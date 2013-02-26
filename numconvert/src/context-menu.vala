/*
 * context-menu.vala - This file is part of the Geany NumConvert plugin
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
    public class ContextMenu : Gtk.Menu
    {
        private Gtk.MenuItem _selection_item;
        private Gtk.MenuItem _all_item;
        private List<ConvertMenuItem> c;
        private List<ConvertMenuItem> vhdl;
        private List<ConvertMenuItem> all;

        public void on_acivate(Gtk.MenuItem item)
        {
            ((ContextMenuAll) _all_item.get_submenu()).on_acivate(item);
            if (Document.get_current() != null)
            {
                unowned FiletypeID ft = Document.get_current().file_type.id;
                unowned Sci sci = Document.get_current().editor.scintilla;
                string sel = sci.get_selection_contents();

                if (ft == FiletypeID.VHDL)
                {
                    foreach (ConvertMenuItem i in vhdl)
                        i.show();
                    foreach (ConvertMenuItem i in c)
                        i.hide();
                }
                else
                {
                    foreach (ConvertMenuItem i in vhdl)
                        i.hide();
                    foreach (ConvertMenuItem i in c)
                        i.show();
                }
                foreach (ConvertMenuItem i in all)
                    i.show();
                _all_item.show();
                
                if (sel != null && sel != "")
                {
                    NumberParser p = new NumberParser(sel);
                    if (p.success)
                    {
                        _selection_item.set_label("Convert " + p.result.to_string() + " to:");
                        _selection_item.set_sensitive(true);
                        this.foreach((w) =>
                        {
                            if (w is ConvertMenuItem)
                            {
                                ((ConvertMenuItem) w).generate_label(p.result);
                            }
                        });
                        return;
                    }
                }
                /* Fallback: hide everything */
                _selection_item.set_label("Select a number to convert...");
                _selection_item.set_sensitive(false);
                _all_item.hide();
                foreach (ConvertMenuItem i in vhdl)
                    i.hide();
                foreach (ConvertMenuItem i in c)
                    i.hide();
                foreach (ConvertMenuItem i in all)
                    i.hide();
            }
        }

        private void add_separator()
        {
            SeparatorMenuItem item = new SeparatorMenuItem();
            this.append(item);
            item.show();
        }

        private void add_convitem(IConverter conv, ref List<ConvertMenuItem> specific_list)
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
            this.append(item);
            specific_list.append(item);
        }

        public ContextMenu()
        {
            Gtk.MenuItem item;
            item = new Gtk.MenuItem();
            item.show();
            append(item);

            /* save to change label later on */
            _selection_item = item;
            
            c    = new List<ConvertMenuItem>();
            vhdl = new List<ConvertMenuItem>();
            all  = new List<ConvertMenuItem>();
            add_separator();
            add_convitem(new CHexConverter(), ref c);
            add_convitem(new COctConverter(), ref c);
            add_convitem(new CBinConverter(), ref c);
            add_convitem(new CBitFieldConverter(), ref c);
            add_convitem(new VhdlHexConverter(), ref vhdl);
            add_convitem(new VhdlOctConverter(), ref vhdl);
            add_convitem(new VhdlBinConverter(), ref vhdl);
            add_convitem(new VhdlBitFieldConverter(), ref vhdl);

            add_separator();
            /* add an "All ->" entry, save to shiw/hide later */
            item = new Gtk.MenuItem.with_label("All");
            item.set_submenu(new ContextMenuAll());
            item.show();
            append(item);
            _all_item = item;;
        }
    }
}
