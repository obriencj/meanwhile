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


#include <glib/glist.h>

#include "mw_channel.h"
#include "mw_debug.h"
#include "mw_error.h"
#include "mw_message.h"
#include "mw_service.h"
#include "mw_session.h"
#include "mw_srvc_store.h"


/* channel identifiers for the storage service */
#define MW_SERVICE_ID  0x00000018
#define MW_PROTO_TYPE  0x00000025
#define MW_PROTO_VER   0x00000001


/* channel message types, also used in mw_storage_request */
enum storage_action {
  action_load     = 0x0004,
  action_loaded   = 0x0005,
  action_save     = 0x0006,
  action_saved    = 0x0007, 
  action_updated  = 0x0008, /**< ?? when another session updates the setting */
};


typedef struct mw_storage_request MwStorageRequest;


struct mw_storage_request {
  guint32 event_id;            /**< unique id for this request */

  guint32 key;
  MwOpaque unit;               /**< only used for load requests */

  guint32 result_code;         /**< result code for completed request */
  enum storage_action action;  /**< load or save */

  MwStorageCallback cb;        /**< callback to notify upon completion */
  gpointer data;               /**< user data to pass with callback */
};


struct mw_storage_service_private {
  GHashTable *pending;
  MwChannel *channel;
  guint event_counter;
};


MwStorageRequest *request_new(MwStorageService *srvc) {
  MwStorageRequest *req;

  req = g_new0(MwStorageRequest, 1);
  req->event_id = ++(srvc->event_counter);

  g_hash_table_insert(srvc->pending, G_UINT_TO_POINTER(req->event_id), req);

  return req;
}


static void request_get(MwGetBuffer *b, MwStorageRequest *req) {
  guint32 id, count, junk;

  if(MwGetBuffer_error(b)) return;

  mw_uint32_get(b, &id);
  mw_uint32_get(b, &req->result_code);

  if(req->action == action_loaded) {
    mw_uint32_get(b, &count);

    if(count > 0) {
      mw_uint32_get(b, &junk);
      mw_uint32_get(b, &req->key);
      MwOpaque_get(b, &req->unit.data);
    }
  }
}


static void request_put(MwPutBuffer *b, const MwStorageRequest *req) {

  mw_uint32_put(b, req->event_id);
  mw_uint32_put(b, 1);

  if(req->action == action_save) {
    mw_uint32_put(b, 20 + req->unit.len); /* ugh, offset garbage */
    mw_uint32_put(b, req->key);
    MwOpaque_put(b, &req->unit.data);

  } else {
    mw_uint32_put(b, req->key);
  }
}


static void request_send(MwChannel *chan, const MwStorageRequest *req) {
  MwPutBuffer *b;
  MwOpaque o;

  b = MwPutBuffer_new();
  request_put(b, req);

  MwPutBuffer_free(b, &o);
  MwChannel_send(chan, req->action, &o);
  MwOpaque_clear(&o);

  if(req->action == action_save) {
    req->action = action_saved;

  } else if(req->action == action_load) {
    req->action = action_loaded;
  }
}


static void request_debug(MwStorageRequest *req) {
  const gchar *nm = "UNKNOWN";

  g_return_if_fail(req != NULL);

  switch(req->action) {
  case action_load:    nm = "load"; break;
  case action_loaded:  nm = "loaded"; break;
  case action_save:    nm = "save"; break;
  case action_saved:   nm = "saved"; break;
  }

  g_debug("storage request %s: key = 0x%x, result = 0x%x, length = %u",
	  nm, req->key, req->result_code, (guint) req.data.len);
}


static void request_trigger(MwStorageService *srvc,
			    MwStorageRequest *req) {
  request_debug(req);

  if(req->cb)
    req->cb(srvc, req->result_code, req->key, &req->unit, req->data);
}


/* should only be used via request_remove's g_hash_table_remove */
static void request_free(MwStorageRequest *req) {
  if(! req) return;
  MwOpaque_clear(&req->unit);
  g_free(req);
}


static MwStorageRequest *
request_find(MwStorageService *self, guint event) {
  return g_hash_table_lookup(srvc->pending, G_UINT_TO_POINTER(event));
}


static void request_remove(MwStorageService *srvc, guint event) {
  g_hash_table_remove(srvc->pending, G_UINT_TO_POINTER(event));
}


static const gchar *get_name(struct mwService *srvc) {
  return "User Storage";
}


static const gchar *get_desc(struct mwService *srvc) {
  return "Stores user data and settings on the server";
}


static void mw_channel_recv(MwChannel *chan,
			    guint type, const MwOpaque *msg,
			    MwStorageService *self) {

  /* process into results, trigger callbacks */

  MwGetBuffer *gb;
  guint32 id;
  MwStorageRequest *req;

  gb = MwGetBuffer_wrap(data);

  id = guint32_peek(gb);
  req = request_find(srvc_stor, id);

  if(! req) {
    g_warning("couldn't find request 0x%x in storage service", id);
    MwGetBuffer_free(gb);
    return;
  }

  g_return_if_fail(req->action == type);
  request_get(gb, req);

  if(MwGetBuffer_error(b)) {
    mw_mailme_opaque(msg, "storage request 0x%x, type: 0x%x", id, type);

  } else {
    request_trigger(self, req);
  }

  MwGetBuffer_free(b);
  request_remove(self, req);
}


static void mw_channel_state(MwChannel *chan, gint state, MwService *self) {
  switch(state) {
  case mw_channel_open:
    /* ok, we're fully started now */
    MwService_started(self);
    break;

  case mw_channel_error:
    /* channel errored out, service does the same */
    MwService_error(self);
    break;

  case mw_channel_closed:
    {
      /* if the service was in the process of stopping, then this completes
	 the process. Otherwise, stop the service */

      gint srvc_state;
      g_object_get(G_OBJECT(self), "state", &srvc_state, NULL);

      if(srvc_state == mw_service_stopping) {
	MwService_stopped(self);
      } else {
	MwService_stop(self);
      }
      break;
    }
  }
}


static gboolean *mw_start(MwService *self) {
  MwChannel *chan = NULL;
  MwSession *session = NULL;

  g_object_get(G_OBJECT(self),
	       "channel", &chan,
	       "session", &session,
	       NULL);

  /* create the chanel if we don't have one sitting around */
  if(! chan) {
    chan = MwSession_newChannel(session);

    g_object_set(G_OBJECT(chan),
		 "service-id", MW_SERVICE_ID,
		 "protocol-type", MW_PROTO_TYPE,
		 "protocol-version", MW_PROTO_VER,
		 "offered-policy", mw_channel_encrypt_WHATEVER,
		 NULL);
    
    g_object_set(G_OBJECT(self), "channel", chan, NULL);

    g_object_connect(G_OBJECT(chan), "incoming",
		     G_CALLBACK(mw_channel_recv), self);

    g_object_connect(G_OBJECT(chan), "state-changed",
		     G_CALLBACK(mw_channel_state), self);
  }

  /* open/re-open our channel */
  MwChannel_open(chan, NULL);

  mw_gobject_unref(session);
  mw_gobject_unref(chan);

  /* we're not fully started until the channel is open */
  return FALSE;
}


static gboolean mw_stop(MwService *self) {
  MwChannel *chan;

  g_object_get(G_OBJECT(self), "channel", &chan, NULL);

  /* @todo clear out all the existing events */

  if(chan) {
    gint chan_state;
    g_object_get(G_OBJECT(chan), "state", &chan_state, NULL);

    if(chan_state == mw_channel_open ||
       chan_state == mw_channel_pending) {

      MwChannel_close(chan, 0x00);
      return FALSE;
    }
  }

  return TRUE;
}


static GObject *
mw_srvc_constructor(GType type, guint props_count,
		    GObjectConstructParam *props) {

  MwStorageServiceClass *klass;

  GObject *obj;
  MwStorageService *self;
  MwStorageServicePrivate *priv;

  mw_debug_enter();

  klass = MW_STORAGE_SERVICE_CLASS(g_type_class_peek(MW_TYPE_STORAGE_SERVICE));

  obj = srvc_parent_class->constructor(type, props_count, props);
  self = (MwStorageService *) obj;

  priv = self->private;

  /* ... */

  mw_debug_exit();
  return obj;
}


static void mw_srvc_class_init(gpointer gclass, gpointer gclass_data) {
  GObjectClass *gobject_class = gclass;
  MwServiceClass *service_class = gclass;
  MwIMServiceClass *klass = gclass;

  srvc_parent_class = g_type_class_peek_parent(gobject_class);

  gobject_class->constructor = mw_srvc_constructor;
  gobject_class->dispose = mw_srvc_dispose;

  /* ... */
}


static void mw_srvc_init(GTypeInstance *instance, gpointer g_class) {
  MwStorageService *self;

  self = (MwStorageService *) instance;
  self->private = g_new0(MwStorageServicePrivate, 1);
}


static const GTypeInfo mw_srvc_info = {
  .class_size = sizeof(MwStorageServiceClass),
  .base_init = NULL,
  .base_finalize = NULL,
  .class_init = mw_srvc_class_init,
  .class_finalize = NULL,
  .class_data = NULL,
  .instance_size = sizeof(MwStorageService),
  .n_preallocs = 0,
  .instance_init = mw_srvc_init,
  .value_table = NULL,
};


GType MwStorageService_getType() {
  GType type = 0;

  if(type == 0) {
    type = g_type_register_static(MW_TYPE_SERVICE, "MwStorageServiceType",
				  &mw_srvc_info, 0);
  }

  return type;
}


MwStorageService *MwStorageService_new(MwSession *session) {
  MwStorageService *self;

  self = g_object_new(MW_TYPE_STORAGE_SERVICE,
		      "session", session,
		      "auto-start", TRUE,
		      NULL);

  return self;
}


/* a MwStorageCallback that invokes a GClosure */
static void mw_closure_storage_cb(MwStorageService *srvc, guint event,
				  guint32 code,
				  guint32 key, const MwOpaque *unit,
				  gpointer closure) {
  GValue a, b, c, d;

  g_value_init(&a, G_TYPE_UINT);
  g_value_set_uint(&a, event);

  g_value_init(&b, G_TYPE_UINT);
  g_value_set_uint(&b, code);

  g_value_init(&c, G_TYPE_UINT);
  g_value_set_uint(&c, key);

  g_value_init(&d, MW_TYPE_OPAQUE);
  g_value_set_boxed(&d, unit);

  g_closure_invoke(closure, NULL, 4, &a, &b, &c, &d, NULL);

  g_value_clear_boxed(&d);
}
				  

guint MwStorageService_load(MwStorageService *srvc, guint32 key,
			    MwStorageCallback cb, gpointer user_data) {

  guint (*fn)(MwStorageService *, guint32, MwStorageCallback, gpointer);

  g_return_val_if_fail(srvc != NULL, NULL);

  fn = MW_STORAGE_SERVICE_GET_CLASS(srvc)->load;
  return fn(srvc, key, cb, user_data);
}


guint MwStorageService_loadClosure(MwStorageService *srvc, guint32 key,
				   GClosure *closure) {

  return MwStorageService_load(srvc, key, mw_closure_storage_cb, closure);
}


guint MwStorageService_save(MwStorageService *srvc,
			    guint32 key, const MwOpaque *unit,
			    MwStorageCallback cb, gpointer user_data) {

  guint (*fn)(MwStorageService *, guint32, const MwOpaque *,
	      MwStorageCallback, gpointer);

  g_return_val_if_fail(srvc != NULL, NULL);

  fn = MW_STORAGE_SERVICE_GET_CLASS(srvc)->save;
  return fn(srvc, key, unit, cb, user_data);
}


guint MwStorageService_saveClosure(MwStorageService *srvc,
				   guint32 key, const MwOpaque *unit,
				   GClosure *closure) {

  return MwStorageService_save(srvc, key, unit,
			       mw_closure_storage_cb, closure);
}


guint MwStorageService_watch(MwStorageService *srvc, guint32 key,
			     MwStorageCallback cb, gpointer user_data) {

  guint (*fn)(MwStorageService *, guint32, MwStorageCallback, gpointer);

  g_return_val_if_fail(srvc != NULL, NULL);

  fn = MW_STORAGE_SERVICE_GET_CLASS(srvc)->watch;
  return fn(srvc, key, cb, user_data);
}


guint MwStorageService_watchClosure(MwStorageService *srvc, guint32 key,
				    GClosure *closure) {
  
  return MwStorageService_watch(srvc, key, mw_closure_storage_cb, closure);
}


void MwStorageService_cancel(MwStorageService *srvc, guint event) {
  void (*fn)(MwStorageService *, guint);

  g_return_if_fail(srvc != NULL);

  fn = MW_STORAGE_SERVICE_GET_CLASS(srvc)->cancel;
  fn(srvc, event);
}


/* ---- */


struct mwStorageUnit *mwStorageUnit_newOpaque(guint32 key,
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

  return mwStorageUnit_newInteger(key, (guint32) val);
}


struct mwStorageUnit *mwStorageUnit_newInteger(guint32 key,
					       guint32 val) {
  struct mwStorageUnit *u;
  struct mwPutBuffer *b;

  u = g_new0(struct mwStorageUnit, 1);
  u->key = key;
  
  b = mwPutBuffer_new();
  guint32_put(b, val);
  mwPutBuffer_finalize(&u->data, b);

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

  return !! mwStorageUnit_asInteger(item, (guint32) val);
}


guint32 mwStorageUnit_asInteger(struct mwStorageUnit *item,
				guint32 val) {
  struct mwGetBuffer *b;
  guint32 v;

  g_return_val_if_fail(item != NULL, val);

  b = mwGetBuffer_wrap(&item->data);

  guint32_get(b, &v);
  if(! mwGetBuffer_error(b)) val = v;
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


struct mwOpaque *mwStorageUnit_asOpaque(struct mwStorageUnit *item) {
  g_return_val_if_fail(item != NULL, NULL);
  return &item->data;
}


void mwStorageUnit_free(struct mwStorageUnit *item) {
  if(! item) return;

  mwOpaque_clear(&item->data);
  g_free(item);
}


static struct mwStorageReq *request_new(struct mwStorageService *srvc,
					struct mwStorageUnit *item,
					mwStorageCallback cb,
					gpointer data, GDestroyNotify df) {

  struct mwStorageReq *req = g_new0(struct mwStorageReq, 1);

  req->id = ++srvc->id_counter;
  req->item = item;
  req->cb = cb;
  req->data = data;
  req->data_free = df;

  return req;
}


void mwStorageService_load(struct mwStorageService *srvc,
			   struct mwStorageUnit *item,
			   mwStorageCallback cb,
			   gpointer data, GDestroyNotify d_free) {

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

  req = request_new(srvc, item, cb, data, d_free);
  req->action = action_load;

  srvc->pending = g_list_append(srvc->pending, req);

  if(MW_SERVICE_IS_STARTED(MW_SERVICE(srvc)))
    request_send(srvc->channel, req);
}


void mwStorageService_save(struct mwStorageService *srvc,
			   struct mwStorageUnit *item,
			   mwStorageCallback cb,
			   gpointer data, GDestroyNotify d_free) {

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

  req = request_new(srvc, item, cb, data, d_free);
  req->action = action_save;

  srvc->pending = g_list_append(srvc->pending, req);

  if(MW_SERVICE_IS_STARTED(MW_SERVICE(srvc)))
    request_send(srvc->channel, req);
}

