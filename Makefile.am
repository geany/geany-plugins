ACLOCAL_AMFLAGS = -I build/cache -I build -I build/bundled --install
AM_DISTCHECK_CONFIGURE_FLAGS = --with-geany-libdir='$${libdir}'

EXTRA_DIST = build/cppcheck-geany-plugins.cfg

SUBDIRS = po

if ENABLE_UTILSLIB
SUBDIRS += utils
endif

if ENABLE_ADDONS
SUBDIRS += addons
endif

if ENABLE_AUTOCLOSE
SUBDIRS += autoclose
endif

if ENABLE_AUTOMARK
SUBDIRS += automark
endif

if ENABLE_CODENAV
SUBDIRS += codenav
endif

if ENABLE_COMMANDER
SUBDIRS += commander
endif

if ENABLE_DEBUGGER
SUBDIRS += debugger
endif

if ENABLE_DEFINEFORMAT
SUBDIRS += defineformat
endif

if ENABLE_GEANYCTAGS
SUBDIRS += geanyctags
endif

if ENABLE_GEANYDOC
SUBDIRS += geanydoc
endif

if ENABLE_GEANYEXTRASEL
SUBDIRS += geanyextrasel
endif

if ENABLE_GEANYGENDOC
SUBDIRS += geanygendoc
endif

if ENABLE_GEANYINSERTNUM
SUBDIRS += geanyinsertnum
endif

if ENABLE_LATEX
SUBDIRS += latex
endif

if ENABLE_GEANYLUA
SUBDIRS += geanylua
endif

if ENABLE_GEANYMACRO
SUBDIRS += geanymacro
endif

if ENABLE_GEANYMINISCRIPT
SUBDIRS += geanyminiscript
endif

if ENABLE_GEANYNUMBEREDBOOKMARKS
SUBDIRS += geanynumberedbookmarks
endif

if ENABLE_GEANYPG
SUBDIRS += geanypg
endif

if ENABLE_GEANYPRJ
SUBDIRS += geanyprj
endif

if ENABLE_GEANYVC
SUBDIRS += geanyvc
endif

if ENABLE_GENIUSPASTE
SUBDIRS += geniuspaste
endif

if ENABLE_GITCHANGEBAR
SUBDIRS += git-changebar
endif

if ENABLE_KEYRECORD
SUBDIRS += keyrecord
endif

if ENABLE_LINEOPERATIONS
SUBDIRS += lineoperations
endif

if ENABLE_LIPSUM
SUBDIRS += lipsum
endif

if ENABLE_LSP
SUBDIRS += lsp
endif

if ENABLE_MARKDOWN
SUBDIRS += markdown
endif

if ENABLE_OVERVIEW
SUBDIRS += overview
endif

if ENABLE_PAIRTAGHIGHLIGHTER
SUBDIRS += pairtaghighlighter
endif

if ENABLE_POHELPER
SUBDIRS += pohelper
endif

if ENABLE_PRETTY_PRINTER
SUBDIRS += pretty-printer
endif

if ENABLE_PROJECTORGANIZER
SUBDIRS += projectorganizer
endif

if ENABLE_SCOPE
SUBDIRS += scope
endif

if ENABLE_SENDMAIL
SUBDIRS += sendmail
endif

if ENABLE_SHIFTCOLUMN
SUBDIRS += shiftcolumn
endif

if ENABLE_SPELLCHECK
SUBDIRS += spellcheck
endif

if ENABLE_TABLECONVERT
SUBDIRS += tableconvert
endif

if ENABLE_TREEBROWSER
SUBDIRS += treebrowser
endif

if ENABLE_UPDATECHECKER
SUBDIRS += updatechecker
endif

if ENABLE_VIMODE
SUBDIRS += vimode
endif

if ENABLE_WEBHELPER
SUBDIRS += webhelper
endif

if ENABLE_WORKBENCH
SUBDIRS += workbench
endif

if ENABLE_XMLSNIPPETS
SUBDIRS += xmlsnippets
endif
