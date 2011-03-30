
if HAVE_CPPCHECK

check-cppcheck: $(srcdir)
	$(CPPCHECK) -q --template gcc --error-exitcode=2 $^

check-local: check-cppcheck

endif
