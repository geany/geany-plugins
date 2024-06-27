project(
  'geany-pretty-printer',
  'c',
  license : 'GPL-2.0-or-later',
)

plugin_name = 'pretty-printer'

subdir('po')
subdir('meson_config')

geany_dep = dependency('geany')
libxml2_dep = dependency('libxml-2.0')

glib_dep = declare_dependency(
  dependencies: dependency('glib-2.0'),
  compile_args: ['-DHAVE_GLIB'],
)

library(
  plugin_name,
  sources: [
    'src/ConfigUI.c',
    'src/PluginEntry.c',
    'src/PrettyPrinter.c',
  ],
  dependencies: [config_dep, geany_dep, glib_dep, libxml2_dep],
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
