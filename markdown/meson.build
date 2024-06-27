project(
  'geany-markdown',
  'c',
  license : 'GPL-2.0-or-later',
)

plugin_name = 'markdown'

subdir('po')
subdir('meson_config')

geany_dep = dependency('geany')
glib_dep = dependency('glib-2.0')

webkit2gtk_dep = dependency('webkit2gtk-4.1', required: false)
if not webkit2gtk_dep.found()
  webkit2gtk_dep = dependency('webkit2gtk-4.0')
endif

discount = get_option('discount')
if not discount.disabled()
  markdown_dep = dependency('libmarkdown', required: discount)
else
  leg_exe = executable(
    'leg',
    sources: [
      'peg-markdown/peg-0.1.9/leg.c',
      'peg-markdown/peg-0.1.9/compile.c',
      'peg-markdown/peg-0.1.9/tree.c',
    ],
    install:false,
  )
  leg_exe_dep = declare_dependency(
    link_with: leg_exe,
  )

  peg_md_inc = include_directories('peg-markdown')

  peg_md_c = custom_target(
      'markdown_parser.c',
      output : 'markdown_parser.c',
      input : 'peg-markdown/markdown_parser.leg',
      command : [leg_exe, '-o', '@OUTPUT@', '@INPUT@'],
  )

  peg_md = static_library(
    'peg_markdown',
    sources: [
      peg_md_c,
      'peg-markdown/markdown_lib.c',
      'peg-markdown/markdown_output.c',
      'peg-markdown/odf.c',
      'peg-markdown/parsing_functions.c',
      'peg-markdown/utility_functions.c',
    ],
    dependencies: [glib_dep],
    include_directories: peg_md_inc,
    install: false,
  )

  markdown_dep = declare_dependency(
    compile_args: ['-DFULL_PRICE'],
    include_directories: peg_md_inc,
    link_with: peg_md,
  )
endif

library(
  plugin_name,
  sources: [
    'src/conf.c',
    'src/plugin.c',
    'src/viewer.c',
    'src/markdown-gtk-compat.c',
  ],
  dependencies: [config_dep, geany_dep, markdown_dep, webkit2gtk_dep],
  name_prefix: '',
  install: true,
  install_dir: plugin_path,
)

install_data(
  sources: [
    'docs/help.html',
    'docs/plugin.png',
    'docs/plugin_mgr.png',
    'docs/plugin_prefs.png',
    'docs/plugin_small.png',
    'docs/set_filetype.png',
    'docs/settings.png',
  ],
  install_dir: plugin_docdir / 'html',
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
