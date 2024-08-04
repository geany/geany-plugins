project(
  'geanyvc',
  'c',
  license : 'GPL-2.0-or-later',
)

plugin_name = 'geanyvc'

subdir('po')
subdir('meson_config')

geany_dep = dependency('geany')

library(
  plugin_name,
  sources: [
    'src/externdiff.c',
    'src/geanyvc.c',
    'src/utils.c',
    'src/vc_bzr.c',
    'src/vc_cvs.c',
    'src/vc_fossil.c',
    'src/vc_git.c',
    'src/vc_hg.c',
    'src/vc_svk.c',
    'src/vc_svn.c',
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

# TODO: add tests
# src/unittests.c
