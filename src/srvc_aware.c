

#include <glib.h>
#include <glib/glist.h>

#include "channel.h"
#include "message.h"
#include "service.h"
#include "session.h"
#include "srvc_aware.h"


static void recv_channelCreate(struct mwService *srvc, struct mwChannel *chan,
			       struct mwMsgChannelCreate *msg) {

  ; /* nuttin'. We only like outgoing blist channels */
}


static void recv_channelAccept(struct mwService *srvc, struct mwChannel *chan,
			       struct mwMsgChannelAccept *msg) {
  ; /* nuttin' */
}


static void recv_channelDestroy(struct mwService *srvc, struct mwChannel *chan,
				struct mwMsgChannelDestroy *msg) {
  /* TODO:
     - re-open the buddy list channel
     - re-send all the buddy subscriptions
  */
}


static guint id_hash(gconstpointer v) {
  return g_str_hash( ((struct mwIdBlock *)v)->user );
}


static gboolean id_equal(gconstpointer a, gconstpointer b) {
  return mwIdBlock_equal((struct mwIdBlock *)a, (struct mwIdBlock *)b);
}


static void id_free(gpointer v) {
  mwIdBlock_clear((struct mwIdBlock *) v);
  g_free(v);
}


static void status_recv(struct mwServiceAware *srvc,
			struct mwSnapshotAwareIdBlock *idb,
			unsigned int count) {

  unsigned int c;
  struct mwIdBlock *i;
  struct mwSnapshotAwareIdBlock *adb;

  for(c = count; c--; ) {
    adb = idb + c;

    i = g_new(struct mwIdBlock, 1);
    i->user = g_strdup(adb->id.user);
    i->community = g_strdup(adb->id.community);

    if(idb->online) {
      g_hash_table_insert(srvc->buddy_text, i, g_strdup(adb->status.desc));

    } else {
      g_hash_table_remove(srvc->buddy_text, i);
      id_free(i);
    }
  }
  
  if(srvc->got_aware)
    srvc->got_aware(srvc, idb, count);
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

  if(srvc->got_aware) {
    idb = g_new0(struct mwSnapshotAwareIdBlock, 1);
    ret = mwSnapshotAwareIdBlock_get((char **) &b, &n, idb);

    if(ret == 0)
      status_recv(srvc, idb, 1);

    mwSnapshotAwareIdBlock_clear(idb);
    g_free(idb);
  }

  return ret;
}


static void recv(struct mwService *srvc, struct mwChannel *chan,
		 guint32 type, const char *b, gsize n) {

  struct mwServiceAware *srvc_aware = (struct mwServiceAware *) srvc;
  int ret;

  g_return_if_fail(srvc->session == chan->session);

  switch(type) {
  case mwChannelSend_AWARE_SNAPSHOT:
    ret = SNAPSHOT_recv(srvc_aware, b, n);
    break;

  case mwChannelSend_AWARE_UPDATE:
    ret = UPDATE_recv(srvc_aware, b, n);
    break;

  default:
    g_warning("unknown message type %x for aware service\n", type);
  }

  if(ret) ; /* handle how? */
}


static void clear(struct mwService *srvc) {
  struct mwServiceAware *srvc_aware = (struct mwServiceAware *) srvc;

  g_hash_table_destroy(srvc_aware->buddy_text);
  srvc_aware->buddy_text = NULL;
}


static const char *name() {
  return "Basic Presence Awareness";
}


static const char *desc() {
  return "A simple Buddy List service";
}


struct mwServiceAware *mwServiceAware_new(struct mwSession *session) {
  struct mwServiceAware *srvc_aware = g_new0(struct mwServiceAware, 1);
  struct mwService *srvc = &srvc_aware->service;

  g_assert(session);

  srvc->session = session;
  srvc->type = Service_AWARE; /* one of the BaseSericeTypes */

  srvc->recv_channelCreate = recv_channelCreate;
  srvc->recv_channelAccept = recv_channelAccept;
  srvc->recv_channelDestroy = recv_channelDestroy;
  srvc->recv = recv;
  srvc->clear = clear;
  srvc->get_name = name;
  srvc->get_desc = desc;

  srvc_aware->buddy_text = g_hash_table_new_full(id_hash, id_equal,
						 id_free, g_free);

  return srvc_aware;
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
  /* six bytes of zero follow */

  ret = mwChannel_create(chan, msg);
  mwMessage_free(MESSAGE(msg));

  /* don't want this channel to time-out */
  chan->inactive = 0x00;

  return ret;
}


static struct mwChannel *make_blist(struct mwChannelSet *cs) {

  struct mwChannel *chan = mwChannel_newOutgoing(cs);
  chan->status = mwChannel_INIT;

  chan->service = Service_AWARE;
  chan->proto_type = Protocol_AWARE;
  chan->proto_ver = 0x00030005;

  return send_create(chan)? NULL: chan;
}


static struct mwChannel *find_blist(struct mwChannelSet *cs) {
  GList *l;

  for(l = cs->outgoing; l; l = l->next) {
    struct mwChannel *chan = (struct mwChannel *) l->data;
    if(chan->service == Service_AWARE) return chan;
  }

  return NULL;
}


static gsize size_up(struct mwIdBlock *list, unsigned int count) {
  gsize len = 0;
  while(count--) len += (2 + mwIdBlock_buflen(list + count));
  return len;
}


int mwServiceAware_add(struct mwServiceAware *srvc,
		       struct mwIdBlock *list, unsigned int count) {

  /* - get the buddy list channel
     - if there isn't one, open one.
     - send the aware_add over the channel
  */

  char *buf, *b;
  gsize len, n;

  int ret;

  struct mwChannelSet *cs;
  struct mwChannel *chan;

  g_message("adding %i buddies", count);

  cs = srvc->service.session->channels;
  
  chan = find_blist(cs);
  if(! chan) chan = make_blist(cs);
  g_return_val_if_fail(chan, -1);

  len = n = size_up(list, count) + 4;
  buf = b = (char *) g_malloc0(n);

  guint32_put(&b, &n, count);

  while(count--) {
    guint16_put(&b, &n, mwAware_USER); /* only supports users */
    mwIdBlock_put(&b, &n, list + count);
  }

  ret = mwChannel_send(chan, mwChannelSend_AWARE_ADD, buf, len);
  g_free(buf);

  return ret;
}


int mwServiceAware_remove(struct mwServiceAware *srvc,
			  struct mwIdBlock *list, unsigned int count) {

  /* - get the buddy list channel
     - if there isn't one, fine.
     - send the aware_remove over the channel
  */

  char *buf, *b;
  gsize len, n;

  int ret;

  struct mwChannelSet *cs;
  struct mwChannel *chan;

  g_message("removing %u buddies", count);

  cs = srvc->service.session->channels;

  g_return_val_if_fail( (chan = find_blist(cs)), -1 );

  len = n = size_up(list, count) + 4;
  buf = b = (char *) g_malloc0(n);

  guint32_put(&b, &n, count);

  while(count--) {
    guint16_put(&b, &n, mwAware_USER);
    mwIdBlock_put(&b, &n, list + count);
  }

  ret = mwChannel_send(chan, mwChannelSend_AWARE_REMOVE, buf, len);
  g_free(buf);

  return ret;
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
				   struct mwIdBlock *user) {

  return g_hash_table_lookup(srvc->buddy_text, user);
}


