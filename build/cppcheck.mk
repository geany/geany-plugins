
if HAVE_CPPCHECK

check-cppcheck: $(srcdir)
	$(CPPCHECK) \
		-q --template gcc --error-exitcode=2 \
		-I$(GEANY_INCLUDEDIR)/geany \
		$(AM_CPPCHECKFLAGS) $(CPPCHECKFLAGS) \
		$(srcdir)

check-local: check-cppcheck

endif
