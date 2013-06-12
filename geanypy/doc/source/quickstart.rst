Writing a Plugin - Quick Start Guide
************************************

This is just a quick tutorial to describe writing a GeanyPy compatible plugin
in Python.  Writing a plugin should be pretty straightforward for any Python
programmers, especially those familiar with writing `regular C plugins for
Geany <http://www.geany.org/manual/reference/howto.html>`_.

To illustrate the similarities to the C API, the example at the end of this
section will create the same plugin as in Geany's Plugin Howto, except
obviously written in Python.

The Plugin Interface
====================

The first thing any plugin will want to do is to import the `geany` module::

    import geany

**Note:** Due to the way the Geany plugin framework works, importing the
`geany` module will certain fail if you just try running it standalone, outside
of Geany/GeanyPy.

After that, you create a regular Python class which inherits from the
`geany.Plugin` class::

    import geany

    class HelloWorld(geany.Plugin):
        pass

This will allow GeanyPy's Python Plugin Manager to locate the plugin as a
GeanyPy plugin.  If it doesn't inherit from `geany.Plugin` it will not be
detected.

There are a few more parts of the interface that must be implemented in order
for the plugin to be detected by GeanyPy::

    import geany

    class HelloWorld(geany.Plugin):

        __plugin_name__ = "HelloWorld" # required
        __plugin_version__ = "version of your plugin"
        __plugin_description__ = "description of your plugin"
        __plugin_author__ = "Your Name <your email address>"

These allow the Python Plugin Manager to glean information about your plugin
which will be shown in the managers plugin list.  All but the `__plugin_name__`
attributes are optional, though recommended.

The next thing that's needed is an entry-point to the plugin.  Since Python
classes have a initialization method already, this seems like a logical
entry-point::

    import geany

    class HelloWorld(geany.Plugin):

        __plugin_name__ = "HelloWorld" # required
        __plugin_version__ = "version of your plugin"
        __plugin_description__ = "description of your plugin"
        __plugin_author__ = "Your Name <your email address>"

        def __init__(self):
            do_stuff_when_loaded()

If you have some de-initialization code that needs to be run, you can add
a `cleanup` method to the class that is guaranteed to be called when your
plugin is unloaded, however it's optional::

    import geany

    class HelloWorld(geany.Plugin):

        __plugin_name__ = "HelloWorld" # required
        __plugin_version__ = "version of your plugin"
        __plugin_description__ = "description of your plugin"
        __plugin_author__ = "Your Name <your email address>"

        def __init__(self):
            do_stuff_when_loaded()

        def cleanup(self):
            do_stuff_when_unloaded()

And there you have it!  That's the minimum requirements for writing a plugin
that will be detected by GeanyPy.  Ok, it doesn't do anything yet, but it
will be shown in the Python Plugin Manager and can be loaded and unloaded.

Real-world Example
==================

To put it into context, here's a plugin that mimics the plugin in
`Geany's Plugin Howto <http://www.geany.org/manual/reference/howto.html>`_::

    import gtk
    import geany

    class HelloWorld(geany.Plugin):

        __plugin_name__ = "HelloWorld"
        __plugin_version__ = "1.0"
        __plugin_description__ = "Just another tool to say hello world"
        __plugin_author__ = "John Doe <john.doe@example.org>"

        def __init__(self):
            self.menu_item = gtk.MenuItem("Hello World")
            self.menu_item.show()
            geany.main_widgets.tools_menu.append(self.menu_item)
            self.menu_item.connect("activate", self.on_hello_item_clicked)

        def cleanup(self):
            self.menu_item.destroy()

        def on_hello_item_clicked(widget, data):
            geany.dialogs.show_msgbox("Hello World")

Hopefully this makes it clear how to write a Python plugin using GeanyPy.  This
sample plugin is provided with GeanyPy if you want to try it out.

