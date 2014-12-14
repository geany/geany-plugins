icondir = $(datadir)/icons/hicolor/16x16/apps

dist_icon_DATA = \
	gproject-file.png	\
	gproject-header.png	\
	gproject-source.png	\
	gproject-expand.png	\
	gproject-collapse.png	\
	gproject-follow.png	\
	gproject-add-external.png	\
	gproject-refresh.png

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
