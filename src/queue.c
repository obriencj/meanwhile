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


#include <glib.h>
#include <glib/ghash.h>
#include <glib/glist.h>
#include <string.h>
#include "mw_debug.h"
#include "mw_object.h"
#include "mw_queue.h"


/** a queue of fixed-size elements based on a circular buffer */
struct mw_queue {
  gpointer head;  /**< first element of circbuf */
  gpointer tail;  /**< last element of circbuf */

  gpointer in;    /**< the spot for the next push */
  gpointer out;   /**< the spot for the next peek/pop */

  guint used;     /**< count of used elements */
  guint count;    /**< count of elements */

  guint grow_step;    /**< count of elements to grow by when full */
  gsize member_size;  /**< size of elements */
};


MwQueue *MwQueue_new(gsize member_size, guint grow_step) {
  MwQueue *q;

  g_return_val_if_fail(member_size > 0, NULL);
  g_return_val_if_fail(grow_step > 0, NULL);

  q = g_new(MwQueue, 1);

  q->head = g_malloc(member_size * grow_step);
  q->tail = q->head + (member_size * (grow_step - 1));
  
  q->in = q->head;
  q->out = q->in;
  
  q->used = 0;
  q->count = grow_step;

  q->member_size = member_size;
  q->grow_step = grow_step;
  
  return q;
}


void MwQueue_clear(MwQueue *q, GDestroyNotify clean) {
  gpointer o;
  while( (o = MwQueue_next(q)) ) {
    if(clean) clean(o);
  }
}


void MwQueue_free(MwQueue *q) {
  if(! q) return;
  g_free(q->head);
  g_free(q);
}


guint MwQueue_size(MwQueue *q) {
  g_return_val_if_fail(q != NULL, 0);
  return q->used;
}


gpointer MwQueue_peek(MwQueue *q) {
  g_return_val_if_fail(q != NULL, NULL);
  return (q->used)? q->out: NULL;
}


gpointer MwQueue_next(MwQueue *q) {
  gpointer ret;
  
  g_return_val_if_fail(q != NULL, NULL);

  if(! q->used)
    return NULL;

  ret = q->out;

  if(q->out == q->tail) {
    q->out = q->head;
  } else {
    q->out += q->member_size;
  }

  q->used--;

  return ret;
}


static void MwQueue_grow(MwQueue *q, gsize by_count) {
  gsize ncount;
  gpointer nhead;

  g_return_if_fail(q != NULL);

  ncount = q->count + by_count;
  nhead = g_malloc(q->member_size * ncount);
  
  if(q->used) {
    if(q->out <= q->in) {
      /* no wraparound, easy copy */
      memcpy(nhead, q->out, q->used * q->member_size);
    } else {
      /* wraparound happened. copy the out-to-tail segment, then copy
	 the head-to-in segment */
      gsize cap = (q->tail - q->out + q->member_size);
      memcpy(nhead, q->out, cap);
      memcpy(nhead + cap, q->head, (q->in - q->head));
    }
  }

  g_free(q->head);

  q->head = nhead;
  q->tail = nhead + (q->member_size * (ncount - 1));
  q->in = nhead + (q->member_size * (q->used));
  q->out = nhead;
  q->count = ncount;
}


gpointer MwQueue_push(MwQueue *q) {
  gpointer ret;

  g_return_val_if_fail(q != NULL, NULL);

  if(q->used >= q->count)
    MwQueue_grow(q, q->grow_step);

  ret = q->in;

  if(q->in == q->tail) {
    q->in = q->head;
  } else {
    q->in += q->member_size;
  }

  memset(ret, 0, q->member_size);

  q->used++;

  return ret;
}


struct mw_meta_queue {
  GHashTable *table;
  GList *loop;

  guint used;

  gsize type_size;
  guint grow_count;
};


static void mw_mq_notify(gpointer data, GObject *gone) {
  MwMetaQueue *mq = data;
  g_debug("key %p destroyed", gone);
  g_hash_table_remove(mq->table, gone);
}


MwMetaQueue *MwMetaQueue_new(gsize type_size, guint grow_count) {
  MwMetaQueue *mq;

  g_return_val_if_fail(type_size > 0, NULL);
  g_return_val_if_fail(grow_count > 0, NULL);

  mq = g_new0(MwMetaQueue, 1);
  mq->table = g_hash_table_new_full(g_direct_hash, g_direct_equal,
				    NULL, (GDestroyNotify) MwQueue_free);
  mq->loop = NULL;
  mq->used = 0;
  mq->type_size = type_size;
  mq->grow_count = grow_count;
  return mq;
}


void MwMetaQueue_clear(MwMetaQueue *mq, GDestroyNotify clean) {
  gpointer o;
  while( (o = MwMetaQueue_next(mq)) ) {
    if(clean) clean(o);
  }
  MwMetaQueue_scour(mq);
}


void MwMetaQueue_free(MwMetaQueue *mq) {
  GList *l;

  if(! mq) return;

  l = mq->loop;
  while(l) {
    mw_gobject_unref(l->data);
    l = g_list_delete_link(l, l);
  }

  g_hash_table_destroy(mq->table);
  g_free(mq);
}


gpointer MwMetaQueue_push(MwMetaQueue *mq, GObject *key) {
  MwQueue *q;

  g_return_val_if_fail(mq != NULL, NULL);
  g_return_val_if_fail(key != NULL, NULL);

  mq->used++;

  q = g_hash_table_lookup(mq->table, key);

  if(! q) {
    g_debug("no queue for key %p", key);
    q = MwQueue_new(mq->type_size, mq->grow_count);
    g_hash_table_insert(mq->table, key, q);
    g_object_weak_ref(key, mw_mq_notify, mq);
  }

  if(! q->used && ! g_list_find(mq->loop, key)) {
    mw_gobject_ref(key);
    mq->loop = g_list_append(mq->loop, key);
  }

  return MwQueue_push(q);
}


gpointer MwMetaQueue_next(MwMetaQueue *mq) {
  GList *l = NULL;

  g_return_val_if_fail(mq != NULL, NULL);

  while( (l = mq->loop) ) {
    GObject *key = NULL;
    MwQueue *q = NULL;
    gpointer ret = NULL;

    mq->loop = g_list_remove_link(mq->loop, l);
    key = l->data;

    q = g_hash_table_lookup(mq->table, key);
  
    if(q && q->used) {
      ret = MwQueue_next(q);
    }

    /* note: we don't immediately unref the key if the queue becomes
       empty from this call. Rather, we do it on the next call when
       this key comes up in the loop again. This is to prevent the
       posibility of the weak reference handler being triggered before
       we have a chance to act on the queue's return value (which the
       weak handler will destroy */

    if(ret) {
      /* queue wasn't empty, put it back on the end of the loop */
      mq->loop = g_list_concat(mq->loop, l);
      mq->used--;

      return ret;

    } else {
      /* found an empty queue in the loop. unref the key, continue to
	 the next queue in the loop */
      g_debug("MwMetaQueue_next is scouring key %p from metaqueue", key);
      mw_gobject_unref(key);
      g_list_free(l);
    }
  }

  return NULL;
}


void MwMetaQueue_scour(MwMetaQueue *mq) {
  GList *l;
  
  g_return_if_fail(mq != NULL);

  l = mq->loop;
  while(l) {
    GList *next;
    GObject *key = NULL;
    MwQueue *q = NULL;

    key = l->data;
    q = g_hash_table_lookup(mq->table, key);

    next = l->next;

    if(q->used) {
      g_debug("key %p has %u left in queue", key, q->used);

    } else {
      g_debug("MwMetaQueue_scour is scouring key %p from metaqueue", key);
      mw_gobject_unref(key);
      mq->loop = g_list_delete_link(mq->loop, l);
    }
    
    l = next;
  }
}


guint MwMetaQueue_size(MwMetaQueue *mq) {
  g_return_val_if_fail(mq != NULL, 0);
  return mq->used;
}


/* The end. */
