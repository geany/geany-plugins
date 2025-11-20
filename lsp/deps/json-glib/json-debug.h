#ifndef __JSON_DEBUG_H__
#define __JSON_DEBUG_H__

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
  JSON_DEBUG_PARSER  = 1 << 0,
  JSON_DEBUG_GOBJECT = 1 << 1,
  JSON_DEBUG_PATH    = 1 << 2,
  JSON_DEBUG_NODE    = 1 << 3
} JsonDebugFlags;

#define JSON_HAS_DEBUG(flag)    (json_get_debug_flags () & JSON_DEBUG_##flag)

#ifdef JSON_ENABLE_DEBUG

# ifdef __GNUC__

# define JSON_NOTE(type,x,a...)                 G_STMT_START {  \
        if (JSON_HAS_DEBUG (type)) {                            \
          g_message ("[" #type "] " G_STRLOC ": " x, ##a);      \
        }                                       } G_STMT_END

# else
/* Try the C99 version; unfortunately, this does not allow us to pass
 * empty arguments to the macro, which means we have to
 * do an intemediate printf.
 */
# define JSON_NOTE(type,...)                    G_STMT_START {  \
        if (JSON_HAS_DEBUG (type)) {                            \
            gchar * _fmt = g_strdup_printf (__VA_ARGS__);       \
            g_message ("[" #type "] " G_STRLOC ": %s",_fmt);    \
            g_free (_fmt);                                      \
        }                                       } G_STMT_END

# endif /* __GNUC__ */

#else

#define JSON_NOTE(type,...)         G_STMT_START { } G_STMT_END

#endif /* JSON_ENABLE_DEBUG */

G_GNUC_INTERNAL
JsonDebugFlags json_get_debug_flags (void);

G_END_DECLS

#endif /* __JSON_DEBUG_H__ */
