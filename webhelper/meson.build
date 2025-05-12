project(
  'geany-webhelper',
  'c',
  license : 'GPL-3.0-or-later',
)

plugin_name = 'webhelper'

subdir('po')
subdir('meson_config')

cc = meson.get_compiler('c')
libm_dep = cc.find_library('m', required: true)

geany_dep = dependency('geany')

webkit2gtk_dep = dependency('webkit2gtk-4.1', required: false)
if not webkit2gtk_dep.found()
  webkit2gtk_dep = dependency('webkit2gtk-4.0')
endif

library(
  plugin_name,
  sources: [
    'src/gwh-browser.c',
    'src/gwh-enum-types.c',
    'src/gwh-keybindings.c',
    'src/gwh-plugin.c',
    'src/gwh-settings.c',
    'src/gwh-utils.c',
  ],
  dependencies: [config_dep, geany_dep, libm_dep, webkit2gtk_dep],
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
