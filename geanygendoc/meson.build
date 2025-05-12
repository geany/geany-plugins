project(
  'geany-gendoc',
  'c',
  license : 'GPL-3.0-or-later',
)

plugin_name = 'gendoc'

subdir('po')
subdir('meson_config')

geany_dep = dependency('geany')
ctpl_dep = dependency('ctpl')

library(
  plugin_name,
  sources: [
    'src/ggd-doc-setting.c',
    'src/ggd-doc-type.c',
    'src/ggd-file-type-loader.c',
    'src/ggd-file-type-manager.c',
    'src/ggd-file-type.c',
    'src/ggd-options.c',
    'src/ggd-plugin.c',
    'src/ggd-tag-utils.c',
    'src/ggd-utils.c',
    'src/ggd-widget-doctype-selector.c',
    'src/ggd-widget-frame.c',
    'src/ggd.c',
  ],
  dependencies: [config_dep, ctpl_dep, geany_dep],
  name_prefix: '',
  install: true,
  install_dir: plugin_path,
)

install_subdir(
  'data',
  exclude_files: ['Makefile.am'],
  install_dir: plugin_datadir,
)

install_data(
  sources: [
    'docs/manual.rst',
  ],
  install_dir: plugin_docdir,
)

install_data(
  sources: [
    'docs/manual.html',
  ],
  install_dir: plugin_docdir / 'html',
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
