project(
  'geany-overview',
  'c',
  license : 'GPL-2.0-or-later',
)

plugin_name = 'overview'

subdir('po')
subdir('meson_config')

geany_dep = dependency('geany')

library(
  plugin_name,
  sources: [
    'overview/overviewcolor.c',
    'overview/overviewplugin.c',
    'overview/overviewprefs.c',
    'overview/overviewprefspanel.c',
    'overview/overviewscintilla.c',
    'overview/overviewui.c',
  ],
  dependencies: [config_dep, geany_dep],
  name_prefix: '',
  install: true,
  install_dir: plugin_path,
)

install_subdir(
  'data',
  exclude_files: ['Makefile.am'],
  install_dir: plugin_datadir,
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
