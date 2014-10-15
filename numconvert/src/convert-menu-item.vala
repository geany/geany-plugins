/*
 * convert-menu-item.vala - This file is part of the Geany NumConvert plugin
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

namespace NumConvert
{
    class ConvertMenuItem : Gtk.MenuItem
    {
        private IConverter _conv;
        private string _conv_result;

        public ConvertMenuItem(IConverter conv)
        {
            _conv = conv;
            set_label("...");
        }

        public void generate_label(int64 value)
        {
            _conv_result = _conv.convert(value);
            set_label("%s (%s)".printf(_conv_result, _conv.describe()));
        }

        public string get_converted()
        {
            return _conv_result;
        }
    }
}
