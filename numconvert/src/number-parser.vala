/*
 * number-parser.vala - This file is part of the Geany NumConvert plugin
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
    class NumberParser : Object
    {       
        public string source { get; private set; }
        public int64 result { get; private set; }
        public bool success { get; private set; }

        [CCode (cname = "g_ascii_strtoll")]
        extern static int64 ascii_strtoll (string nptr, out char* endptr, uint _base);

        public NumberParser(string from)
        {
            source = from;
            int64 ret;

            result = -1;
            success = false;

			try
			{
                /* x"ff", o"77", b"22" */
				Regex vhdlLit1 = new Regex("([BbOoXx]*)\"([_0-9A-Fa-f]+)\"");
                /* 2#0110#. 5#01234#, ... */
				Regex vhdlLit2 = new Regex("([0-9])#([_0-9A-Za-z]+)#");
				MatchInfo match;
				
				if (vhdlLit1.match(from, 0, out match))
				{
                    string str;
                    char *end;
                    int bas = 10;
                    switch (match.fetch(1).down())
                    {
                        case "" : bas =  2; break;
                        case "b": bas =  2; break;
                        case "o": bas =  8; break;
                        case "x": bas = 16; break;
                    }
                    str = match.fetch(2).replace("_", "");
                    /* Check taken from glib vapi file (int64.try_parse) */
					result = ascii_strtoll(str, out end, bas);
                    success = (end == (char*) str + str.length);
				}
                else if (vhdlLit2.match(from, 0, out match))
				{
                    string str;
                    char *end;
                    int bas = int.parse(match.fetch(1));
                    /* VHDL doesn't specify larger bases than 16 */
                    if (bas <= 16)
                    {
                        str = match.fetch(2).replace("_", "");
                        /* Check taken from glib vapi file (int64.try_parse) */
                        result = ascii_strtoll(str, out end, bas);
                        success = (end == (char*) str + str.length);
                    }
				}
				else
				{
					success = int64.try_parse(from, out ret);
					if (success)
						result = ret;
				}
			}
			catch (GLib.RegexError e)
			{
				println("Error " + e.message);
			}
        }
    }

    void println(string str)
    {
		stdout.printf("%s\n", str);
	}

#if TEST
    struct TestTouple {
        string str;
        int    result;
    }

	static int main()
	{
        
		TestTouple[] tests = {
			TestTouple() { str = """0x123""",       result = 0x123 },
			TestTouple() { str = """X"123"""",      result = 0x123 },
			TestTouple() { str = """X"12_3"""",     result = 0x123 },
			TestTouple() { str = """X"Af_12"""",    result = 0xaf12 },    
			TestTouple() { str = """B"01_10"""",    result = 0x6 },
			TestTouple() { str = """O"0377"""",     result = 0377 },
			TestTouple() { str = """B"0377"""",     result = 0 },
            TestTouple() { str = """8#3_77#""",     result = 0377 },
            TestTouple() { str = """2#1001_0001#""",result = 145 },
            TestTouple() { str = """16#FFCC#""",    result = 0xffcc },
            TestTouple() { str = """12#FFCC#""",    result = 0 }    
		};

		for(int i = 0; i < tests.length; i++)
		{
			NumberParser n = new NumberParser(tests[i].str);
            if (n.success && (n.result != tests[i].result))
            {
                stderr.printf("Test failed at %d\n", i);
                return -1;
            }
            if (!n.success && tests[i].result != 0)
            {
                stderr.printf("Test failed at %d\n", i);
                return -1;
            }
		}
		return 0;
	}
#endif    
}
