project(
  'geany-xmlsnippets',
  'c',
  license : 'GPL-2.0-or-later',
)

plugin_name = 'xmlsnippets'

subdir('po')
subdir('meson_config')

geany_dep = dependency('geany')

library(
  plugin_name,
  sources: [
    'src/plugin.c',
    'src/test-stubs.c',
    'src/tests.c',
    'src/xmlsnippets.c',
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
