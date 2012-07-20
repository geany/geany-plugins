# -*- coding: utf-8 -*-
#
# WAF build script for geany-plugins
#
# Copyright 2008-2010 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# $Id$

"""
Waf build script for Geany Plugins.

If you want to add your plugin to this build system, simply put a file called
'wscript_build' into your plugin directory and add necessary code to build your
plugin or simply copy the wscript_build file from another plugin
and adjust the values.
If special configuration is necessary, e.g. checking for dependencies of
the plugin, create a file 'wscript_configure' and put the code into.


The code of this file itself loosely follows PEP 8 with some exceptions
(line width 100 characters and some other minor things).

Requires WAF 1.6.1 and Python 2.5 (or later).
"""

import os
import tempfile
from glob import glob
from waflib import Logs, Scripting, Utils
from waflib.Tools import c_preproc
from waflib.Errors import ConfigurationError
from waflib.TaskGen import feature
from build.wafutils import (
    add_define_to_env,
    add_to_env_and_define,
    check_cfg_cached,
    get_plugins,
    get_enabled_plugins,
    get_svn_rev,
    install_docs,
    launch,
    load_intltool_if_available,
    set_lib_dir,
    setup_makefile,
    target_is_win32)


APPNAME = 'geany-plugins'
VERSION = '1.23'
LINGUAS_FILE = 'po/LINGUAS'

top = '.'
out = '_build_'


c_preproc.go_absolute = True
c_preproc.standard_includes = []


def configure(conf):

    conf.check_waf_version(mini='1.6.1')

    # a C compiler is the very minimum we need
    conf.load('compiler_c')

    # common for all plugins
    check_cfg_cached(conf,
                   package='gtk+-2.0',
                   atleast_version='2.16.0',
                   uselib_store='GTK',
                   mandatory=True,
                   args='--cflags --libs')
    check_cfg_cached(conf,
                   package='geany',
                   atleast_version='1.23',
                   uselib_store='GEANY',
                   mandatory=True,
                   args='--cflags --libs')

    set_lib_dir(conf)
    # SVN/GIT detection
    svn_rev = get_svn_rev(conf)
    conf.define('REVISION', svn_rev, 1)
    # GTK/Geany versions
    geany_version = conf.check_cfg(modversion='geany') or 'Unknown'
    gtk_version = conf.check_cfg(modversion='gtk+-2.0') or 'Unknown'

    load_intltool_if_available(conf)

    # build plugin list
    enabled_plugins = get_enabled_plugins(conf)

    # execute plugin specific coniguration code
    configure_plugins(conf, enabled_plugins)
    # now add the enabled_plugins to the env to remember them
    conf.env.append_value('enabled_plugins', enabled_plugins)

    setup_configuration_env(conf)
    setup_makefile(conf)
    conf.write_config_header('config.h')

    # enable debug when compiling from VCS
    if svn_rev > 0:
        conf.env.append_value('CFLAGS', '-g -DDEBUG'.split()) # -DGEANY_DISABLE_DEPRECATED

    # summary
    Logs.pprint('BLUE', 'Summary:')
    conf.msg('Install Geany Plugins ' + VERSION + ' in', conf.env['G_PREFIX'])
    conf.msg('Using GTK version', gtk_version)
    conf.msg('Using Geany version', geany_version)
    if svn_rev > 0:
        conf.msg('Compiling Subversion revision', svn_rev)
    conf.msg('Plugins to compile', ' '.join(enabled_plugins))


def configure_plugins(conf, enabled_plugins):
    # we need to iterate over the plugin directories ourselves to be able
    # to catch plugin ConfigurationError's and remove the plugin in this case
    plugins = list(enabled_plugins)
    # iterate over a copy of the list as we remove items during iteration
    for plugin in plugins:
        try:
            # this calls the configure() functions of each plugin's wscript
            conf.recurse(plugin, mandatory=False)
        except ConfigurationError:
            enabled_plugins.remove(plugin)


def setup_configuration_env(conf):
    # Windows specials
    if target_is_win32(conf):
        if conf.env['PREFIX'].lower() == tempfile.gettempdir().lower():
            # overwrite default prefix on Windows (tempfile.gettempdir() is the Waf default)
            new_prefix = os.path.join(str(conf.root), '%s-%s' % (APPNAME, VERSION))
            add_to_env_and_define(conf, 'PREFIX', new_prefix, quote=True)
        conf.env['G_PREFIX'] = conf.env['PREFIX']
        # paths
        add_to_env_and_define(conf, 'PREFIX', '', quote=True)
        add_to_env_and_define(conf, 'LIBDIR', '', quote=True)
        add_to_env_and_define(conf, 'LIBEXECDIR', '', quote=True)
        add_to_env_and_define(conf, 'DOCDIR', 'doc', quote=True)
        conf.define('LOCALEDIR', 'share/locale', 1)
        # overwrite LOCALEDIR to install message catalogues properly
        conf.env['LOCALEDIR'] = os.path.join(conf.env['G_PREFIX'], 'share/locale')
        # DATADIR is defined in objidl.h, so we remove it from config.h
        conf.undefine('DATADIR')
        add_to_env_and_define(conf, 'GEANYPLUGINS_DATADIR', 'share')
    else:
        prefix = conf.env['PREFIX']
        conf.env['G_PREFIX'] = conf.env['PREFIX']
        # DATADIR and LOCALEDIR are defined by the intltool tool
        # but they are not added to the environment, so we need to
        add_define_to_env(conf, 'DATADIR')
        add_define_to_env(conf, 'LOCALEDIR')
        conf.env['cshlib_PATTERN'] = '%s.so'
        doc_dir = '%s/doc/geany-plugins' % conf.env['DATADIR']
        conf.define('PREFIX', prefix, quote=True)
        add_to_env_and_define(conf, 'DOCDIR', doc_dir, quote=True)
        add_to_env_and_define(conf, 'GEANYPLUGINS_DATADIR', conf.env['DATADIR'], quote=True)
        conf.env['GEANYPLUGINS_DATADIR'] = conf.env['DATADIR']
    # common
    pkgdatadir = os.path.join(conf.env['GEANYPLUGINS_DATADIR'], 'geany-plugins')
    pkglibdir = os.path.join(conf.env['LIBDIR'], 'geany-plugins')
    pkgincludedir = os.path.join(conf.env['G_PREFIX'], 'include')
    add_to_env_and_define(conf, 'INCLUDEDIR', pkgincludedir, quote=True)
    add_to_env_and_define(conf, 'DATAROOTDIR', conf.env['GEANYPLUGINS_DATADIR'], quote=True)
    add_to_env_and_define(conf, 'PKGDATADIR', pkgdatadir, quote=True)
    add_to_env_and_define(conf, 'PKGLIBDIR', pkglibdir, quote=True)
    add_to_env_and_define(conf, 'VERSION', VERSION, quote=True)
    add_to_env_and_define(conf, 'PACKAGE', APPNAME, quote=True)
    add_to_env_and_define(conf, 'GETTEXT_PACKAGE', APPNAME, quote=True)
    add_to_env_and_define(conf, 'ENABLE_NLS', 1)

    conf.env.append_value('CFLAGS', ['-DHAVE_CONFIG_H'])


def options(opt):
    opt.tool_options('compiler_cc')
    opt.tool_options('intltool')

    plugins = get_plugins()

    # execute plugin specific option code
    opt.recurse(plugins, mandatory=False)

    # Paths
    opt.add_option('--libdir', type='string', default='',
        help='object code libraries', dest='libdir')
    opt.add_option('--libexecdir', type='string', default='',
        help='program executables', dest='libexecdir')
    # Actions
    opt.add_option('--list-plugins', action='store_true', default=False,
        help='list plugins which can be built', dest='list_plugins')

    opt.add_option('--enable-plugins', action='store', default='',
        help='plugins to be built [plugins in CSV format, e.g. "%(1)s,%(2)s"]' % \
        { '1' : plugins[0], '2' : plugins[1] }, dest='enable_plugins')
    opt.add_option('--skip-plugins', action='store', default='',
        help='plugins which should not be built, ignored when --enable-plugins is set, same format as --enable-plugins',
        dest='skip_plugins')


def build(bld):
    is_win32 = target_is_win32(bld)
    enabled_plugins = bld.env['enabled_plugins']

    if bld.cmd == 'clean':
        remove_linguas_file()
    if bld.cmd in ('install', 'uninstall'):
        bld.add_post_fun(_post_install)

    if bld.env['INTLTOOL']:
        install_path = '${G_PREFIX}/share/locale' if is_win32 else '${LOCALEDIR}'
        bld.new_task_gen(
            features     = ['linguas', 'intltool_po'],
            podir        = 'po',
            appname      = APPNAME,
            install_path = install_path)

    # install additional docs
    install_docs(bld, '', ('README', 'NEWS'))
    if is_win32:
        bld.install_as('${G_PREFIX}/ReadMe.Windows.txt', 'README.windows')

    # execute plugin specific build code
    bld.recurse(enabled_plugins)


def _post_install(ctx):
    is_win32 = target_is_win32(ctx)
    if is_win32:
        return
    theme_dir = Utils.subst_vars('${DATADIR}/icons/hicolor', ctx.env)
    icon_cache_updated = False
    if not ctx.options.destdir:
        ctx.exec_command('gtk-update-icon-cache -q -f -t %s' % theme_dir)
        Logs.pprint('GREEN', 'GTK icon cache updated.')
        icon_cache_updated = True
    if not icon_cache_updated:
        Logs.pprint('YELLOW', 'Icon cache not updated. After install, run this:')
        Logs.pprint('YELLOW', 'gtk-update-icon-cache -q -f -t %s' % theme_dir)


def init(ctx):
    if ctx.options.list_plugins:
        listplugins(ctx)


def listplugins(ctx):
    """list plugins which can be built"""
    plugins = get_plugins()
    Logs.pprint('GREEN', 'The following targets can be chosen with the --enable-plugins option:')
    Logs.pprint('NORMAL', ' '.join(plugins))

    Logs.pprint('GREEN', \
    '\nTo compile only "%(1)s" and "%(2)s", use "./waf configure --enable-plugins=%(1)s,%(2)s".' % \
            { '1' : plugins[0], '2' : plugins[1] } )
    exit(0)


def distclean(ctx):
    Scripting.distclean(ctx)
    remove_linguas_file()


def remove_linguas_file():
    # remove LINGUAS file as well
    try:
        os.unlink(LINGUAS_FILE)
    except OSError:
        pass


@feature('linguas')
def write_linguas_file(self):
    if os.path.exists(LINGUAS_FILE):
        return
    linguas = ''
    if 'LINGUAS' in self.env:
        files = self.env['LINGUAS']
        for po_filename in files.split(' '):
            if os.path.exists ('po/%s.po' % po_filename):
                linguas += '%s ' % po_filename
    else:
        files = os.listdir('%s/po' % self.path.abspath())
        files.sort()
        for filename in files:
            if filename.endswith('.po'):
                linguas += '%s ' % filename[:-3]
    file_h = open(LINGUAS_FILE, 'w')
    file_h.write('# This file is autogenerated. Do not edit.\n%s\n' % linguas)
    file_h.close()


def create_installer(ctx):
    """create the Windows installer (maintainer and Win32 only)"""
    # must be called *after* everything has been installed
    do_sign = os.path.exists('sign.bat') # private file to sign the binary files, not needed
    def sign_binary(filename):
        if do_sign:
            ctx.exec_command('sign.bat %s' % filename)

    # strip all binaries
    Logs.pprint('CYAN', 'Stripping %sfiles' % ('and signing binary ' if do_sign else ''))
    install_dir = '%s-%s' % (APPNAME, VERSION) # should be ctx.env['G_PREFIX']
    files = glob(os.path.join(install_dir, 'lib', '*.dll'))
    files.append(os.path.join(install_dir, 'lib\geany-plugins\geanylua\libgeanylua.dll'))
    for filename in files: # sign the DLL files
        ctx.exec_command('strip %s' % filename)
        sign_binary(filename)
    # create the installer
    launch(ctx, 'makensis /V2 /NOCD build/geany-plugins.nsi', 'Creating the installer', 'CYAN')
    sign_binary('geany-plugins-%s_setup.exe' % VERSION)


def updatepo(ctx):
    """update the message catalogs for internationalization"""
    potfile = '%s.pot' % APPNAME
    os.chdir('%s/po' % top)
    try:
        try:
            old_size = os.stat(potfile).st_size
        except OSError:
            old_size = 0
        ctx.exec_command('intltool-update --pot -g %s' % APPNAME)
        size_new = os.stat(potfile).st_size
        if size_new != old_size:
            Logs.pprint('CYAN', 'Updated POT file.')
            launch(ctx, 'intltool-update -r -g %s' % APPNAME, 'Updating translations', 'CYAN')
        else:
            Logs.pprint('CYAN', 'POT file is up to date.')
    except OSError:
        Logs.pprint('RED', 'Failed to generate pot file.')
    os.chdir('..')
