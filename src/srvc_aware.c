

#include <glib.h>
#include <glib/ghash.h>
#include <glib/glist.h>

#include "channel.h"
#include "error.h"
#include "message.h"
#include "mw_debug.h"
#include "service.h"
#include "session.h"

#include "srvc_aware.h"


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
};


/** an actual awareness entry, belonging to any number of aware lists */
struct aware_entry {
  struct mwSnapshotAwareIdBlock aware;
  GList *membership; /* set of mwAwareList containing this entry */
};


#define ENTRY_KEY(entry) &entry->aware.id


/** the channel send types used by this service */
enum send_types {
  mwChannelSend_AWARE_ADD       = 0x0068,
  mwChannelSend_AWARE_REMOVE    = 0x0069,
  mwChannelSend_AWARE_SNAPSHOT  = 0x01f4,
  mwChannelSend_AWARE_UPDATE    = 0x01f5
};


static void recv_channelCreate(struct mwService *srvc, struct mwChannel *chan,
			       struct mwMsgChannelCreate *msg) {

  /* We only like outgoing blist channels */
  mwChannel_destroyQuick(chan, ERR_FAILURE);
}


static void aware_entry_free(gpointer v) {
  struct aware_entry *ae = (struct aware_entry *) v;
  mwSnapshotAwareIdBlock_clear(&ae->aware);
  g_list_free(ae->membership);
  g_free(v);
}


static void compose_list(GList *id_list, char **buf, gsize *len) {
  GList *l;
  char *b;
  gsize n = 4;

  for(l = id_list; l; l = l->next)
    n += mwAwareIdBlock_buflen((struct mwAwareIdBlock *) l->data);

  *len = n;
  *buf = b = (char *) g_malloc0(n);

  guint32_put(&b, &n, g_list_length(id_list));
  for(l = id_list; l; l = l->next)
    mwAwareIdBlock_put(&b, &n, (struct mwAwareIdBlock *) l->data);
}


static int send_add(struct mwChannel *chan, GList *id_list) {
  char *buf;
  gsize len;
  int ret;

  compose_list(id_list, &buf, &len);  
  ret = mwChannel_send(chan, mwChannelSend_AWARE_ADD, buf, len);
  g_free(buf);

  return ret;  
}


static int send_rem(struct mwChannel *chan, GList *id_list) {
  char *buf;
  gsize len;
  int ret;

  compose_list(id_list, &buf, &len);
  ret = mwChannel_send(chan, mwChannelSend_AWARE_REMOVE, buf, len);
  g_free(buf);

  return ret;
}


static gboolean collect_dead(gpointer key, gpointer val, gpointer data) {
  struct aware_entry *aware = (struct aware_entry *) val;
  GList **dead = (GList **) data;

  if(aware->membership == NULL) {
    g_info(" removing %s, %s",
	   aware->aware.id.user, aware->aware.id.community);
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


static void recv_channelAccept(struct mwService *srvc, struct mwChannel *chan,
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
    mwChannel_destroyQuick(chan, ERR_FAILURE);
  }
}


static void recv_channelDestroy(struct mwService *srvc, struct mwChannel *chan,
				struct mwMsgChannelDestroy *msg) {

  mwService_stop(srvc);
  /* TODO: session sense service. */
}


/** called from SNAPSHOT_recv, UPDATE_recv, and mwServiceAware_setStatus */
static void status_recv(struct mwServiceAware *srvc,
			struct mwSnapshotAwareIdBlock *idb_array,
			guint32 count) {

  struct mwAwareList *alist;
  struct aware_entry *aware;
  GList *l;

  for(; count--; idb_array++) {

    aware = (struct aware_entry *)
      g_hash_table_lookup(srvc->entries, &idb_array->id);

    /* we don't deal with receiving status for something we're not
       monitoring */
    if(! aware) continue;

    /* clear the existing status, then clone in the new status */
    mwSnapshotAwareIdBlock_clear(&aware->aware);
    mwSnapshotAwareIdBlock_clone(&aware->aware, idb_array);

    /* trigger each of the entry's lists */
    for(l = aware->membership; l; l = l->next) {
      alist = (struct mwAwareList *) l->data;
      if(alist->on_aware)
	alist->on_aware(alist, &aware->aware, alist->on_aware_data);
    }
  }

}


static int SNAPSHOT_recv(struct mwServiceAware *srvc,
			 const char *b, gsize n) {
  int ret;
  unsigned int count, c;
  struct mwSnapshotAwareIdBlock *idb;

  ret = guint32_get((char **) &b, &n, &count);

  if(ret == 0) {
    idb = g_new0(struct mwSnapshotAwareIdBlock, count);
    
    for(c = count; c--; )
      if( (ret = mwSnapshotAwareIdBlock_get((char **) &b, &n, idb + c)) )
	break;
    
    if(ret == 0)
      status_recv(srvc, idb, count);
    
    for(c = count; c--; )
      mwSnapshotAwareIdBlock_clear(idb + c);
    g_free(idb);
  }  
  return ret;
}


static int UPDATE_recv(struct mwServiceAware *srvc,
		       const char *b, gsize n) {
  int ret = 0;
  struct mwSnapshotAwareIdBlock *idb;

  idb = g_new0(struct mwSnapshotAwareIdBlock, 1);
  ret = mwSnapshotAwareIdBlock_get((char **) &b, &n, idb);

  if(ret == 0)
    status_recv(srvc, idb, 1);

  mwSnapshotAwareIdBlock_clear(idb);
  g_free(idb);

  return ret;
}


static void recv(struct mwService *srvc, struct mwChannel *chan,
		 guint16 type, const char *b, gsize n) {

  struct mwServiceAware *srvc_aware = (struct mwServiceAware *) srvc;
  int ret;

  g_return_if_fail(srvc_aware->channel == chan);
  g_return_if_fail(srvc->session == chan->session);

  switch(type) {
  case mwChannelSend_AWARE_SNAPSHOT:
    ret = SNAPSHOT_recv(srvc_aware, b, n);
    break;

  case mwChannelSend_AWARE_UPDATE:
    ret = UPDATE_recv(srvc_aware, b, n);
    break;

  default:
    g_warning("unknown message type 0x%04x for aware service", type);
  }

  if(ret) ; /* handle how? */
}


static void clear(struct mwService *srvc) {
  struct mwServiceAware *srvc_aware = (struct mwServiceAware *) srvc;

  g_return_if_fail(srvc != NULL);

  while(srvc_aware->lists)
    mwAwareList_free( (struct mwAwareList *) srvc_aware->lists->data );

  g_hash_table_destroy(srvc_aware->entries);
  srvc_aware->entries = NULL;
}


static const char *name() {
  return "Presence Awareness";
}


static const char *desc() {
  return "A simple Buddy List service";
}


static guint id_hash(gconstpointer v) {
  return g_str_hash( ((struct mwAwareIdBlock *)v)->user );
}


static gboolean id_equal(gconstpointer a, gconstpointer b) {
  return mwAwareIdBlock_equal((struct mwAwareIdBlock *) a,
			      (struct mwAwareIdBlock *) b);
}


static int send_create(struct mwChannel *chan) {
  struct mwMsgChannelCreate *msg;
  char *b;
  gsize n;

  int ret;

  msg = (struct mwMsgChannelCreate *)
    mwMessage_new(mwMessage_CHANNEL_CREATE);

  msg->channel = chan->id;
  msg->service = chan->service;
  msg->proto_type = chan->proto_type;
  msg->proto_ver = chan->proto_ver;

  msg->encrypt.opaque.len = n = 10; /* 4 + 4 + 2 */
  msg->encrypt.opaque.data = b = (char *) g_malloc0(n);
  guint32_put(&b, &n, 0x00000001);
  /* six bytes of zero trail after this. */

  ret = mwChannel_create(chan, msg);
  mwMessage_free(MW_MESSAGE(msg));

  return ret;
}


static struct mwChannel *make_blist(struct mwChannelSet *cs) {

  struct mwChannel *chan = mwChannel_newOutgoing(cs);
  chan->status = mwChannel_INIT;

  chan->service = mwService_AWARE;
  chan->proto_type = mwProtocol_AWARE;
  chan->proto_ver = 0x00030005;

  return send_create(chan)? NULL: chan;
}


static void start(struct mwService *srvc) {
  struct mwServiceAware *srvc_aware;
  struct mwChannel *chan;

  srvc_aware = (struct mwServiceAware *) srvc;
  chan = make_blist(srvc->session->channels);

  if(chan != NULL) {
    srvc_aware->channel = chan;
  } else {
    mwService_stopped(srvc);
  }
}


static void stop(struct mwService *srvc) {
  struct mwServiceAware *srvc_aware;

  srvc_aware = (struct mwServiceAware *) srvc;
  mwChannel_destroyQuick(srvc_aware->channel, ERR_SUCCESS);
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
			    gpointer data) {

  g_return_if_fail(list != NULL);
  list->on_aware = cb;
  list->on_aware_data = data;
}


int mwAwareList_addAware(struct mwAwareList *list,
			 struct mwAwareIdBlock *id_list, guint count) {

  /* for each awareness id:
     - if it's already in the list, continue
     - if it's not in the service list:
       - create an awareness
       - add it to the service list
     - add this list to the membership
     - add to the list

     - send all added IDs (even those already added). It's easier :)
  */

  struct mwServiceAware *srvc;
  struct aware_entry *aware;

  GList *additions = NULL;
  int ret = 0;

  g_return_val_if_fail(list != NULL, -1);
  g_return_val_if_fail(list->service != NULL, -1);

  srvc = list->service;

  g_message("adding %i buddies", count);

  for(; count--; id_list++) {
    if(id_list->user == NULL || *id_list->user == '\0') {
      g_info("buddy's user id is empty, skipping");
      continue;
    }

    aware = g_hash_table_lookup(list->entries, id_list);
    if(aware) {
      g_info("buddy: %s, %s already exists",
	     id_list->user, id_list->community);
      continue;
    }

    aware = g_hash_table_lookup(srvc->entries, id_list);
    if(! aware) {
      g_info("adding buddy %s, %s to the aware service",
	     id_list->user, id_list->community);

      aware = g_new0(struct aware_entry, 1);
      mwAwareIdBlock_clone(ENTRY_KEY(aware), id_list);

      g_hash_table_insert(srvc->entries, ENTRY_KEY(aware), aware);
    }

    g_info("adding buddy %s, %s to the list",
	   id_list->user, id_list->community);
    aware->membership = g_list_append(aware->membership, list);
    g_hash_table_insert(list->entries, ENTRY_KEY(aware), aware);

    additions = g_list_prepend(additions, ENTRY_KEY(aware));
  }

  /* if the service is alive-- or getting there-- we'll need to send
     these additions upstream */
  if(MW_SERVICE_IS_LIVE(srvc))
    ret = send_add(srvc->channel, additions);

  g_list_free(additions);
  return ret;
}


int mwAwareList_removeAware(struct mwAwareList *list,
			    struct mwAwareIdBlock *id_list, guint count) {

  /* for each awareness id:
     - if it's not in the list, forget it
     - remove from the list
     - remove list from the membership

     - call remove round
  */

  struct mwServiceAware *srvc;
  struct aware_entry *aware;

  guint c = count;
  struct mwAwareIdBlock *idb = id_list;

  g_return_val_if_fail(list != NULL, -1);
  g_return_val_if_fail(list->service != NULL, -1);

  srvc = list->service;

  g_message("removing %u buddies", count);

  for(; c--; idb++) {
    aware = g_hash_table_lookup(list->entries, idb);

    if(! aware) {
      g_warning("buddy %s, %s not in list", idb->user, idb->community);
      continue;
    }

    g_hash_table_remove(list->entries, idb);
    aware->membership = g_list_remove(aware->membership, list);
  }

  return remove_unused(srvc);
}


void mwServiceAware_setStatus(struct mwServiceAware *srvc,
			      struct mwAwareIdBlock *user,
			      struct mwUserStatus *stat) {

  struct mwSnapshotAwareIdBlock *idb;
  idb = g_new0(struct mwSnapshotAwareIdBlock, 1);

  /* just reference the strings. then we don't need to free them */
  idb->id.type = user->type;
  idb->id.user = user->user;
  idb->id.community = user->community;

  idb->online = 1;

  idb->status.status = stat->status;
  idb->status.time = stat->time;
  idb->status.desc = stat->desc;

  status_recv(srvc, idb, 1);

  g_free(idb);
}


const char *mwServiceAware_getText(struct mwServiceAware *srvc,
				   struct mwAwareIdBlock *user) {

  struct aware_entry *aware;

  g_return_val_if_fail(srvc != NULL, NULL);
  g_return_val_if_fail(user != NULL, NULL);

  aware = g_hash_table_lookup(srvc->entries, user);
  g_return_val_if_fail(aware != NULL, NULL);

  return aware->aware.status.desc;
}



