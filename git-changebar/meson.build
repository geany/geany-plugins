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
