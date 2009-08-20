#include <stdlib.h>
#include <stdio.h>
#include <check.h>

#include <gtk/gtk.h>
#include "geany.h"


Suite *
my_suite(void)
{
	Suite *s = suite_create("GeanyDoc");
	return s;
}

int
main(void)
{
	int nf;
	Suite *s = my_suite();
	SRunner *sr = srunner_create(s);
	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
