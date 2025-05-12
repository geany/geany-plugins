project(
  'geanydoc',
  'c',
  license : 'GPL-3.0-or-later',
)

plugin_name = 'geanydoc'

subdir('po')
subdir('meson_config')

geany_dep = dependency('geany')

library(
  plugin_name,
  sources: [
    'src/config.c',
    'src/geanydoc.c',
  ],
  dependencies: [config_dep, geany_dep],
  name_prefix: '',
  install: true,
  install_dir: plugin_path,
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
