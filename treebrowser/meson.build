project(
  'geany-treebrowser',
  'c',
  license : 'GPL-2.0-or-later',
)

plugin_name = 'treebrowser'

subdir('po')
subdir('meson_config')

geany_dep = dependency('geany')
vte_dep = dependency('vte-2.91')

gp_utils_dep = dependency(
  'gp_utils',
  fallback: ['utils', 'gp_utils_dep'],
  default_options: ['default_library=static'],
)

library(
  plugin_name,
  sources: [
    'src/treebrowser.c',
  ],
  dependencies: [config_dep, geany_dep, gp_utils_dep, vte_dep],
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
