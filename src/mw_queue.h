#ifndef _MW_QUEUE_H
#define _MW_QUEUE_H


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


/** @file mw_queue.h

    ...
    Implemented as a growable circular array.
*/


#include <glib.h>
#include <glib-object.h>


G_BEGIN_DECLS


typedef struct mw_queue MwQueue;


/** @struct mw_queue
    @since 2.0.0

    A circular buffer utilized in evenly-sized chunks. The first item
    returned from a call to push will be the first item returned from
    a call to peek or next.

    Used in session.c and channel.c to cache outgoing message data
*/
struct mw_queue;


MwQueue *MwQueue_new(gsize member_size, guint grow_count);


/** @since 2.0.0

    destroy the queue
*/
void MwQueue_free(MwQueue *q);


/** @since 2.0.0

    count of queued items. This value is incremented by MwQueue_push
    and decremented by MwQueue_next.
*/
guint MwQueue_size(MwQueue *q);


/** @since 2.0.0

    returns a pointer to the next chunk of memory in the queue, without
    advancing the queue. Returns NULL if there are no entries in the queue
*/
gpointer MwQueue_peek(MwQueue *q);


/** @since 2.0.0

    Decrements the queue size and returns a pointer to the next chunk
    of memory in the queue, advancing the queue. This pointer is valid
    only until the next call to MwQueue_push or MwQueue_free. Returns
    NULL if there are no more entries in the queue.
*/
gpointer MwQueue_next(MwQueue *q);


/** @since 2.0.0

    returns a pointer to an allocated chunk of memory of the size
    specified to MwQueue_new. This chunk can be cast to an appropriate
    structure and have values assigned to its members.
*/
gpointer MwQueue_push(MwQueue *q);


typedef struct mw_meta_queue MwMetaQueue;


/**
   A circular list of queues, keyed to a GObject instance
*/
struct mw_meta_queue;


MwMetaQueue *MwMetaQueue_new(gsize member_size, guint grow_count);


void MwMetaQueue_free(MwMetaQueue *mq);


gpointer MwMetaQueue_push(MwMetaQueue *mq, GObject *key);


/** return the next value in the first non-empty queue in the loop,
    and puts the queue on the end of the loop. Any empty queues on the
    front of the loop before the first non-empty queue will have their
    keys unref'd, and will be removed from the loop */
gpointer MwMetaQueue_next(MwMetaQueue *mq);


/** removes and unrefs keys of all empty queues in loop. Can be called
    after MwMetaQueue_next to ensure unref happens. This may cause
    pointers returned by MwMetaQueue_next to be invalidated as the
    backing queue will be destroyed if the refcount on the key object
    reaches zero */
void MwMetaQueue_scour(MwMetaQueue *mq);


/** count of all queued entries */
guint MwMetaQueue_size(MwMetaQueue *mq);


G_END_DECLS


#endif /* _MW_QUEUE_H */
