include $(top_srcdir)/build/vars.build.mk
plugin = lsp

geanyplugins_LTLIBRARIES = lsp.la

lsp_la_SOURCES = \
	spawn/lspunixinputstream.c \
	spawn/lspunixinputstream.h \
	spawn/lspunixoutputstream.c \
	spawn/lspunixoutputstream.h \
	spawn/spawn.c \
	spawn/spawn.h \
	lsp-autocomplete.c \
	lsp-autocomplete.h \
	lsp-code-lens.c \
	lsp-code-lens.h \
	lsp-command.c \
	lsp-command.h \
	lsp-diagnostics.c \
	lsp-diagnostics.h \
	lsp-extension.c \
	lsp-extension.h \
	lsp-format.c \
	lsp-format.h \
	lsp-goto-anywhere.c \
	lsp-goto-anywhere.h \
	lsp-goto.c \
	lsp-goto.h \
	lsp-goto-panel.c \
	lsp-goto-panel.h \
	lsp-highlight.c \
	lsp-highlight.h \
	lsp-hover.c \
	lsp-hover.h \
	lsp-log.c \
	lsp-log.h \
	lsp-main.c \
	lsp-progress.c \
	lsp-progress.h \
	lsp-rename.c \
	lsp-rename.h \
	lsp-rpc.c \
	lsp-rpc.h \
	lsp-semtokens.c \
	lsp-semtokens.h \
	lsp-selection-range.c \
	lsp-selection-range.h \
	lsp-server.c \
	lsp-server.h \
	lsp-signature.c \
	lsp-signature.h \
	lsp-symbol.c \
	lsp-symbol.h \
	lsp-symbols.c \
	lsp-symbols.h \
	lsp-symbol-kinds.c \
	lsp-symbol-kinds.h \
	lsp-symbol-tree.c \
	lsp-symbol-tree.h \
	lsp-sync.c \
	lsp-sync.h \
	lsp-utils.c \
	lsp-utils.h \
	lsp-workspace-folders.c \
	lsp-workspace-folders.h

lsp_la_CPPFLAGS = $(AM_CPPFLAGS) \
	-DG_LOG_DOMAIN=\"LSP\" \
	-I$(top_srcdir)/lsp/src
if ENABLE_BUILTIN_JSONRPC
lsp_la_CPPFLAGS += -I$(top_srcdir)/lsp/deps/jsonrpc-glib -I$(top_srcdir)/lsp/deps
endif

if ENABLE_BUILTIN_JSONRPC
lsp_la_CFLAGS = $(AM_CFLAGS)
else
lsp_la_CFLAGS = $(AM_CFLAGS) $(LSP_CFLAGS)
endif

lsp_la_LIBADD = $(COMMONLIBS)
if ENABLE_BUILTIN_JSONRPC
lsp_la_LIBADD += $(top_builddir)/lsp/deps/libjsonrpc.la
else
lsp_la_LIBADD += $(LSP_LIBS)
endif

include $(top_srcdir)/build/cppcheck.mk
