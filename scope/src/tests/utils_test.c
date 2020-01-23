#include <check.h>
#include "../src/common.h"
#include "../src/utils.h"

typedef struct S_EXPR_TEST_ITEM
{
	const gchar *input;
	guint peek_index;
	const gchar *expected;
}EXPR_TEST_ITEM;

EXPR_TEST_ITEM expr_test_items [] =
{
	{ " variable ",         sizeof(" var")-1,            "variable"           },
	{ " (variable) ",       sizeof(" (var")-1,           "variable"           },
	{ "struct.item",        sizeof("str")-1,             "struct"             },
	{ "struct.item",        sizeof("str")-1,             "struct"             },
	{ "struct.item",        sizeof("struct.it")-1,       "struct.item"        },
	{ "struct->item",       sizeof("str")-1,             "struct"             },
	{ "struct->item",       sizeof("struct->it")-1,      "struct->item"       },
	{ "&(struct->item)",    sizeof("&(str")-1,           "struct"             },
	{ "&(struct->item)",    sizeof("&(struct->it")-1,    "struct->item"       },
	{ "foobar(item)",       sizeof("foobar(it")-1,       "item"               },
	{ "sizeof(item)",       sizeof("sizeof(it")-1,       "sizeof(item)"       },
	{ "array[5]",           sizeof("arr")-1,             "array"              },
	{ "array[5]",           sizeof("array[")-1,          "array[5]"           },
	{ "*pointer",           sizeof("*poi")-1,            "pointer"            },
	{ "*pointer",           0,                           "*pointer"           },
	{ "&variable",          sizeof("&var")-1,            "variable"           },
	{ "&variable",          0,                           "&variable"          },
	{ "foo variable",       sizeof("foo var")-1,         "variable"           },
	{ "variable foo",       sizeof("var")-1,             "variable"           },
	{ "int var_a, var_b;",  sizeof("int var")-1,         "var_a"              },
	{ "int var_a, var_b;",  sizeof("int var_a, va")-1,   "var_b"              },
	{ "foo(var_a, var_b);", sizeof("foo(var")-1,         "var_a"              },
	{ "foo(var_a, var_b);", sizeof("foo(var_a, va")-1,   "var_b"              },
	{ "array[index].item",  sizeof("array[index].i")-1,  "array[index].item"  },
	{ "array[index]->item", sizeof("array[index]->i")-1, "array[index]->item" },
	{ NULL,                 0,                         NULL           },
};

START_TEST(test_parser_for_evaluate)
{
	EXPR_TEST_ITEM *test_item;
	guint index = 0;
	gchar *expr, *chunk;

	test_item = &(expr_test_items[index]);
	while (test_item->input != NULL)
	{
		chunk = g_strdup(test_item->input);
		expr = utils_evaluate_expr_from_string (chunk, test_item->peek_index);
		ck_assert_ptr_ne(expr, NULL);
		if (g_strcmp0(expr, test_item->expected) != 0)
		{
			ck_abort_msg("Index #%lu: Input: '%s', Peek-Ind.: %lu, Result: '%s' != '%s' (expected)",
				index, test_item->input, test_item->peek_index, expr, test_item->expected);
		}
		g_free(chunk);
		g_free(expr);

		index++;
		test_item = &(expr_test_items[index]);
	}
}
END_TEST;

TCase *utils_create_tests(void)
{
	TCase *tc_utils = tcase_create("utils");
	tcase_add_test(tc_utils, test_parser_for_evaluate);
	return tc_utils;
}
