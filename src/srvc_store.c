

#include <glib/glist.h>

#include "srvc_store.h"

#include "channel.h"
#include "error.h"
#include "message.h"
#include "mw_debug.h"
#include "service.h"
#include "session.h"


enum storage_action {
  action_load    = 0x0004,
  action_loaded  = 0x0005,
  action_save    = 0x0006,
  action_saved   = 0x0007,
};


struct mwStorageUnit {
  /** key by which data is referenced in service
      @see mwStorageKey */
  guint32 key;

  /** Data associated with key in service */
  struct mwOpaque data;
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


static void request_get(struct mwGetBuffer *b, struct mwStorageReq *req) {
  guint32 id, count, junk;

  if(req->item->data.len)
    mwOpaque_clear(&req->item->data);

  if(mwGetBuffer_error(b)) return;

  guint32_get(b, &id);
  guint32_get(b, &req->result_code);

  if(req->action == action_loaded) {
    guint32_get(b, &count);

    if(count > 0) {
      guint32_get(b, &junk);
      guint32_get(b, &req->item->key);
      mwOpaque_get(b, &req->item->data);
    }
  }
}


static void request_put(struct mwPutBuffer *b, struct mwStorageReq *req) {

  guint32_put(b, req->id);
  guint32_put(b, 1);

  if(req->action == action_save) {
    guint32_put(b, 20 + req->item->data.len); /* ugh, offset garbage */
    guint32_put(b, req->item->key);
    mwOpaque_put(b, &req->item->data);

  } else {
    guint32_put(b, req->item->key);
  }
}


static int request_send(struct mwChannel *chan, struct mwStorageReq *req) {
  struct mwPutBuffer *b;
  struct mwOpaque o;
  int ret;

  b = mwPutBuffer_new();
  request_put(b, req);

  mwPutBuffer_finalize(&o, b);
  ret = mwChannel_send(chan, req->action, &o);
  mwOpaque_clear(&o);

  if(! ret) {
    if(req->action == action_save) {
      req->action = action_saved;
    } else if(req->action == action_load) {
      req->action = action_loaded;
    }
  }

  return ret;
}


static struct mwStorageReq *request_find(struct mwServiceStorage *srvc,
					 guint32 id) {
  GList *l;

  for(l = srvc->pending; l; l = l->next) {
    struct mwStorageReq *r = l->data;
    if(r->id == id) return r;
  }

  return NULL;
}


static void request_trigger(struct mwServiceStorage *srvc,
			    struct mwStorageReq *req) {

  struct mwStorageUnit *item = req->item;

  g_message("storage request completed:"
	    " key = 0x%08x, result = 0x%08x, length = 0x%08x",
	    item->key, req->result_code, item->data.len);
  
  if(req->cb)
    req->cb(srvc, req->result_code, item, req->data);
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


static const char *get_name(struct mwService *srvc) {
  return "Storage Service";
}


static const char *get_desc(struct mwService *srvc) {
  return "Simple implementation of the storage service";
}


static struct mwChannel *make_channel(struct mwServiceStorage *srvc) {
  struct mwSession *session;
  struct mwChannelSet *cs;
  struct mwChannel *chan;

  session = mwService_getSession(MW_SERVICE(srvc));
  cs = mwSession_getChannels(session);
  chan = mwChannel_newOutgoing(cs);
 
  mwChannel_setService(chan, MW_SERVICE(srvc));
  mwChannel_setProtoType(chan, mwProtocol_STORAGE);
  mwChannel_setProtoVer(chan, 0x01);

  return mwChannel_create(chan)? NULL: chan;
}


static void start(struct mwService *srvc) {
  struct mwServiceStorage *srvc_store;
  struct mwChannel *chan;

  g_return_if_fail(srvc != NULL);
  srvc_store = (struct mwServiceStorage *) srvc;

  chan = make_channel(srvc_store);
  if(chan) {
    srvc_store->channel = chan;
  } else {
    mwService_stopped(srvc);
  }
}


static void stop(struct mwService *srvc) {
  /* - close the channel.
     - Reset all pending requests to a an "unsent" state. */

  struct mwServiceStorage *srvc_store;
  GList *l;

  g_return_if_fail(srvc != NULL);
  srvc_store = (struct mwServiceStorage *) srvc;

  if(srvc_store->channel) {
    mwChannel_destroy(srvc_store->channel, ERR_SUCCESS, NULL);
    srvc_store->channel = NULL;
  }

  /* reset all of the started requests to their unstarted states */
  for(l = srvc_store->pending; l; l = l->next) {
    struct mwStorageReq *req = l->data;

    if(req->action == action_loaded) {
      req->action = action_load;
    } else if(req->action == action_saved) {
      req->action = action_save;
    }
  }

  mwService_stopped(srvc);
}


static void recv_channelCreate(struct mwService *srvc,
			       struct mwChannel *chan,
			       struct mwMsgChannelCreate *msg) {

  /* what the?! we don't accept no incoming channels */
  mwChannel_destroy(chan, ERR_FAILURE, NULL);
}


static void recv_channelAccept(struct mwService *srvc,
			       struct mwChannel *chan,
			       struct mwMsgChannelAccept *msg) {
 
  struct mwServiceStorage *srvc_stor;
  GList *l;

  g_return_if_fail(srvc != NULL);
  srvc_stor = (struct mwServiceStorage *) srvc;

  g_return_if_fail(chan != NULL);
  g_return_if_fail(chan == srvc_stor->channel);

  /* send all pending requests */
  for(l = srvc_stor->pending; l; l = l->next) {
    struct mwStorageReq *req = l->data;

    if(req->action == action_save || req->action == action_load) {
      request_send(chan, req);
    }
  }

  mwService_started(srvc);
}


static void recv_channelDestroy(struct mwService *srvc,
				struct mwChannel *chan,
				struct mwMsgChannelDestroy *msg) {

  /* close, instruct the session to re-open */
  struct mwServiceStorage *srvc_stor;

  g_return_if_fail(srvc != NULL);
  g_return_if_fail(chan != NULL);

  srvc_stor = (struct mwServiceStorage *) srvc;
  srvc_stor->channel = NULL;

  mwService_stop(srvc);
}


static void recv(struct mwService *srvc, struct mwChannel *chan,
		 guint16 type, struct mwOpaque *data) {

  /* process into results, trigger callbacks */

  struct mwGetBuffer *b;
  struct mwServiceStorage *srvc_stor;
  struct mwStorageReq *req;
  guint32 id;

  g_return_if_fail(srvc != NULL);
  srvc_stor = (struct mwServiceStorage *) srvc;

  g_return_if_fail(chan != NULL);
  g_return_if_fail(chan == srvc_stor->channel);

  b = mwGetBuffer_wrap(data);

  id = guint32_peek(b);
  req = request_find(srvc_stor, id);

  if(! req) {
    g_warning("couldn't find request 0x%08x in storage service", id);
    mwGetBuffer_free(b);
    return;
  }

  g_return_if_fail(req->action == type);
  request_get(b, req);

  if(mwGetBuffer_error(b)) {
    g_warning("failed to parse storage service"
	      " request: 0x%08x, type: 0x%04x", type, id);

  } else {
    request_trigger(srvc_stor, req);
  }

  mwGetBuffer_free(b);
  request_remove(srvc_stor, req);
}


static void clear(struct mwService *srvc) {
  struct mwServiceStorage *srvc_stor;
  GList *l;

  srvc_stor = (struct mwServiceStorage *) srvc;

  for(l = srvc_stor->pending; l; l = l->next)
    request_free(l->data);

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

  if(data)
    mwOpaque_clone(&u->data, data);

  return u;
}


struct mwStorageUnit *mwStorageUnit_newBoolean(guint32 key,
					       gboolean val) {
  struct mwStorageUnit *u;

  u = g_new0(struct mwStorageUnit, 1);
  u->key = key;

  /* storage service expects booleans as full 32bit values */
  u->data.len = 4;
  u->data.data = g_malloc0(4);
  if(val) u->data.data[3] = 0x01;

  return u;
}


struct mwStorageUnit *mwStorageUnit_newString(guint32 key,
					      const char *str) {
  struct mwStorageUnit *u;
  struct mwPutBuffer *b;

  u = g_new0(struct mwStorageUnit, 1);
  u->key = key;

  b = mwPutBuffer_new();
  mwString_put(b, str);
  mwPutBuffer_finalize(&u->data, b);

  return u;
}


guint32 mwStorageUnit_getKey(struct mwStorageUnit *item) {
  g_return_val_if_fail(item != NULL, 0x00); /* feh, unsafe */
  return item->key;
}


gboolean mwStorageUnit_asBoolean(struct mwStorageUnit *item,
				 gboolean val) {
  struct mwGetBuffer *b;
  guint32 v = (guint32) val;

  g_return_val_if_fail(item != NULL, val);

  b = mwGetBuffer_wrap(&item->data);

  guint32_get(b, &v);
  val = !! v;

  mwGetBuffer_free(b);

  return val;
}


char *mwStorageUnit_asString(struct mwStorageUnit *item) {
  struct mwGetBuffer *b;
  char *c = NULL;

  g_return_val_if_fail(item != NULL, NULL);

  b = mwGetBuffer_wrap(&item->data);

  mwString_get(b, &c);

  if(mwGetBuffer_error(b))
    g_debug("error obtaining string value from opaque");

  mwGetBuffer_free(b);

  return c;
}


struct mwOpaque *mwStorageUnit_asData(struct mwStorageUnit *item) {
  g_return_val_if_fail(item != NULL, NULL);
  return &item->data;
}


void mwStorageUnit_free(struct mwStorageUnit *item) {
  if(! item) return;

  mwOpaque_clear(&item->data);
  g_free(item);
}


static struct mwStorageReq *request_new(struct mwServiceStorage *srvc,
					struct mwStorageUnit *item,
					mwStorageCallback cb,
					gpointer data) {

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

  if(MW_SERVICE_IS_STOPPED(srvc)) {
    mwService_start(MW_SERVICE(srvc));
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

  if(MW_SERVICE_IS_STOPPED(srvc)) {
    mwService_start(MW_SERVICE(srvc));
    return;
  }

  request_send(srvc->channel, req);
}

