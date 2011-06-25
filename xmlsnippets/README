XML Snippets Plugin
===================

About
-----
This plugin extends XML/HTML tag autocompletion provided by Geany.
It automatically inserts a matching snippet after you type an opening tag.

Usage
-----
Enable the plugin in the Plugin Manager and add snippets you need to your
``snippets.conf``.  For example, to get an HTML definition list template
automatically inserted after you type "<dl>", add the following snippet to
the ``[HTML]`` section::

  dl=<dl>\n\t<dt>%cursor%</dt>\n\t<dd>%cursor%</dd>\n</dl>

The plugin will automatically catch up snippets supplied with Geany (for
example, "table").

Notes:

* The plugin will only use snippets which body starts with a tag, optionally
  preceded by whitespace.  This is to prevent erroneus insertion of snippets
  which you may have defined for, e.g., JavaScript.
* If you typed some attributes within a tag which is then automatically
  completed by the plugin, the attributes will be copied to the first tag
  of the snippet body.  If that tag already contains attributes,
  autocompletion will not proceed.

You can use this plugin together with tag autocompletion provided by Geany.
Tags, for which you defined snippets, will be completed by the plugin;
Geany will deal with other tags.

Requirements
------------
For compiling the plugin yourself, you will need the GTK (>= 2.8.0) libraries
and header files. You will also need its dependency libraries and header
files, such as Pango, Glib and ATK. All these files are available at
http://www.gtk.org.

And obviously, you will need have Geany installed.
If you have Geany installed from the sources, you should be ready to go.
If you used a prepared package e.g. from your distribution you probably need to
install an additional package, this might be called geany-dev or geany-devel.

Furthermore you need, of course, a C compiler and the Make tool.
The GNU versions of these tools are recommended.

Installation
------------
Compiling and installing the code is done by the following three commands::

  $ ./configure
  $ make
  $ make install

For more configuration details run::

  $ ./configure --help

If there are any errors during compilation, check your build environment
and try to find the error, otherwise contact one of the authors.

Running the built-in test suite
-------------------------------
This stage is completely optional and does not affect the work of the plugin.
It is useful for plugin developers so that they are sure the innards of the
plugin are working as expected.

To run the tests, you have to compile a test executable and run it using the
commands below.  These commands will only work if you are using a Unix
environment and GCC compiler.  Before running the commands, go to the source
code directory (``xmlsnippets/src``).
::

  $ gcc -Wall -DTEST -ggdb `pkg-config --cflags --libs glib-2.0` *.c -o xmlsnippets
  $ ./xmlsnippets

The executable will warn you about the tests which failed.  Ignore non-warning
informational messages like those starting with ``** Message:``.

License
-------
This plugin is distributed under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.  A copy of this license
can be found in the file COPYING included with the source code.

Ideas, questions, patches and bug reports
-----------------------------------------
Report them to the Geany mailing list (see
http://www.geany.org/Support/MailingList).
