

#include <glib/glist.h>

#include "channel.h"
#include "error.h"
#include "message.h"
#include "service.h"
#include "session.h"
#include "srvc_store.h"



enum storage_action {
  action_load    = 0x0004,
  action_loaded  = 0x0005,
  action_save    = 0x0006,
  action_saved   = 0x0007
};


struct mwStorageReq {
  guint32 id;                  /**< unique id for this request */
  guint32 result_code;         /**< result code for completed request */
  enum storage_action action;  /**< load or save */
  struct mwStorageUnit *item;  /**< the key/data pair */ 
  mwStorageCallback cb;        /**< callback to notify upon completion */
  gpointer data;               /**< user data to pass with callback */
};


struct mwServiceStorage {
  struct mwService service;

  /** collection of mwStorageReq */
  GList *pending;

  /** current service channel */
  struct mwChannel *channel;

  /** keep track of the counter */
  guint32 id_counter;
};


static gsize request_buflen(struct mwStorageReq *req) {
  /* req id, count, key */
  gsize len = 4 + 4 + 4;

  if(req->action == action_save) {
    len += 4; /* offset */
    len += mwOpaque_buflen(&req->item->data);
  }

  return len;
}


static int request_get(char **b, gsize *n, struct mwStorageReq *req) {
  guint32 id, count, junk;

  if( guint32_get(b, n, &id) ||
      guint32_get(b, n, &req->result_code) )
    return *n;

  if(req->action == action_loaded) {
    if( guint32_get(b, n, &count) ||
	guint32_get(b, n, &junk) ||
	guint32_get(b, n, &req->item->key) ||
	mwOpaque_get(b, n, &req->item->data) )
      return *n;
  }
  
  return 0;
}


static int request_put(char **b, gsize *n, struct mwStorageReq *req) {
  gsize len = *n;

  if( guint32_put(b, n, req->id) ||
      guint32_put(b, n, 1) )
    return *n;

  if(req->action == action_save) {
    if( guint32_put(b, n, len) ||
	guint32_put(b, n, req->item->key) ||
	mwOpaque_put(b, n, &req->item->data) )
      return *n;

  } else {
    if( guint32_put(b, n, req->item->key) )
      return *n;
  }

  return 0;
}


static int request_send(struct mwChannel *chan, struct mwStorageReq *req) {
  char *buf, *b;
  gsize len, n;

  int ret;

  len = n = request_buflen(req);
  buf = b = g_malloc0(len);

  if(request_put(&b, &n, req)) {
    g_free(buf);
    return -1;
  }

  ret = mwChannel_send(chan, req->action, buf, len);
  if(! ret) {
    if(req->action == action_save) {
      req->action = action_saved;
    } else if(req->action == action_load) {
      req->action = action_loaded;
    }
  }

  g_free(buf);
  return ret;
}


static struct mwStorageReq *request_find(struct mwServiceStorage *srvc,
					 guint32 id) {
  struct mwStorageReq *req = NULL;
  GList *l;

  for(l = srvc->pending; l; l = l->next) {
    struct mwStorageReq *r = (struct mwStorageReq *) l->data;
    if(r->id == id) {
      req = r;
      break;
    }
  }

  return req;
}


static void request_trigger(struct mwStorageReq *req) {
  if(req->cb)
    req->cb(req->result_code, req->item, req->data);
}


static void request_free(struct mwStorageReq *req) {
  mwStorageUnit_free(req->item);
  g_free(req);
}


static void request_remove(struct mwServiceStorage *srvc,
			   struct mwStorageReq *req) {

  srvc->pending = g_list_remove_all(srvc->pending, req);
  request_free(req);
}


static const char *get_name() {
  return "Storage Service";
}


static const char *get_desc() {
  return "Simple implementation of the storage service";
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

  msg->encrypt.type = chan->encrypt.type;
  msg->encrypt.opaque.len = n = 10;
  msg->encrypt.opaque.data = b = (char *) g_malloc(n);
   
  /* I really want to know what this opaque means. Every client seems
     to send it or something similar, and it doesn't work without it */
  guint32_put(&b, &n, 0x00000001);
  guint32_put(&b, &n, 0x00000000);
  guint16_put(&b, &n, 0x0000);

  ret = mwChannel_create(chan, msg);
  mwMessage_free(MW_MESSAGE(msg));

  if(ret) mwChannel_destroy(chan, NULL);

  return ret;
}


static struct mwChannel *make_channel(struct mwChannelSet *cs) {

  int ret = 0;
  struct mwChannel *chan = mwChannel_newOutgoing(cs);

  chan->status = mwChannel_INIT;
  chan->service = mwService_STORAGE;
  chan->proto_type = mwProtocol_STORAGE;
  chan->proto_ver = 0x01;
  chan->encrypt.type = mwEncrypt_RC2_40;

  ret = send_create(chan);
  return ret? NULL: chan;
}


static void start(struct mwService *srvc) {
  /* start to open the channel */

  struct mwServiceStorage *srvc_store;
  struct mwChannel *chan;

  g_return_if_fail(srvc != NULL);
  g_return_if_fail(MW_SERVICE_STOPPED(srvc));

  srvc_store = (struct mwServiceStorage *) srvc;
  srvc->state = mwServiceState_STARTING;

  g_message(" starting storage service");

  chan = make_channel(srvc->session->channels);
  if(chan) {
    srvc_store->channel = chan;
  } else {
    g_message(" stopped storage service");
    srvc->state = mwServiceState_STOPPED;
  }
}


static void stop(struct mwService *srvc) {
  /* - close the channel.
     - Reset all pending requests to a an "unsent" state. */

  struct mwServiceStorage *srvc_store;
  GList *l;

  g_return_if_fail(srvc != NULL);
  g_return_if_fail(MW_SERVICE_STARTED(srvc));

  srvc_store = (struct mwServiceStorage *) srvc;

  srvc->state = mwServiceState_STOPPING;
  g_message(" stopping storage service");

  if(srvc_store->channel) {
    mwChannel_destroyQuick(srvc_store->channel, ERR_ABORT);
    srvc_store->channel = NULL;
  }

  for(l = srvc_store->pending; l; l = l->next) {
    struct mwStorageReq *req = (struct mwStorageReq *) l->data;
    if(req->action == action_loaded) {
      req->action = action_load;
    } else if(req->action == action_saved) {
      req->action = action_save;
    }
  }

  srvc->state = mwServiceState_STOPPED;
  g_message(" stopped storage service");
}


static void recv_channelCreate(struct mwService *srvc, struct mwChannel *chan,
			       struct mwMsgChannelCreate *msg) {

  /* close it! we don't accept incoming channels. */
  mwChannel_destroyQuick(chan, ERR_FAILURE);
}


static void recv_channelAccept(struct mwService *srvc, struct mwChannel *chan,
			       struct mwMsgChannelAccept *msg) {
  /* send all pending */
  
  struct mwServiceStorage *srvc_stor;
  GList *l;

  g_return_if_fail(srvc != NULL);
  g_return_if_fail(chan != NULL);

  g_return_if_fail(srvc->state == mwServiceState_STARTING);

  srvc_stor = (struct mwServiceStorage *) srvc;
  g_return_if_fail(chan == srvc_stor->channel);

  for(l = srvc_stor->pending; l; l = l->next) {
    struct mwStorageReq *req = (struct mwStorageReq *) l->data;
    if(req->action == action_save || req->action == action_load) {
      request_send(chan, req);
    }
  }

  srvc->state = mwServiceState_STARTED;
  g_message(" started storage service");
}


static void recv_channelDestroy(struct mwService *srvc, struct mwChannel *chan,
				struct mwMsgChannelDestroy *msg) {

  /* close, instruct the session to re-open */
  struct mwServiceStorage *srvc_stor;

  g_return_if_fail(srvc != NULL);
  g_return_if_fail(chan != NULL);

  srvc_stor = (struct mwServiceStorage *) srvc;
  srvc_stor->channel = NULL;

  stop(srvc);
}


static void recv(struct mwService *srvc, struct mwChannel *chan,
		 guint16 msg_type, const char *buf, gsize len) {

  /* process into results, trigger callbacks */

  struct mwServiceStorage *srvc_stor;
  struct mwStorageReq *req;
  guint32 id;
  int ret = 0;

  char *b = (char *) buf;
  gsize n = len;

  g_return_if_fail(chan != NULL);
  g_return_if_fail(srvc != NULL);

  srvc_stor = (struct mwServiceStorage *) srvc;
  g_return_if_fail(chan == srvc_stor->channel);

  id = guint32_peek(buf, len);
  req = request_find(srvc_stor, id);

  if(! req) {
    g_warning("couldn't find request 0x%08x in storage service", id);
    return;
  }

  g_return_if_fail(req->action == msg_type);
  ret = request_get(&b, &n, req);

  if(ret) {
    g_warning("failed to parse request 0x%08x of type 0x%04x"
	      " for storage service", msg_type, id);
  } else {
    request_trigger(req);
  }

  request_remove(srvc_stor, req);
}


static void clear(struct mwService *srvc) {

  struct mwServiceStorage *srvc_stor;
  GList *l;

  stop(srvc);

  g_return_if_fail(srvc != NULL);
  srvc_stor = (struct mwServiceStorage *) srvc;

  for(l = srvc_stor->pending; l; l = l->next) {
    request_free((struct mwStorageReq *) l->data);
    l->data = NULL;
  }
  g_list_free(srvc_stor->pending);
  srvc_stor->pending = NULL;

  srvc_stor->id_counter = 0;
}


struct mwServiceStorage *mwServiceStorage_new(struct mwSession *session) {
  struct mwServiceStorage *srvc_store;
  struct mwService *srvc;

  srvc_store = g_new0(struct mwServiceStorage, 1);

  srvc = MW_SERVICE(srvc_store);
  mwService_init(srvc, session, mwService_STORAGE);
  srvc->get_name = get_name;
  srvc->get_desc = get_desc;
  srvc->recv_channelCreate = recv_channelCreate;
  srvc->recv_channelAccept = recv_channelAccept;
  srvc->recv_channelDestroy = recv_channelDestroy;
  srvc->recv = recv;
  srvc->start = start;
  srvc->stop = stop;
  srvc->clear = clear;

  return srvc_store;
}


struct mwStorageUnit *mwStorageUnit_new(guint32 key) {
  struct mwStorageUnit *u;
  u = g_new0(struct mwStorageUnit, 1);
  u->key = key;
  return u;
}


struct mwStorageUnit *mwStorageUnit_newData(guint32 key,
					    struct mwOpaque *data) {
  struct mwStorageUnit *u;
  u = g_new0(struct mwStorageUnit, 1);
  u->key = key;

  if(data) mwOpaque_clone(&u->data, data);

  return u;
}


struct mwStorageUnit *mwStorageUnit_newBoolean(guint32 key,
					       gboolean val) {
  struct mwStorageUnit *u;
  u = g_new0(struct mwStorageUnit, 1);
  u->key = key;

  u->data.len = 4;
  u->data.data = g_malloc0(4);
  if(val) u->data.data[3] = 0x01;

  return u;
}


struct mwStorageUnit *mwStorageUnit_newString(guint32 key,
					      const char *str) {
  struct mwStorageUnit *u;
  char *b;
  gsize n;

  u = g_new0(struct mwStorageUnit, 1);
  u->key = key;

  n = u->data.len = mwString_buflen(str);
  b = u->data.data = (char *) g_malloc0(n);
  mwString_put(&b, &n, str);

  return u;
}


gboolean mwStorageUnit_asBoolean(struct mwStorageUnit *item, gboolean val) {
  g_return_val_if_fail(item != NULL, val);

  if(item->data.len == 4)
    val = !! guint32_peek(item->data.data, item->data.len);

  return val;
}


char *mwStorageUnit_asString(struct mwStorageUnit *item) {
  char *c = NULL;
  gsize a, b;

  g_return_val_if_fail(item != NULL, NULL);

  a = item->data.len;
  if(a > 2) {
    char *buf = item->data.data;
    gsize len = item->data.len;
    
    b = guint16_peek(buf, len);
    if(a > b) {
      mwString_get(&buf, &len, &c);
    } else {
      g_message("tried to get a string from %u bytes, "
		"but the heading only indicated %u bytes",
		a, b);
    }
  }

  return c;
}


void mwStorageUnit_free(struct mwStorageUnit *item) {
  if(item) {
    mwOpaque_clear(&item->data);
    g_free(item);
  }
}


static struct mwStorageReq *request_new(struct mwServiceStorage *srvc,
					struct mwStorageUnit *item,
					mwStorageCallback cb, gpointer data) {

  struct mwStorageReq *req = g_new0(struct mwStorageReq, 1);

  req->id = ++srvc->id_counter;
  req->item = item;
  req->cb = cb;
  req->data = data;

  return req;
}


void mwServiceStorage_load(struct mwServiceStorage *srvc,
			   struct mwStorageUnit *item,
			   mwStorageCallback cb, gpointer data) {

  /* - construct a request
     - put request at end of pending
     - if channel is open and connected
       - compose the load message
       - send message
       - set request to sent
     - else
       - start service
  */ 

  struct mwStorageReq *req;

  req = request_new(srvc, item, cb, data);
  req->action = action_load;

  srvc->pending = g_list_append(srvc->pending, req);

  if(MW_SERVICE_STOPPED(MW_SERVICE(srvc))) {
    start(MW_SERVICE(srvc));
    return;
  }

  request_send(srvc->channel, req);
}


void mwServiceStorage_save(struct mwServiceStorage *srvc,
			   struct mwStorageUnit *item,
			   mwStorageCallback cb, gpointer data) {

  /* - construct a request
     - put request at end of pending
     - if channel is open and connected
       - compose the save message
       - send message
       - set request to sent
     - else
       - start service
  */

  struct mwStorageReq *req;

  req = request_new(srvc, item, cb, data);
  req->action = action_save;

  srvc->pending = g_list_append(srvc->pending, req);

  if(MW_SERVICE_STOPPED(MW_SERVICE(srvc))) {
    start(MW_SERVICE(srvc));
    return;
  }

  request_send(srvc->channel, req);
}

