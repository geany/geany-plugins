project(
  'geany-spellcheck',
  'c',
  license : 'GPL-2.0-or-later',
)

plugin_name = 'spellcheck'

subdir('po')
subdir('meson_config')

geany_dep = dependency('geany')
enchant = dependency(
  'enchant-2',
  version: '>=2.0',
  required: false,
)

if enchant.found()
  enchant_dep = declare_dependency(
    compile_args: ['-DHAVE_ENCHANT_1_5', '-DHAVE_ENCHANT_2_0'],
    dependencies: [enchant],
  )
else
  enchant = dependency(
    'enchant',
    version: '>=1.5',
  )
  enchant_dep = declare_dependency(
    compile_args: ['-DHAVE_ENCHANT_1_5'],
    dependencies: [enchant],
  )
endif

library(
  plugin_name,
  sources: [
    'src/gui.c',
    'src/scplugin.c',
    'src/speller.c',
  ],
  dependencies: [config_dep, enchant_dep, geany_dep],
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
