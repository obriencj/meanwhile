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

#include "mw_common.h"
#include "mw_channel.h"
#include "mw_debug.h"
#include "mw_error.h"
#include "mw_marshal.h"
#include "mw_message.h"
#include "mw_object.h"
#include "mw_service.h"
#include "mw_session.h"
#include "mw_srvc_store.h"


static GObjectClass *parent_class = NULL;


/* channel identifiers for the storage service */
#define MW_SERVICE_ID  0x00000018
#define MW_PROTO_TYPE  0x00000025
#define MW_PROTO_VER   0x00000001


enum properties {
  property_channel = 1,
};


/* channel message types, also used in mw_storage_request */
enum storage_action {
  action_load     = 0x0004,
  action_loaded   = 0x0005,
  action_save     = 0x0006,
  action_saved    = 0x0007, 
};


typedef struct mw_storage_request MwStorageRequest;


struct mw_storage_request {
  guint32 event_id;            /**< unique id for this request */
  guint32 key;
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
  MwStorageServicePrivate *priv;
  MwStorageRequest *req;

  priv = srvc->private;
  g_return_val_if_fail(priv != NULL, NULL);

  req = g_new0(MwStorageRequest, 1);
  req->event_id = ++(priv->event_counter);

  g_hash_table_insert(priv->pending, GUINT_TO_POINTER(req->event_id), req);

  return req;
}


static void request_send(MwStorageService *self,
			 MwStorageRequest *req,
			 const MwOpaque *data) {

  MwChannel *chan = NULL;
  MwPutBuffer *b;
  MwOpaque o;
  enum storage_action act = req->action;

  g_object_get(G_OBJECT(self), "channel", &chan, NULL);

  g_return_if_fail(chan != NULL);

  b = MwPutBuffer_new();

  mw_uint32_put(b, req->event_id);
  mw_uint32_put(b, 1);

  if(act == action_save) {
    req->action = action_saved;

    mw_uint32_put(b, 20 + data? data->len: 0);
    mw_uint32_put(b, req->key);
    MwOpaque_put(b, data);

  } else if(act == action_load) {
    req->action = action_loaded;

    mw_uint32_put(b, req->key);
  }

  MwPutBuffer_free(b, &o);

  MwChannel_send(chan, act, &o, TRUE);
  MwOpaque_clear(&o);

  mw_gobject_unref(chan);
}


static void request_trigger(MwStorageService *srvc,
			    MwStorageRequest *req,
			    guint32 code,
			    const MwOpaque *unit) {

  if(req->cb) {
    req->cb(srvc, req->event_id, code, req->key, unit, req->data);
  }
}


/* should only be used via request_remove's g_hash_table_remove */
static void request_free(MwStorageRequest *req) {
  g_free(req);
}


static MwStorageRequest *
request_find(MwStorageService *self, guint event) {
  MwStorageServicePrivate *priv;

  priv = self->private;
  g_return_val_if_fail(priv != NULL, NULL);

  return g_hash_table_lookup(priv->pending, GUINT_TO_POINTER(event));
}


static void request_remove(MwStorageService *self, guint event) {
  MwStorageServicePrivate *priv;

  priv = self->private;
  g_return_if_fail(priv != NULL);

  g_hash_table_remove(priv->pending, GUINT_TO_POINTER(event));
}


static const gchar *get_name(MwService *srvc) {
  return "User Storage";
}


static const gchar *get_desc(MwService *srvc) {
  return "Stores user data and settings on the server";
}


static void mw_recv_action_loaded(MwStorageService *self, MwGetBuffer *gb) {
  guint32 id, code;
  MwStorageRequest *req;
  guint32 count, key;
  MwOpaque o;

  mw_uint32_get(gb, &id);
  mw_uint32_get(gb, &code);

  req = request_find(self, id);

  if(! req) {
    g_warning("no pending storage request: 0x%x", id);
    MwGetBuffer_free(gb);
    return;
  }
  
  g_return_if_fail(req->action == action_loaded);

  mw_uint32_get(gb, &count);
  g_return_if_fail(count > 0);

  mw_uint32_skip(gb);
  mw_uint32_get(gb, &key);
  MwOpaque_steal(gb, &o);

  request_trigger(self, req, code, &o);
  request_remove(self, id);
}


static void mw_recv_action_saved(MwStorageService *self, MwGetBuffer *gb) {

}


static void mw_channel_recv(MwChannel *chan,
			    guint type, const MwOpaque *msg,
			    MwStorageService *self) {

  /* process into results, trigger callbacks */

  MwGetBuffer *gb = MwGetBuffer_wrap(msg);

  switch(type) {
  case action_loaded:
    mw_recv_action_loaded(self, gb);
    break;
  case action_saved:
    mw_recv_action_saved(self, gb);
    break;
  default:
    ;
  }

  if(MwGetBuffer_error(gb)) {
    mw_mailme_opaque(msg, "storage request message type: 0x%x", type);
  }

  MwGetBuffer_free(gb);
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


static gboolean mw_start(MwService *self) {
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

    g_signal_connect(G_OBJECT(chan), "incoming",
		     G_CALLBACK(mw_channel_recv), self);

    g_signal_connect(G_OBJECT(chan), "state-changed",
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

      MwChannel_close(chan, 0x00, NULL);
      return FALSE;
    }
  }

  return TRUE;
}


static guint mw_load(MwStorageService *self, guint32 key,
		     MwStorageCallback cb, gpointer user_data) {
  /* 
     create a storage request
     set it to load
     send it
  */

  MwStorageRequest *req;
  
  mw_object_require_state(self, mw_service_started, 0);
  
  req = request_new(self);
  req->key = key;
  req->action = action_load;
  req->cb = cb;
  req->data = user_data;

  request_send(self, req, NULL);

  return req->event_id;
}


static guint mw_save(MwStorageService *self,
		     guint32 key, const MwOpaque *unit,
		     MwStorageCallback cb, gpointer user_data) {
  /*
    create a storage request
    set it to save
    send it
  */

  MwStorageRequest *req;

  mw_object_require_state(self, mw_service_started, 0);
  
  req = request_new(self);
  req->key = key;
  req->action = action_save;
  req->cb = cb;
  req->data = user_data;

  request_send(self, req, unit);

  return req->event_id;
}


static void mw_cancel(MwStorageService *self, guint event) {
  /*
    find a storage request
    remove it
  */

  request_remove(self, event);
}


static void mw_set_property(GObject *object,
			    guint property_id, const GValue *value,
			    GParamSpec *pspec) {

  MwStorageService *self = MW_STORAGE_SERVICE(object);
  MwStorageServicePrivate *priv = self->private;

  switch(property_id) {
  case property_channel:
    mw_gobject_unref(priv->channel);
    priv->channel = MW_CHANNEL(g_value_dup_object(value));
    break;

  default:
    ;
  }
}


static void mw_get_property(GObject *object,
			    guint property_id, GValue *value,
			    GParamSpec *pspec) {

  MwStorageService *self = MW_STORAGE_SERVICE(object);
  MwStorageServicePrivate *priv = self->private;

  switch(property_id) {
  case property_channel:
    g_value_set_object(value, G_OBJECT(priv->channel));
    break;

  default:
    ;
  }
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

  obj = parent_class->constructor(type, props_count, props);
  self = (MwStorageService *) obj;

  priv = self->private;

  priv->pending = g_hash_table_new_full(g_direct_hash, g_direct_equal,
					NULL, (GDestroyNotify) request_free);

  mw_debug_exit();
  return obj;
}


static void mw_srvc_dispose(GObject *object) {
  /* todo, cleanup */
}


static guint mw_signal_key_updated() {
  return g_signal_new("key-updated",
		      MW_TYPE_STORAGE_SERVICE,
		      0,
		      0,
		      NULL, NULL,
		      mw_marshal_VOID__UINT,
		      G_TYPE_NONE,
		      1,
		      G_TYPE_UINT);
}


static void mw_srvc_class_init(gpointer gclass, gpointer gclass_data) {
  GObjectClass *gobject_class = gclass;
  MwServiceClass *service_class = gclass;
  MwStorageServiceClass *klass = gclass;

  parent_class = g_type_class_peek_parent(gobject_class);

  gobject_class->constructor = mw_srvc_constructor;
  gobject_class->dispose = mw_srvc_dispose;
  gobject_class->set_property = mw_set_property;
  gobject_class->get_property = mw_get_property;

  klass->signal_key_updated = mw_signal_key_updated();

  mw_prop_obj(gobject_class, property_channel,
	      "channel", "get/set backing channel",
	      MW_TYPE_CHANNEL, G_PARAM_READWRITE|G_PARAM_PRIVATE);

  service_class->start = mw_start;
  service_class->stop = mw_stop;
  service_class->get_name = get_name;
  service_class->get_desc = get_desc;

  klass->load = mw_load;
  klass->save = mw_save;
  klass->cancel = mw_cancel;
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
  static GType type = 0;

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
  GValue val[4] = {};

  g_value_init(val+0, G_TYPE_UINT);
  g_value_set_uint(val+0, event);

  g_value_init(val+1, G_TYPE_UINT);
  g_value_set_uint(val+1, code);

  g_value_init(val+2, G_TYPE_UINT);
  g_value_set_uint(val+2, key);

  g_value_init(val+3, MW_TYPE_OPAQUE);
  g_value_set_boxed(val+3, unit);

  g_closure_invoke(closure, NULL, 4, val, NULL);
}
				  

guint MwStorageService_load(MwStorageService *srvc, guint32 key,
			    MwStorageCallback cb, gpointer user_data) {

  guint (*fn)(MwStorageService *, guint32, MwStorageCallback, gpointer);

  g_return_val_if_fail(srvc != NULL, 0x00);

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

  g_return_val_if_fail(srvc != NULL, 0x00);

  fn = MW_STORAGE_SERVICE_GET_CLASS(srvc)->save;
  return fn(srvc, key, unit, cb, user_data);
}


guint MwStorageService_saveClosure(MwStorageService *srvc,
				   guint32 key, const MwOpaque *unit,
				   GClosure *closure) {

  return MwStorageService_save(srvc, key, unit,
			       mw_closure_storage_cb, closure);
}


void MwStorageService_cancel(MwStorageService *srvc, guint event) {
  void (*fn)(MwStorageService *, guint);

  g_return_if_fail(srvc != NULL);

  fn = MW_STORAGE_SERVICE_GET_CLASS(srvc)->cancel;
  fn(srvc, event);
}


/* ---- */
#if 0

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

#endif
