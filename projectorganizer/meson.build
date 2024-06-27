project(
  'geany-projectorganizer',
  'c',
  license : 'GPL-2.0-or-later',
)

plugin_name = 'projectorganizer'

subdir('po')
subdir('meson_config')

geany_dep = dependency('geany')

library(
  plugin_name,
  sources: [
    'src/prjorg-main.c',
    'src/prjorg-menu.c',
    'src/prjorg-project.c',
    'src/prjorg-sidebar.c',
    'src/prjorg-utils.c',
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
