/*
  Meanwhile - Unofficial Lotus Sametime Community Client Library
  Copyright (C) 2004  Christopher (siege) O'Brien
  
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.
  
  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.
  
  You should have received a copy of the GNU Library General Public
  License along with this library; if not, write to the Free
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/


#ifndef _MW_UTIL_H
#define _MW_UTIL_H


#include <glib.h>
#include <glib/ghash.h>
#include <glib/glist.h>
#include <glib-object.h>


#define mw_map_guint_new()				\
  (g_hash_table_new(g_direct_hash, g_direct_equal))


#define mw_map_guint_new_full(valfree)					\
  (g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, (valfree)))


#define mw_map_guint_insert(ht, key, val)				\
  (g_hash_table_insert((ht), GUINT_TO_POINTER((guint)(key)), (val)))


#define mw_map_guint_replace(ht, key, val)				\
  (g_hash_table_replace((ht), GUINT_TO_POINTER((guint)(key)), (val)))


#define mw_map_guint_lookup(ht, key)				\
  (g_hash_table_lookup((ht), GUINT_TO_POINTER((guint)(key))))


#define mw_map_guint_remove(ht, key)				\
  (g_hash_table_remove((ht), GUINT_TO_POINTER((guint)(key))))


#define mw_map_guint_steal(ht, key)				\
  (g_hash_table_steal((ht), GUINT_TO_POINTER((guint)(key))))


GList *mw_map_collect_keys(GHashTable *ht);


GList *mw_map_collect_values(GHashTable *ht);


/** a GFunc that appends each item to the GList** in glistpp */
void mw_collect_gfunc(gpointer item, gpointer glistpp);


/** like g_hash_table_foreach, but uses a GFunc and only passes the
    values of each contained mapping */
void mw_map_foreach_val(GHashTable *ht, GFunc func, gpointer data);


/** a GFunc that invokes a GClosure, passing the @p item as a GValue
    of type G_TYPE_POINTER */
void mw_closure_gfunc(gpointer item, gpointer closure);


/** uses mw_map_foreach_val with mw_closure_gfunc */
#define mw_map_foreach_val_closure(ht, gclosure)		\
  (mw_map_foreach_val((ht), mw_closure_gfunc, (gclosure)))



/** uses g_list_foreach with mw_closure_gfunc */
#define mw_list_foreach_closure(glist, gclosure)		\
  (g_list_foreach((glist), mw_closure_gfunc, (closure)))


#endif /* _MW_UTIL_H */
