The :mod:`app` module
*********************

.. module:: app
    :synopsis: Application settings

This modules contains a class to access application settings.

:class:`App` Objects
====================

.. class:: App

This class is initialized automatically and by the :mod:`geany` module and
shouldn't be initalized by users.  An instance of it is available through
the :data:`geany.app` attribute of the :mod:`geany` module.

All members of the :class:`App` are read-only properties.

    .. attribute:: App.configdir

        User configuration directory, usually ~/.config/geany.  To store configuration
        files for your plugin, it's a good idea to use something like this::

            conf_path = os.path.join(geany.app.configdir, "plugins", "yourplugin",
                            "yourconfigfile.conf")

    .. attribute:: App.debug_mode

        If True, debug messages should be printed.  For example, if you want to make
        a :py:func:`print` function that only prints when :attr:`App.debug_mode`
        is active, you could do something like this::

            def debug_print(message):
                if geany.app.debug_mode:
                    print(message)

    .. attribute:: App.project

        If not :py:obj:`None`, the a :class:`project.Project` for currently active project.
