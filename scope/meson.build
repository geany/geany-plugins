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
    'AUTHORS',
    'COPYING',
    'ChangeLog',
    'NEWS',
    'README',
  ],
  install_dir: plugin_docdir,
)

install_data(
  sources: [
    'docs/codes.html',
    'docs/scope.html',
  ],
  install_dir: plugin_docdir / 'html',
)

install_data(
  sources: [
    'data/BreakPoint.png',
    'data/BreakPoint22.png',
    'data/Continue.png',
    'data/Continue22.png',
    'data/Continue24.png',
    'data/Kill.png',
    'data/Kill22.png',
    'data/Kill24.png',
    'data/Pause.png',
    'data/Pause22.png',
    'data/Pause24.png',
    'data/RunToCursor.png',
    'data/RunToCursor22.png',
    'data/RunToCursor24.png',
    'data/RunToMethod.png',
    'data/RunToMethod22.png',
    'data/RunToMethod24.png',
    'data/StepInto.png',
    'data/StepInto22.png',
    'data/StepInto24.png',
    'data/StepOut.png',
    'data/StepOut22.png',
    'data/StepOut24.png',
    'data/StepOver.png',
    'data/StepOver22.png',
    'data/StepOver24.png',
    'data/scope.glade',
    'data/scope_gtk3.glade',
  ],
  install_dir: plugin_libdir,
)
