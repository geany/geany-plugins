project(
  'geany-debugger',
  'c',
  license : 'GPL-2.0-or-later',
)

plugin_name = 'debugger'

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
    'src/atree.c',
    'src/bptree.c',
    'src/breakpoint.c',
    'src/breakpoints.c',
    'src/btnpanel.c',
    'src/callbacks.c',
    'src/calltip.c',
    'src/dbm_gdb.c',
    'src/dconfig.c',
    'src/debug.c',
    'src/debug_module.c',
    'src/dpaned.c',
    'src/envtree.c',
    'src/gdb_mi.c',
    'src/gui.c',
    'src/keys.c',
    'src/markers.c',
    'src/pixbuf.c',
    'src/plugin.c',
    'src/stree.c',
    'src/tabs.c',
    'src/tpage.c',
    'src/utils.c',
    'src/vtree.c',
    'src/watch_model.c',
    'src/wtree.c',
    'src/cell_renderers/cellrendererbreakicon.c',
    'src/cell_renderers/cellrendererframeicon.c',
    'src/cell_renderers/cellrenderertoggle.c',
  ],
  dependencies: [config_dep, geany_dep, gp_utils_dep, vte_dep],
  name_prefix: '',
  install: true,
  install_dir: plugin_path,
)

install_data(
  sources: [
    'img/continue.png',
    'img/restart.gif',
    'img/run.gif',
    'img/run_to_cursor.gif',
    'img/step_in.png',
    'img/step_out.gif',
    'img/step_over.gif',
    'img/stop.gif',
    'img/tabs.gif',
  ],
  install_dir: plugin_datadir,
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
