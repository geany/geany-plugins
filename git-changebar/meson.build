project(
  'geany-git-changebar',
  'c',
  license : 'GPL-3.0-or-later',
)

plugin_name = 'git-changebar'

subdir('po')
subdir('meson_config')

geany_dep = dependency('geany')
libgit2_dep = dependency('libgit2')

library(
  plugin_name,
  sources: [
    'src/gcb-plugin.c',
  ],
  dependencies: [config_dep, geany_dep, libgit2_dep],
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
