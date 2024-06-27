project(
  'geany-workbench',
  'c',
  license : 'GPL-2.0-or-later',
)

plugin_name = 'workbench'

subdir('po')
subdir('meson_config')

geany_dep = dependency('geany')
libgit2_dep = dependency('libgit2')
vte_dep = dependency('vte-2.91')

gp_utils_dep = dependency(
  'gp_utils',
  fallback: ['utils', 'gp_utils_dep'],
  default_options: ['default_library=static'],
)

wb_inc = include_directories('src')
library(
  plugin_name,
  sources: [
    'src/dialogs.c',
    'src/idle_queue.c',
    'src/menu.c',
    'src/plugin_main.c',
    'src/popup_menu.c',
    'src/search_projects.c',
    'src/sidebar.c',
    'src/tm_control.c',
    'src/utils.c',
    'src/wb_globals.c',
    'src/wb_monitor.c',
    'src/wb_project.c',
    'src/workbench.c',
  ],
  include_directories: wb_inc,
  dependencies: [config_dep, geany_dep, gp_utils_dep, libgit2_dep, vte_dep],
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
