

#include <glib/glist.h>

#include "channel.h"
#include "error.h"
#include "message.h"
#include "service.h"
#include "session.h"
#include "srvc_store.h"



enum storage_action {
  action_load   = 0x0004,
  action_loaded = 0x0005,
  action_save   = 0x0006,
  action_saved  = 0x0007
};


struct mwStorageReq {
  guint32 id;                  /**< unique id for this request */
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


static const char *get_name() {
  return "Storage Service";
}


static const char *get_desc() {
  return "Simple implementation of the storage service";
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

  srvc_stor = (struct mwServiceStorage *) srvc;

  for(l = srvc_stor->pending; l; l = l->next) {
    
  }
}


static void recv_channelDestroy(struct mwService *srvc, struct mwChannel *chan,
				struct mwMsgChannelDestroy *msg) {

  /* close, instruct the session to re-open */
}


static void recv(struct mwService *srvc, struct mwChannel *chan,
		 guint32 msg_type, const char *b, gsize n) {

  /* process into results, trigger callbacks */
}


static void start(struct mwService *srvc) {
  /* start to open the channel */
}


static void stop(struct mwService *srvc) {
  /* - close the channel.
     - Reset all pending requests to a an "unsent" state. */

  struct mwServiceStorage *srvc_store;
  GList *l;

  g_return_if_fail(srvc != NULL);
  srvc_store = (struct mwServiceStorage *) srvc;

  mwChannel_destroyQuick(srvc_store->channel, ERR_ABORT);

  for(l = srvc_store->pending; l; l = l->next) {
    struct mwStorageReq *req = (struct mwStorageReq *) l->data;
    if(req->action == action_loaded) {
      req->action = action_load;
    } else if(req->action == action_saved) {
      req->action = action_save;
    }
  }
}


static void clear(struct mwService *srvc) {
  stop(srvc);
  /** @todo free all pending */
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
    
    b = guint16_peek(buf,len);
    if(a == (b+2)) {
      mwString_get(&buf, &len, &c);
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

  /* todo:
     compose load message
     send load message
     set request to action_loaded */
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

  /* todo:
     compose save message
     send save message
     set request to action_saved */
}

