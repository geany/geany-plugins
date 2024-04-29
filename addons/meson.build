project(
  'geany-addons',
  'c',
  license : 'GPL-2.0-or-later',
)

plugin_name = 'addons'

subdir('po')
subdir('meson_config')

geany_dep = dependency('geany')

library(
  plugin_name,
  sources: [
    'src/addons.c',
    'src/ao_blanklines.c',
    'src/ao_bookmarklist.c',
    'src/ao_colortip.c',
    'src/ao_copyfilepath.c',
    'src/ao_doclist.c',
    'src/ao_markword.c',
    'src/ao_openuri.c',
    'src/ao_systray.c',
    'src/ao_tasks.c',
    'src/ao_wrapwords.c',
    'src/ao_xmltagging.c',
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
