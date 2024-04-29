project(
  'geany-pairtaghighlighter',
  'c',
  license : 'BSD-2-Clause',
)

plugin_name = 'pairtaghighlighter'

subdir('po')
subdir('meson_config')

geany_dep = dependency('geany')

library(
  plugin_name,
  sources: [
    'src/pair_tag_highlighter.c',
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
