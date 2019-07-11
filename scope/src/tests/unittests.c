#include <stdlib.h>
#include <stdio.h>
#include <check.h>

#include <gtk/gtk.h>
#include "geany.h"

TCase *utils_create_tests(void);

Suite *
my_suite(void)
{
	Suite *s = suite_create("Scope");
	suite_add_tcase(s, utils_create_tests());
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
