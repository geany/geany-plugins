project(
  'geany-updatechecker',
  'c',
  license : 'GPL-2.0-or-later',
)

plugin_name = 'updatechecker'

subdir('po')
subdir('meson_config')

geany_dep = dependency('geany')
libsoup_dep = dependency('libsoup-3.0')

library(
  plugin_name,
  sources: [
    'src/updatechecker.c',
  ],
  dependencies: [config_dep, geany_dep, libsoup_dep],
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
