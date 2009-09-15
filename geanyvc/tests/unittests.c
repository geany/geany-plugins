#include <stdlib.h>
#include <stdio.h>
#include <check.h>
#include <string.h>

#include <gtk/gtk.h>
#include "geany.h"

extern TCase *utils_test_case_create();

Suite *
my_suite(void)
{
	Suite *s = suite_create("VC");
	TCase *tc_utils = utils_test_case_create();
	suite_add_tcase(s, tc_utils);
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
