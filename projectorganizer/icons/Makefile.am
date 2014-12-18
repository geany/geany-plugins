icondir = $(datadir)/icons/hicolor/16x16/apps

dist_icon_DATA = \
	prjorg-file.png	\
	prjorg-header.png	\
	prjorg-source.png	\
	prjorg-expand.png	\
	prjorg-collapse.png	\
	prjorg-follow.png	\
	prjorg-add-external.png	\
	prjorg-refresh.png

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
