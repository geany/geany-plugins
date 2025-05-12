project(
  'geany-plugins',
  'c',
  license : 'GPL-2.0-or-later',
  default_options : ['buildtype=release', 'prefix=/usr'],
  version : '2.0'
)

plugin_name = 'none'
subdir('meson_config')

install_data(
  sources: [
    'HACKING',
    'MAINTAINERS',
    'NEWS',
    'README',
  ],
  install_dir: project_docdir,
)

geany_dep = dependency('geany')
glib_dep = dependency('glib-2.0')

lua_dep = dependency('lua51', required: get_option('geanylua'))

ctpl_dep = dependency('ctpl', required: get_option('geanygendoc'))

gpgme_dep = dependency('gpgme', required: get_option('geanypg'))

libgit2_dep = dependency('libgit2',
  required: get_option('git-changebar').enabled()
    or get_option('workbench').enabled()
)

libsoup_dep = dependency('libsoup-2.4',
  required: get_option('geniuspaste').enabled()
    or get_option('updatechecker').enabled()
)

libxml2_dep = dependency('libxml-2.0', required: get_option('pretty-printer'))

vte_dep = dependency('vte-2.91',
  required: get_option('debugger').enabled()
    or get_option('geanynumberedbookmarks').enabled()
    or get_option('scope').enabled()
    or get_option('treebrowser').enabled()
    or get_option('workbench').enabled()
)

markdown_dep = dependency('libmarkdown', required: get_option('discount'))

webkit2gtk_dep = dependency('webkit2gtk-4.1', required: false)
if not webkit2gtk_dep.found()
  webkit2gtk_dep = dependency('webkit2gtk-4.0',
    required: get_option('markdown').enabled()
      or get_option('webhelper').enabled()
  )
endif

if not get_option('addons').disabled()
  subproject('addons', required: get_option('addons'))
endif

if not get_option('autoclose').disabled()
  subproject('autoclose', required: get_option('autoclose'))
endif

if not get_option('automark').disabled()
  subproject('automark', required: get_option('automark'))
endif

if not get_option('codenav').disabled()
  subproject('codenav', required: get_option('codenav'))
endif

if not get_option('commander').disabled()
  subproject('commander', required: get_option('commander'))
endif

if not get_option('debugger').disabled()
  subproject('debugger', required: get_option('debugger'))
endif

if not get_option('defineformat').disabled()
  subproject('defineformat', required: get_option('defineformat'))
endif

if not get_option('geanyctags').disabled()
  subproject('geanyctags', required: get_option('geanyctags'))
endif

if not get_option('geanydoc').disabled()
  subproject('geanydoc', required: get_option('geanydoc'))
endif

if not get_option('geanyextrasel').disabled()
  subproject('geanyextrasel', required: get_option('geanyextrasel'))
endif

if not get_option('geanygendoc').disabled()
  subproject('geanygendoc', required: get_option('geanygendoc'))
endif

if not get_option('geanyinsertnum').disabled()
  subproject('geanyinsertnum', required: get_option('geanyinsertnum'))
endif

if not get_option('geanylua').disabled()
  subproject('geanylua', required: get_option('geanylua'))
endif

if not get_option('geanymacro').disabled()
  subproject('geanymacro', required: get_option('geanymacro'))
endif

if not get_option('geanyminiscript').disabled()
  subproject('geanyminiscript', required: get_option('geanyminiscript'))
endif

if not get_option('geanynumberedbookmarks').disabled()
  subproject('geanynumberedbookmarks', required: get_option('geanynumberedbookmarks'))
endif

if not get_option('geanypg').disabled()
  subproject('geanypg', required: get_option('geanypg'))
endif

if not get_option('geanyprj').disabled()
  subproject('geanyprj', required: get_option('geanyprj'))
endif

if not get_option('geanyvc').disabled()
  subproject('geanyvc', required: get_option('geanyvc'))
endif

if not get_option('geniuspaste').disabled()
  subproject('geniuspaste', required: get_option('geniuspaste'))
endif

if not get_option('git-changebar').disabled()
  subproject('git-changebar', required: get_option('git-changebar'))
endif

if not get_option('keyrecord').disabled()
  subproject('keyrecord', required: get_option('keyrecord'))
endif

if not get_option('latex').disabled()
  subproject('latex', required: get_option('latex'))
endif

if not get_option('lineoperations').disabled()
  subproject('lineoperations', required: get_option('lineoperations'))
endif

if not get_option('lipsum').disabled()
  subproject('lipsum', required: get_option('lipsum'))
endif

if not get_option('markdown').disabled()
  subproject('markdown', required: get_option('markdown'))
endif

if not get_option('overview').disabled()
  subproject('overview', required: get_option('overview'))
endif

if not get_option('pairtaghighlighter').disabled()
  subproject('pairtaghighlighter', required: get_option('pairtaghighlighter'))
endif

if not get_option('pohelper').disabled()
  subproject('pohelper', required: get_option('pohelper'))
endif

if not get_option('pretty-printer').disabled()
  subproject('pretty-printer', required: get_option('pretty-printer'))
endif

if not get_option('projectorganizer').disabled()
  subproject('projectorganizer', required: get_option('projectorganizer'))
endif

if not get_option('scope').disabled()
  subproject('scope', required: get_option('scope'))
endif

if not get_option('sendmail').disabled()
  subproject('sendmail', required: get_option('sendmail'))
endif

if not get_option('shiftcolumn').disabled()
  subproject('shiftcolumn', required: get_option('shiftcolumn'))
endif

if not get_option('spellcheck').disabled()
  subproject('spellcheck', required: get_option('spellcheck'))
endif

if not get_option('tableconvert').disabled()
  subproject('tableconvert', required: get_option('tableconvert'))
endif

if not get_option('treebrowser').disabled()
  subproject('treebrowser', required: get_option('treebrowser'))
endif

if not get_option('updatechecker').disabled()
  subproject('updatechecker', required: get_option('updatechecker'))
endif

if not get_option('vimode').disabled()
  subproject('vimode', required: get_option('vimode'))
endif

if not get_option('webhelper').disabled()
  subproject('webhelper', required: get_option('webhelper'))
endif

if not get_option('workbench').disabled()
  subproject('workbench', required: get_option('workbench'))
endif

if not get_option('xmlsnippets').disabled()
  subproject('xmlsnippets', required: get_option('xmlsnippets'))
endif
