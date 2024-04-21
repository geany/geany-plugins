include $(top_srcdir)/build/vars.build.mk
plugin = lsp

geanyplugins_LTLIBRARIES = lsp.la

json_glib_srcs = \
	json-glib/json-array.c \
	json-glib/json-builder.c \
	json-glib/json-builder.h \
	json-glib/json-debug.c \
	json-glib/json-debug.h \
	json-glib/json-enum-types.c \
	json-glib/json-enum-types.h \
	json-glib/json-gboxed.c \
	json-glib/json-generator.c \
	json-glib/json-generator.h \
	json-glib/json-glib.h \
	json-glib/json-gobject.c \
	json-glib/json-gobject.h \
	json-glib/json-gobject-private.h \
	json-glib/json-gvariant.c \
	json-glib/json-gvariant.h \
	json-glib/json-node.c \
	json-glib/json-object.c \
	json-glib/json-parser.c \
	json-glib/json-parser.h \
	json-glib/json-path.c \
	json-glib/json-path.h \
	json-glib/json-reader.c \
	json-glib/json-reader.h \
	json-glib/json-scanner.c \
	json-glib/json-scanner.h \
	json-glib/json-serializable.c \
	json-glib/json-types.h \
	json-glib/json-types-private.h \
	json-glib/json-utils.c \
	json-glib/json-utils.h \
	json-glib/json-value.c \
	json-glib/json-version.h \
	json-glib/json-version-macros.h

jsonrpc_glib_srcs = \
	jsonrpc-glib/jsonrpc-client.c \
	jsonrpc-glib/jsonrpc-client.h \
	jsonrpc-glib/jsonrpc-glib.h \
	jsonrpc-glib/jsonrpc-input-stream.c \
	jsonrpc-glib/jsonrpc-input-stream.h \
	jsonrpc-glib/jsonrpc-input-stream-private.h \
	jsonrpc-glib/jsonrpc-marshalers.c \
	jsonrpc-glib/jsonrpc-marshalers.h \
	jsonrpc-glib/jsonrpc-message.c \
	jsonrpc-glib/jsonrpc-message.h \
	jsonrpc-glib/jsonrpc-output-stream.c \
	jsonrpc-glib/jsonrpc-output-stream.h \
	jsonrpc-glib/jsonrpc-server.c \
	jsonrpc-glib/jsonrpc-server.h \
	jsonrpc-glib/jsonrpc-version.h \
	jsonrpc-glib/jsonrpc-version-macros.h

lsp_la_SOURCES = \
	$(json_glib_srcs) \
	$(jsonrpc_glib_srcs) \
	lsp-autocomplete.c \
	lsp-autocomplete.h \
	lsp-code-lens.c \
	lsp-code-lens.h \
	lsp-command.c \
	lsp-command.h \
	lsp-diagnostics.c \
	lsp-diagnostics.h \
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
	lsp-server.c \
	lsp-server.h \
	lsp-signature.c \
	lsp-signature.h \
	lsp-symbol-kinds.c \
	lsp-symbol-kinds.h \
	lsp-symbol.c \
	lsp-symbol.h \
	lsp-symbols.c \
	lsp-symbols.h \
	lsp-sync.c \
	lsp-sync.h \
	lsp-utils.c \
	lsp-utils.h
    
lsp_la_CPPFLAGS = $(AM_CPPFLAGS) \
	-DG_LOG_DOMAIN=\"LSP\" \
	-DJSON_COMPILATION \
	-DJSONRPC_GLIB_COMPILATION \
	-I$(top_srcdir)/lsp/src/json-glib \
	-I$(top_srcdir)/lsp/src/jsonrpc-glib \
	-I$(top_srcdir)/lsp/src
lsp_la_CFLAGS = $(AM_CFLAGS)
lsp_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
