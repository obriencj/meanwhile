
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


#include "mw_util.h"


void mw_collect_gfunc(gpointer item, gpointer data) {
  GList **list = data;
  *list = g_list_append(*list, item);
}


static void mw_collect_keys(gpointer key, gpointer val, gpointer data) {
  GList **list = data;
  *list = g_list_append(*list, key);
}


GList *mw_map_collect_keys(GHashTable *ht) {
  GList *ret = NULL;
  g_hash_table_foreach(ht, mw_collect_keys, &ret);
  return ret;
}


static void mw_collect_values(gpointer key, gpointer val, gpointer data) {
  GList **list = data;
  *list = g_list_append(*list, val);
}


GList *mw_map_collect_values(GHashTable *ht) {
  GList *ret = NULL;
  g_hash_table_foreach(ht, mw_collect_values, &ret);
  return ret;
}


struct ghfunc_cl {
  GFunc func;
  gpointer data;
};


/* a GHFunc that calls a GFunc */
static void mw_foreach_ghfunc(gpointer key, gpointer val, gpointer udata) {
  struct ghfunc_cl *ghf = udata;
  ghf->func(val, ghf->data);
}


void mw_map_foreach_val(GHashTable *ht, GFunc func, gpointer data) {
  struct ghfunc_cl ghf = { .func = func, .data = data };
  g_hash_table_foreach(ht, mw_foreach_ghfunc, &ghf);
}


void mw_closure_gfunc(gpointer item, gpointer closure) {
  GValue dv = {};
  g_value_init(&dv, G_TYPE_POINTER);
  g_value_set_pointer(&dv, item);
  g_closure_invoke(closure, NULL, 1, &dv, NULL);
}


/* The end. */
