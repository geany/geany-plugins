
if HAVE_CPPCHECK

check-cppcheck: $(srcdir)
	$(CPPCHECK) \
		-q --template=gcc --error-exitcode=2 \
		--library=$(top_srcdir)/build/cppcheck-geany-plugins.cfg \
		-I$(GEANY_INCLUDEDIR)/geany \
		-UGEANY_PRIVATE \
		-DGETTEXT_PACKAGE=\"$(GETTEXT_PACKAGE)\" \
		$(filter -j%,$(MAKEFLAGS)) \
		$(AM_CPPCHECKFLAGS) $(CPPCHECKFLAGS) \
		$(srcdir)

check-local: check-cppcheck

endif
