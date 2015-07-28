include build/vars.init.mk
include build/vars.build.mk

ACLOCAL_AMFLAGS = -I build/cache -I build -I build/bundled -I geanypy/m4 --install
AM_DISTCHECK_CONFIGURE_FLAGS = --with-geany-libdir='$${libdir}'

SUBDIRS = po

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

if ENABLE_DEVHELP
SUBDIRS += devhelp
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

if ENABLE_GEANYLATEX
SUBDIRS += geanylatex
endif

if ENABLE_GEANYLIPSUM
SUBDIRS += geanylipsum
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

include geanyprj/Makefile.rel.am
if ENABLE_GEANYPY
SUBDIRS += geanypy
endif

include geanysendmail/Makefile.rel.am
include geanyvc/Makefile.rel.am
include geniuspaste/Makefile.rel.am
if ENABLE_GITCHANGEBAR
SUBDIRS += git-changebar
endif

if ENABLE_MARKDOWN
SUBDIRS += markdown
endif

include multiterm/Makefile.rel.am
if ENABLE_OVERVIEW
SUBDIRS += overview
endif

if ENABLE_PAIRTAGHIGHLIGHTER
SUBDIRS += pairtaghighlighter
endif

if ENABLE_POHELPER
SUBDIRS += pohelper
endif

include pretty-printer/Makefile.rel.am
if ENABLE_PROJECTORGANIZER
SUBDIRS += projectorganizer
endif

if ENABLE_SCOPE
SUBDIRS += scope
endif

include shiftcolumn/Makefile.rel.am
include spellcheck/Makefile.rel.am
include tableconvert/Makefile.rel.am
include treebrowser/Makefile.rel.am
include updatechecker/Makefile.rel.am
include webhelper/Makefile.rel.am
include xmlsnippets/Makefile.rel.am

EXTRA_DIST += \
	build/__init__.py \
	build/wafcache.py \
	build/wafutils.py \
	waf \
	wscript \
	README.waf \
	README.windows
