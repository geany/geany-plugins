
/*
 * gdb-lex.c - A GLib-based parser for GNU debugger machine interface output.
 *
 * See the file "gdb-lex.h" for license information.
 *
 */

#include <string.h>
#include <glib.h>
#include "gdb-lex.h"


static void
free_value(GdbLxValue * v)
{
	if (v)
	{
		switch (v->type)
		{
			case vt_STRING:
				{
					g_free(v->string);
					break;
				}
			case vt_HASH:
				{
					g_hash_table_destroy(v->hash);
					break;
				}
			case vt_LIST:
				{
					GSList *p;
					for (p = v->list; p; p = p->next)
					{
						free_value(p->data);
					}
					g_slist_free(v->list);
					break;
				}
		}
	}
}


static GdbLxValue *
new_value(GdbLxValueType type, gpointer data)
{
	GdbLxValue *v = g_new0(GdbLxValue, 1);
	v->type = type;
	v->data = data;
	return v;
}


static void
scan_error(GScanner * scanner, gchar * message, gboolean error)
{
	g_printerr("\n%s\n", message);
}




#define ID_NTH G_CSET_a_2_z G_CSET_A_2_Z G_CSET_DIGITS "_-"
static GScanner *
init_scanner()
{
	GScanner *scanner = g_scanner_new(NULL);
	scanner->msg_handler = scan_error;
	scanner->config->cset_identifier_nth = ID_NTH;
	return scanner;
}


#define new_hash() g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)free_value)
#define curr ((GdbLxValue*)(g_queue_peek_head(queue)))



static void
add_node(GScanner * scanner, gchar ** key, GdbLxValueType type, gpointer data, GQueue * queue)
{
	GdbLxValue *v = new_value(type, data);
	switch (curr->type)
	{
		case vt_STRING:
			{
				g_scanner_error(scanner, "***** queue head is a string\n");
				break;
			}
		case vt_HASH:
			{
				if (*key)
				{
					g_hash_table_insert(curr->hash, *key, v);
				}
				else
				{
					g_scanner_error(scanner, "***** no key for hash\n");
				}
				break;
			}
		case vt_LIST:
			{
				curr->list = g_slist_append(curr->list, v);
				break;
			}
	}
	*key = NULL;
	if (type != vt_STRING)
	{
		g_queue_push_head(queue, v);
	}
}


static GScanner *scanner = NULL;

GHashTable *
gdblx_parse_results(gchar * results)
{
	gchar *key = NULL;
	gboolean equals = FALSE;
	GHashTable *rv = new_hash();
	GdbLxValue *top = new_value(vt_HASH, rv);
	GQueue *queue = g_queue_new();
	GTokenType tt;
	g_queue_push_head(queue, top);
	if (!scanner)
	{
		scanner = init_scanner();
	}
	g_scanner_input_text(scanner, results, strlen(results));
	do
	{
		tt = g_scanner_get_next_token(scanner);
		switch (tt)
		{
			case G_TOKEN_LEFT_CURLY:
				{
					add_node(scanner, &key, vt_HASH, new_hash(), queue);
					break;
				}
			case G_TOKEN_RIGHT_CURLY:
				{
					g_queue_pop_head(queue);
					break;
				}
			case G_TOKEN_LEFT_BRACE:
				{
					add_node(scanner, &key, vt_LIST, NULL, queue);
					break;
				}
			case G_TOKEN_RIGHT_BRACE:
				{
					g_queue_pop_head(queue);
					break;
				}
			case G_TOKEN_STRING:
				{
					add_node(scanner, &key, vt_STRING,
						 g_strdup(scanner->value.v_string), queue);
					break;
				}

			case G_TOKEN_IDENTIFIER:
				{
					if (g_scanner_peek_next_token(scanner) ==
					    G_TOKEN_EQUAL_SIGN)
					{
						gchar *p;
						if (key)
						{
							g_scanner_error(scanner,
									"multiple keys: found %s and %s\n",
									key,
									scanner->value.
									v_identifier);
							g_free(key);
						}
						key = g_strdup(scanner->value.v_identifier);
						for (p = key; *p; p++)
						{
							if (*p == '-')
							{
								*p = '_';
							}
						}
					}
					break;
				}
			default:
				{
				}
				equals = (tt == G_TOKEN_EQUAL_SIGN);
		}
	}
	while ((tt != G_TOKEN_EOF) && (tt != G_TOKEN_ERROR));
	g_queue_free(queue);
	return rv;
}


void
gdblx_scanner_done()
{
	if (scanner)
	{
		g_scanner_destroy(scanner);
		scanner = NULL;
	}
}


/* don't print the newline until after the rval of the equal sign is printed. */
static gboolean dump_value_rval_pending = FALSE;

#define indent(s) if (dump_value_rval_pending) \
g_printerr("%s\n", s); \
else g_printerr("%*c%s\n", depth,  ' ', s); \
  dump_value_rval_pending = FALSE;


static void dump_rec(GHashTable * h, gint depth);


static void dump_list_cb(gpointer data, gpointer user_data);

static void
dump_value(GdbLxValue * v, gint depth)
{
	switch (v->type)
	{
		case vt_STRING:
			{
				indent(v->string);
				break;
			}
		case vt_HASH:
			{
				indent("{");
				dump_rec(v->hash, depth);
				indent("}");
				break;
			}
		case vt_LIST:
			{
				indent("[");
				g_slist_foreach(v->list, dump_list_cb, GINT_TO_POINTER(depth + 1));
				indent("]");
				break;
			}
	}
}



static void
dump_list_cb(gpointer data, gpointer user_data)
{
	dump_value(data, GPOINTER_TO_INT(user_data));
}



static void
dump_rec_cb(gpointer key, gpointer value, gpointer user_data)
{
	g_printerr("%*c%s=", GPOINTER_TO_INT(user_data), ' ', (gchar *) key);
	dump_value_rval_pending = TRUE;
	dump_value(value, GPOINTER_TO_INT(user_data));
}



static void
dump_rec(GHashTable * h, gint depth)
{
	if (h)
	{
		g_hash_table_foreach(h, dump_rec_cb, GINT_TO_POINTER(depth + 1));
	}
	else
	{
		g_printerr("(null hash table)\n");
	}
}



void
gdblx_dump_table(GHashTable * hash)
{
	dump_rec(hash, 0);
}




static GdbLxValue *
find_value(GHashTable * hash, gchar * key, GdbLxValueType type)
{
	GdbLxValue *v = hash ? g_hash_table_lookup(hash, key) : NULL;
	return v && v->type == type ? v : NULL;
}

gchar *
gdblx_lookup_string(GHashTable * hash, gchar * key)
{
	GdbLxValue *v = find_value(hash, key, vt_STRING);
	return v ? v->string : NULL;
}

GHashTable *
gdblx_lookup_hash(GHashTable * hash, gchar * key)
{
	GdbLxValue *v = find_value(hash, key, vt_HASH);
	return v ? v->hash : NULL;
}


GSList *
gdblx_lookup_list(GHashTable * hash, gchar * key)
{
	GdbLxValue *v = find_value(hash, key, vt_LIST);
	return v ? v->list : NULL;
}


/* True if key exists, it's a string, and its value matches 'expected' */
gboolean
gdblx_check_keyval(GHashTable * hash, gchar * key, gchar * expected)
{
	gchar *value = gdblx_lookup_string(hash, key);
	return value && g_str_equal(value, expected);
}
