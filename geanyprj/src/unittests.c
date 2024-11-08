#include <stdlib.h>
#include <stdio.h>
#include <check.h>
#include <string.h>

#include <gtk/gtk.h>
#include "geany.h"
#include "geanyprj.h"


void
file_setup(void)
{
	system("mkdir -p test_list_dir/nested/nested2");
	system("mkdir -p test_list_dir/nested/nested3");
	system("echo A > test_list_dir/A.txt");
	system("echo A > test_list_dir/B.txt");
	system("echo A > test_list_dir/nested/C.txt");
	system("echo A > test_list_dir/nested/nested2/D.txt");
}

void
file_teardown(void)
{
	system("rm -rf test_list_dir");
}

START_TEST(test_get_file_list)
{
	GSList *files = get_file_list("test_list_dir", NULL, NULL, NULL);
	fail_unless(g_slist_length(files) == 4, "expected %d, get %d", 4, g_slist_length(files));
}

END_TEST;

START_TEST(test_get_full_path)
{
	gchar *newpath = get_full_path("/home/yura/src/geanyprj/.geanyprj", "./abc.c");
	fail_unless(strcmp(newpath, "/home/yura/src/geanyprj/abc.c") == 0,
		    "expected %s, get %s", "/home/yura/src/geanyprj/abc.c", newpath);
	g_free(newpath);
}

END_TEST;

START_TEST(test_get_relative_path)
{
	gchar *newpath = get_relative_path("/home/yura/src/geanyprj/.geanyprj",
					   "/home/yura/src/geanyprj/src/a.c");
	fail_unless(strcmp(newpath, "src/a.c") == 0, "expected %s, get %s", "src/a.c", newpath);
	g_free(newpath);

	newpath = get_relative_path("/home/yura/src/geanyprj/.geanyprj", "/home/yura/src/geanyprj");
	fail_unless(strcmp(newpath, "./") == 0, "expected %s, get %s", "./", newpath);
	g_free(newpath);
}

END_TEST;

START_TEST(test_find_file_path)
{
	fail_unless(find_file_path("insane_dir", "insane_file!!!!!!!!!!!!!!!2") == NULL);
}

END_TEST;


START_TEST(test_normpath)
{
	gchar *newpath = normpath("/a/b");
	fail_unless(strcmp(newpath, "/a/b") == 0, "expected %s, get %s", "/a/b", newpath);
	g_free(newpath);

	newpath = normpath("./a/b");
	fail_unless(strcmp(newpath, "./a/b") == 0, "expected %s, get %s", "./a/b", newpath);
	g_free(newpath);

	newpath = normpath("/a/./b");
	fail_unless(strcmp(newpath, "/a/b") == 0, "expected %s, get %s", "/a/b", newpath);
	g_free(newpath);

	newpath = normpath("/a/.//b");
	fail_unless(strcmp(newpath, "/a/b") == 0, "expected %s, get %s", "/a/b", newpath);
	g_free(newpath);

	newpath = normpath("c:\\a\\.\\b");
	fail_unless(strcmp(newpath, "c:/a/b") == 0, "expected %s, get %s", "c:/a/b", newpath);
	g_free(newpath);

	newpath = normpath("/a/../b");
	fail_unless(strcmp(newpath, "/b") == 0, "expected %s, get %s", "/b", newpath);
	g_free(newpath);

	newpath = normpath("../a");
	fail_unless(strcmp(newpath, "../a") == 0, "expected %s, get %s", "../a", newpath);
	g_free(newpath);

	newpath = normpath("../a/..");
	fail_unless(strcmp(newpath, "..") == 0, "expected %s, get %s", "..", newpath);
	g_free(newpath);
}

END_TEST;

Suite *
my_suite(void)
{
	Suite *s = suite_create("Project");
	TCase *tc_core = tcase_create("utils");

	suite_add_tcase(s, tc_core);
	tcase_add_test(tc_core, test_get_full_path);
	tcase_add_test(tc_core, test_get_relative_path);
	tcase_add_test(tc_core, test_find_file_path);
	tcase_add_test(tc_core, test_normpath);

	TCase *tc_file = tcase_create("file_utils");
	suite_add_tcase(s, tc_file);
	tcase_add_test(tc_file, test_get_file_list);

	tcase_add_checked_fixture(tc_file, file_setup, file_teardown);
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
