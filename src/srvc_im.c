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
#include "mw_debug.h"
#include "mw_error.h"
#include "mw_marshal.h"
#include "mw_object.h"
#include "mw_service.h"
#include "mw_session.h"
#include "mw_srvc_im.h"
#include "mw_util.h"


#define MW_SERVICE_ID  0x00001000
#define MW_PROTO_TYPE  0x00001000
#define MW_PROTO_VER   0x00000003  


static GObjectClass *srvc_parent_class;
static GObjectClass *conv_parent_class;


struct mw_im_service_private {
  GHashTable *convs;
};


static const gchar *mw_get_name(MwService *self) {
  return "Basic IM";
}


static const gchar *mw_get_desc(MwService *self) {
  return "Meanwhile's Basic Instant Messaging Service";
}


static void mw_conv_weak_cb(gpointer data, GObject *gone) {
  gpointer *mem = data;
  MwIMService *self;
  MwIMServicePrivate *priv;

  self = mem[0];
  priv = self->private;

  if(priv) {
    g_hash_table_remove(priv->convs, mem[1]);
  }

  /* free what's allocated in mw_conv_setup */
  g_free(mem);
}


static void mw_conv_setup(MwIMService *self, MwConversation *conv) {
  MwIMServicePrivate *priv;
  GHashTable *ht;
  MwIdentity *id;
  gpointer *mem;

  priv = self->private;
  ht = priv->convs;

  /* id will be free'd as a key when the hash table is destroyed */
  g_object_get(G_OBJECT(conv), "target", &id, NULL);

  g_hash_table_insert(ht, id, conv);

  mem = g_new0(gpointer, 2);
  mem[0] = self;
  mem[1] = id;
  g_object_weak_ref(G_OBJECT(conv), mw_conv_weak_cb, mem);
}


static void mw_incoming_accept(MwSession *session, MwIMService *self,
			       MwChannel *chan) {
  MwStatus stat;
  MwPutBuffer *pb;
  MwOpaque o;
  gchar *user = NULL, *community = NULL;
  MwConversation *conv;
  MwIMServiceClass *klass;
  
  g_object_get(G_OBJECT(session),
	       "status", &stat.status,
	       "status-idle-since", &stat.time,
	       "status-message", &stat.desc,
	       NULL);

  /* compose accept addtl */
  pb = MwPutBuffer_new();
  mw_uint32_put(pb, 0x01);
  mw_uint32_put(pb, 0x01);
  mw_uint32_put(pb, 0x02);
  MwStatus_put(pb, &stat);
  MwPutBuffer_free(pb, &o);

  /* accept channel */
  MwChannel_open(chan, &o);

  /* cleanup from accept */
  MwStatus_clear(&stat, TRUE);
  MwOpaque_clear(&o);

  g_object_get(G_OBJECT(chan),
	       "user", &user,
	       "community", &community,
	       NULL);

  /* find or get a conversation */
  conv = MwIMService_getConversation(self, user, community);

  g_free(user);
  g_free(community);

  g_object_set(G_OBJECT(conv),
	       "channel", chan,
	       "state", mw_conversation_open,
	       NULL);

  klass = MW_IM_SERVICE_GET_CLASS(self);

  g_signal_emit(self, klass->signal_incoming_conversation, 0,
		conv, NULL);

  mw_gobject_unref(conv);
}


static gboolean mw_incoming_channel(MwSession *session, MwChannel *chan,
				    MwIMService *self) {
  guint srvc, proto_type, proto_ver;
  gboolean ret = FALSE;

  mw_debug_enter();

  g_object_get(G_OBJECT(chan),
	       "service-id", &srvc,
	       "protocol-type", &proto_type,
	       "protocol-version", &proto_ver,
	       NULL);

  if(srvc == MW_SERVICE_ID &&
     proto_type == MW_PROTO_TYPE &&
     proto_ver == MW_PROTO_VER) {
    
    MwOpaque *info;
    MwGetBuffer *gb;
    guint a, b;
    
    g_object_get(G_OBJECT(chan), "offered-info", &info, NULL);

    gb = MwGetBuffer_wrap(info);
    mw_uint32_get(gb, &a);
    mw_uint32_get(gb, &b);

    MwGetBuffer_free(gb);
    MwOpaque_free(info);

    if(a == 0x01 && b == 0x01) {
      mw_incoming_accept(session, self, chan);

    } else {
      MwChannel_close(chan, ERR_IM_NOT_REGISTERED, NULL);
    }

    ret = TRUE;
  }
  
  mw_debug_exit();
  return ret;
}


static gboolean mw_start(MwService *srvc) {
  MwIMService *self;
  MwSession *session;

  self = MW_IM_SERVICE(srvc);

  g_object_get(G_OBJECT(self), "session", &session, NULL);

  g_signal_connect(G_OBJECT(session), "channel",
		   G_CALLBACK(mw_incoming_channel), self);

  mw_gobject_unref(session);

  return TRUE;
}


static gboolean mw_stop(MwService *srvc) {
  /* stop all conversations */

  MwIMService *self;
  GList *l;

  self = MW_IM_SERVICE(srvc);
  l = MwIMService_getConversations(self);

  for(; l; l = g_list_delete_link(l, l)) {
    MwConversation_close(l->data, 0x00);
  }

  return TRUE;
}


static MwConversation *mw_new_conv(MwIMService *self,
				   const gchar *user,
				   const gchar *community) {
  MwConversation *conv;

  conv = g_object_new(MW_TYPE_CONVERSATION,
		      "service", self,
		      "user", user,
		      "community", community,
		      NULL);

  return conv;
}


static MwConversation *mw_get_conv(MwIMService *self,
				   const gchar *user,
				   const gchar *community) {
    MwConversation *conv;

  conv = MwIMService_findConversation(self, user, community);

  if(! conv) {
    conv = MW_IM_SERVICE_GET_CLASS(self)->new_conv(self, user, community);
    mw_conv_setup(self, conv);
  }

  return conv;
}


static MwConversation *mw_find_conv(MwIMService *self,
				    const gchar *user,
				    const gchar *community) {

  MwIMServicePrivate *priv;
  MwConversation *conv;
  GHashTable *ht;
  MwIdentity id = { .user = (gchar *) user,
		    .community = (gchar *) community };

  priv = self->private;
  ht = priv->convs;
  conv = g_hash_table_lookup(ht, &id);

  return mw_gobject_ref(conv);
}


static void mw_foreach_conv(MwIMService *self,
			    GFunc func, gpointer data) {

  MwIMServicePrivate *priv;
  GHashTable *ht;

  priv = self->private;
  ht = priv->convs;

  mw_map_foreach_val(ht, func, data);
}


static GObject *
mw_srvc_constructor(GType type, guint props_count,
		    GObjectConstructParam *props) {

  MwIMServiceClass *klass;

  GObject *obj;
  MwIMService *self;
  MwIMServicePrivate *priv;

  mw_debug_enter();

  klass = MW_IM_SERVICE_CLASS(g_type_class_peek(MW_TYPE_IM_SERVICE));

  obj = srvc_parent_class->constructor(type, props_count, props);
  self = (MwIMService *) obj;

  /* set in mw_srvc_init */
  priv = self->private;

  priv->convs = g_hash_table_new_full((GHashFunc) MwIdentity_hash,
				       (GEqualFunc) MwIdentity_equal,
				       (GDestroyNotify) MwIdentity_free,
				       NULL);

  mw_debug_exit();
  return obj;
}


static void mw_srvc_dispose(GObject *object) {
  MwIMService *self;
  MwIMServicePrivate *priv;

  mw_debug_enter();

  self = MW_IM_SERVICE(object);
  priv = self->private;

  if(priv) {
    self->private = NULL;
    g_hash_table_destroy(priv->convs);
    g_free(priv);
  }

  srvc_parent_class->dispose(object);

  mw_debug_exit();
}


static guint mw_signal_incoming_conversation() {
  return g_signal_new("incoming-conversation",
		      MW_TYPE_IM_SERVICE,
		      0,
		      0,
		      NULL, NULL,
		      mw_marshal_VOID__POINTER,
		      G_TYPE_NONE,
		      1,
		      G_TYPE_POINTER);
}


static void mw_srvc_class_init(gpointer gclass, gpointer gclass_data) {
  GObjectClass *gobject_class = gclass;
  MwServiceClass *service_class = gclass;
  MwIMServiceClass *klass = gclass;

  srvc_parent_class = g_type_class_peek_parent(gobject_class);

  gobject_class->constructor = mw_srvc_constructor;
  gobject_class->dispose = mw_srvc_dispose;

  service_class->get_name = mw_get_name;
  service_class->get_desc = mw_get_desc;
  service_class->start = mw_start;
  service_class->stop = mw_stop;

  klass->signal_incoming_conversation = mw_signal_incoming_conversation();
  
  klass->new_conv = mw_new_conv;
  klass->get_conv = mw_get_conv;
  klass->find_conv = mw_find_conv;
  klass->foreach_conv = mw_foreach_conv;
}


static void mw_srvc_init(GTypeInstance *instance, gpointer g_class) {
  MwIMService *self;

  self = (MwIMService *) instance;
  self->private = g_new0(MwIMServicePrivate, 1);
}


static const GTypeInfo mw_srvc_info = {
  .class_size = sizeof(MwIMServiceClass),
  .base_init = NULL,
  .base_finalize = NULL,
  .class_init = mw_srvc_class_init,
  .class_finalize = NULL,
  .class_data = NULL,
  .instance_size = sizeof(MwIMService),
  .n_preallocs = 0,
  .instance_init = mw_srvc_init,
  .value_table = NULL,
};


GType MwIMService_getType() {
  static GType type = 0;

  if(type == 0) {
    type = g_type_register_static(MW_TYPE_SERVICE, "MwIMServiceType",
				  &mw_srvc_info, 0);
  }

  return type;
}


MwIMService *MwIMService_new(MwSession *session) {
  MwIMService *self;

  self = g_object_new(MW_TYPE_IM_SERVICE,
		      "session", session,
		      "auto-start", TRUE,
		      NULL);
  return self;
}


MwConversation *
MwIMService_newConversation(MwIMService *self,
			    const gchar *user, const gchar *community) {

  MwConversation *(*fn)(MwIMService *, const gchar *, const gchar *);

  g_return_val_if_fail(self != NULL, NULL);
  g_return_val_if_fail(user != NULL, NULL);

  return fn(self, user, community);
}


MwConversation *
MwIMService_getConversation(MwIMService *self,
			    const gchar *user, const gchar *community) {
  MwConversation *(*fn)(MwIMService *, const gchar *, const gchar *);

  g_return_val_if_fail(self != NULL, NULL);
  g_return_val_if_fail(user != NULL, NULL);

  fn = MW_IM_SERVICE_GET_CLASS(self)->get_conv;
  return fn(self, user, community);
}


MwConversation *
MwIMService_findConversation(MwIMService *self,
			     const gchar *user, const gchar *community) {
  MwConversation *(*fn)(MwIMService *, const gchar *, const gchar *);

  g_return_val_if_fail(self != NULL, NULL);
  g_return_val_if_fail(user != NULL, NULL);

  fn = MW_IM_SERVICE_GET_CLASS(self)->find_conv;
  return fn(self, user, community);
}


void MwIMService_foreachConversation(MwIMService *self,
				     GFunc func, gpointer data) {

  void (*fn)(MwIMService *, GFunc, gpointer);

  g_return_if_fail(self != NULL);

  fn = MW_IM_SERVICE_GET_CLASS(self)->foreach_conv;
  fn(self, func, data);
}


void MwSession_foreachConversationClosure(MwIMService *self,
					  GClosure *closure) {

  MwIMService_foreachConversation(self, mw_closure_gfunc_obj, closure);
}


GList *MwIMService_getConversations(MwIMService *self) {
  GList *list = NULL;
  MwIMService_foreachConversation(self, mw_collect_gfunc, &list);
  return list;
}


enum conv_properties {
  conv_property_channel = 1,
  conv_property_service,
  conv_property_user,
  conv_property_community,
  conv_property_target,
};



struct mw_conversation_private {
  MwChannel *channel;    /**< reference to backing channel */
  MwIMService *service;  /**< reference to owning service */
  MwIdentity target;     /**< targeted user,community */

  glong ev_in;
  glong ev_state;
};


static void mw_open(MwConversation *self) {
  MwChannel *chan;

  g_object_get(G_OBJECT(self), "channel", &chan, NULL);

  if(! chan) {
    gchar *user, *community;
    MwIMService *srvc;
    MwSession *sess;
    MwPutBuffer *pb;
    MwOpaque o;

    g_object_get(G_OBJECT(self),
		 "service", &srvc,
		 "user", &user,
		 "community", &community,
		 NULL);

    g_object_get(G_OBJECT(srvc), "session", &sess, NULL);

    chan = MwSession_newChannel(sess);

    g_object_set(G_OBJECT(chan),
		 "user", user,
		 "community", community,
		 "service-id", MW_SERVICE_ID,
		 "protocol-type", MW_PROTO_TYPE,
		 "protocol-version", MW_PROTO_VER,
		 "offered-policy", mw_channel_encrypt_ANY,
		 NULL);
    
    mw_gobject_unref(srvc);
    mw_gobject_unref(sess);
    g_free(user);
    g_free(community);

    pb = MwPutBuffer_new();
    mw_uint32_put(pb, 0x01);
    mw_uint32_put(pb, 0x01);
    MwPutBuffer_free(pb, &o);

    g_object_set(G_OBJECT(self), "channel", chan, NULL);

    MwChannel_open(chan, &o);
    MwOpaque_clear(&o);
  }

  mw_gobject_unref(chan);
}


static void mw_close(MwConversation *self, guint32 code) {
  MwChannel *chan;

  mw_debug_enter();

  g_object_get(G_OBJECT(self), "channel", &chan, NULL);

  if(chan) {
    /* will trigger a state change on the channel, which will in turn
       trigger a state change on the conversation */
    MwChannel_close(chan, code, NULL);
    g_object_set(G_OBJECT(self), "channel", NULL, NULL);

  } else {
    g_object_set(G_OBJECT(self), "state", mw_conversation_closed, NULL);
  }

  mw_gobject_unref(chan);

  mw_debug_exit();
}


static void mw_send_text(MwConversation *self, const gchar *text) {
  MwChannel *chan;
  MwPutBuffer *pb;
  MwOpaque o;
  guint state;

  g_object_get(G_OBJECT(self), "channel", &chan, NULL);

  g_return_if_fail(chan != NULL);

  g_object_get(G_OBJECT(self), "state", &state, NULL);
  g_return_if_fail(state == mw_channel_open);

  pb = MwPutBuffer_new();
  mw_uint32_put(pb, 0x01);  /* indicate text */
  mw_str_put(pb, text);
  MwPutBuffer_free(pb, &o);

  MwChannel_send(chan, 0x64, &o, TRUE);
  MwOpaque_clear(&o);

  mw_gobject_unref(chan);
}


static void mw_send_typing(MwConversation *self, gboolean typing) {

  /* typing data type is 0x01, subtype is 0 for typing, 1 for not
     typing, and there's no actual data block */
  MwConversation_sendData(self, 0x01, !typing, NULL);
}


static void mw_send_data(MwConversation *self, guint type, guint subtype,
			 const MwOpaque *data) {
  MwChannel *chan;
  MwPutBuffer *pb;
  MwOpaque o;
  guint state;

  g_object_get(G_OBJECT(self), "channel", &chan, NULL);

  g_return_if_fail(chan != NULL);
  
  g_object_get(G_OBJECT(self), "state", &state, NULL);
  g_return_if_fail(state == mw_channel_open);

  pb = MwPutBuffer_new();
  mw_uint32_put(pb, 0x02);  /* indicate data */
  mw_uint32_put(pb, type);
  mw_uint32_put(pb, subtype);
  MwOpaque_put(pb, data);
  MwPutBuffer_free(pb, &o);

  MwChannel_send(chan, 0x64, &o, TRUE);
  MwOpaque_clear(&o);

  mw_gobject_unref(chan);
}


static GObject *
mw_conv_constructor(GType type, guint props_count,
		    GObjectConstructParam *props) {

  MwConversationClass *klass;

  GObject *obj;
  MwConversation *self;

  mw_debug_enter();
  
  klass = MW_CONVERSATION_CLASS(g_type_class_peek(MW_TYPE_CONVERSATION));

  obj = conv_parent_class->constructor(type, props_count, props);
  self = (MwConversation *) obj;

  g_object_set(obj, "state", mw_conversation_closed, NULL);

  mw_debug_exit();
  return obj;
}


static void mw_conv_dispose(GObject *object) {
  MwConversation *self;
  MwConversationPrivate *priv;

  mw_debug_enter();

  self = (MwConversation *) object;
  priv = self->private;

  if(priv) {
    self->private = NULL;

    mw_gobject_unref(priv->service);
    MwIdentity_clear(&priv->target, TRUE);

    g_free(priv);
  }

  mw_debug_exit();
}


/* called from mw_conv_attach_chan */
static void mw_conv_detach_chan(MwConversation *conv) {
  MwConversationPrivate *priv;
  MwChannel *chan;
  
  mw_debug_enter();

  priv = conv->private;
  if(! priv) return;

  chan = priv->channel;
  if(! chan) return;

  g_signal_handler_disconnect(chan, priv->ev_in);
  g_signal_handler_disconnect(chan, priv->ev_state);

  g_object_set_data(G_OBJECT(chan), "conversation", NULL);
  
  MwChannel_close(chan, 0x00, NULL);
  
  mw_gobject_unref(chan);
  priv->channel = NULL;

  mw_debug_exit();
}


static void mw_conv_recv_text(MwConversation *self, MwGetBuffer *gb) {
  MwConversationClass *klass;
  gchar *message = NULL;

  klass = MW_CONVERSATION_GET_CLASS(self);

  mw_str_get(gb, &message);
  g_signal_emit(self, klass->signal_got_text, 0, message, NULL);
  g_free(message);
}


static void mw_conv_recv_data(MwConversation *self, MwGetBuffer *gb) {
  MwConversationClass *klass;
  guint32 type, subtype;

  klass = MW_CONVERSATION_GET_CLASS(self);

  mw_uint32_get(gb, &type);
  mw_uint32_get(gb, &subtype);

  if(type == 0x01) {
    g_signal_emit(self, klass->signal_got_typing, 0, !subtype, NULL);

  } else {
    MwOpaque data;

    MwOpaque_get(gb, &data);
    g_signal_emit(self, klass->signal_got_data, 0, type, subtype, &data, NULL);
    MwOpaque_clear(&data);
  }
}


static void mw_chan_recv(MwChannel *chan, guint type, const MwOpaque *msg,
			 MwConversation *conv) {

  MwGetBuffer *gb;
  guint32 a;

  mw_debug_enter();

  /* parse text vs. data */

  if(type != 0x64)
    return;

  gb = MwGetBuffer_wrap(msg);
  
  mw_uint32_get(gb, &a);
  
  switch(a) {
  case 0x01:
    mw_conv_recv_text(conv, gb);
    break;
  case 0x02:
    mw_conv_recv_data(conv, gb);
    break;
  default:
    ;
  }

  MwGetBuffer_free(gb);

  mw_debug_exit();
}


static void mw_chan_state(MwChannel *chan, guint state,
			  MwConversation *conv) {

  /* if state is closed or error,
       mark conv as closed, emit state changed
     elif state is open,
       mark conv as open, emit state changed
  */

  if(state == mw_channel_error) {
    guint code;

    g_object_get(G_OBJECT(conv), "error", &code, NULL);
    
    g_object_set(G_OBJECT(conv),
		 "error", code,
		 "state", mw_conversation_error,
		 NULL);

  } else if(state == mw_channel_closed) {
    g_object_set(G_OBJECT(conv), "state", mw_conversation_closed, NULL);
    mw_conv_detach_chan(conv);

  } else if(state == mw_channel_open) {
    g_object_set(G_OBJECT(conv), "state", mw_conversation_open, NULL);
  }
}


/* called from mw_set_property. Use g_object_set to trigger this */
static void mw_conv_attach_chan(MwConversation *conv, MwChannel *chan) {

  MwConversationPrivate *priv;

  priv = conv->private;

  if(chan == priv->channel) {
    g_debug("already attached to channel %p", chan);
    return;
  }

  mw_conv_detach_chan(conv);

  if(chan) {
    g_debug("attaching to channel %p, who has %u refs",
	    chan, mw_gobject_refcount(chan));
    
    mw_gobject_ref(chan);
    priv->channel = chan;

    mw_gobject_ref(conv);
    g_object_set_data_full(G_OBJECT(chan), "conversation", conv,
			   (GDestroyNotify) mw_gobject_unref);
    
    priv->ev_in = g_signal_connect(G_OBJECT(chan), "incoming",
				   G_CALLBACK(mw_chan_recv), conv);
    
    priv->ev_state = g_signal_connect(G_OBJECT(chan), "state-changed",
				      G_CALLBACK(mw_chan_state), conv);
  }
}


static void mw_conv_set_property(GObject *object,
				 guint property_id, const GValue *value,
				 GParamSpec *pspec) {

  MwConversation *self;
  MwConversationPrivate *priv;

  self = MW_CONVERSATION(object);
  priv = self->private;

  switch(property_id) {
  case conv_property_channel:
    mw_conv_attach_chan(self, MW_CHANNEL(g_value_get_object(value)));
    break;
  case conv_property_service:
    mw_gobject_unref(priv->service);
    priv->service = MW_IM_SERVICE(g_value_dup_object(value));
    break;
  case conv_property_user:
    g_free(priv->target.user);
    priv->target.user = g_value_dup_string(value);
    break;
  case conv_property_community:
    g_free(priv->target.community);
    priv->target.community = g_value_dup_string(value);
    break;
  default:
    ;
  }
}


static void mw_conv_get_property(GObject *object,
				 guint property_id, GValue *value,
				 GParamSpec *pspec) {

  MwConversation *self;
  MwConversationPrivate *priv;

  self = MW_CONVERSATION(object);
  priv = self->private;

  switch(property_id) {
  case conv_property_channel:
    g_value_set_object(value, priv->channel);
    break;
  case conv_property_service:
    g_value_set_object(value, priv->service);
    break;
  case conv_property_user:
    g_value_set_string(value, priv->target.user);
    break;
  case conv_property_community:
    g_value_set_string(value, priv->target.community);
    break;
  case conv_property_target:
    g_value_set_boxed(value, &priv->target);
    break;
  default:
    ;
  }
}


static guint mw_conv_signal_got_text() {
  return g_signal_new("got-text",
		      MW_TYPE_CONVERSATION,
		      0,
		      0,
		      NULL, NULL,
		      mw_marshal_VOID__POINTER,
		      G_TYPE_NONE,
		      1,
		      G_TYPE_POINTER);
}


static guint mw_conv_signal_got_typing() {
  return g_signal_new("got-typing",
		      MW_TYPE_CONVERSATION,
		      0,
		      0,
		      NULL, NULL,
		      mw_marshal_VOID__BOOLEAN,
		      G_TYPE_NONE,
		      1,
		      G_TYPE_BOOLEAN);
}


static guint mw_conv_signal_got_data() {
  return g_signal_new("got-data",
		      MW_TYPE_CONVERSATION,
		      0,
		      0,
		      NULL, NULL,
		      mw_marshal_VOID__UINT_UINT_POINTER,
		      G_TYPE_NONE,
		      1,
		      G_TYPE_UINT,
		      G_TYPE_UINT,
		      G_TYPE_POINTER);
}


static void mw_conv_class_init(gpointer gclass, gpointer gclass_data) {
  GObjectClass *gobject_class = gclass;
  MwConversationClass *klass = gclass;

  conv_parent_class = g_type_class_peek_parent(gobject_class);

  gobject_class->constructor = mw_conv_constructor;
  gobject_class->dispose = mw_conv_dispose;
  gobject_class->set_property = mw_conv_set_property;
  gobject_class->get_property = mw_conv_get_property;

  mw_prop_obj(gobject_class, conv_property_channel,
	      "channel", "get conversation's backing channel",
	      MW_TYPE_CHANNEL, G_PARAM_READWRITE);

  mw_prop_obj(gobject_class, conv_property_service,
	      "service", "get conversation's owning service",
	      MW_TYPE_IM_SERVICE, G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY);

  mw_prop_str(gobject_class, conv_property_user,
	      "user", "get remote user",
	      G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY);

  mw_prop_str(gobject_class, conv_property_community,
	      "community", "get remote community",
	      G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY);

  mw_prop_boxed(gobject_class, conv_property_target,
		"target", "get remote user,community as a MwIdentity",
		MW_TYPE_IDENTITY, G_PARAM_READABLE);
  
  klass->signal_got_text = mw_conv_signal_got_text();
  klass->signal_got_typing = mw_conv_signal_got_typing();
  klass->signal_got_data = mw_conv_signal_got_data();

  klass->open = mw_open;
  klass->close = mw_close;
  klass->send_text = mw_send_text;
  klass->send_typing = mw_send_typing;
  klass->send_data = mw_send_data;
}


static void mw_conv_init(GTypeInstance *instance, gpointer gclass) {
  MwConversation *self;

  mw_debug_enter();

  self = (MwConversation *) instance;
  self->private = g_new0(MwConversationPrivate, 1);

  mw_debug_exit();
}


static const GTypeInfo mw_conv_info = {
  .class_size = sizeof(MwConversationClass),
  .base_init = NULL,
  .base_finalize = NULL,
  .class_init = mw_conv_class_init,
  .class_finalize = NULL,
  .class_data = NULL,
  .instance_size = sizeof(MwConversation),
  .n_preallocs = 0,
  .instance_init = mw_conv_init,
  .value_table = NULL,
};


GType MwConversation_getType() {
  static GType type = 0;

  if(type == 0) {
    type = g_type_register_static(MW_TYPE_OBJECT, "MwConversationType",
				  &mw_conv_info, 0);
  }

  return type;
}


void MwConversation_open(MwConversation *self) {
  void (*fn)(MwConversation *);

  g_return_if_fail(self != NULL);

  fn = MW_CONVERSATION_GET_CLASS(self)->open;
  fn(self);
}


void MwConversation_close(MwConversation *self, guint32 code) {
  void (*fn)(MwConversation *, guint32);

  g_return_if_fail(self != NULL);

  fn = MW_CONVERSATION_GET_CLASS(self)->close;
  fn(self, code);
}


void MwConversation_sendText(MwConversation *self, const gchar *text) {
  void (*fn)(MwConversation *, const gchar *);

  g_return_if_fail(self != NULL);
  
  fn = MW_CONVERSATION_GET_CLASS(self)->send_text;
  fn(self, text);
}


void MwConversation_sendTyping(MwConversation *self, gboolean typing) {
  void (*fn)(MwConversation *, gboolean);

  g_return_if_fail(self != NULL);
  
  fn = MW_CONVERSATION_GET_CLASS(self)->send_typing;
  fn(self, typing);
}


void MwConversation_sendData(MwConversation *self,
			     guint32 type, guint32 subtype,
			     const MwOpaque *data) {

  void (*fn)(MwConversation *, guint32, guint32, const MwOpaque *);

  g_return_if_fail(self != NULL);
  
  fn = MW_CONVERSATION_GET_CLASS(self)->send_data;
  fn(self, type, subtype, data);
}


#define enum_val(val, alias) { val, #val, alias }


static const GEnumValue values[] = {
  enum_val(mw_conversation_closed, "closed"),
  enum_val(mw_conversation_pending, "pending"),
  enum_val(mw_conversation_open, "open"),
  enum_val(mw_conversation_error, "error"),
};


GType MwConversationStateEnum_getType() {
  static GType type = 0;

  if(type == 0) {
    type = g_enum_register_static("MwConversationStateEnumType", values);
  }

  return type;
}


/* The end. */
