

#include <glib.h>
#include <glib/ghash.h>
#include <glib/glist.h>
#include <string.h>

#include "srvc_aware.h"

#include "channel.h"
#include "error.h"
#include "message.h"
#include "mw_debug.h"
#include "service.h"
#include "session.h"


struct mwServiceAware {
  struct mwService service;

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

  /** handler for awarenes events */
  mwAwareList_onAwareHandler on_aware;

  /** user data for handler */
  gpointer on_aware_data;
  GDestroyNotify on_aware_data_free;
};


/** an actual awareness entry, belonging to any number of aware lists */
struct aware_entry {
  struct mwAwareSnapshot aware;
  GList *membership; /* set of mwAwareList containing this entry */
};


#define ENTRY_KEY(entry) &entry->aware.id


/** the channel send types used by this service */
enum send_types {
  mwChannelSend_AWARE_ADD       = 0x0068,
  mwChannelSend_AWARE_REMOVE    = 0x0069,
  mwChannelSend_AWARE_SNAPSHOT  = 0x01f4,
  mwChannelSend_AWARE_UPDATE    = 0x01f5,
  mwChannelSend_AWARE_GROUP     = 0x01f6,
};


static void recv_channelCreate(struct mwService *srvc,
			       struct mwChannel *chan,
			       struct mwMsgChannelCreate *msg) {

  /* We only like outgoing blist channels */
  mwChannel_destroy(chan, ERR_FAILURE, NULL);
}


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

  ret = mwChannel_send(chan, mwChannelSend_AWARE_ADD, &o);
  mwOpaque_clear(&o);

  return ret;  
}


static int send_rem(struct mwChannel *chan, GList *id_list) {
  struct mwPutBuffer *b = mwPutBuffer_new();
  struct mwOpaque o;
  int ret;

  compose_list(b, id_list);
  mwPutBuffer_finalize(&o, b);

  ret = mwChannel_send(chan, mwChannelSend_AWARE_REMOVE, &o);
  mwOpaque_clear(&o);

  return ret;
}


static gboolean collect_dead(gpointer key, gpointer val, gpointer data) {
  struct aware_entry *aware = (struct aware_entry *) val;
  GList **dead = (GList **) data;

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

  g_info("removing orphan aware entries");
  g_hash_table_foreach_steal(srvc->entries, collect_dead, &dead);
 
  if(dead) {
    if(MW_SERVICE_IS_LIVE(srvc))
      ret = send_rem(srvc->channel, dead);
    
    for(l = dead; l; l = l->next)
      aware_entry_free(l->data);

    g_list_free(dead);
  }

  return ret;
}


/** fill a GList with living memberships */
static void collect_aware(gpointer key, gpointer val, gpointer data) {
  struct aware_entry *aware = (struct aware_entry *) val;
  GList **list = (GList **) data;

  if(aware->membership != NULL)
    *list = g_list_append(*list, aware);
}


static void recv_channelAccept(struct mwService *srvc,
			       struct mwChannel *chan,
			       struct mwMsgChannelAccept *msg) {

  struct mwServiceAware *srvc_aware = (struct mwServiceAware *) srvc;
  g_return_if_fail(srvc_aware->channel == chan);

  if(MW_SERVICE_IS_STARTING(srvc)) {
    GList *list = NULL;

    g_hash_table_foreach(srvc_aware->entries, collect_aware, &list);
    send_add(chan, list);
    g_list_free(list);

    mwService_started(srvc);

  } else {
    mwChannel_destroy(chan, ERR_FAILURE, NULL);
  }
}


static void recv_channelDestroy(struct mwService *srvc,
				struct mwChannel *chan,
				struct mwMsgChannelDestroy *msg) {

  mwService_stop(srvc);
  /* TODO: session sense service. */
}


/** called from SNAPSHOT_recv, UPDATE_recv, and
    mwServiceAware_setStatus */
static void status_recv(struct mwServiceAware *srvc,
			struct mwAwareSnapshot *idb) {

  struct aware_entry *aware;
  GList *l;

  aware = entry_find(srvc, &idb->id);

  /* we don't deal with receiving status for something we're not
     monitoring */
  if(! aware) return;
  
  /* clear the existing status, then clone in the new status */
  mwAwareSnapshot_clear(&aware->aware);
  mwAwareSnapshot_clone(&aware->aware, idb);
  
  /* trigger each of the entry's lists */
  for(l = aware->membership; l; l = l->next) {
    struct mwAwareList *alist = l->data;
    if(alist->on_aware)
      alist->on_aware(alist, &aware->aware, alist->on_aware_data);
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
    g_debug("adding buddy %s, %s to the aware service",
	    NSTR(id->user), NSTR(id->community));
    
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
  struct mwAwareSnapshot snap;

  guint32_get(b, &count);

  while(count--) {
    mwAwareSnapshot_get(b, &snap);

    if(mwGetBuffer_error(b)) {
      mwAwareSnapshot_clear(&snap);
      break;
    }

    if(snap.group) {
      group_member_recv(srvc, &snap);
    }
    status_recv(srvc, &snap);
  }
}


static void UPDATE_recv(struct mwServiceAware *srvc,
			struct mwGetBuffer *b) {

  struct mwAwareSnapshot snap;

  mwAwareSnapshot_get(b, &snap);

  if(! mwGetBuffer_error(b))
    status_recv(srvc, &snap);

  mwAwareSnapshot_clear(&snap);
}


static void GROUP_recv(struct mwServiceAware *srvc,
		       struct mwGetBuffer *b) {

  struct mwAwareIdBlock idb;

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
  case mwChannelSend_AWARE_SNAPSHOT:
    SNAPSHOT_recv(srvc_aware, b);
    break;

  case mwChannelSend_AWARE_UPDATE:
    UPDATE_recv(srvc_aware, b);
    break;

  case mwChannelSend_AWARE_GROUP:
    GROUP_recv(srvc_aware, b);
    break;

  default:
    g_warning("unknown message type 0x%04x for aware service", type);
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
  return "A simple Buddy List service";
}


static guint id_hash(gconstpointer v) {
  return g_str_hash( ((struct mwAwareIdBlock *)v)->user );
}


static gboolean id_equal(gconstpointer a, gconstpointer b) {
  return mwAwareIdBlock_equal((struct mwAwareIdBlock *) a,
			      (struct mwAwareIdBlock *) b);
}


static struct mwChannel *make_blist(struct mwServiceAware *srvc,
				    struct mwChannelSet *cs) {

  struct mwChannel *chan = mwChannel_newOutgoing(cs);

  mwChannel_setService(chan, MW_SERVICE(srvc));
  mwChannel_setProtoType(chan, mwProtocol_AWARE);
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


struct mwServiceAware *mwServiceAware_new(struct mwSession *session) {
  struct mwServiceAware *srvc_aware;
  struct mwService *srvc;

  g_assert(session);

  srvc_aware = g_new0(struct mwServiceAware, 1);
  srvc = &srvc_aware->service;

  mwService_init(srvc, session, mwService_AWARE);

  srvc->recv_channelCreate = recv_channelCreate;
  srvc->recv_channelAccept = recv_channelAccept;
  srvc->recv_channelDestroy = recv_channelDestroy;
  srvc->recv = recv;
  srvc->start = start;
  srvc->stop = stop;
  srvc->clear = clear;
  srvc->get_name = name;
  srvc->get_desc = desc;

  srvc_aware->entries = g_hash_table_new_full(id_hash, id_equal,
					      NULL, aware_entry_free);

  return srvc_aware;
}


struct mwAwareList *mwAwareList_new(struct mwServiceAware *srvc) {
  struct mwAwareList *a_list;

  g_return_val_if_fail(srvc, NULL);

  a_list = g_new0(struct mwAwareList, 1);
  a_list->service = srvc;

  a_list->entries = g_hash_table_new_full(id_hash, id_equal, NULL, NULL);
  return a_list;
}


static void dismember_aware(gpointer key, gpointer val, gpointer data) {
  struct aware_entry *aware = (struct aware_entry *) val;
  struct mwAwareList *list = (struct mwAwareList *) data;

  aware->membership = g_list_remove(aware->membership, list);
}


void mwAwareList_free(struct mwAwareList *list) {
  struct mwServiceAware *srvc;

  g_return_if_fail(list != NULL);
  g_return_if_fail(list->entries != NULL);
  g_return_if_fail(list->service != NULL);

  if(list->on_aware_data_free)
    list->on_aware_data_free(list->on_aware_data);

  srvc = list->service;
  srvc->lists = g_list_remove(srvc->lists, list);

  /* for each entry, remove the aware list from the service entry's
     membership collection */
  g_hash_table_foreach(list->entries, dismember_aware, list);
  g_hash_table_destroy(list->entries);
  g_free(list);

  remove_unused(srvc);
}


void mwAwareList_setOnAware(struct mwAwareList *list,
			    mwAwareList_onAwareHandler cb,
			    gpointer data, GDestroyNotify data_free) {

  g_return_if_fail(list != NULL);
  list->on_aware = cb;
  list->on_aware_data = data;
  list->on_aware_data_free = data_free;
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
  struct aware_entry *aware;

  g_return_val_if_fail(list != NULL, -1);

  srvc = list->service;
  g_return_val_if_fail(srvc != NULL, -1);

  for(; id_list; id_list = id_list->next) {
    aware = list_entry_find(list, id_list->data);

    if(! aware) {
      g_warning("buddy %s, %s not in list",
		NSTR(aware->aware.id.user),
		NSTR(aware->aware.id.community));
      continue;
    }

    g_hash_table_remove(list->entries, id_list->data);
    aware->membership = g_list_remove(aware->membership, list);
  }

  return remove_unused(srvc);
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

  idb.online = TRUE;

  idb.status.status = stat->status;
  idb.status.time = stat->time;
  idb.status.desc = stat->desc;

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



