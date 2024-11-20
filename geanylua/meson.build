project(
  'geanylua',
  'c',
  license : 'GPL-2.0-or-later',
)

plugin_name = 'geanylua'

subdir('po')
subdir('meson_config')

geany_dep = dependency('geany')
lua_dep = dependency('lua51')

glib_dep = declare_dependency(
  dependencies: [dependency('glib-2.0'), dependency('gmodule-2.0')],
  compile_args: ['-DHAVE_GLIB'],
)

libgeanylua = library(
  'libgeanylua',
  sources: [
    'glspi_app.c',
    'glspi_dlg.c',
    'glspi_doc.c',
    'glspi_init.c',
    'glspi_kfile.c',
    'glspi_run.c',
    'glspi_sci.c',
    'gsdlg.c',
    'gsdlg_lua.c',
  ],
  dependencies: [config_dep, geany_dep, glib_dep, lua_dep],
  name_prefix: '',
  install: true,
  install_dir: plugin_libdir,
)
libgeanylua_dep = declare_dependency(
  link_with: [libgeanylua],
  compile_args: ['-DG_LOG_DOMAIN="GeanyLua"'],
)

library(
  plugin_name,
  sources: [
    'geanylua.c',
  ],
  dependencies: [config_dep, geany_dep, glib_dep, libgeanylua_dep, lua_dep],
  name_prefix: '',
  install: true,
  install_dir: plugin_path,
)

install_subdir(
  'examples',
  exclude_files: ['Makefile.am'],
  install_dir: plugin_datadir,
  strip_directory: true,
)

install_subdir(
  'docs',
  exclude_files: ['Makefile.am'],
  install_dir: plugin_docdir,
  strip_directory: true,
)

install_data(
  sources: [
    'COPYING',
    'ChangeLog',
    'NEWS',
    'README',
  ],
  install_dir: plugin_docdir,
)
