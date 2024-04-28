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

install_data(
  sources: [
    'data/prefs.ui',
  ],
  install_dir: plugin_datadir,
)

install_data(
  sources: [
    'AUTHORS',
    'COPYING',
    'ChangeLog',
    'NEWS',
    'README',
  ],
  install_dir: plugin_docdir,
)
