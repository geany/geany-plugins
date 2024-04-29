project(
  'geany-pohelper',
  'c',
  license : 'GPL-3.0-or-later',
)

plugin_name = 'pohelper'

subdir('po')
subdir('meson_config')

geany_dep = dependency('geany')

library(
  plugin_name,
  sources: [
    'src/gph-plugin.c',
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
