project(
  'geany-latex',
  'c',
  license : 'GPL-2.0-or-later',
)

plugin_name = 'latex'

subdir('po')
subdir('meson_config')

geany_dep = dependency('geany')

library(
  plugin_name,
  sources: [
    'src/bibtex.c',
    'src/bibtexlabels.c',
    'src/formatpatterns.c',
    'src/formatutils.c',
    'src/latex.c',
    'src/latexencodings.c',
    'src/latexenvironments.c',
    'src/latexkeybindings.c',
    'src/latexstructure.c',
    'src/latexutils.c',
    'src/letters.c',
    'src/reftex.c',
    'src/templates.c',
  ],
  dependencies: [config_dep, geany_dep],
  name_prefix: '',
  install: true,
  install_dir: plugin_path,
)

install_data(
  sources: [
    'doc/img/bibtex_reference.png',
    'doc/img/insert_environment.png',
    'doc/img/insert_label.png',
    'doc/img/insert_reference.png',
    'doc/img/insert_usepackage.png',
    'doc/img/latexwizard.png',
    'doc/img/plugin_manager.png',
    'doc/img/toolbar.png',
    'doc/latex.css',
    'doc/latex.html',
    'doc/latex.pdf',
    'doc/latex.tex',
    'doc/latex2.html',
    'doc/latex3.html',
    'doc/latex4.html',
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
