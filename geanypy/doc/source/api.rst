API Documentation
*****************

GeanyPy's API mimics quite closely Geany's C plugin API.  The following
documentation is broken down by file/module:

The :mod:`geany` modules:

.. toctree::
    :maxdepth: 2

    app.rst
    dialogs.rst
    document.rst

The :mod:`geany` package and module
===================================

.. module:: geany

All of GeanyPy's bindings are inside the :mod:`geany` package which also
contains some stuff in it's :mod:`__init__` file, acting like a module itself.


.. data:: app

    An instance of :class:`app.App` to store application information.

.. data:: main_widgets

    An instance of :class:`mainwidgets.MainWidgets` to provide access to
    Geany's important GTK+ widgets.

.. data:: signals

    An instance of :class:`signalmanager.SignalManager` which manages the
    connection, disconnection, and emission of Geany's signals.  You can
    use this as follows::

        geany.signals.connect('document-open', some_callback_function)

.. function:: is_realized()

    This function, which is actually in the :mod:`geany.main` module will tell
    you if Geany's main window is realized (shown).

.. function:: locale_init()

    Again, from the :mod:`geany.main` module, this will initialize the `gettext`
    translation system.

.. function:: reload_configuration()

    Also from the :mod:`geany.main` module, this function will cause Geany to
    reload most if it's configuration files without restarting.

    Currently the following files are reloaded:

        * all template files
        * new file templates
        * the New (with template) menus will be updated
        * Snippets (snippets.conf)
        * filetype extensions (filetype_extensions.conf)
        * `settings` and `build_settings` sections of the filetype definition files.

    Plugins may call this function if they changed any of these files (e.g. a
    configuration file editor plugin).
