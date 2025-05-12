project(
  'geanypg',
  'c',
  license : 'GPL-2.0-or-later',
)

plugin_name = 'geanypg'

subdir('po')
subdir('meson_config')

geany_dep = dependency('geany')
gpgme_dep = dependency('gpgme')

library(
  plugin_name,
  sources: [
    'src/decrypt_cb.c',
    'src/encrypt_cb.c',
    'src/geanypg.c',
    'src/helper_functions.c',
    'src/key_selection_dialog.c',
    'src/pinentry.c',
    'src/sign_cb.c',
    'src/verify_aux.c',
    'src/verify_cb.c',
  ],
  dependencies: [config_dep, geany_dep, gpgme_dep],
  name_prefix: '',
  install: true,
  install_dir: plugin_path,
)

install_data(
  sources: [
    'doc/geanypg.html',
  ],
  install_dir: plugin_docdir,
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
