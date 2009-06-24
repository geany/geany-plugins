#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# WAF build script for geany-plugins
#
# Copyright 2008-2009 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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

If you want to add your plugin to this build system, simply add a Plugin object
to the plugin list below by adding a new line and passing the appropiate
values to the Plugin constructor. Everything below doesn't need to be changed.

If you need additional checks for header files, functions in libraries or
need to check for library packages (using pkg-config), please ask Enrico
before committing changes. Thanks.

Requires WAF 1.5.7 and Python 2.4 (or later).
"""


import Build, Options, Utils, preproc
import os, sys, tempfile


APPNAME = 'geany-plugins'
VERSION = '0.17'

srcdir = '.'
blddir = '_build_'


class Plugin:
	def __init__(self, n, s, i, l=[]):
		# plugin name, must be the same as the corresponding subdirectory
		self.name = n
		# may be None to auto-detect source files in all directories specified in 'includes'
		self.sources = s
		# do not include '.'
		self.includes = i
		# a list of lists of libs and their versions, e.g. [ [ 'enchant', '1.3' ],
		# [ 'gtkspell-2.0', '2.0', False ] ], the third argument defines whether
		# the dependency is mandatory
		self.libs = l


# add a new element for your plugin
plugins = [
	Plugin('addons', None, [ 'addons/src' ]),
	Plugin('geanylatex', None, [ 'geanylatex/src']),
	Plugin('geanylipsum', None, [ 'geanylipsum/src']),
	Plugin('geanysendmail',
		[ 'geanysendmail/src/geanysendmail.c' ],
		[ 'geanysendmail/src' ]),
	Plugin('geanyvc', None, [ 'geanyvc/src/'], [ [ 'gtkspell-2.0', '2.0', True ] ]),
	Plugin('shiftcolumn', None, [ 'shiftcolumn/src']),
	Plugin('spellcheck', None, [ 'spellcheck/src' ], [ [ 'enchant', '1.3', True ] ]),
	Plugin('geanygdb',
		 map(lambda x: "geanygdb/src/" + x, ['gdb-io-break.c',
		   'gdb-io-envir.c', 'gdb-io-frame.c', 'gdb-io-read.c', 'gdb-io-run.c',
		   'gdb-io-stack.c', 'gdb-lex.c', 'gdb-ui-break.c', 'gdb-ui-envir.c',
		   'gdb-ui-frame.c',  'gdb-ui-locn.c', 'gdb-ui-main.c',
		   'geanydebug.c']), # source files
		 [ 'geanygdb', 'geanygdb/src' ], # include dirs
		 [ [ 'elf.h', '', True ] ]
		 ),
	Plugin('geanylua',
		 [ 'geanylua/geanylua.c' ], # the other source files are listed in build_lua()
		 [ 'geanylua' ],
		 # maybe you need to modify the package name of Lua, try one of these: lua5.1 lua51 lua-5.1
		 [ [ 'lua', '5.1', True ] ])
]

'''
temporary_disabled_plugins
	Plugin('externdbg',
		 [ 'externdbg/src/dbg.c' ], # source files
		 [ 'externdbg', 'externdbg/src' ] # include dirs
		 ),
	Plugin('geanydoc',
		 [ 'geanydoc/src/config.c', 'geanydoc/src/geanydoc.c' ], # source files
		 [ 'geanydoc', 'geanydoc/src' ] # include dirs
		 ),
	Plugin('geanyprj',
		 [ 'geanyprj/src/geanyprj.c', 'geanyprj/src/menu.c', 'geanyprj/src/project.c',
		   'geanyprj/src/sidebar.c', 'geanyprj/src/utils.c', 'geanyprj/src/xproject.c' ],
		 [ 'geanyprj', 'geanyprj/src' ] # include dirs
		 ),
	Plugin('geany-mini-script',
		 [ 'geany-mini-script/src/gms.c', 'geany-mini-script/src/gms_gui.c' ], # source files
		 [ 'geany-mini-script', 'geany-mini-script/src' ] # include dirs
		 )
'''


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


preproc.go_absolute = True
preproc.standard_includes = []

def configure(conf):
	def conf_get_svn_rev():
		# try GIT
		if os.path.exists('.git'):
			cmds = [ 'git svn find-rev HEAD 2>/dev/null',
					 'git svn find-rev origin/trunk 2>/dev/null',
					 'git svn find-rev trunk 2>/dev/null',
					 'git svn find-rev master 2>/dev/null' ]
			for c in cmds:
				try:
					stdout = Utils.cmd_output(c)
					if stdout:
						return stdout.strip()
				except:
					pass
		# try SVN
		elif os.path.exists('.svn'):
			try:
				_env = None if is_win32 else {'LANG' : 'C'}
				stdout = Utils.cmd_output(cmd='svn info --non-interactive', silent=True, env=_env)
				lines = stdout.splitlines(True)
				for line in lines:
					if line.startswith('Last Changed Rev'):
						key, value = line.split(': ', 1)
						return value.strip()
			except:
				pass
		else:
			pass
		return '-1'

	def set_lib_dir():
		# use the libdir specified on command line
		if Options.options.libdir:
			conf.define('LIBDIR', Options.options.libdir, 1)
		else:
			# get Geany's libdir (this should be the default case for most users)
			libdir = conf.check_cfg(package='geany', args='--variable=libdir')
			if libdir:
				conf.define('LIBDIR', libdir.strip(), 1)
			else:
				conf.define('LIBDIR', conf.env['PREFIX'] + '/lib', 1)

	conf.check_tool('compiler_cc')
	# we don't require intltool on Windows (it would require Perl) though it works well
	try:
		conf.check_tool('intltool')
	except:
		pass

	is_win32 = target_is_win32(conf.env)

	if not is_win32:
		set_lib_dir()

	conf.check_cfg(package='gtk+-2.0', atleast_version='2.8.0', uselib_store='GTK',
		mandatory=True, args='--cflags --libs')
	conf.check_cfg(package='geany', atleast_version='0.17', mandatory=True, args='--cflags --libs')

	gtk_version = conf.check_cfg(modversion='gtk+-2.0') or 'Unknown'
	geany_version = conf.check_cfg(modversion='geany') or 'Unknown'

	enabled_plugins = []
	if Options.options.enable_plugins:
		for p_name in Options.options.enable_plugins.split(','):
			enabled_plugins.append(p_name.strip())
	else:
		skipped_plugins = Options.options.skip_plugins.split(',')
		for p in plugins:
				if not p.name in skipped_plugins:
					enabled_plugins.append(p.name)

	# remove enabled but not existent plugins
	for p in plugins:
		if p.name in enabled_plugins and not os.path.exists(p.name):
			enabled_plugins.remove(p.name)

	# check for plugin deps
	for p in plugins:
		if p.name in enabled_plugins:
			for l in p.libs:
				if l[0].endswith('.h'):
					# check for header files
					conf.check(header_name=l[0])
					if not conf.env['HAVE_%s' % Utils.quote_define_name(l[0])] == 1:
						if l[2]:
							enabled_plugins.remove(p.name)
				else:
					# check for libraries (with pkg-config)
					uselib = Utils.quote_define_name(l[0])
					conf.check_cfg(package=l[0], uselib_store=uselib, atleast_version=l[1],
						args='--cflags --libs')
					if not conf.env['HAVE_%s' % uselib] == 1:
						if l[2]:
							enabled_plugins.remove(p.name)


	# Windows specials
	if is_win32:
		if conf.env['PREFIX'] == tempfile.gettempdir():
			# overwrite default prefix on Windows (tempfile.gettempdir() is the Waf default)
			conf.define('PREFIX', os.path.join(conf.srcdir, '%s-%s' % (APPNAME, VERSION)), 1)
		# hack: we add the parent directory of the first include directory as this is missing in
		# list returned from pkg-config
		conf.env['CPPPATH_GTK'].insert(0, os.path.dirname(conf.env['CPPPATH_GTK'][0]))
		# we don't need -fPIC when compiling on or for Windows
		if '-fPIC' in conf.env['shlib_CCFLAGS']:
			conf.env['shlib_CCFLAGS'].remove('-fPIC')
		conf.env['shlib_PATTERN'] = '%s.dll'
		conf.env['program_PATTERN'] = '%s.exe'
	else:
		conf.env['shlib_PATTERN'] = '%s.so'

	svn_rev = conf_get_svn_rev()
	conf.define('REVISION', svn_rev, 1)

	conf.env['G_PREFIX'] = conf.env['PREFIX']

	if 'geanyvc' in enabled_plugins:
		# hack for GeanyVC
		conf.define('USE_GTKSPELL', 1);

	if is_win32:
		conf.define('PREFIX', '', 1)
		conf.define('LIBDIR', '', 1)
		conf.define('DOCDIR', 'doc', 1)
		conf.define('LOCALEDIR', 'share/locale', 1)
		# DATADIR is defined in objidl.h, so we remove it from config.h
		conf.undefine('DATADIR')
	else:
		conf.define('PREFIX', conf.env['PREFIX'], 1)
		conf.define('DOCDIR', '%s/doc/geany-plugins/' % conf.env['DATADIR'], 1)
	conf.define('VERSION', VERSION, 1)
	conf.define('PACKAGE', APPNAME, 1)
	conf.define('GETTEXT_PACKAGE', APPNAME, 1)
	conf.define('ENABLE_NLS', 1)
	conf.write_config_header('config.h')

	if is_win32: # overwrite LOCALEDIR to install message catalogues properly
		conf.env['LOCALEDIR'] = os.path.join(conf.env['G_PREFIX'], 'share/locale')

	Utils.pprint('BLUE', 'Summary:')
	print_message(conf, 'Install Geany Plugins ' + VERSION + ' in', conf.env['G_PREFIX'])
	print_message(conf, 'Using GTK version', gtk_version)
	print_message(conf, 'Using Geany version', geany_version)
	if svn_rev != '-1':
		print_message(conf, 'Compiling Subversion revision', svn_rev)
		conf.env.append_value('CCFLAGS', '-g -O0 -DDEBUG'.split()) # -DGEANY_DISABLE_DEPRECATED')

	print_message(conf, 'Plugins to compile', ' '.join(enabled_plugins))

	conf.env.append_value('enabled_plugins', enabled_plugins)
	conf.env.append_value('CCFLAGS', '-DHAVE_CONFIG_H'.split())

	if is_win32: # convenience script (script content copied from the original waf.bat)
		f = open('waf.bat', 'wb')
		f.write('@python -x %~dp0waf %* & exit /b')
		f.close
	else: # write a simple Makefile
		f = open('Makefile', 'w')
		f.write(makefile_template)
		f.close


def set_options(opt):
	opt.tool_options('compiler_cc')
	opt.tool_options('intltool')

	# Paths
	opt.add_option('--libdir', type='string', default='',
		help='object code libraries', dest='libdir')
	# Actions
	opt.add_option('--update-po', action='store_true', default=False,
		help='update the message catalogs for translation', dest='update_po')
	opt.add_option('--list-plugins', action='store_true', default=False,
		help='list plugins which can be built', dest='list_plugins')

	opt.add_option('--enable-plugins', action='store', default='',
		help='plugins to be built [plugins in CSV format, e.g. "%(1)s,%(2)s"]' % \
		{ '1' : plugins[0].name, '2' : plugins[1].name }, dest='enable_plugins')
	opt.add_option('--skip-plugins', action='store', default='',
		help='plugins which should not be built, ignored when --enable-plugins is set, same format as --enable-plugins' % \
		{ '1' : plugins[0].name, '2' : plugins[1].name }, dest='skip_plugins')


def build(bld):
	is_win32 = target_is_win32(bld.env)

	def build_lua(bld, p, libs):
		lua_sources = [ 'geanylua/glspi_init.c', 'geanylua/glspi_app.c', 'geanylua/glspi_dlg.c',
						'geanylua/glspi_doc.c', 'geanylua/glspi_kfile.c', 'geanylua/glspi_run.c',
						'geanylua/glspi_sci.c', 'geanylua/gsdlg_lua.c' ]

		bld.new_task_gen(
			features		= 'cc cshlib',
			source			= lua_sources,
			includes		= p.includes,
			target			= 'libgeanylua',
			uselib			= libs,
			install_path	= '${G_PREFIX}/lib/geany-plugins/geanylua' if is_win32
				else '${LIBDIR}/geany-plugins/geanylua'
		)

		# install docs
		docdir = '${G_PREFIX}/doc/geanylua' if is_win32 else '${DOCDIR}/geanylua'
		bld.install_files(docdir, 'geanylua/docs/*.html')
		# install examples (Waf doesn't support installing files recursively, yet)
		datadir = '${G_PREFIX}/share/' if is_win32 else '${DATADIR}'
		bld.install_files('%s/geany-plugins/geanylua/dialogs' % datadir, 'geanylua/examples/dialogs/*.lua')
		bld.install_files('%s/geany-plugins/geanylua/edit' % datadir, 'geanylua/examples/edit/*.lua')
		bld.install_files('%s/geany-plugins/geanylua/info' % datadir, 'geanylua/examples/info/*.lua')
		bld.install_files('%s/geany-plugins/geanylua/scripting' % datadir, 'geanylua/examples/scripting/*.lua')
		bld.install_files('%s/geany-plugins/geanylua/work' % datadir, 'geanylua/examples/work/*.lua')


	def build_debug(bld, p, libs):
		bld.new_task_gen(
			features	= 'cc cprogram',
			source		= [ 'geanygdb/src/ttyhelper.c' ],
			includes	= p.includes,
			target		= 'ttyhelper',
			uselib		= libs,
			install_path = '${LIBDIR}/geany'
		)

	def install_docs(bld, pname, files):
		ext = '.txt' if is_win32 else ''
		docdir = '${G_PREFIX}/doc/%s' % pname if is_win32 else '${DOCDIR}/%s' % pname
		for file in files:
			if os.path.exists(os.path.join(p.name, file)):
				bld.install_as(
					'%s/%s%s' % (docdir, ucFirst(file, is_win32), ext),
					'%s/%s' % (pname, file))
		if pname == 'geanylatex':
			bld.install_files('%s' % docdir, 'geanylatex/doc/geanylatex.*')
			bld.install_files('%s/img' % docdir, 'geanylatex/doc/img/*.png')


	for p in plugins:
		if not p.name in bld.env['enabled_plugins']:
			continue

		libs = 'GTK GEANY' # common for all plugins
		for l in p.libs:   # add plugin specific libs
			libs += ' %s' % Utils.quote_define_name(l[0])

		if p.name == 'geanylua':
			build_lua(bld, p, libs) # build additional lib for the lua plugin

		if p.name == 'geanygdb':
			build_debug(bld, p, libs) # build additional binary for the debug plugin

		t = bld.new_task_gen(
			features		= 'cc cshlib',
			source			= p.sources,
			includes		= p.includes,
			target			= p.name,
			uselib			= libs,
			install_path	= '${G_PREFIX}/lib' if is_win32 else '${LIBDIR}/geany/'
		)
		if not p.sources:
			for dir in p.includes:
				t.find_sources_in_dirs(dir)

		install_docs(bld, p.name, 'AUTHORS ChangeLog COPYING NEWS README THANKS TODO'.split())


	if bld.env['INTLTOOL']:
			bld.new_task_gen(
				features	= 'intltool_po',
				podir		= 'po',
				appname		= APPNAME,
				install_path = '${G_PREFIX}/share/locale' if is_win32 else '${LOCALEDIR}'
			)


def init():
	if Options.options.list_plugins:
		Utils.pprint('GREEN', \
			'The following targets can be chosen with the --enable-plugins option:')
		for p in plugins:
			print p.name,
		Utils.pprint('GREEN', \
	'\nTo compile only "%(1)s" and "%(2)s", use "./waf configure --enable-plugins=%(1)s,%(2)s".' % \
			{ '1' : plugins[0].name, '2' : plugins[1].name } )
		sys.exit(0)


def shutdown():
	if Options.options.update_po:
		# the following code was taken from midori's WAF script, thanks
		potfile = '%s.pot' % (APPNAME)
		os.chdir('%s/po' % srcdir)
		try:
			try:
				size_old = os.stat(potfile).st_size
			except:
				size_old = 0
			Utils.exec_command('intltool-update --pot -g %s' % APPNAME)
			if os.stat(potfile).st_size != size_old:
				Utils.pprint('CYAN', 'Updated POT file.')
				launch('intltool-update -r -g %s' % APPNAME, 'Updating translations', 'CYAN')
			else:
				Utils.pprint('CYAN', 'POT file is up to date.')
		except:
			Utils.pprint('RED', 'Failed to generate pot file.')
		os.chdir('..')


# Simple function to execute a command and print its exit status
def launch(command, status, success_color='GREEN'):
	ret = 0
	Utils.pprint(success_color, status)
	try:
		ret = Utils.exec_command(command)
	except OSError, e:
		ret = 1
		print str(e), ":", command
	except:
		ret = 1

	if ret != 0:
		Utils.pprint('RED', status + ' failed')

	return ret


def print_message(conf, msg, result, color = 'GREEN'):
	conf.check_message_1(msg)
	conf.check_message_2(result, color)


def ucFirst(s, is_win32):
	if is_win32:
		return s.title()
	return s


def target_is_win32(env):
	if sys.platform == 'win32':
		return True
	if env and 'CC' in env:
		cc = env['CC']
		if not isinstance(cc, str):
			cc = ''.join(cc)
		return cc.find('mingw') != -1
	return False
