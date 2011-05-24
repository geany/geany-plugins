icondir = $(datadir)/icons/hicolor/16x16/apps

ICONS = \
	gproject-file.png	\
	gproject-header.png	\
	gproject-source.png	\
	gproject-expand.png	\
	gproject-collapse.png	\
	gproject-follow.png	\
	gproject-refresh.png

EXTRA_DIST = $(ICONS)

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor
install-data-hook: update-icon-cache
uninstall-hook: update-icon-cache
update-icon-cache:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
		$(gtk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated.  After (un)install, run this:"; \
		echo "***   $(gtk_update_icon_cache)"; \
	fi

if ENABLE_GPROJECT
icon_DATA = $(ICONS)
else
EXTRA_DATA = $(ICONS)
endif
