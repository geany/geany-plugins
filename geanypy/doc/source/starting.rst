Getting Started
***************

Before diving into the details and API docs for programming plugins with
GeanyPy, it's important to note how it works and some features it provides.

What the heck is GeanyPy, really?
=================================

GeanyPy is "just another Geany plugin", really.  Geany sees GeanyPy as any
other `plugin <http://www.geany.org/manual/current/index.html#plugins>`_, so
to activate GeanyPy, use Geany's
`Plugin Manager <http://www.geany.org/manual/current/index.html#plugin-manager>`_
under the Tools menu as you would for any other plugin.

Once the GeanyPy plugin has been activated, a few elements are added to Geany's
user interface as described below.

Python Plugin Manager
=====================

Under the Tools menu, you will find the Python Plugin Manager, which is meant
to be similar to Geany's own Plugin Manager.  This is where you will activate
any plugins written in Python.

The Python Plugin Manager looks in exactly two places for plugins:

1. For system-wide plugins, it will search in PREFIX/share/geany/geanypy/plugins.
2. In Geany's config directory under your home directory, typically ~/.config/geany/plugins/geanypy/plugins.

Where `PREFIX` is the prefix used at configure time with Geany/GeanyPy (see
the previous section, Installation).  Both of these paths may vary depending on
your platform, but for most \*nix systems, the above paths should hold true.

Any plugins which follow the proper interface found in either of those two
directories will be listed in the Python Plugin Manager and you will be able
to activate and deactivate them there.

Python Console
==============

Another pretty cool feature of GeanyPy is the Python Console, which similar
to the regular Python interactive interpreter console, but it's found in the
Message Window area (bottom) in Geany.  The `geany` Python module used to
interact with Geany will be pre-imported for you, so you can mess around with
Geany using the console, without ever having to even write a plugin.

**Credits:** The Python Console was taken, almost in its entirety, from the
`medit text editor <http://mooedit.sourceforge.net>`_.  Props to the
author(s) for such a nice `piece of source code
<https://bitbucket.org/medit/medit/src/83c24f751493/moo/moopython/plugins/lib/pyconsole.py>`_

Future Plans
============

Some time in the near future, there should be support for sending text from
the active document into the Python Console.  It will also be possible to
have the Python Console either in a separate window, in the sidebar notebook
or in the message window notebook.

Also, either integration with Geany's keybindings UI under the preferences
dialog or a separate but similar UI just for Python plugins will be added.
Currently using keybindings requires a certain amount of hackery, due to
Geany expecting all plugins to be shared libraries written in C.
