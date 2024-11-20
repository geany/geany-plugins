project(
  'geany-codenav',
  'c',
  license : 'GPL-2.0-or-later',
)

plugin_name = 'codenav'

subdir('po')
subdir('meson_config')

geany_dep = dependency('geany')

library(
  plugin_name,
  sources: [
    'src/codenavigation.c',
    'src/goto_file.c',
    'src/switch_head_impl.c',
    'src/utils.c',
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
