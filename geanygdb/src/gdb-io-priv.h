
/*
 * gdb-io-priv.h - private header for GDB wrapper library.
 *
 * See the file "gdb-io.h" for license information.
 *
 */


#include "gdb-lex.h"
#include "gdb-io.h"


void gdbio_free_var_list(GSList * args);


typedef void (*ResponseHandler) (gint seq, gchar ** lines, gchar * resp);


/*
  Sends a command to GDB, and returns the results to the specified
  ResponseHandler function.
*/
gint gdbio_send_seq_cmd(ResponseHandler func, const gchar * fmt, ...);


/* Look up a handler function */
ResponseHandler gdbio_seq_lookup(gint seq);


/*
  gdbio_pop_seq() removes a handler function from the sequencer.
  This should (almost) always be called from the associated
  ResponseHandler function, to avoid filling up the sequencer
  with stale commands.
*/
void gdbio_pop_seq(gint seq);


/*
  Parses the output of GDB and returns it as a hash table,
  unless the response is an error message, it calls the
  error handler function and then returns NULL.
*/
GHashTable *gdbio_get_results(gchar * resp, gchar ** list);

/*
Preprocessor sugar for declaring C variables from hash key names, e.g.
  HSTR(myhash,somevar)
expands to:
  gchar *somevar = gdblx_lookup_string ( myhash, "somevar" );
*/
#define HSTR(hash,token) gchar* token = gdblx_lookup_string(hash, #token"")
#define HTAB(hash,token) GHashTable* token = gdblx_lookup_hash(hash, #token"")
#define HLST(hash,token) GSList* token = gdblx_lookup_list(hash, #token"")


#if 0
#define do_loop()  \
  while (g_main_context_pending(NULL)) \
    g_main_context_iteration(NULL,FALSE);
#else
#define do_loop() g_main_context_iteration(NULL,FALSE);
#endif




void gdbio_info_func(gchar * fmt, ...);
void gdbio_error_func(gchar * fmt, ...);
void gdbio_do_status(GdbStatus s);


void gdbio_target_exited(gchar * reason);
void gdbio_set_target_pid(GPid pid);
GPid gdbio_get_target_pid();
void gdbio_set_running(gboolean running);

/*
 Max/Min values for sequencer tokens.
The current values of 100000-999999 allow for 899999 pending commands.
I can't imagine why you would need more, but if you change this,keep in mind
that the number of digits for any possible value *must* be exactly SEQ_LEN.
*/

#define SEQ_MIN 100000
#define SEQ_MAX 999999
#define SEQ_LEN 6



void gdbio_consume_response(GString * recv_buf);

void gdbio_set_starting(gboolean s);
void gdbio_target_started(gint seq, gchar ** list, gchar * resp);
