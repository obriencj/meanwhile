
#ifndef __mw_marshal_MARSHAL_H__
#define __mw_marshal_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* VOID:VOID (marshal.list:5) */
#define mw_marshal_VOID__VOID	g_cclosure_marshal_VOID__VOID

/* VOID:BOOLEAN (marshal.list:6) */
#define mw_marshal_VOID__BOOLEAN	g_cclosure_marshal_VOID__BOOLEAN

/* VOID:BOOLEAN,POINTER,POINTER (marshal.list:7) */
extern void mw_marshal_VOID__BOOLEAN_POINTER_POINTER (GClosure     *closure,
                                                      GValue       *return_value,
                                                      guint         n_param_values,
                                                      const GValue *param_values,
                                                      gpointer      invocation_hint,
                                                      gpointer      marshal_data);

/* VOID:INT (marshal.list:8) */
#define mw_marshal_VOID__INT	g_cclosure_marshal_VOID__INT

/* VOID:UINT (marshal.list:9) */
#define mw_marshal_VOID__UINT	g_cclosure_marshal_VOID__UINT

/* VOID:UINT,POINTER (marshal.list:10) */
#define mw_marshal_VOID__UINT_POINTER	g_cclosure_marshal_VOID__UINT_POINTER

/* VOID:UINT,UINT,POINTER (marshal.list:11) */
extern void mw_marshal_VOID__UINT_UINT_POINTER (GClosure     *closure,
                                                GValue       *return_value,
                                                guint         n_param_values,
                                                const GValue *param_values,
                                                gpointer      invocation_hint,
                                                gpointer      marshal_data);

/* VOID:UINT,UINT,UINT,POINTER (marshal.list:12) */
extern void mw_marshal_VOID__UINT_UINT_UINT_POINTER (GClosure     *closure,
                                                     GValue       *return_value,
                                                     guint         n_param_values,
                                                     const GValue *param_values,
                                                     gpointer      invocation_hint,
                                                     gpointer      marshal_data);

/* VOID:POINTER (marshal.list:13) */
#define mw_marshal_VOID__POINTER	g_cclosure_marshal_VOID__POINTER

/* VOID:POINTER,UINT (marshal.list:14) */
extern void mw_marshal_VOID__POINTER_UINT (GClosure     *closure,
                                           GValue       *return_value,
                                           guint         n_param_values,
                                           const GValue *param_values,
                                           gpointer      invocation_hint,
                                           gpointer      marshal_data);

/* BOOLEAN:VOID (marshal.list:16) */
extern void mw_marshal_BOOLEAN__VOID (GClosure     *closure,
                                      GValue       *return_value,
                                      guint         n_param_values,
                                      const GValue *param_values,
                                      gpointer      invocation_hint,
                                      gpointer      marshal_data);

/* BOOLEAN:POINTER (marshal.list:17) */
extern void mw_marshal_BOOLEAN__POINTER (GClosure     *closure,
                                         GValue       *return_value,
                                         guint         n_param_values,
                                         const GValue *param_values,
                                         gpointer      invocation_hint,
                                         gpointer      marshal_data);

G_END_DECLS

#endif /* __mw_marshal_MARSHAL_H__ */

