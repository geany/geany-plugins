# -*- coding: utf-8 -*-
#
# WAF build script utilities for geany-plugins
#
# Copyright 2010 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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
Misc utility functions
"""

import os
import sys
from waflib.Errors import ConfigurationError, WafError
from waflib import Logs
from build import cache



makefile_template = '''#!/usr/bin/make -f
# Waf Makefile wrapper

all:
	@./waf build

update-po:
	@./waf --update-po

install:
	@if test -n "$(DESTDIR)"; then \\
		./waf install --destdir="$(DESTDIR)"; \\
	else \\
		./waf install; \\
	fi;

uninstall:
	@if test -n "$(DESTDIR)"; then \\
		./waf uninstall --destdir="$(DESTDIR)"; \\
	else \\
		./waf uninstall; \\
	fi;

clean:
	@./waf clean

distclean:
	@./waf distclean

.PHONY: clean uninstall install all
'''


def add_define_to_env(conf, key):
    value = conf.get_define(key)
    # strip quotes
    value = value.replace('"', '')
    conf.env[key] = value


def add_to_env_and_define(conf, key, value, quote=False):
    conf.define(key, value, quote)
    conf.env[key] = value


def build_plugin(ctx, name, sources=None, includes=None, defines=None, libraries=None, features=None):
    """
    Common build task for plugins, every plugin should call this in its wscript_build module

    Argument sources might be None or an empty string to use all
    source files in the plugin directory.

    @param ctx (waflib.Build.BuildContext)
    @param name (str)
    @param sources (list)
    @param includes (list)
    @param defines (list)
    @param libraries (list)
    @param features (list)
    """
    log_domain = name
    plugin_name = name.lower()
    includes = includes or []
    defines = defines or []
    libraries = libraries or []
    features = features or []

    is_win32 = target_is_win32(ctx)

    if not sources:
        sources = []
        for directory in includes:
            bld = ctx.srcnode
            files = bld.ant_glob('%s/*.c' % directory, src=True, dir=False)
            sources.extend(files)

    includes.append(ctx.out_dir)
    defines.append('G_LOG_DOMAIN="%s"' % log_domain)
    libraries.append('GTK')
    libraries.append('GEANY')
    features.append('c')
    features.append('cshlib')
    task = ctx.new_task_gen(
        features      = features,
        source        = sources,
        includes      = includes,
        defines       = defines,
        target        = plugin_name,
        use           = libraries,
        install_path  = '${G_PREFIX}/lib' if is_win32 else '${LIBDIR}/geany/')

    install_docs(ctx, plugin_name, 'AUTHORS ChangeLog COPYING NEWS README THANKS TODO'.split())
    return task


def check_c_header_cached(conf, **kw):
    # Works as conf.check_cc(header_name=...) but tries to cache already checked headers
    header_name = kw.get('header_name')
    mandatory = kw.get('mandatory')
    key = (header_name, mandatory)
    if key in cache:
        return cache[key]

    result = conf.check_cc(**kw)
    cache[key] = result
    return result


def check_cfg_cached(conf, **kw):
    # Works as conf.check_cfg() but tries to cache already checked packages
    package = kw.get('package')
    uselib_store = kw.get('uselib_store')
    args = kw.get('args')
    atleast_version = kw.get('atleast_version')
    mandatory = kw.get('mandatory')
    key = (package, uselib_store, args, atleast_version, mandatory)
    if key in cache:
        return cache[key]

    result = conf.check_cfg(**kw)
    cache[key] = result
    return result


def get_enabled_plugins(conf):
    plugins = get_plugins()
    enabled_plugins = []
    if conf.options.enable_plugins:
        for plugin_name in conf.options.enable_plugins.split(','):
            enabled_plugins.append(plugin_name.strip())
    else:
        skipped_plugins = conf.options.skip_plugins.split(',')
        for plugin_name in plugins:
            if not plugin_name in skipped_plugins:
                enabled_plugins.append(plugin_name)
    if not enabled_plugins:
        raise ConfigurationError('No plugins to compile found')
    return enabled_plugins


def get_plugins():
    plugins = []
    for path in os.listdir('.'):
        plugin_wscript = os.path.join(path, 'wscript_build')
        if os.path.exists(plugin_wscript):
            plugins.append(path)

    return sorted(plugins)


def get_svn_rev(conf):
    def in_git():
        cmd = 'git ls-files >/dev/null 2>&1'
        return (conf.exec_command(cmd) == 0)

    def in_svn():
        return os.path.exists('.svn')

    # try GIT
    if in_git():
        cmds = [ 'git svn find-rev HEAD 2>/dev/null',
                 'git svn find-rev origin/trunk 2>/dev/null',
                 'git svn find-rev trunk 2>/dev/null',
                 'git svn find-rev master 2>/dev/null'
                ]
        for cmd in cmds:
            try:
                stdout = conf.cmd_and_log(cmd)
                if stdout:
                    return int(stdout.strip())
            except WafError:
                pass
            except ValueError:
                Logs.pprint('RED', 'Unparseable revision number')
    # try SVN
    elif in_svn():
        try:
            _env = None if target_is_win32(conf) else dict(LANG='C')
            stdout = conf.cmd_and_log(cmd='svn info --non-interactive', env=_env)
            lines = stdout.splitlines(True)
            for line in lines:
                if line.startswith('Last Changed Rev'):
                    value = line.split(': ', 1)[1]
                    return int(value.strip())
        except WafError:
            pass
        except (IndexError, ValueError):
            Logs.pprint('RED', 'Unparseable revision number')
    return 0


def install_docs(ctx, name, files):
    is_win32 = target_is_win32(ctx)
    ext = '.txt' if is_win32 else ''
    docdir = '${G_PREFIX}/doc/plugins/%s' % name if is_win32 else '${DOCDIR}/%s' % name
    for filename in files:
        if os.path.exists(os.path.join(name, filename)):
            basename = uc_first(filename, ctx)
            destination_filename = '%s%s' % (basename, ext)
            destination = os.path.join(docdir, destination_filename)
            ctx.install_as(destination, filename)


# Simple function to execute a command and print its exit status
def launch(ctx, command, status, success_color='GREEN'):
    ret = 0
    error_message = ''
    Logs.pprint(success_color, status)
    ret = ctx.exec_command(command)
    if ret == -1:
        _, e, _ = sys.exc_info()
        ret = 1
        error_message = ' (%s: %s)' % (str(e), command)

    if ret != 0:
        Logs.pprint('RED', '%s failed%s' % (status, error_message))

    return ret


def load_intltool_if_available(conf):
    try:
        conf.check_tool('intltool')
        if 'LINGUAS' in os.environ:
            conf.env['LINGUAS'] = os.environ['LINGUAS']
    except WafError:
        # on Windows, we don't hard depend on intltool, on all other platforms raise an error
        if not target_is_win32(conf):
            raise


def set_lib_dir(conf):
    if target_is_win32(conf):
        # nothing to do
        return
    # use the libdir specified on command line
    if conf.options.libdir:
        add_to_env_and_define(conf, 'LIBDIR', conf.options.libdir, True)
    else:
        # get Geany's libdir (this should be the default case for most users)
        libdir = conf.check_cfg(package='geany', args='--variable=libdir')
        if libdir:
            add_to_env_and_define(conf, 'LIBDIR', libdir.strip(), True)
        else:
            add_to_env_and_define(conf, 'LIBDIR', conf.env['PREFIX'] + '/lib', True)
    # libexec (e.g. for geanygdb)
    if conf.options.libexecdir:
        add_to_env_and_define(conf, 'LIBEXECDIR', conf.options.libexecdir, True)

    else:
        add_to_env_and_define(conf, 'LIBEXECDIR', conf.env['PREFIX'] + '/libexec', True)


def setup_makefile(ctx):
    # convenience script (script content copied from the original waf.bat)
    if target_is_win32(ctx):
        file_h = open('waf.bat', 'wb')
        file_h.write('@python -x %~dp0waf %* & exit /b')
        file_h.close()
    # write a simple Makefile
    else:
        file_h = open('Makefile', 'w')
        file_h.write(makefile_template)
        file_h.close()


def target_is_win32(ctx):
    if 'is_win32' in ctx.env:
        # cached
        return ctx.env['is_win32']
    is_win32 = None
    if sys.platform == 'win32':
        is_win32 = True
    if is_win32 is None:
        if ctx.env and 'CC' in ctx.env:
            env_cc = ctx.env['CC']
            if not isinstance(env_cc, str):
                env_cc = ''.join(env_cc)
            is_win32 = (env_cc.find('mingw') != -1)
    if is_win32 is None:
        is_win32 = False
    # cache for future checks
    ctx.env['is_win32'] = is_win32
    return is_win32


def uc_first(string, ctx):
    if target_is_win32(ctx):
        return string.title()
    return string
