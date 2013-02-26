/*
 * converters.vala - This file is part of the Geany NumConvert plugin
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

using GLib;

namespace NumConvert
{
    private string myiota(int max_len, int64 from, int _base)
    {
        int i = 0;
        StringBuilder sb = new StringBuilder.sized(int.max(max_len, 64));

        while(from > 0)
        {
            int mod = (int)(from % _base);
            if (mod > 9)
                sb.prepend_c('a' + (char)(mod-10));
            else
                sb.prepend_c('0' + (char)mod);
            if (max_len > 0 && ++i > max_len)
                break;

            from /= _base;
        }

        return sb.str;
    }

    private string bithelper(int64 from, int num_bits)
    {
        var strb = new StringBuilder();
        for(int i = num_bits-1; i >= 0; i--)
            strb.append((from & (1<<i)) != 0 ? "1":"0");

        return strb.str;
    }
    interface IConverter : Object {
        public abstract string describe();
        public abstract string convert(int64 from);
    }

    /* template 
    class ClassName : Object, IConverter
    {
        public string describe()
        {
        }

        public string convert(int64 from)
        {
        }
    }
    */

    class CHexConverter : Object, IConverter
    {
        public string describe()
        {
            return "Hex";
        }

        public string convert(int64 from)
        {
            return from.to_string("0x%08x");
        }
    }

    class VhdlHexConverter : Object, IConverter
    {
        public string describe()
        {
            return "Hex (VHDL)";
        }

        public string convert(int64 from)
        {
            return from.to_string("x\"%08x\"");
        }
    }

    class COctConverter : Object, IConverter
    {
        public string describe()
        {
            return "Octal";
        }

        public string convert(int64 from)
        {
            return from.to_string("0%o");
        }
    }
     
    class VhdlOctConverter : Object, IConverter
    {
        public string describe()
        {
            return "Octal (VHDL)";
        }

        public string convert(int64 from)
        {
            return from.to_string("o\"%o\"");
        }
    }
 
    class CBinConverter : Object, IConverter
    {
        public string describe()
        {
            return "Binary";
        }

        public string convert(int64 from)
        {
            return "0b" + bithelper(from, 16);
        }
    }
 
    class VhdlBinConverter : Object, IConverter
    {
        public string describe()
        {
            return "Binary (VHDL)";
        }

        public string convert(int64 from)
        {
            return "\"" + bithelper(from, 16) + "\"";
        }
    }

    class CBitFieldConverter : Object, IConverter
    {
        public string describe()
        {
            return "Bit Field";
        }

        public string convert(int64 from)
        {
            int i;
            string[] ret = {};
            for(i = 15; i != 0; i--)
            {
                if ((from & (1<<i)) != 0)
                    ret += @"(1<<$i)";
            }

            if (ret.length == 0)
                return "0";
            else
                return string.joinv(" | ", ret);
        }
    }

    class VhdlBitFieldConverter : Object, IConverter
    {
        public string describe()
        {
            return "Bit Field (VHDL)";
        }

        public string convert(int64 from)
        {
            int i;
            string[] ret = {};
            for(i = 15; i != 0; i--)
                ret += (from & (1<<i)) != 0 ? "\"1\"" : "\"0\"";

            return string.joinv(" & ", ret);
        }
    }

    class VhdlGenericConverter : Object, IConverter
    {
        private int _base;
        public VhdlGenericConverter(int bas)
        {
            _base = bas;
        }

        public string describe()
        {
            return "Generic literal (VHDL)";
        }

        public string convert(int64 from)
        {
            return "%d#%s#".printf(_base, myiota(16, from, _base));
        }
    }

#if TEST
    int main()
    {
        for(int i = 2; i <= 16; i++)
        {
            VhdlGenericConverter c = new VhdlGenericConverter(i);
            stdout.printf("%s: %s\n", c.describe(), c.convert(212));
        }
        return 0;
    }
#endif
}
