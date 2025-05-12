project(
  'geany-lineoperations',
  'c',
  license : 'GPL-2.0-or-later',
)

plugin_name = 'lineoperations'

subdir('po')
subdir('meson_config')

geany_dep = dependency('geany')

library(
  plugin_name,
  sources: [
    'src/lineoperations.c',
    'src/lo_fns.c',
    'src/lo_prefs.c',
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
