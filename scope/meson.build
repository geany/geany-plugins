project(
  'geany-scope',
  'c',
  license : 'GPL-2.0-or-later',
)

plugin_name = 'scope'

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
    'src/break.c',
    'src/conterm.c',
    'src/debug.c',
    'src/inspect.c',
    'src/local.c',
    'src/memory.c',
    'src/menu.c',
    'src/parse.c',
    'src/plugme.c',
    'src/prefs.c',
    'src/program.c',
    'src/register.c',
    'src/scope.c',
    'src/stack.c',
    'src/thread.c',
    'src/tooltip.c',
    'src/utils.c',
    'src/views.c',
    'src/watch.c',
    'src/store/scptreedata.c',
    'src/store/scptreestore.c',
  ],
  dependencies: [config_dep, geany_dep, gp_utils_dep, vte_dep],
  name_prefix: '',
  install: true,
  install_dir: plugin_path,
)

install_data(
  sources: [
    'docs/codes.html',
    'docs/scope.html',
  ],
  install_dir: plugin_docdir / 'html',
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
