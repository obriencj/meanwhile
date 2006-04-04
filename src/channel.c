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

#include "mw_channel.h"
#include "mw_encrypt.h"
#include "mw_debug.h"
#include "mw_error.h"
#include "mw_marshal.h"
#include "mw_message.h"
#include "mw_object.h"
#include "mw_session.h"
#include "mw_util.h"


/** set in mw_channel_class_init */
static GObjectClass *parent_class;


enum properties {
  property_state = 1,

  property_channel_id,
  property_session,
  property_error,

  property_remote_user,
  property_remote_community,
  property_remote_client_type,
  property_remote_login_id,

  property_service_id,
  property_protocol_type,
  property_protocol_version,

  property_offered_policy,
  property_accepted_policy,
  property_cipher,

  property_offered_info,
  property_accepted_info,

  property_close_code,
  property_close_info,
};


struct mw_channel_private {
  guint32 id;

  guint error;

  MwSession *session;

  MwLogin remote;

  guint32 service_id;
  guint32 protocol_type;
  guint32 protocol_version;

  guint16 offered_policy;
  guint16 accepted_policy;
  MwCipher *cipher;

  MwOpaque offered_info;
  MwOpaque accepted_info;
  
  guint32 close_code;
  MwOpaque close_info;

  GHashTable *ciphers;
};


static void mw_add_cipher(MwChannel *self, MwCipher *ci) {
  MwChannelPrivate *priv;
  GHashTable *ht;
  guint id;
  
  id = MwCipherClass_getIdentifier(MW_CIPHER_GET_CLASS(ci));

  g_debug("adding cipher by id 0x%x", id);
  
  g_return_if_fail(self->private != NULL);
  priv = self->private;
  ht = priv->ciphers;
  
  g_hash_table_insert(ht, GUINT_TO_POINTER(id), ci);
}


static MwCipher *mw_get_cipher(MwChannel *self, guint id) {
  MwChannelPrivate *priv;
  GHashTable *ht;
  
  g_debug("looking up cipher by id 0x%x", id);

  g_return_val_if_fail(self->private != NULL, NULL);
  priv = self->private;
  ht = priv->ciphers;
  
  return g_hash_table_lookup(ht, GUINT_TO_POINTER(id));
}


static void mw_msg_send(MwChannel *self, MwMessage *msg) {
  MwChannelClass *klass;
  guint sid;

  klass = MW_CHANNEL_GET_CLASS(self);
  sid = klass->signal_outgoing;

  g_signal_emit(self, sid, 0, msg, NULL);
}


static void mw_create(MwChannel *self, const MwOpaque *info) {
  MwChannelPrivate *priv;
  MwMsgChannelCreate *msg;
  guint id, service, proto_type, proto_ver, policy;

  priv = self->private;
  MwOpaque_clone(&priv->offered_info, info);

  msg = MwMessage_new(mw_message_CHANNEL_CREATE);
  
  g_object_get(G_OBJECT(self),
	       "id", &id,
	       "service-id", &service,
	       "protocol-type", &proto_type,
	       "protocol-version", &proto_ver,
	       "offered-policy", &policy,
	       "user", &msg->target.user,
	       "community", &msg->target.community,
	       NULL);
  
  msg->channel = id;
  msg->service = service;
  msg->proto_type = proto_type;
  msg->proto_ver = proto_ver;
  MwOpaque_clone(&msg->addtl, info);

  msg->enc_mode = policy;
  msg->enc_extra = policy;

  g_debug("offering encryption policy 0x%x", msg->enc_mode);

  /* populate the message's enc block, and the channel's ciphers */
  if(policy != mw_channel_encrypt_NONE) {
    GList *l = MwSession_getCiphers(priv->session);
    
    if(! l) {
      g_debug("backing policy down to NONE, no ciphers to offer");
      msg->enc_mode = mw_channel_encrypt_NONE;
      msg->enc_extra = mw_channel_encrypt_NONE;

    } else {
      guint i, ls = g_list_length(l);

      msg->enc_count = ls;
      msg->enc_items = g_new0(MwEncItem, ls);

      g_debug("offering %u encryption items", ls);
      
      for(i = 0; i < ls; i++) {
	MwCipher *ci = MwCipherClass_newInstance(l->data, self);
	MwEncItem *ei = msg->enc_items + i;
	ei->cipher = MwCipherClass_getIdentifier(l->data);
	MwCipher_offer(ci, &ei->info);
	mw_add_cipher(self, ci);
	l = g_list_delete_link(l, l);
      }
    }
  }

  mw_msg_send(self, MW_MESSAGE(msg));
  MwMessage_free(MW_MESSAGE(msg));

  g_object_set(G_OBJECT(self), "state", mw_channel_pending, NULL);
}


static MwCipher *mw_find_best(MwChannel *self) {
  MwChannelPrivate *priv;
  GList *l;
  MwCipher *cipher = NULL;
  guint pol;

  priv = self->private;
  l = mw_map_collect_values(priv->ciphers);

  for(; l; l = g_list_delete_link(l, l)) {
    MwCipher *c = l->data;
    guint p = MwCipherClass_getPolicy(MW_CIPHER_GET_CLASS(c));

    if(!cipher || p > pol) {
      g_debug("cipher id 0x%x looks good...", p);
      cipher = c;
      pol = p;
    }
  }

  return cipher;
}


static MwCipher *mw_find_match(MwChannel *self, guint pol) {
  MwChannelPrivate *priv;
  GList *l;
  MwCipher *cipher = NULL;

  priv = self->private;
  l = mw_map_collect_values(priv->ciphers);

  for(; l; l = g_list_delete_link(l, l)) {
    MwCipher *c = l->data;
    guint p = MwCipherClass_getPolicy(MW_CIPHER_GET_CLASS(c));
    if(p == pol) {
      cipher = c;
      break;
    }
  }
  g_list_free(l);

  return cipher;
}


static void mw_accept(MwChannel *self, const MwOpaque *info) {
  MwChannelPrivate *priv;
  MwMsgChannelAccept *msg;
  guint id, service, proto_type, proto_ver, o_policy, a_policy;
  MwCipher *cipher = NULL;

  priv = self->private;
  MwOpaque_clone(&priv->accepted_info, info);

  msg = MwMessage_new(mw_message_CHANNEL_ACCEPT);
  
  /* it seems appropriate to use this to get the attributes, but maybe
     it would be better to just get them from priv */
  g_object_get(G_OBJECT(self),
	       "id", &id,
	       "service-id", &service,
	       "protocol-type", &proto_type,
	       "protocol-version", &proto_ver,
	       "offered-policy", &o_policy,
	       NULL);

  msg->head.channel = id;
  msg->service = service;
  msg->proto_type = proto_type;
  msg->proto_ver = proto_ver;
  MwOpaque_clone(&msg->addtl, info);

  msg->enc_extra = o_policy;

  switch(o_policy) {
  case mw_channel_encrypt_NONE:
    cipher = NULL;
    break;

  case mw_channel_encrypt_ANY:
  case mw_channel_encrypt_WHATEVER:
    cipher = mw_find_best(self);
    break;

  default:
    cipher = mw_find_match(self, o_policy);
    if(! cipher) {
      g_warning("couldn't meet channel encryption policy of 0x%x", o_policy);
      cipher = mw_find_best(self);
    }
  }

  if(cipher) {
    MwCipherClass *cc = MW_CIPHER_GET_CLASS(cipher);
    a_policy = MwCipherClass_getPolicy(cc);

    msg->enc_item.cipher = MwCipherClass_getIdentifier(cc);
    MwCipher_accept(cipher, &msg->enc_item.info);

    priv->cipher = cipher;

  } else {
    a_policy = mw_channel_encrypt_NONE;
  }

  g_debug("accepted channel with policy 0x%x", a_policy);
  priv->accepted_policy = a_policy;
  msg->enc_mode = a_policy;

  mw_msg_send(self, MW_MESSAGE(msg));
  MwMessage_free(MW_MESSAGE(msg));

  g_object_set(G_OBJECT(self), "state", mw_channel_open, NULL);
}


static void mw_open(MwChannel *self, const MwOpaque *info) {
  gint state;

  g_object_get(G_OBJECT(self), "state", &state, NULL);

  switch(state) {
  case mw_channel_closed:
    mw_create(self, info);
    break;
  case mw_channel_pending:
    mw_accept(self, info);
    break;
  default:
    ;
  }
}


static void mw_close(MwChannel *self, guint32 code, const MwOpaque *info) {
  MwChannelPrivate *priv;
  MwMsgChannelClose *msg;
  guint id;
  gint state;

  g_object_get(G_OBJECT(self), "state", &state, NULL);

  if(state == mw_channel_error ||
     state == mw_channel_closed) {

    /* prevent circular calls from a messy handler */
    return;
  }

  g_return_if_fail(self->private != NULL);
  priv = self->private;

  /* fill in on the channel */
  priv->close_code = code;
  MwOpaque_clone(&priv->close_info, info);

  g_object_get(G_OBJECT(self), "id", &id, NULL);
  
  msg = MwMessage_new(mw_message_CHANNEL_CLOSE);
  msg->head.channel = id;
  msg->reason = code;
  MwOpaque_clone(&msg->data, info);

  mw_msg_send(self, MW_MESSAGE(msg));
  MwMessage_free(MW_MESSAGE(msg));

  if(code) {
    g_object_set(G_OBJECT(self),
		 "error", code,
		 "state", mw_channel_error,
		 NULL);
  }
  
  g_object_set(G_OBJECT(self), "state", mw_channel_closed, NULL);
}


static void recv_CREATE(MwChannel *self, MwMsgChannelCreate *msg) {
  /* fill internal fields,
     set state to mw_channel_PENDING */

  MwChannelPrivate *priv;

  g_return_if_fail(self->private != NULL);
  priv = self->private;

  priv->id = msg->channel;
  priv->service_id = msg->service;
  priv->protocol_type = msg->proto_type;
  priv->protocol_version = msg->proto_ver;
  MwOpaque_clone(&priv->offered_info, &msg->addtl);
  MwLogin_clone(&priv->remote, &msg->creator, TRUE);
  priv->offered_policy = msg->enc_mode;

  g_debug("offered channel with policy 0x%x", msg->enc_mode);

  if(msg->enc_mode != mw_channel_encrypt_NONE) {
    guint i, count = msg->enc_count;

    g_debug("loading %u offered ciphers", count);

    for(i = 0; i < count; i++) {
      MwEncItem *ei;
      MwCipherClass *cc;

      ei = msg->enc_items + i;
      cc = MwSession_getCipher(priv->session, ei->cipher);

      if(cc) {
	MwCipher *ci = MwCipherClass_newInstance(cc, self);
	MwCipher_offered(ci, &ei->info);
	mw_add_cipher(self, ci);
      }
    }
  }

  g_object_set(G_OBJECT(self), "state", mw_channel_pending, NULL);
}


static void recv_ACCEPT(MwChannel *self, MwMsgChannelAccept *msg) {
  /* fill internal fields,
     set state to mw_channel_OPEN */

  MwChannelPrivate *priv;
  
  g_return_if_fail(self->private != NULL);
  priv = self->private;

  MwOpaque_clone(&priv->accepted_info, &msg->addtl);
  MwLogin_clone(&priv->remote, &msg->acceptor, TRUE);
  priv->accepted_policy = msg->enc_mode;

  g_debug("accepted with policy 0x%x", msg->enc_mode);
  g_debug("accepted with cipher 0x%x", msg->enc_item.cipher);

  if(priv->accepted_policy != mw_channel_encrypt_NONE) {
    MwCipher *ci;
    ci = mw_get_cipher(self, msg->enc_item.cipher);
    MwCipher_accepted(ci, &msg->enc_item.info);
    priv->cipher = ci;
  }

  g_object_set(G_OBJECT(self), "state", mw_channel_open, NULL);
}


static void recv_CLOSE(MwChannel *self, MwMsgChannelClose *msg) {
  /* fill internal fields,
     set state to mw_channel_CLOSED */

  MwChannelPrivate *priv;
  
  g_return_if_fail(self->private != NULL);
  priv = self->private;

  priv->close_code = msg->reason;
  MwOpaque_clone(&priv->close_info, &msg->data);

  if(msg->reason) {
    g_object_set(G_OBJECT(self),
		 "error", msg->reason,
		 "state", mw_channel_error,
		 NULL);
  }

  g_object_set(G_OBJECT(self), "state", mw_channel_closed, NULL);
}


static void recv_SEND(MwChannel *self, MwMsgChannelSend *msg) {
  /* decrypt as necessary,
     trigger signal_incoming */

  MwChannelClass *klass = MW_CHANNEL_GET_CLASS(self);

  if(msg->head.options & mw_message_option_ENCRYPT) {
    MwOpaque dec;
    MwCipher *cipher;

    g_object_get(G_OBJECT(self), "cipher", &cipher, NULL);

    MwCipher_decrypt(cipher, &msg->data, &dec);
    g_signal_emit(G_OBJECT(self), klass->signal_incoming, 0,
		  (guint) msg->type, &dec, NULL);
    
    mw_gobject_unref(cipher);
    MwOpaque_clear(&dec);

  } else {
    g_signal_emit(G_OBJECT(self), klass->signal_incoming, 0,
		  (guint) msg->type, &msg->data, NULL);
  }
}


static void mw_feed(MwChannel *self, MwMessage *msg) {
  switch(msg->type) {
  case mw_message_CHANNEL_CREATE:
    recv_CREATE(self, (MwMsgChannelCreate *) msg);
    break;
  case mw_message_CHANNEL_ACCEPT:
    recv_ACCEPT(self, (MwMsgChannelAccept *) msg);
    break;
  case mw_message_CHANNEL_CLOSE:
    recv_CLOSE(self, (MwMsgChannelClose *) msg);
    break;
  case mw_message_CHANNEL_SEND:
    recv_SEND(self, (MwMsgChannelSend *) msg);
    break;
  default:
    /* todo warning */
    ;
  }
}


static void mw_send(MwChannel *self, guint16 type, const MwOpaque *data,
		    gboolean enc) {

  MwChannelPrivate *priv;
  MwCipher *cipher;
  MwMsgChannelSend *msg;
  guint id, policy;
  
  priv = self->private;

  g_object_get(G_OBJECT(self),
	       "cipher", &cipher,
	       "id", &id,
	       "accepted-policy", &policy,
	       NULL);

  msg = MwMessage_new(mw_message_CHANNEL_SEND);
  msg->head.channel = id;
  msg->type = type;

  if(policy == mw_channel_encrypt_NONE ||
     (policy == mw_channel_encrypt_WHATEVER && !enc)) {

    /* send unencrypted */
    MwOpaque_clone(&msg->data, data);
    
  } else {
    /* send encrypted */
    msg->head.options |= mw_message_option_ENCRYPT;
    MwCipher_encrypt(cipher, data, &msg->data);
  }

  mw_msg_send(self, MW_MESSAGE(msg));
  MwMessage_free(MW_MESSAGE(msg));

  mw_gobject_unref(cipher);
}


static GObject *
mw_channel_constructor(GType type, guint props_count,
		       GObjectConstructParam *props) {
  
  MwChannelClass *klass;

  GObject *obj;
  MwChannel *self;
  MwChannelPrivate *priv;

  mw_debug_enter();

  klass = MW_CHANNEL_CLASS(g_type_class_peek(MW_TYPE_CHANNEL));

  obj = parent_class->constructor(type, props_count, props);
  self = (MwChannel *) obj;

  g_return_val_if_fail(self->private != NULL, NULL);
  priv = self->private;

  priv->ciphers = g_hash_table_new_full(g_direct_hash, g_direct_equal,
					NULL,
					(GDestroyNotify) mw_gobject_unref);

  g_object_set(obj, "state", mw_channel_closed, NULL);

  mw_debug_exit();
  return obj;
}


static void mw_channel_dispose(GObject *obj) {
  MwChannel *self = (MwChannel *) obj;
  MwChannelPrivate *priv;

  mw_debug_enter();

  priv = self->private;
  self->private = NULL;

  if(priv) {
    g_debug("cleaning out private");

    MwLogin_clear(&priv->remote, TRUE);

    MwOpaque_clear(&priv->offered_info);
    MwOpaque_clear(&priv->accepted_info);
    MwOpaque_clear(&priv->close_info);
    
    /* will unref all our ciphers */
    g_hash_table_destroy(priv->ciphers);

    /* the above should do this for us */
    /* g_object_unref(G_OBJECT(priv->cipher)); */

    mw_gobject_unref(priv->session);
    
    g_free(priv);
  }

  parent_class->dispose(obj);

  mw_debug_exit();
}


static gboolean mw_set_requires_state(MwChannel *self,
				      enum mw_channel_state req_state,
				      GParamSpec *pspec) {

  /* todo add property name and state name */

  gint state;

  g_object_get(G_OBJECT(self), "state", &state, NULL);

  if(state != req_state) {
    g_warning("MwChannel property cannot be set in this state");
    return FALSE;

  } else {
    return TRUE;
  }
}


static void mw_set_property(GObject *object,
			    guint property_id, const GValue *value,
			    GParamSpec *pspec) {

  MwChannel *self = MW_CHANNEL(object);
  MwChannelPrivate *priv;

  g_return_if_fail(self->private != NULL);
  priv = self->private;

  switch(property_id) {
  case property_state:
    MwObject_setState(MW_OBJECT(object), value);
    break;

  case property_channel_id:
    priv->id = g_value_get_uint(value);
    break;
  case property_session:
    {
      MwSession *session = MW_SESSION(g_value_dup_object(value));
      mw_gobject_unref(priv->session);
      priv->session = session;
      g_debug("channel sets session to %p, refcount of %u",
	      session, mw_gobject_refcount(session));
    }
    break;
  case property_remote_user:
    if(mw_set_requires_state(self, mw_channel_closed, pspec)) {
      g_free(priv->remote.id.user);
      priv->remote.id.user = g_value_dup_string(value);
    }
    break;
  case property_remote_community:
    if(mw_set_requires_state(self, mw_channel_closed, pspec)) {
      g_free(priv->remote.id.community);
      priv->remote.id.community = g_value_dup_string(value);
    }
    break;
  case property_service_id:
    if(mw_set_requires_state(self, mw_channel_closed, pspec)) {
      priv->service_id = g_value_get_uint(value);
    }
    break;
  case property_protocol_type:
    if(mw_set_requires_state(self, mw_channel_closed, pspec)) {
      priv->protocol_type = g_value_get_uint(value);
    }
    break;
  case property_protocol_version:
    if(mw_set_requires_state(self, mw_channel_closed, pspec)) {
      priv->protocol_version = g_value_get_uint(value);
    }
    break;
  case property_offered_policy:
    if(mw_set_requires_state(self, mw_channel_closed, pspec)) {
      priv->offered_policy = g_value_get_uint(value);
    }
    break;
  case property_accepted_policy:
    if(mw_set_requires_state(self, mw_channel_pending, pspec)) {
      guint offered = priv->offered_policy;
      guint accepted = g_value_get_uint(value);

      if(accepted < offered || offered == mw_channel_encrypt_NONE) {
	g_warning("cannot accept with policy 0x%x when offered 0x%x",
		  accepted, offered);
      } else {
	priv->accepted_policy = accepted;
      }
    }
    break;
  default:
    ;
  }
}


static void mw_get_property(GObject *object,
			    guint property_id, GValue *value,
			    GParamSpec *pspec) {

  MwChannel *self = MW_CHANNEL(object);
  MwChannelPrivate *priv;

  g_return_if_fail(self->private != NULL);
  priv = self->private;

  switch(property_id) {
  case property_state:
    MwObject_getState(MW_OBJECT(self), value);
    break;
  case property_error:
    g_value_set_uint(value, priv->error);
    break;
  case property_channel_id:
    g_value_set_uint(value, priv->id);
    break;
  case property_session:
    g_value_set_object(value, priv->session);
    break;
  case property_remote_user:
    g_value_set_string(value, priv->remote.id.user);
    break;
  case property_remote_community:
    g_value_set_string(value, priv->remote.id.community);
    break;
  case property_remote_client_type:
    g_value_set_uint(value, priv->remote.client);
    break;
  case property_remote_login_id:
    g_value_set_string(value, priv->remote.login_id);
    break;
  case property_service_id:
    g_value_set_uint(value, priv->service_id);
    break;
  case property_protocol_type:
    g_value_set_uint(value, priv->protocol_type);
    break;
  case property_protocol_version:
    g_value_set_uint(value, priv->protocol_version);
    break;
  case property_offered_policy:
    g_value_set_uint(value, priv->offered_policy);
    break;
  case property_accepted_policy:
    g_value_set_uint(value, priv->accepted_policy);
    break;
  case property_cipher:
    g_value_set_object(value, priv->cipher);
    break;
  case property_offered_info:
    g_value_set_boxed(value, &priv->offered_info);
    break;
  case property_accepted_info:
    g_value_set_boxed(value, &priv->accepted_info);
    break;
  case property_close_code:
    g_value_set_uint(value, priv->close_code);
    break;
  case property_close_info:
    g_value_set_boxed(value, &priv->close_info);
    break;
  default:
    ;
  }
}


static guint mw_signal_outgoing() {
  return g_signal_new("outgoing",
		      MW_TYPE_CHANNEL,
		      0,
		      0,
		      NULL, NULL,
		      mw_marshal_VOID__POINTER,
		      G_TYPE_NONE,
		      1,
		      G_TYPE_POINTER);
}


static guint mw_signal_incoming() {
  return g_signal_new("incoming",
		      MW_TYPE_CHANNEL,
		      0,
		      0,
		      NULL, NULL,
		      mw_marshal_VOID__UINT_POINTER,
		      G_TYPE_NONE,
		      2,
		      G_TYPE_UINT,
		      G_TYPE_POINTER);
}


static void mw_channel_class_init(gpointer g_class, gpointer g_class_data) {
  GObjectClass *gobject_class;
  MwObjectClass *mwobject_class;
  MwChannelClass *klass;

  gobject_class = G_OBJECT_CLASS(g_class);
  mwobject_class = MW_OBJECT_CLASS(g_class);
  klass = MW_CHANNEL_CLASS(g_class);

  parent_class = g_type_class_peek_parent(gobject_class);

  gobject_class->constructor = mw_channel_constructor;
  gobject_class->dispose = mw_channel_dispose;
  gobject_class->set_property = mw_set_property;
  gobject_class->get_property = mw_get_property;

  mw_prop_uint(gobject_class, property_error,
	       "error", "get/set channel state's error code",
	       G_PARAM_READWRITE);

  mw_prop_uint(gobject_class, property_channel_id,
	       "id", "get channel ID",
	       G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY);

  mw_prop_obj(gobject_class, property_session,
	      "session", "get session",
	      MW_TYPE_SESSION, G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY);

  mw_prop_str(gobject_class, property_remote_user,
	      "user", "get/set the channel's remote user",
	      G_PARAM_READWRITE);

  mw_prop_str(gobject_class, property_remote_community,
	      "community", "get/set the channel's remote community",
	      G_PARAM_READWRITE);

  mw_prop_uint(gobject_class, property_remote_client_type,
	       "client", "get the remote user's client type id",
	       G_PARAM_READABLE);

  mw_prop_str(gobject_class, property_remote_login_id,
	      "login-id", "get the remote user's login id",
	      G_PARAM_READABLE);

  mw_prop_uint(gobject_class, property_service_id,
	       "service-id", "get/set the service identifier",
	       G_PARAM_READWRITE);

  mw_prop_uint(gobject_class, property_protocol_type,
	       "protocol-type", "get/set the protocol type",
	       G_PARAM_READWRITE);

  mw_prop_uint(gobject_class, property_protocol_version,
	       "protocol-version", "get/set the protocol version",
	       G_PARAM_READWRITE);

  mw_prop_uint(gobject_class, property_offered_policy,
	       "offered-policy", "get/set the offered encrypt policy",
	       G_PARAM_READWRITE);

  mw_prop_uint(gobject_class, property_accepted_policy,
	       "accepted-policy", "get/set the accepted encrypt policy",
	       G_PARAM_READWRITE);

  mw_prop_obj(gobject_class, property_cipher,
	      "cipher", "get the accepted cipher",
	      MW_TYPE_OBJECT, G_PARAM_READABLE);

  mw_prop_boxed(gobject_class, property_offered_info,
		"offered-info", "get the offered additional info opaque",
		MW_TYPE_OPAQUE, G_PARAM_READABLE);

  mw_prop_boxed(gobject_class, property_accepted_info,
		"accepted-info", "get the accepted additional info opaque",
		MW_TYPE_OPAQUE, G_PARAM_READABLE);

  mw_prop_uint(gobject_class, property_close_code,
	       "close-code", "get the closing code",
	       G_PARAM_READABLE);
  
  mw_prop_boxed(gobject_class, property_close_info,
		"close-info", "get the closeing info opaque",
		MW_TYPE_OPAQUE, G_PARAM_READABLE);

  mw_prop_enum(gobject_class, property_state,
	       "state", "get the channel state",
	       MW_TYPE_CHANNEL_STATE_ENUM, G_PARAM_READWRITE);

  klass->signal_outgoing = mw_signal_outgoing();
  klass->signal_incoming = mw_signal_incoming();

  klass->open = mw_open;
  klass->close = mw_close;
  klass->feed = mw_feed;
  klass->send = mw_send;
}


static void mw_channel_init(GTypeInstance *instance, gpointer g_class) {
  MwChannel *self = (MwChannel *) instance;
  MwChannelPrivate *priv;

  priv = g_new0(MwChannelPrivate, 1);
  self->private = priv;
}


static const GTypeInfo info = {
  .class_size = sizeof(MwChannelClass),
  .base_init = NULL,
  .base_finalize = NULL,
  .class_init = mw_channel_class_init,
  .class_finalize = NULL,
  .class_data = NULL,
  .instance_size = sizeof(MwChannel),
  .n_preallocs = 0,
  .instance_init = mw_channel_init,
  .value_table = NULL,
};


GType MwChannel_getType() {
  static GType type = 0;

  if(type == 0) {
    type = g_type_register_static(MW_TYPE_OBJECT, "MwChannelType",
				  &info, 0);
  }
  
  return type;
}


void MwChannel_open(MwChannel *chan, const MwOpaque *info) {
  void (*fn)(MwChannel *, const MwOpaque *);

  g_return_if_fail(chan != NULL);

  fn = MW_CHANNEL_GET_CLASS(chan)->open;
  fn(chan, info);
}


void MwChannel_close(MwChannel *chan, guint32 code, const MwOpaque *info) {
  void (*fn)(MwChannel *, guint32, const MwOpaque *);

  g_return_if_fail(chan != NULL);

  fn = MW_CHANNEL_GET_CLASS(chan)->close;
  fn(chan, code, info);
}


void MwChannel_feed(MwChannel *chan, MwMessage *msg) {
  void (*fn)(MwChannel *, MwMessage *);

  g_return_if_fail(chan != NULL);
  g_return_if_fail(msg != NULL);

  fn = MW_CHANNEL_GET_CLASS(chan)->feed;
  fn(chan, msg);
}


void MwChannel_send(MwChannel *chan, guint16 type, const MwOpaque *msg,
		    gboolean encrypt) {

  void (*fn)(MwChannel *, guint16, const MwOpaque *, gboolean);

  g_return_if_fail(chan != NULL);

  fn = MW_CHANNEL_GET_CLASS(chan)->send;
  fn(chan, type, msg, encrypt);
}


gboolean MwChannel_isState(MwChannel *chan, enum mw_channel_state state) {
  guint cs;

  g_return_val_if_fail(chan != NULL, FALSE);

  g_object_get(G_OBJECT(chan), "state", &cs, NULL);

  return cs == state;
}


#define enum_val(val, alias) { val, #val, alias }


static const GEnumValue values[] = {
  enum_val(mw_channel_closed, "closed"),
  enum_val(mw_channel_pending, "pending"),
  enum_val(mw_channel_open, "open"),
  enum_val(mw_channel_error, "error"),
  { 0, NULL, NULL },
};


GType MwChannelStateEnum_getType() {
  static GType type = 0;
   
  if(type == 0) {
    type = g_enum_register_static("MwChannelStateEnumType", values);
  }

  return type;
}


/* The end. */
