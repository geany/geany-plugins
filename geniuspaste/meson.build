project(
  'geany-geniuspaste',
  'c',
  license : 'GPL-2.0-or-later',
)

plugin_name = 'geniuspaste'

subdir('po')
subdir('meson_config')

geany_dep = dependency('geany')
libsoup_dep = dependency('libsoup-3.0')

library(
  plugin_name,
  sources: [
    'src/geniuspaste.c',
  ],
  dependencies: [config_dep, geany_dep, libsoup_dep],
  name_prefix: '',
  install: true,
  install_dir: plugin_path,
)

install_data(
  sources: [
    'data/codepad.org.conf',
    'data/dpaste.de.conf',
    'data/fpaste.org.conf',
    'data/paste.debian.net.conf',
    'data/pastebin.geany.org.conf',
    'data/sprunge.us.conf',
  ],
  install_dir: plugin_datadir / 'pastebins',
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
