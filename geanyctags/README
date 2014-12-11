==========
GeanyCtags
==========

.. contents::

About
=====

GeanyCtags adds a simple support for generating and querying ctags files for a Geany
project. It requires that the ctags command is installed in a system path. On
unix systems, distributions usually provide the ctags package; on Windows, the
ctags binary can be found in the zip Windows distribution from the ctags home
page (http://ctags.sourceforge.net).

Even though Geany supports symbol definition searching by itself within the open files
(and with a plugin support within the whole project), tag regeneration can become
too slow for really big projects. This is why this plugin was created. It makes
it possible to generate the tag file only once and just query it when searching
for a particular symbol definition/declaration. This approach is fine for big projects
where most of the codebase remains unchanged and the tag positions remain more
or less static.

Usage
=====

GeanyCtags only works for Geany projects so in order to use it, a Geany project
has to be open. The "File patterns" entry under Project->Properties determines 
for which files the tag list will be generated. All files within the project base
path satisfying the file patterns will be included for the tag generation.

Tag Generation
--------------

To generate the list of tags, make sure that

* a project is open, and
* "File patterns" are specified correctly under Project->Properties

Then, select Project->Generate tags. After this, a file whose name corresponds to
the project name with the suffix ".tags" is created in the same directory as the
project file.

Tag Querying
------------

There are two ways to find a symbol's definition/declaration:

* using the context menu by placing a cursor at the searched symbol, right-clicking
  and selecting one of the two top items "Find Tag Definition" or "Find Tag Declaration".
* using Project->Find tag

If the symbol is found, the editor is opened at the tag line. In addition, all the
found tags are written into the Messages window. This is useful when there are more
tags of the same name - by clicking a line in the Messages window it is possible
to jump to any of the found tags.

When using the Project->Find tag search, it is possible to select from several
search types:

* prefix (default) - finds all tags with the specified prefix
* exact - finds all tags matching the name exactly
* pattern - finds all tags matching the provided glob pattern

Note that the pattern option is the slowest of the three possibilities because it
has to go through the tag list sequentially instead of using binary search used
by the other two methods. By default, tag definitions are searched; to search tag
declarations, select the Declaration option.

Known issues
============

* Right now it's not possible to just right-click the searched symbol in the editor
  and search for definition/declaration - first, a cursor has to be placed at
  the symbol. This is because at the moment the necessary functionality  to do this 
  is private in Geany and not exported to plugins so it cannot be implemented.
  I will try to get the necessary functions public in Geany to make it possible
  in the future.
* Under Windows, GeanyCtags generates tags for all the files within the project base
  path instead of just the files defined by the patterns. This happens because
  of the missing (or at least not normally installed) find command used under
  unix systems to restrict the file list to those matching the patterns. It
  would be possible to perform the filtering manually but frankly I'm rather lazy
  to do so (and don't have Windows installed to test). Patches are welcome.

License
=======

GeanyCtags is distributed under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.  A copy of this license
can be found in the file COPYING included with the source code of this
program.

Downloads
=========

GeanyCtags is part of the combined Geany Plugins release.
For more information and downloads, please visit
http://plugins.geany.org/geany-plugins/

Development Code
================

Get the code from::

    git clone https://github.com/geany/geany-plugins.git

Ideas, questions, patches and bug reports
=========================================

Please direct all questions, bug reports and patches to the plugin author using the
email address listed below or to the Geany mailing list to get some help from other
Geany users.

2010-2014 by Jiří Techet
techet(at)gmail(dot)com
