
helpdir = $(docdir)/markdown/html
dist_help_DATA = \
	help.html \
	plugin.png \
	plugin_mgr.png \
	plugin_prefs.png \
	plugin_small.png \
	set_filetype.png \
	settings.png

# To update the HTML help file
help.html: $(top_srcdir)/markdown/README
	rst2html $< > $@
