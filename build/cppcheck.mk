
if HAVE_CPPCHECK

check-cppcheck: $(srcdir)
	$(CPPCHECK) \
		--inline-suppr \
		-q --template=gcc --error-exitcode=2 \
		--library=gtk \
		--library=$(top_srcdir)/build/cppcheck-geany-plugins.cfg \
		--suppressions-list=$(top_srcdir)/build/cppcheck-geany-plugins.suppressions \
		-I$(GEANY_INCLUDEDIR)/geany \
		-UGEANY_PRIVATE \
		-DGETTEXT_PACKAGE=\"$(GETTEXT_PACKAGE)\" \
		$(filter -j%,$(MAKEFLAGS)) \
		$(LOCAL_AM_CFLAGS) \
		$(AM_CPPCHECKFLAGS) $(CPPCHECKFLAGS) \
		$(srcdir)

check-local: check-cppcheck

endif
