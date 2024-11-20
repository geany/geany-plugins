project(
  'geanyprj',
  'c',
  license : 'GPL-3.0-or-later',
)

plugin_name = 'geanyprj'

subdir('po')
subdir('meson_config')

geany_dep = dependency('geany')

library(
  plugin_name,
  sources: [
    'src/geanyprj.c',
    'src/menu.c',
    'src/project.c',
    'src/sidebar.c',
    'src/utils.c',
    'src/xproject.c',
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

# TODO: add tests
# src/unittests.c
