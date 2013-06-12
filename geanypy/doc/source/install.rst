Installation
************

Currently there are no binary packages available for installing GeanyPy so it
must be installed from source.  The following instructions will describe how
to do this.

Getting the Source
==================

The best way currently to get GeanyPy is to check it out from `it's repository
on GitHub.com <https://github.com/codebrainz/geanypy>`_.  You can clone
GeanyPy's `master` branch by issuing the following command in a directory
where you want to store its source code::

    $ git clone git://github.com/codebrainz/geanypy.git
    $ cd geanypy

Alternatively, you can download the `master` branch
`compressed into a tarball
<https://github.com/codebrainz/geanypy/tarball/master>`_
or `zip file <https://github.com/codebrainz/geanypy/zipball/master>`_.  Then
extract it where you want to store GeanyPy's source, for example::

    $ cd ~/src
    $ wget -O geanypy.tar.gz https://github.com/codebrainz/geanypy/tarball/master
    $ tar xf geanypy.tar.gz
    $ cd codebrainz-geanypy-*

The first method using `Git <http://git-scm.com/>`_ is the best, since it
allows you to update your copy of GeanyPy quite easily and also makes it
easier to contribute back to the GeanyPy project if you want to.

Dependencies and where to get them
==================================

Of course depending on what operating system and distribution you're using,
getting setup for this process may vary wildly.  At present, the following
dependencies are required to compile GeanyPy:

GCC, Autotools, and all the usual build tools
---------------------------------------------

For example on Debian (Ubuntu, Mint, etc.) do::

    $ apt-get install build-essential

Or on Fedora, something like this should do::

    $ yum groupinstall "Development Tools" "Legacy Software Development"

The latest development version of Geany (0.21+)
-----------------------------------------------

Since GeanyPy is wrapping the current development version of Geany, to use it
you are required to use that version of Geany.  Until the next Geany release,
you must either checkout the source code from Geany's
`Subversion repository <http://www.geany.org/Download/SVN>`_ or
`Git mirror <http://git.geany.org>`_ or you can get one of the
`Nightly builds <http://nightly.geany.org>`_ if you'd rather not compile
it yourself.

For more information on installing Geany, please refer to
`Geany's excellent manual
<http://www.geany.org/manual/current/index.html#installation>`_

Grabbing the dependencies for Geany on a Debian-based disto could be similar to
this::

    $ apt-get install libgtk2.0-0 libgtk2.0-0-dev

Or you might even have better luck with::

    $ apt-get build-dep geany

A quick session for installing Geany on a Debian-based distro might look
something like this::

    $ cd ~/src
    $ git clone http://git.geany.org/git/geany
    $ cd geany
    $ ./autogen.sh
    $ ./configure
    $ make
    $ make install # possibly as root

By default, Geany will install into `/usr/local` so if you want to install it
somewhere else, for example `/opt/geany`, then you would run the `configure`
command above with the `prefix` argument, like::

    $ ./configure --prefix=/opt/geany

It's important when installing GeanyPy later that you configure it with the
same prefix where Geany is installed, otherwise Geany won't find the GeanyPy
plugin.

Python 2.X and development files
--------------------------------

As GeanPy makes use of Python's C API to gain access to Geany's C plugin API,
both Python and the development files are required to compile GeanyPy.  In
theory, any Python version >= 2.6 and < 3.0 should be compatible with GeanyPy.
You can download Python `from its website <http://www.python.org/download>`_ or
you can install the required packages using your distro's package manager, for
example with Debian-based distros, run this::

    $ apt-get install python python-dev

**Note:** Python 3.0+ is not supported yet, although at some point in the
future, there are plans support it.

PyGTK and development files
---------------------------

Since Geany uses GTK+ as it's UI toolkit, GeanyPy uses PyGTK to interact with
Geany's UI.  You can either `download PyGTK from it's website
<http://www.pygtk.org/downloads.html>`_ or you can install it with your
system's pacakge manager, for example in Debian distros::

    $ apt-get install python-gtk2 python-gtk2-dev

**Note:** Although PyGTK is all but deprecated (or is completely deprecated?)
in favour of the newer and shinier PyGobject/GObject-introspection, it is
still used in new code in GeanyPy due to lack of documentation and pacakge
support for the newer stuff.

One fell swoop
--------------

If you're running a Debian-based distro, you should be able to install all
the required dependencies, not including Geany itself, with the following
command (as root)::

    $ apt-get install build-essential libgtk2.0-0 libgtk2.0-dev \
        python python-dev python-gtk2 python-gtk2-dev

And finally ... installing GeanyPy
==================================

Once all the dependencies are satisfied, installing GeanyPy should be pretty
straight-forward, continuing on from `Getting the Source`_ above::

    $ ./autogen.sh
    $ ./configure --prefix=/the/same/prefix/used/for/geany
    $ make
    $ make install # possibly as root
