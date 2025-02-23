project(
  'geany-utils',
  'c',
  license : 'GPL-2.0-or-later',
)

plugin_name = 'utils'

subdir('meson_config')

geany_dep = dependency('geany')
glib_dep = dependency('glib-2.0')
vte_dep = dependency('vte-2.91')

gp_utils_inc = include_directories('src')
gp_utils = library(
  plugin_name,
  sources: [
    'src/filelist.c',
    'src/gp_vtecompat.c',
  ],
  include_directories: gp_utils_inc,
  dependencies: [config_dep, geany_dep, vte_dep],
  name_prefix: '',
  install: false
)
gp_utils_dep = declare_dependency(
  include_directories: gp_utils_inc,
  link_with: [gp_utils],
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
