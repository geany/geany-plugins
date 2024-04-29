project(
  'geany-vimode',
  'c',
  license : 'GPL-2.0-or-later',
)

plugin_name = 'vimode'

subdir('po')
subdir('meson_config')

geany_dep = dependency('geany')

vimode_inc = include_directories('src')
library(
  plugin_name,
  sources: [
    'src/cmd-params.c',
    'src/cmd-runner.c',
    'src/excmd-prompt.c',
    'src/excmd-runner.c',
    'src/keypress.c',
    'src/sci.c',
    'src/utils.c',
    'src/vi.c',
    'src/cmds/changemode.c',
    'src/cmds/edit.c',
    'src/cmds/excmds.c',
    'src/cmds/motion-word.c',
    'src/cmds/motion.c',
    'src/cmds/special.c',
    'src/cmds/txtobjs.c',
  ],
  include_directories: vimode_inc,
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
