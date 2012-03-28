
#ifndef ___dh_marshal_MARSHAL_H__
#define ___dh_marshal_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* VOID:BOOLEAN (dh-marshal.list:1) */
#define _dh_marshal_VOID__BOOLEAN	g_cclosure_marshal_VOID__BOOLEAN

/* VOID:POINTER (dh-marshal.list:2) */
#define _dh_marshal_VOID__POINTER	g_cclosure_marshal_VOID__POINTER

/* VOID:STRING (dh-marshal.list:3) */
#define _dh_marshal_VOID__STRING	g_cclosure_marshal_VOID__STRING

/* VOID:VOID (dh-marshal.list:4) */
#define _dh_marshal_VOID__VOID	g_cclosure_marshal_VOID__VOID

/* BOOLEAN:STRING (dh-marshal.list:5) */
extern void _dh_marshal_BOOLEAN__STRING (GClosure     *closure,
                                         GValue       *return_value,
                                         guint         n_param_values,
                                         const GValue *param_values,
                                         gpointer      invocation_hint,
                                         gpointer      marshal_data);

/* VOID:STRING,FLAGS (dh-marshal.list:6) */
extern void _dh_marshal_VOID__STRING_FLAGS (GClosure     *closure,
                                            GValue       *return_value,
                                            guint         n_param_values,
                                            const GValue *param_values,
                                            gpointer      invocation_hint,
                                            gpointer      marshal_data);

/* VOID:BOOLEAN (dh-marshal.list:1) */

/* VOID:POINTER (dh-marshal.list:2) */

/* VOID:STRING (dh-marshal.list:3) */

/* VOID:VOID (dh-marshal.list:4) */

/* BOOLEAN:STRING (dh-marshal.list:5) */

/* VOID:STRING,FLAGS (dh-marshal.list:6) */

G_END_DECLS

#endif /* ___dh_marshal_MARSHAL_H__ */

