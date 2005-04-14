
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

#include "mw_channel.h"
#include "mw_debug.h"
#include "mw_error.h"
#include "mw_message.h"
#include "mw_service.h"
#include "mw_session.h"
#include "mw_srvc_aware.h"
#include "mw_util.h"


struct mwServiceAware {
  struct mwService service;

  struct mwAwareHandler *handler;

  /** map of ENTRY_KEY(aware_entry):aware_entry */
  GHashTable *entries;

  /** collection of lists of awareness for this service. Each item is
      a mwAwareList */
  GList *lists;

  /** the buddy list channel */
  struct mwChannel *channel;
};


struct mwAwareList {

  /** the owning service */
  struct mwServiceAware *service;

  /** map of ENTRY_KEY(aware_entry):aware_entry */
  GHashTable *entries;

  struct mwAwareListHandler *handler;
  struct mw_datum client_data;
};


struct mwAwareAttribute {
  guint32 key;
  struct mwOpaque data;
};


/** an actual awareness entry, belonging to any number of aware lists */
struct aware_entry {
  struct mwAwareSnapshot aware;
  GList *membership; /* set of mwAwareList containing this entry */
};


#define ENTRY_KEY(entry) &entry->aware.id


/** the channel send types used by this service */
enum msg_types {
  msg_AWARE_ADD       = 0x0068,  /**< remove an aware */
  msg_AWARE_REMOVE    = 0x0069,  /**< add an aware */

  msg_OPT_DO_SET      = 0x00c9,  /**< set an attribute */
  msg_OPT_DO_UNSET    = 0x00ca,  /**< unset an attribute */
  msg_OPT_WATCH       = 0x00cb,  /**< watch an attribute */

  msg_AWARE_SNAPSHOT  = 0x01f4,  /**< recv aware snapshot */
  msg_AWARE_UPDATE    = 0x01f5,  /**< recv aware update */
  msg_AWARE_GROUP     = 0x01f6,  /**< recv group aware */

  msg_OPT_GOT_SET     = 0x0259,  /**< recv attribute update */

  msg_OPT_DID_SET     = 0x025d,  /**< attribute set response */
  msg_OPT_DID_UNSET   = 0x025f,  /**< attribute unset response */
};


static void aware_entry_free(gpointer v) {
  struct aware_entry *ae = (struct aware_entry *) v;
  mwAwareSnapshot_clear(&ae->aware);
  g_list_free(ae->membership);
  g_free(v);
}


static struct aware_entry *entry_find(struct mwServiceAware *srvc,
				      struct mwAwareIdBlock *srch) {
  g_assert(srvc != NULL);
  g_assert(srvc->entries != NULL);
  g_return_val_if_fail(srch != NULL, NULL);
  
  return g_hash_table_lookup(srvc->entries, srch);
}


static struct aware_entry *list_entry_find(struct mwAwareList *list,
					   struct mwAwareIdBlock *srch) {
  g_return_val_if_fail(list != NULL, NULL);
  g_return_val_if_fail(list->entries != NULL, NULL);
  g_return_val_if_fail(srch != NULL, NULL);

  return g_hash_table_lookup(list->entries, srch);
}


static void compose_list(struct mwPutBuffer *b, GList *id_list) {
  guint32_put(b, g_list_length(id_list));
  for(; id_list; id_list = id_list->next)
    mwAwareIdBlock_put(b, id_list->data);
}


static int send_add(struct mwChannel *chan, GList *id_list) {
  struct mwPutBuffer *b = mwPutBuffer_new();
  struct mwOpaque o;
  int ret;

  compose_list(b, id_list);

  mwPutBuffer_finalize(&o, b);

  ret = mwChannel_send(chan, msg_AWARE_ADD, &o);
  mwOpaque_clear(&o);

  return ret;  
}


static int send_rem(struct mwChannel *chan, GList *id_list) {
  struct mwPutBuffer *b = mwPutBuffer_new();
  struct mwOpaque o;
  int ret;

  compose_list(b, id_list);
  mwPutBuffer_finalize(&o, b);

  ret = mwChannel_send(chan, msg_AWARE_REMOVE, &o);
  mwOpaque_clear(&o);

  return ret;
}


static gboolean collect_dead(gpointer key, gpointer val, gpointer data) {
  struct aware_entry *aware = val;
  GList **dead = data;

  if(aware->membership == NULL) {
    g_info(" removing %s, %s",
	   NSTR(aware->aware.id.user), NSTR(aware->aware.id.community));
    *dead = g_list_append(*dead, aware);
    return TRUE;

  } else {
    return FALSE;
  }
}


static int remove_unused(struct mwServiceAware *srvc) {
  /* - create a GList of all the unused aware entries
     - remove each unused aware from the service
     - if the service is alive, send a removal message for the collected
     unused.
  */

  int ret = 0;
  GList *dead = NULL, *l;

  if(srvc->entries) {
    g_info("removing orphan aware entries");
    g_hash_table_foreach_steal(srvc->entries, collect_dead, &dead);
  }
 
  if(dead) {
    if(MW_SERVICE_IS_LIVE(srvc))
      ret = send_rem(srvc->channel, dead);
    
    for(l = dead; l; l = l->next)
      aware_entry_free(l->data);

    g_list_free(dead);
  }

  return ret;
}


static void recv_accept(struct mwServiceAware *srvc,
			struct mwChannel *chan,
			struct mwMsgChannelAccept *msg) {

  g_return_if_fail(srvc->channel != NULL);
  g_return_if_fail(srvc->channel == chan);

  if(MW_SERVICE_IS_STARTING(MW_SERVICE(srvc))) {
    GList *list = NULL;

    list = map_collect_values(srvc->entries);
    send_add(chan, list);
    g_list_free(list);

    mwService_started(MW_SERVICE(srvc));

  } else {
    mwChannel_destroy(chan, ERR_FAILURE, NULL);
  }
}


static void recv_destroy(struct mwServiceAware *srvc,
			 struct mwChannel *chan,
			 struct mwMsgChannelDestroy *msg) {

  srvc->channel = NULL;
  mwService_stop(MW_SERVICE(srvc));

  /** @todo session sense service */
}


/** called from SNAPSHOT_recv, UPDATE_recv, and
    mwServiceAware_setStatus */
static void status_recv(struct mwServiceAware *srvc,
			struct mwAwareSnapshot *idb) {

  struct aware_entry *aware;
  GList *l;

  aware = entry_find(srvc, &idb->id);

  if(! aware) {
    /* we don't deal with receiving status for something we're not
       monitoring, but it will happen sometimes, eg from manually set
       status */
    return;
  }
  
  /* clear the existing status, then clone in the new status */
  mwAwareSnapshot_clear(&aware->aware);
  mwAwareSnapshot_clone(&aware->aware, idb);
  
  /* trigger each of the entry's lists */
  for(l = aware->membership; l; l = l->next) {
    struct mwAwareList *alist = l->data;
    struct mwAwareListHandler *handler = alist->handler;
    if(handler && handler->on_aware)
      handler->on_aware(alist, &aware->aware);
  }
}


gboolean list_add(struct mwAwareList *list, struct mwAwareIdBlock *id) {

  struct mwServiceAware *srvc = list->service;
  struct aware_entry *aware;

  g_return_val_if_fail(id->user != NULL, FALSE);
  g_return_val_if_fail(strlen(id->user) > 0, FALSE);

  aware = list_entry_find(list, id);
  if(aware) return FALSE;

  aware = entry_find(srvc, id);
  if(! aware) {
    aware = g_new0(struct aware_entry, 1);
    mwAwareIdBlock_clone(ENTRY_KEY(aware), id);

    g_hash_table_insert(srvc->entries, ENTRY_KEY(aware), aware);
  }

  aware->membership = g_list_append(aware->membership, list);
  g_hash_table_insert(list->entries, ENTRY_KEY(aware), aware);
  return TRUE;
}


static void group_member_recv(struct mwServiceAware *srvc,
			      struct mwAwareSnapshot *idb) {
  /* @todo
     - look up group by id
     - find each list group belongs to
     - add user to lists
  */

  struct mwAwareIdBlock gsrch = { mwAware_GROUP, idb->group, NULL };
  struct aware_entry *grp;
  GList *m;

  grp = entry_find(srvc, &gsrch);
  g_return_if_fail(grp != NULL); /* this could happen, with timing. */

  for(m = grp->membership; m; m = m->next) {
    list_add(m->data, &idb->id);
    /* mwAwareList_addAware(m->data, &idb->id, 1); */
  }
}


static void SNAPSHOT_recv(struct mwServiceAware *srvc,
			  struct mwGetBuffer *b) {

  guint32 count;

  struct mwAwareSnapshot *snap;
  snap = g_new0(struct mwAwareSnapshot, 1);

  guint32_get(b, &count);

  while(count--) {
    mwAwareSnapshot_get(b, snap);

    if(mwGetBuffer_error(b)) {
      mwAwareSnapshot_clear(snap);
      break;
    }

    if(snap->group)
      group_member_recv(srvc, snap);

    status_recv(srvc, snap);
    mwAwareSnapshot_clear(snap);
  }

  g_free(snap);
}


static void UPDATE_recv(struct mwServiceAware *srvc,
			struct mwGetBuffer *b) {

  struct mwAwareSnapshot *snap;

  snap = g_new0(struct mwAwareSnapshot, 1);
  mwAwareSnapshot_get(b, snap);

  if(snap->group)
    group_member_recv(srvc, snap);

  if(! mwGetBuffer_error(b))
    status_recv(srvc, snap);

  mwAwareSnapshot_clear(snap);
  g_free(snap);
}


static void GROUP_recv(struct mwServiceAware *srvc,
		       struct mwGetBuffer *b) {

  struct mwAwareIdBlock idb = { 0, 0, 0 };

  /* really nothing to be done with this. The group should have
     already been added to the list and service, and is now simply
     awaiting a snapshot with users listed as belonging in said
     group. */

  mwAwareIdBlock_get(b, &idb);
  mwAwareIdBlock_clear(&idb);
}


static void recv(struct mwService *srvc, struct mwChannel *chan,
		 guint16 type, struct mwOpaque *data) {

  struct mwServiceAware *srvc_aware = (struct mwServiceAware *) srvc;
  struct mwGetBuffer *b;

  g_return_if_fail(srvc_aware->channel == chan);
  g_return_if_fail(srvc->session == mwChannel_getSession(chan));
  g_return_if_fail(data != NULL);

  b = mwGetBuffer_wrap(data);

  switch(type) {
  case msg_AWARE_SNAPSHOT:
    SNAPSHOT_recv(srvc_aware, b);
    break;

  case msg_AWARE_UPDATE:
    UPDATE_recv(srvc_aware, b);
    break;

  case msg_AWARE_GROUP:
    GROUP_recv(srvc_aware, b);
    break;

  default:
    mw_debug_mailme(data, "unknown message in aware service: 0x%04x", type);   
  }

  mwGetBuffer_free(b);  
}


static void clear(struct mwService *srvc) {
  struct mwServiceAware *srvc_aware = (struct mwServiceAware *) srvc;

  g_return_if_fail(srvc != NULL);

  while(srvc_aware->lists)
    mwAwareList_free( (struct mwAwareList *) srvc_aware->lists->data );

  g_hash_table_destroy(srvc_aware->entries);
  srvc_aware->entries = NULL;
}


static const char *name(struct mwService *srvc) {
  return "Presence Awareness";
}


static const char *desc(struct mwService *srvc) {
  return "Buddy list service with support for server-side groups";
}


static struct mwChannel *make_blist(struct mwServiceAware *srvc,
				    struct mwChannelSet *cs) {

  struct mwChannel *chan = mwChannel_newOutgoing(cs);

  mwChannel_setService(chan, MW_SERVICE(srvc));
  mwChannel_setProtoType(chan, 0x00000011);
  mwChannel_setProtoVer(chan, 0x00030005);

  return mwChannel_create(chan)? NULL: chan;
}


static void start(struct mwService *srvc) {
  struct mwServiceAware *srvc_aware;
  struct mwChannel *chan;

  srvc_aware = (struct mwServiceAware *) srvc;
  chan = make_blist(srvc_aware, mwSession_getChannels(srvc->session));

  if(chan != NULL) {
    srvc_aware->channel = chan;
  } else {
    mwService_stopped(srvc);
  }
}


static void stop(struct mwService *srvc) {
  struct mwServiceAware *srvc_aware;

  srvc_aware = (struct mwServiceAware *) srvc;
  mwChannel_destroy(srvc_aware->channel, ERR_SUCCESS, NULL);
  mwService_stopped(srvc);
}


struct mwServiceAware *
mwServiceAware_new(struct mwSession *session,
		   struct mwAwareHandler *handler) {

  struct mwService *service;
  struct mwServiceAware *srvc;

  g_return_val_if_fail(session != NULL, NULL);
  g_return_val_if_fail(handler != NULL, NULL);

  srvc = g_new0(struct mwServiceAware, 1);
  srvc->handler = handler;
  srvc->entries = g_hash_table_new_full((GHashFunc) mwAwareIdBlock_hash,
					(GEqualFunc) mwAwareIdBlock_equal,
					NULL,
					(GDestroyNotify) aware_entry_free);

  service = MW_SERVICE(srvc);
  mwService_init(service, session, SERVICE_AWARE);

  service->recv_accept = (mwService_funcRecvAccept) recv_accept;
  service->recv_destroy = (mwService_funcRecvDestroy) recv_destroy;
  service->recv = recv;
  service->start = start;
  service->stop = stop;
  service->clear = clear;
  service->get_name = name;
  service->get_desc = desc;

  return srvc;
}


int mwServiceAware_setAttribute(struct mwServiceAware *srvc,
				struct mwAwareAttribute *attrib) {
  struct mwPutBuffer *b;
  struct mwOpaque o;
  int ret;

  b = mwPutBuffer_new();

  guint32_put(b, 0x00);
  guint32_put(b, attrib->data.len);
  guint32_put(b, 0x00);
  guint32_put(b, attrib->key);
  mwOpaque_put(b, &attrib->data);

  mwPutBuffer_finalize(&o, b);
  ret = mwChannel_send(srvc->channel, msg_OPT_DO_SET, &o);
  mwOpaque_clear(&o);

  return ret;
}


int mwServiceAware_setAttributeBoolean(struct mwServiceAware *srvc,
				       guint32 key, gboolean val) {
  struct mwAwareAttribute *attrib;
  int ret;

  attrib = mwAwareAttribute_newBoolean(key, val);
  ret = mwServiceAware_setAttribute(srvc, attrib);

  mwAwareAttribute_free(attrib);
  return ret;
}


int mwServiceAware_setAttributeInteger(struct mwServiceAware *srvc,
				       guint32 key, guint32 val) {
  struct mwAwareAttribute *attrib;
  int ret;

  attrib = mwAwareAttribute_newInteger(key, val);
  ret = mwServiceAware_setAttribute(srvc, attrib);

  mwAwareAttribute_free(attrib);
  return ret;
}


int mwServiceAware_setAttributeString(struct mwServiceAware *srvc,
				      guint32 key, const char *str) {
  struct mwAwareAttribute *attrib;
  int ret;

  attrib = mwAwareAttribute_newString(key, str);
  ret = mwServiceAware_setAttribute(srvc, attrib);

  mwAwareAttribute_free(attrib);
  return ret;
}


int mwServiceAware_deleteAttribute(struct mwServiceAware *srvc,
				   guint32 key) {
  struct mwPutBuffer *b;
  struct mwOpaque o;
  int ret;

  b = mwPutBuffer_new();

  guint32_put(b, 0x00);
  guint32_put(b, key);
  
  mwPutBuffer_finalize(&o, b);
  ret = mwChannel_send(srvc->channel, msg_OPT_DO_UNSET, &o);
  mwOpaque_clear(&o);

  return ret;
}


int mwServiceAware_watchAttribute(struct mwServiceAware *srvc,
				  guint32 key) {
  struct mwPutBuffer *b;
  struct mwOpaque o;
  int ret;

  b = mwPutBuffer_new();

  guint32_put(b, 0x00);
  guint32_put(b, key);
  
  mwPutBuffer_finalize(&o, b);
  ret = mwChannel_send(srvc->channel, msg_OPT_DO_UNSET, &o);
  mwOpaque_clear(&o);

  return ret;
}


int mwServiceAware_watchAttributes(struct mwServiceAware *srvc,
				   guint32 key, ...) {
  struct mwPutBuffer *b;
  guint32 k;
  va_list args;
  struct mwOpaque o;
  int ret = 0;

  /* count the number of keys in the null terminated vararg list */
  va_start(args, key);
  for(k = key; k; k = va_arg(args, guint32)) ret++;
  va_end(args);

  b = mwPutBuffer_new();

  /* write the count */
  guint32_put(b, (guint32) ret);
  
  /* write all the keys */
  va_start(args, key);
  for(k = key; k; k = va_arg(args, guint32)) guint32_put(b, k);
  va_end(args);

  mwPutBuffer_finalize(&o, b);
  ret = mwChannel_send(srvc->channel, msg_OPT_WATCH, &o);
  mwOpaque_clear(&o);

  return ret;
}


struct mwAwareAttribute *mwAwareAttribute_new(guint32 key) {
  struct mwAwareAttribute *attrib;

  attrib = g_new0(struct mwAwareAttribute, 1);
  attrib->key = key;

  return attrib;
}


struct mwAwareAttribute *
mwAwareAttribute_newBoolean(guint32 key, gboolean val) {
  struct mwAwareAttribute *attrib;
  struct mwPutBuffer *b;
  
  b = mwPutBuffer_new();
  gboolean_put(b, FALSE);
  gboolean_put(b, val);

  attrib = mwAwareAttribute_new(key);
  mwPutBuffer_finalize(&attrib->data, b);

  return attrib;
}


struct mwAwareAttribute *
mwAwareAttribute_newInteger(guint32 key, guint32 val) {
  struct mwAwareAttribute *attrib;
  struct mwPutBuffer *b;
  
  b = mwPutBuffer_new();
  guint32_put(b, val);

  attrib = mwAwareAttribute_new(key);
  mwPutBuffer_finalize(&attrib->data, b);

  return attrib;
}


struct mwAwareAttribute *
mwAwareAttribute_newString(guint32 key, const char *str) {
  struct mwAwareAttribute *attrib;
  struct mwPutBuffer *b;
  
  b = mwPutBuffer_new();
  mwString_put(b, str);

  attrib = mwAwareAttribute_new(key);
  mwPutBuffer_finalize(&attrib->data, b);

  return attrib;
}


guint32 mwAwareAttribute_getKey(struct mwAwareAttribute *attrib) {
  g_return_val_if_fail(attrib != NULL, 0x00);
  return attrib->key;
}


gboolean mwAwareAttribute_asBoolean(struct mwAwareAttribute *attrib) {
  if(! attrib) return FALSE;
  /* @todo */
  return FALSE;
}


guint32 mwAwareAttribute_asInteger(struct mwAwareAttribute *attrib) {
  if(! attrib) return 0x00;
  /* @todo */
  return 0x00;
}


char *mwAwareAttribute_asString(struct mwAwareAttribute *attrib) {
  if(! attrib) return NULL;
  /* @todo */
  return NULL;
}


struct mwOpaque *mwAwareAttribute_asOpaque(struct mwAwareAttribute *attrib) {
  g_return_val_if_fail(attrib != NULL, NULL);
  return &attrib->data;
}


void mwAwareAttribute_free(struct mwAwareAttribute *attrib) {
  if(! attrib) return;

  mwOpaque_clear(&attrib->data);
  g_free(attrib);
}
			  

struct mwAwareList *
mwAwareList_new(struct mwServiceAware *srvc,
		struct mwAwareListHandler *handler) {

  struct mwAwareList *al;

  g_return_val_if_fail(srvc != NULL, NULL);
  g_return_val_if_fail(handler != NULL, NULL);

  al = g_new0(struct mwAwareList, 1);
  al->service = srvc;
  al->entries = g_hash_table_new((GHashFunc) mwAwareIdBlock_hash,
				 (GEqualFunc) mwAwareIdBlock_equal);
  al->handler = handler;

  return al;
}


static void dismember_aware(gpointer k, struct aware_entry *aware,
			    struct mwAwareList *list) {

  aware->membership = g_list_remove(aware->membership, list);
}


void mwAwareList_free(struct mwAwareList *list) {
  struct mwServiceAware *srvc;
  struct mwAwareListHandler *handler;

  g_return_if_fail(list != NULL);
  g_return_if_fail(list->entries != NULL);
  g_return_if_fail(list->service != NULL);

  handler = list->handler;
  if(handler && handler->clear) {
    handler->clear(list);
    list->handler = NULL;
  }

  mw_datum_clear(&list->client_data);

  srvc = list->service;
  srvc->lists = g_list_remove(srvc->lists, list);

  /* for each entry, remove the aware list from the service entry's
     membership collection */
  g_hash_table_foreach(list->entries, (GHFunc) dismember_aware, list);
  g_hash_table_destroy(list->entries);
  g_free(list);

  remove_unused(srvc);
}


struct mwAwareListHandler *mwAwareList_getHandler(struct mwAwareList *list) {
  g_return_val_if_fail(list != NULL, NULL);
  return list->handler;
}


int mwAwareList_addAware(struct mwAwareList *list, GList *id_list) {

  /* for each awareness id:
     - if it's already in the list, continue
     - if it's not in the service list:
       - create an awareness
       - add it to the service list
     - add this list to the membership
     - add to the list
  */

  struct mwServiceAware *srvc;
  GList *additions = NULL;
  int ret = 0;

  g_return_val_if_fail(list != NULL, -1);

  srvc = list->service;
  g_return_val_if_fail(srvc != NULL, -1);

  for(; id_list; id_list = id_list->next) {
    if(list_add(list, id_list->data))
      additions = g_list_prepend(additions, id_list->data);
  }

  /* if the service is alive-- or getting there-- we'll need to send
     these additions upstream */
  if(MW_SERVICE_IS_LIVE(srvc) && additions)
    ret = send_add(srvc->channel, additions);

  g_list_free(additions);
  return ret;
}


int mwAwareList_removeAware(struct mwAwareList *list, GList *id_list) {

  /* for each awareness id:
     - if it's not in the list, forget it
     - remove from the list
     - remove list from the membership

     - call remove round
  */

  struct mwServiceAware *srvc;
  struct mwAwareIdBlock *id;
  struct aware_entry *aware;

  g_return_val_if_fail(list != NULL, -1);

  srvc = list->service;
  g_return_val_if_fail(srvc != NULL, -1);

  for(; id_list; id_list = id_list->next) {
    id = id_list->data;
    aware = list_entry_find(list, id);

    if(! aware) {
      g_warning("buddy %s, %s not in list",
		NSTR(id->user),
		NSTR(id->community));
      continue;
    }

    aware->membership = g_list_remove(aware->membership, list);
    g_hash_table_remove(list->entries, id);
  }

  return remove_unused(srvc);
}


void mwAwareList_setClientData(struct mwAwareList *list,
			       gpointer data, GDestroyNotify clear) {

  g_return_if_fail(list != NULL);
  mw_datum_set(&list->client_data, data, clear);
}


gpointer mwAwareList_getClientData(struct mwAwareList *list) {
  g_return_val_if_fail(list != NULL, NULL);
  return mw_datum_get(&list->client_data);
}


void mwAwareList_removeClientData(struct mwAwareList *list) {
  g_return_if_fail(list != NULL);
  mw_datum_clear(&list->client_data);
}


void mwServiceAware_setStatus(struct mwServiceAware *srvc,
			      struct mwAwareIdBlock *user,
			      struct mwUserStatus *stat) {

  struct mwAwareSnapshot idb;

  g_return_if_fail(srvc != NULL);
  g_return_if_fail(user != NULL);
  g_return_if_fail(stat != NULL);

  /* just reference the strings. then we don't need to free them */
  idb.id.type = user->type;
  idb.id.user = user->user;
  idb.id.community = user->community;

  idb.group = NULL;
  idb.online = TRUE;
  idb.alt_id = NULL;

  idb.status.status = stat->status;
  idb.status.time = stat->time;
  idb.status.desc = stat->desc;

  idb.name = NULL;

  status_recv(srvc, &idb);
}


const char *mwServiceAware_getText(struct mwServiceAware *srvc,
				   struct mwAwareIdBlock *user) {

  struct aware_entry *aware;

  g_return_val_if_fail(srvc != NULL, NULL);
  g_return_val_if_fail(user != NULL, NULL);

  aware = entry_find(srvc, user);
  g_return_val_if_fail(aware != NULL, NULL);

  return aware->aware.status.desc;
}


