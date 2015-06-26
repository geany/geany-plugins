Tableconvert
============

.. contents::


About
-----

Tableconvert is a plugin which helps on converting a tabulator
separated selection into a table.


Installation
------------

This version of the plugin is installed with the combined
geany-plugins release. Please check README of this package.


Usage
-----

After the plugins has been installed successfully, load the plugin via
Geany's plugin manager. This will add a new keybinding into Geany's
list of keybindings inside the preferences dialog. You might like to
set up a keybinding for the function. Once this is done, the plugin is
ready to use. Alternativly it's also possible to use the plugin via the
menu entry at the Tools menu.

Inside the Tools menu there is also another submenu avaialble wich is
allowing you to convert to a table without changing Geany's file type
settings for the current document.

When using the plugin, just mark the area you like to transform and
push your keybinding. All line endings will be taken as end of a row
and therefor transformed to fitting row endings. The plugin is taking
care of you line ending stil.

Currently the plugin is supporting
* HTML
* LaTeX
* SQL

Rectangle selections are currently not supported and might lead to
some weird output you didn't expect -- just don't try it if you have
heart issues or working on an important document :)

HTML
^^^^

When transforming HTML, line endings will be replace with <tr> and
</tr>. Occurrences of tabulator will be interpreted as column
separators and replaced by <td> and </td>.

The first line will be treaded as header and surrounded by
<thead></thead> tags. The rest of the table will be included into
<tbody></tbody>.


LaTeX
^^^^^

In case of working with LaTeX line endings will be replaced with \\
and tabulators with &.


However, after you transformed your selection to a table you might
want to update the number and orientation of columns at begin of
tabular-environment.

SQL
^^^

At SQL line endings will be replaced with with closing braces )
followed by a komma if is not the last line.

Tabulators will be repace with ', ', so in the end, a tabulator
seperated liste as e.g.

foo	baa
baa foo

will be transformed to

(
	('foo', 'baa'),
	('baa', 'foo')
)


Development
-----------

You can checkout the current source code from the git repository at
github.com. Get the code by:

git clone https://github.com/geany/geany-plugins.git

If you want to create a patch, please respect the license of
Tableconvert as well as intellectual property of third. Patches that
should be included to the default distribution must be licensed under
the same conditions as Tableconvert by the copyright owner (GPL2+).


Known issues
------------

The plugin currently is not supporting rectangle selections very
well. Also it is not able to support <thead> and <tbody> at the
moment. However, at least the second point is planned for some later
release.

For more recent information reported issues will be tracked at
https://github.com/geany/geany-plugins/issues


License
-------

Tableconvert and all its parts is distributed under the terms of the
GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any
later version. A copy of this license can be found in the file COPYING
included with the source code of this program. If not, you will be
able to get a copy by contacting the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.


Bugs, questions, bugs, homepage
-------------------------------

If you found any bugs or want to provide a patch, please contact Frank
Lanitz (frank(at)geany(dot)org). Please also do so, if you got any
questions.
