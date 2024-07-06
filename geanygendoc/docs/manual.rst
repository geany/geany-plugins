=======================
GeanyGenDoc User Manual
=======================
-------------------------------------------------
A handy hand guide for the lazy documenter in you
-------------------------------------------------


Introduction
============

First of all, welcome to this manual. Then, what is GeanyGenDoc? It is a
plug-in for Geany as you might have noticed; but what is it meant to do?
Basically, it generates and inserts text chunks, particularly from document's
symbols. Its goal is to ease writing documentation for the good.

If you are impatient, you will probably want to discover the `user interface in
Geany`_ first; but if you have the time to discover the tool, take a look at the
`design`_ and learn how GeanyGenDoc works and how you can make the most of it.


.. contents::


User interface in Geany
=======================

Menus
-----

GeanyGenDoc adds an item named `Insert Documentation Comment` in the editor's
pop-up under the `Insert Comments` sub-menu; and a menu named
`Documentation Generator` into the `Tools` menu.

Editor's pop-up menu
~~~~~~~~~~~~~~~~~~~~

The item `Editor's pop-up → Insert Comments → Insert Documentation Comment`
generates documentation for the current symbol. It has a keyboard shortcut
that can be configured through Geany's keybinding configuration system, under
`GeanyGenDoc → Insert Documentation Comment`.

Tools menu
~~~~~~~~~~

The `Documentation Generator` menu under `Tools` contains the following items:

`Document Current Symbol`
  This generates documentation for the current symbol. It is equivalent to the
  item `Insert Documentation Comment` that can be found in the editor's pop-up
  menu.

`Document All Symbols`
  This generates documentation for all symbols in the document. This is
  equivalent to manually requesting documentation generation for each symbol in
  the document.

`Reload Configuration Files`
  This force reloading of all the `file type`_ configuration files. It is
  useful when a file type configuration file was modified, in order to the new
  configuration to be used without reloading the plugin.

`Edit Current Language Configuration`
  This opens the configuration file that applies to the current document for
  editing. The opened configuration file has write permissions: if it was a
  system configuration file it is copied under your personal `configuration
  directory`_ transparently.

`Open Manual`
  Opens this manual in a browser.


Preferences dialog
------------------

The preferences dialog, that can either be opened through `Edit →
Plugin Preferences` or with the `Preferences` button in the plugin manager,
allows to modify the following preferences:

`General`
  `Save file before generating documentation`
    Choose whether the current document should be saved to disc before
    generating the documentation. This is a technical detail, but it is
    currently needed to have an up-to-date tag list. If you disable this option
    and ask for documentation generation on a modified document, the behavior
    may be surprising since the comment will be generated for the last saved
    state of the document and not the current one.

  `Indent inserted documentation`
    Chooses whether the inserted documentation should be indented to fit the
    indentation at the insertion position.

`Documentation type`
  This list allows you to choose the documentation type to use with each file
  type. The special language `All` on top of the list is used to choose the
  default documentation type, used for all languages that haven't one set.

`Global environment`
  Global environment overrides and additions. This is an environment that will
  be merged with the `file type`_-specific ones, possibly overriding some parts.
  It can be used to define some values for all the file types, such as whether
  to write the common `Since` tag, define the `Doxygen`_ prefix an so on.
  Its most practical use case is not to need to change a file type's environment
  to change the value of one of its elements.


Design
======

GeanyGenDoc has an extensible design based on three points: file type,
documentation type and rules.

`File type`_
  The file type determines which configuration applies to which document. For
  example, the "c" file type corresponds to C source, and so on.

`Documentation type`_
  A documentation type is an arbitrary name for a set of rules. The goal of
  documentation types is to allow different set of rules to be defined for each
  file type.
  One might want to have separate rules to generate for example `Doxygen`_
  and `Gtk-Doc`_ documentation from C sources, and should then create two
  documentation types in the C `file type configuration file`_, such as
  "doxygen" and "gtkdoc".

`Rule`_
  A rule is a group of settings controlling how a documentation comment is
  generated. For example, it can define a template, describe how to handle
  particular imbrications and so on.


Syntax
======

Key-Value pairs
---------------

The syntax used by the configuration files is an extended key-value tree 
definition based on common concepts (trees, string literals, semicolon-ended 
values, etc.).

The key-value syntax is as follows::

  key = value

where value is either a semicolon-ended single value::

  value;

or a brace-surrounded list of key-value pairs that use the same syntax again::

  {
    key1 = value1
    key2 = value2
  }

Here a little example of the *syntax* (not any actual configuration example)::

  key1 = value1;
  key2 = {
    sub-key1 = sub-value1;
    sub-key2 = {
      sub-sub-key1 = sub-sub-value1;
    }
  }


Naming
------

Key-value pairs are often referred as *group* when they are meant to have
multiple values and as *setting* when they have a single value.


Comments
--------

Is considered as comment (and therefore ignored) everything between a number
sign (``#``) and the following end of line, unless the ``#`` occurs as part of
another syntactic element (such as a string literal).

A short example::

  # This is a comment
  key = value; # This is also a comment
  string = "A string. # This isn't a comment but a string";


Value types
-----------

string
  A string literal. String literals are surrounded by either single (``'``) or
  double (``"``) quotes.
  
  Some special characters can be inserted in a string with an escape sequence:

  ``\t``
    A tabulation.
  ``\n``
    A new line.
  ``\r``
    A carriage return.
  ``\\``
    A backslash.
  ``\'``
    A single quote (escaping only needed in single-quotes surrounded strings).
  ``\"``
    A double quote (escaping only needed in double-quotes surrounded strings).
  
  Note that backslashes are used as the escaping character, which means that it
  must be escaped to be treated as a simple backslash character.
  
  A simple example::
  
    "This is a string with \"special\" characters.\nThis is another line!"

boolean
  A boolean. It can take one of the two symbolic values ``True`` and ``False``.

enumeration
  An enumeration. It consists of a named constant, generally in capital letters.
  The possible values depend on the setting using this type.

flags
  A logical OR of named constants. This is similar to enumerations but can
  combine different values.
  
  The syntax is common for such types and uses the pipe (``|``) as
  combination character. Considering the ``A``, ``B`` and ``C`` constants, a
  valid value could be ``A | C``, which represents both ``A`` and ``C`` but
  not ``B``.

list
  A list of values (often referred as array).


File types
==========

The file type determines which configuration applies to which document.
*File type identifiers* are the lowercased name of Geany's file type, for
example "c" or "python".

Configuration for a particular file type goes in a file named
``file-type-identifier.conf`` in the ``filetypes`` sub-directory of a
`configuration directory`_.

A file type configuration can contain two type of things: file-type-wide
settings and any number of `documentation types`_.


The ``settings`` group
----------------------

This group contains the file-type-wide settings.

``match_function_arguments`` (string)
  A regular expression used to extract arguments from a function-style argument
  list (functions, methods, macros, etc.). This regular expression should match
  one argument at a time and capture only the argument's name.
  This setting is a little odd but currently required to extract argument list
  from function declarations.

``global_environment`` (string)
  A description of a CTPL_ environment to add when parsing rule_'s templates.


The ``doctypes`` group
----------------------

This group contains a list of `documentation types`_.


Documentation types
===================

A documentation type is a named set of rules_ for a `file type`_, describing how
to generate a particular type of documentation (i.e. Doxygen_, `Gtk-Doc`_,
Valadoc_ or whatever).

A documentation type is identified by its name and must therefore be unique
in a file type. But of course, different file types can define the same
documentation type. It is even recommended for a better consistency to use the
same identifier in different file types when they generate the same type of
documentation (even though it is completely up to you).


Short example
-------------

::

  doxygen = {
    struct.member = {
      template = " /**< {cursor} */";
      position = AFTER;
    }
    struct.template = "/**\n * @brief: {cursor}\n * \n * \n */\n";
  }


Rules: the cool thing
=====================

Rules are the actual definition of how documentation is generated. A rule
applies to a symbol type and hierarchy, allowing fine control over which and
how symbols are documented.

A rule is represented as a group of `settings`_ in a `documentation type`_.
The name of this group is the `type hierarchy`_ to which the settings applies.


Type hierarchy
--------------

A type hierarchy is a hierarchy of the types that a symbol must have to match
this rule.

In the symbol side, the type hierarchy is the types of the symbol's parents,
terminated by the symbol's own type. For example, a method in a class would
have a hierarchy like ``class -> method``; and if the class is itself in a
namespace, the hierarchy would the look like ``namespace -> class -> method``,
and so on.

For a rule to apply, its type hierarchy must match *the end* of the symbol
type hierarchy. For example a rule with the type hierarchy ``class`` will match
a symbol with the type hierarchy ``namespace -> class`` but not one with
``class -> method``.

A type hierarchy uses dots (``.``) to separate types and build the hierarchy.
For example, the type hierarchy representing ``namespace -> class`` would be
written ``namespace.class``.


Known types
~~~~~~~~~~~

``class``
  A class.
``enum``
  An enumeration.
``enumval``
  An enumeration value.
``field``
  A field (of a class for example).
``function``
  A function.
``include``
  An include directive.
``interface``
  An interface.
``local``
  A local variable.
``member``
  A member (of a structure for example).
``method``
  A method.
``namespace``
  A namespace.
``other``
  A non-specific type that highly depend on the language.
``package``
  A package.
``prototype``
  A prototype.
``struct``
  A structure.
``typedef``
  A type alias definition (*typedef* in C).
``union``
  An union.
``variable``
  A variable.
``extern``
  `???`
``define``
  A definition (like the *define* C preprocessor macro).
``macro``
  A macro.
``file``
  A file (will never match).


Rule settings
-------------

``template`` (string)
  A CTPL_ template that can include references to the following predefined
  variables in addition to the file-type-wide and the global environments:
  
  ``argument_list`` (string list)
    A list of the arguments of the currently documented symbol.
  
  ``returns`` (boolean)
    Indicates whether the currently documented symbol returns a value
    (makes sense only for symbols that may return a value).
  
  ``children`` (string list)
    A list of the current symbol's first-level children. This is only set if
    the rule's setting ``children`` is set to ``MERGE``.
  
  ``symbol`` (string)
    The name of the symbol that is documented.
  
  **[...]**
  
  ``cursor`` (special, described below)
    This can be used to mark in the template the position where the editor's
    cursor should be moved to after comment insertion.
    This mark will be removed from the generated documentation.
    Note that even if this mark may occur as many times as you want in a
    template, only the first will be actually honored, the latter being
    only removed.

``position`` (enumeration)
  The position where the documentation should be inserted. Possible values are:
  
  ``BEFORE`` [default]_
    Inserts the documentation just before the symbol.
  
  ``AFTER``
    Inserts the documentation just after the symbol (currently quite limited, it
    inserts the documentation at the end of the symbol's first line).
  
  ``CURSOR``
    Inserts the documentation at the current cursor position.

``policy`` (enumeration)
  How the symbol is documented. Possible values are:
  
  ``KEEP`` [default]_
    The symbol documents itself.
  
  ``FORWARD``
    Forward the documentation request to the parent. This is useful for symbols
    that are documented by their parent, such as `Gtk-Doc`_'s enumerations.
  
  ``PASS``
    Completely ignore the symbol and handle the documentation request as if it
    hasn't existed at all. This can be useful to ignore e.g. variables if they
    are extracted by the tag parser of the language and you don't want to
    document them, and don't want them to "eat" the documentation request.

``children`` (enumeration)
  How the symbol's children can be used in the template. Possible values are:
  
  ``SPLIT`` [default]_
    The symbol's children are provided as per-type lists.
  
  ``MERGE``
    The symbol's children are provided as a single list named ``children``.

``matches`` (flags)
  List of the children types that should be provided. Only useful if the
  ``children`` setting is set to ``MERGE``.
  Defaults to all.
  **FIXME: check the exactitude of this description**

``auto_doc_children`` (boolean)
  Whether to also document symbol's children (according to their own rules).


Miscellaneous
=============

Configuration directories
-------------------------

Configuration directories hold GeanyGenDoc's configuration. They are the
following:

*
  The user-specific configuration directory, containing the user-defined
  settings is ``$GEANY_USER_CONFIG/plugins/geanygendoc/``.
  ``$GEANY_USER_CONFIG`` is generally ``~/.config/geany/`` on UNIX systems.
*
  The system-wide configuration directory containing the default and
  pre-installed configuration is ``$GEANY_SYS_CONFIG/plugins/geanygendoc/``.
  ``$GEANY_SYS_CONFIG`` is generally ``/usr/share/geany/`` or
  ``/usr/local/share/geany`` on UNIX systems.

When searching for a configuration, GeanyGenDoc will first look in the
user's configuration directory, and if it wasn't found there, in the system
configuration directory. If both failed, it assumes that there is no
configuration at all.


Appendix
========

License
-------

| GeanyGenDoc, a Geany plugin to ease generation of source code documentation
| Copyright (C) 2010-2011  Colomban Wendling <ban(at)herbesfolles(dot)org>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.


Configuration syntax summary
----------------------------

::

  string               ::= ( """ .* """ | "'" .* "'" )
  constant             ::= [_A-Z][_A-Z0-9]+
  integer              ::= [0-9]+
  boolean              ::= ( "True" | "False" )
  setting_value        ::= ( string | constant | integer )
  setting              ::= "setting-name" "=" setting_value ";"
  setting_list         ::= ( "{" setting* "}" | setting )
  setting_section      ::= "settings" "=" setting_list
  
  position             ::= ( "BEFORE" | "AFTER" | "CURSOR" )
  policy               ::= ( "KEEP" | "FORWARD" | "PASS" )
  children             ::= ( "SPLIT" | "MERGE" )
  type                 ::= ( "class" | "enum" | "enumval" | "field" |
                             "function" | "interface" | "member" | "method" |
                             "namespace" | "package" | "prototype" | "struct" |
                             "typedef" | "union" | "variable" | "extern" |
                             "define" | "macro" | "file" )
  matches              ::= type ( "|" type )*
  doctype_subsetting   ::= ( "template"          "=" string |
                             "position"          "=" position |
                             "policy"            "=" policy |
                             "children"          "=" children |
                             "matches"           "=" matches |
                             "auto_doc_children" "=" boolean ) ";"
  match                ::= type ( "." type )*
  doctype_setting      ::= ( match "=" "{" doctype_subsetting* "}" |
                             match "." doctype_subsetting )
  doctype_setting_list ::= ( "{" doctype_setting* "}" | doctype_setting )
  doctype              ::= "doctype-name" "=" doctype_setting_list
  doctype_list         ::= ( "{" doctype* "}" | doctype )
  doctype_section      ::= "doctypes" "=" doctype_list
  
  document             ::= ( setting_section? doctype_section? |
                             doctype_section? setting_section? )


.. Content end, begin references

.. External links
..
.. _Doxygen: http://www.doxygen.org
.. _Gtk-Doc: http://www.gtk.org/gtk-doc/
.. _Valadoc: http://www.valadoc.org/
.. _CTPL: http://ctpl.tuxfamily.org/

.. Internal links
..
.. _file type: `File types`_
.. _file type configuration file: `File types`_
.. _documentation type: `Documentation types`_
.. _rule: `Rules: the cool thing`_
.. _rules: `Rules: the cool thing`_
.. _settings: `Rule settings`_
.. _configuration directory: `Configuration directories`_

-------------------

.. [default] This is the default value of the setting
