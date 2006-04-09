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
#include <string.h>
#include "mw_channel.h"
#include "mw_common.h"
#include "mw_debug.h"
#include "mw_error.h"
#include "mw_encrypt.h"
#include "mw_marshal.h"
#include "mw_object.h"
#include "mw_parser.h"
#include "mw_queue.h"
#include "mw_session.h"
#include "mw_typedef.h"
#include "mw_util.h"


/** parent class of MwSessionClass, populated by mw_session_class_init */
static GObjectClass *parent_class;


enum properties {
  property_state = 1,

  property_master_channel,

  property_redirect_host,
  property_error,

  property_auth_user,
  property_auth_type,
  property_auth_password,
  property_auth_token,

  property_login_community,
  property_login_name,
  property_login_id,
  property_login_desc,
  property_login_ip,

  property_status,
  property_status_idle,
  property_status_desc,

  property_client_type,
  property_client_host,
  property_client_ver_major,
  property_client_ver_minor,

  property_server_id,
  property_server_ver_major,
  property_server_ver_minor,
};


#define MW_SESSION_GET_PRIVATE(o)					\
  (G_TYPE_INSTANCE_GET_PRIVATE((o), MW_TYPE_SESSION, MwSessionPrivate))


typedef struct mw_session_private MwSessionPrivate;


struct mw_session_private {

  MwLogin login;
  MwPrivacy privacy;
  MwStatus status;

  /** parser for incoming messages */
  MwParser *parser;

  /** queue for outgoing session messages */
  MwQueue *queue;

  /** meta queue for outgoing channel messages */
  MwMetaQueue *chan_queue;

  guint32 channel_counter;  /**< counter for outgoing channel ID */
  GHashTable *channels;     /**< associated channels, keyed by ID */

  GHashTable *ciphers;        /**< (guint16)ID:MwCipherClass */
  GHashTable *ciphers_by_pol; /**< (guint16)policy:MwCipherClass */

  guint prop_auth_type;
  gchar *prop_auth_password;
  MwOpaque *prop_auth_token;
  gchar *prop_client_host;

  guint master_channel;

  guint prop_client_ver_major;
  guint prop_client_ver_minor;
  guint prop_server_ver_major;
  guint prop_server_ver_minor;

  /* state dependant */
  gchar *redir_host;
  guint32 error_code;

  /** counter for outgoing one time messages */
  guint32 otm_counter;
};


/** handles "outgoing" signal on all channels created via a session */
static void mw_channel_outgoing(MwChannel *chan, MwMessage *msg,
				gpointer data) {
  MwSession *session = data;
  MwSessionPrivate *priv;
  MwMetaQueue *mq;
  MwPutBuffer *pb;
  MwOpaque tmp;
  guint sig;
  gboolean ret = FALSE;

  mw_debug_enter();

  priv = MW_SESSION_GET_PRIVATE(session);
  mq = priv->chan_queue;

  /* take the outgoing message, write it to a buffer, stick it in the
     channel meta-queue */
  pb = MwPutBuffer_new();
  MwMessage_put(pb, msg);
  MwPutBuffer_free(pb, &tmp);

  pb = MwPutBuffer_new();
  MwOpaque_put(pb, &tmp);
  MwPutBuffer_free(pb, MwMetaQueue_push(mq, G_OBJECT(chan)));

  /* trigger the session's pending signal */
  sig = MW_SESSION_GET_CLASS(session)->signal_pending;
  g_signal_emit(session, sig, 0, &ret);

  mw_debug_exit();
}


/** triggered when a channel is destroyed, via a weak reference
    handler*/
static void mw_channel_weak_cb(gpointer data, GObject *gone) {
  gpointer *mem = data;
  MwSession *session;
  MwSessionPrivate *priv;

  session = mem[0];
  priv = MW_SESSION_GET_PRIVATE(session);

  if(priv) {
    g_hash_table_remove(priv->channels, mem[1]);
  }

  /* free what's allocated in mw_chan_setup */
  g_free(mem);
}


static void mw_chan_setup(MwSession *s, MwChannel *chan) {
  MwSessionPrivate *priv;
  GHashTable *ht;
  guint id;
  gpointer *mem;

  priv = MW_SESSION_GET_PRIVATE(s);
  ht = priv->channels;

  g_object_get(G_OBJECT(chan), "id", &id, NULL);

  /* put the channel in a weak mapping by its ID. So long as something
     else has a reference to the channel, we'll accept incoming
     channel messages and route them to the channel itself */
  g_hash_table_insert(ht, GUINT_TO_POINTER(id), chan);

  /* annoying, we want to send along two pieces of information to the
     weak event, but we can only pass one */
  mem = g_new0(gpointer, 2);
  mem[0] = s;
  mem[1] = GUINT_TO_POINTER(id);
  g_object_weak_ref(G_OBJECT(chan), mw_channel_weak_cb, mem);

  /* catch channel outgoing data so we can queue it up for sending */
  g_signal_connect(chan, "outgoing", G_CALLBACK(mw_channel_outgoing), s);
}


static void mw_compose_auth_PLAIN(MwSession *s, MwMsgLogin *ml,
				  MwMsgHandshakeAck *mha) {
  MwPutBuffer *b;
  gchar *pass = NULL;

  g_object_get(G_OBJECT(s), "auth-password", &pass, NULL);

  b = MwPutBuffer_new();
  mw_str_put(b, pass);
  MwPutBuffer_free(b, &ml->auth_data);

  g_free(pass);
}


#if 0
static void mw_compose_auth_TOKEN(MwSession *s, MwMsgLogin *ml,
				  MwMsgHandshakeAck *mha) {
  MwOpaque *tok = NULL;

  g_object_get(G_OBJECT(s), "auth-token", &tok, NULL);

  ml->auth_data.data = tok->data;
  ml->auth_data.len = tok->len;

  /* we stole the contents of the opaque, so we only need to free the
     structure itself */
  g_free(tok);
}
#endif


static void mw_compose_auth_RC2(MwSession *s, MwMsgLogin *ml,
				MwMsgHandshakeAck *mha) {

  MwMPI *rand;
  MwOpaque key, plain, enc;
  MwRC2Cipher *ci;
  MwPutBuffer *pb;

  /* get the random key */
  rand = MwMPI_new();
  MwMPI_random(rand, 40);
  MwMPI_export(rand, &key);
  MwMPI_free(rand);

  /* get the plain password */
  g_object_get(G_OBJECT(s), "auth-password", &plain.data, NULL);
  plain.len = strlen((gchar *) plain.data);

  /* encrypt plain with key */
  ci = g_object_new(MW_TYPE_RC2_CIPHER, NULL);
  MwRC2Cipher_setEncryptKey(ci, key.data, key.len);
  MwCipher_encrypt(MW_CIPHER(ci), &plain, &enc);

  /* don't need the cipher or the plain any more */
  mw_gobject_unref(ci);
  MwOpaque_clear(&plain);

  /* assemble the auth block */
  pb = MwPutBuffer_new();
  MwOpaque_put(pb, &key);
  MwOpaque_put(pb, &enc);

  /* don't need these anymore */
  MwOpaque_clear(&key);
  MwOpaque_clear(&enc);

  /* put the auth block in the message */
  MwPutBuffer_free(pb, &ml->auth_data);
}


static void mw_compose_auth_DH_RC2(MwSession *s, MwMsgLogin *ml,
				   MwMsgHandshakeAck *mha) {
  gchar *pass = NULL;
  MwCipher *ci;
  MwPutBuffer *p;
  MwOpaque a, b, c;
  
  if(! mha->data.len) {
    /* there's no DH public key being offered, fall back to the
       crappier RC2 method */

    g_debug("falling back to RC2 authentication");

    ml->auth_type = mw_auth_type_RC2;
    mw_compose_auth_RC2(s, ml, mha);
    return;
  }

  g_object_get(G_OBJECT(s), "auth-password", &pass, NULL);

  ci = g_object_new(MW_TYPE_DH_RC2_CIPHER, NULL);

  /* assemble the unencrypted auth data from the magic value and the
     password, put it in (a) */
  p = MwPutBuffer_new();
  mw_uint32_put(p, mha->magic);
  mw_str_put(p, pass);
  MwPutBuffer_free(p, &a);

  /* setup the cipher with the remote public key, and put the local
     public key in (b) */
  MwCipher_offered(ci, &mha->data);
  MwCipher_accept(ci, &b);

  /* encrypt the password (a), put the result in (c) */
  MwCipher_encrypt(ci, &a, &c);

  /* assemble the final auth data from the encrypted data (c) and the
     local public key (b) and put it in the login message */
  p = MwPutBuffer_new();
  mw_uint16_put(p, 0x0001);
  MwOpaque_put(p, &b);
  MwOpaque_put(p, &c);
  MwPutBuffer_free(p, &ml->auth_data);

  /* clean up after ourselves */
  g_free(pass);
  mw_gobject_unref(ci);
  MwOpaque_clear(&a);
  MwOpaque_clear(&b);
  MwOpaque_clear(&c);
}


static void recv_HANDSHAKE_ACK(MwSession *s, MwMsgHandshakeAck *m) {
  MwSessionPrivate *priv;
  MwMsgLogin *msglogin;
  guint auth_type, client_type;
  gchar *user;
  gint state;

  g_object_get(G_OBJECT(s), "state", &state, NULL);

  g_return_if_fail(state == mw_session_handshake);
  
  priv = MW_SESSION_GET_PRIVATE(s);

  priv->prop_server_ver_major = m->major;
  priv->prop_server_ver_minor = m->minor;

  g_object_set(G_OBJECT(s), "state", mw_session_handshake_ack, NULL);

  msglogin = MwMessage_new(mw_message_LOGIN);

  g_object_get(G_OBJECT(s),
	       "client-type", &client_type,
	       "auth-type", &auth_type,
	       "auth-user", &user,
	       NULL);

  msglogin->client_type = client_type;
  msglogin->auth_type = auth_type;
  msglogin->name = user;

  switch(auth_type) {
  case mw_auth_type_PLAIN:
    mw_compose_auth_PLAIN(s, msglogin, m);
    break;
  case mw_auth_type_RC2:
    mw_compose_auth_RC2(s, msglogin, m);
    break;
  case mw_auth_type_DH_RC2:
    mw_compose_auth_DH_RC2(s, msglogin, m);
    break;
  default:
    g_warning("unknown session auth-type, 0x%x", auth_type);
  }

  MwSession_sendMessage(s, MW_MESSAGE(msglogin));

  msglogin->name = NULL;
  MwMessage_free(MW_MESSAGE(msglogin));

  g_object_set(G_OBJECT(s), "state", mw_session_login, NULL);
}


static void recv_LOGIN_REDIRECT(MwSession *s, MwMsgLoginRedirect *m) {
  gint state;

  g_object_get(G_OBJECT(s), "state", &state, NULL);

  g_return_if_fail(state == mw_session_login);
  
  g_object_set(G_OBJECT(s),
	       "redirect-host", m->host,
	       "state", mw_session_login_redirect,
	       NULL);
}


static void recv_LOGIN_ACK(MwSession *s, MwMsgLoginAck *m) {
  MwSessionClass *klass;
  MwSessionPrivate *priv;
  gint state;

  g_object_get(G_OBJECT(s), "state", &state, NULL);

  g_return_if_fail(state == mw_session_login ||
		   state == mw_session_login_force);

  klass = MW_SESSION_GET_CLASS(s);

  priv = MW_SESSION_GET_PRIVATE(s);

  MwLogin_clone(&priv->login, &m->login, TRUE);
  MwPrivacy_clone(&priv->privacy, &m->privacy, TRUE);
  MwStatus_clone(&priv->status, &m->status, TRUE);

  g_signal_emit(s, klass->signal_got_status, 0, NULL);
  g_signal_emit(s, klass->signal_got_privacy, 0, NULL);
  
  g_object_set(G_OBJECT(s), "state", mw_session_login_ack, NULL);
  g_object_set(G_OBJECT(s), "state", mw_session_started, NULL);
}


static void recv_CHANNEL_CREATE(MwSession *s, MwMsgChannelCreate *m) {
  MwChannel *chan;
  guint sig;
  gboolean wanted = FALSE;

  mw_debug_enter();

  chan = g_object_new(MW_TYPE_CHANNEL, "session", s, NULL);
  MwChannel_feed(chan, MW_MESSAGE(m));

  mw_chan_setup(s, chan);

  sig = MW_SESSION_GET_CLASS(s)->signal_channel;
  g_signal_emit(s, sig, 0, chan, &wanted);

  mw_gobject_unref(chan);

  mw_debug_exit();
}


static void recv_CHANNEL_CLOSE(MwSession *s, MwMsgChannelClose *m) {
  guint master = 0x00;

  g_object_get(G_OBJECT(s), "master-channel-id", &master, NULL);

  if(m->head.channel == master) {
    MwSession_stop(s, m->reason);

  } else {
    MwChannel *chan = MwSession_getChannel(s, m->head.channel);
    MwChannel_feed(chan, MW_MESSAGE(m));
  }
}


static void recv_CHANNEL_SEND(MwSession *s, MwMsgChannelSend *m) {
  MwChannel *chan = MwSession_getChannel(s, m->head.channel);
  MwChannel_feed(chan, MW_MESSAGE(m));
}


static void recv_CHANNEL_ACCEPT(MwSession *s, MwMsgChannelAccept *m) {
  MwChannel *chan = MwSession_getChannel(s, m->head.channel);
  MwChannel_feed(chan, MW_MESSAGE(m));
}


static void recv_ONE_TIME(MwSession *s, MwMsgOneTime *m) {
  MwSessionClass *klass;

  mw_debug_enter();
  
  klass = MW_SESSION_GET_CLASS(s);
  g_signal_emit(s, klass->signal_got_one_time, 0,
		m->id, m->service, m->proto_type, m->proto_ver,
		m->type, &m->data, NULL);
  
  mw_debug_exit();
}


static void recv_STATUS(MwSession *s, MwMsgStatus *m) {
  MwSessionClass *klass;
  MwSessionPrivate *priv;

  mw_debug_enter();

  klass = MW_SESSION_GET_CLASS(s);
  priv = MW_SESSION_GET_PRIVATE(s);

  MwStatus_clone(&priv->status, &m->status, TRUE);
  g_signal_emit(s, klass->signal_got_status, 0, NULL);

  mw_debug_exit();
}


static void recv_PRIVACY(MwSession *s, MwMsgPrivacy *m) {
  MwSessionClass *klass;
  MwSessionPrivate *priv;

  mw_debug_enter();

  klass = MW_SESSION_GET_CLASS(s);
  priv = MW_SESSION_GET_PRIVATE(s);

  MwPrivacy_clone(&priv->privacy, &m->privacy, TRUE);
  g_signal_emit(s, klass->signal_got_privacy, 0, NULL);

  mw_debug_exit();
}


static void recv_SENSE_SERVICE(MwSession *s, MwMsgSenseService *m) {
  MwSessionClass *klass = MW_SESSION_GET_CLASS(s);
  g_signal_emit(s, klass->signal_got_sense_service, 0,
		m->service);
}


static void recv_ADMIN(MwSession *s, MwMsgAdmin *m) {
  MwSessionClass *klass = MW_SESSION_GET_CLASS(s);
  g_signal_emit(s, klass->signal_got_admin, 0, m->text);
}


static void recv_ANNOUNCE(MwSession *s, MwMsgAnnounce *m) {
  MwSessionClass *klass = MW_SESSION_GET_CLASS(s);
  g_signal_emit(s, klass->signal_got_announce, 0,
		m->may_reply, m->sender, m->text);
}


static void recv_msg(MwParser *parser,
		     const guchar *buf, gsize len,
		     gpointer data) {

  MwSession *session = data;
  MwOpaque o = { .data = (guchar *) buf, .len = len };
  MwGetBuffer *b;
  MwMessage *msg;

  mw_debug_enter();

  g_return_if_fail(session != NULL);

  b = MwGetBuffer_wrap(&o);
  msg = MwMessage_get(b);
  MwGetBuffer_free(b);

  if(! msg) {
    mw_debug_data(buf, len, "Unknown message data");
    return;
  }

  switch(msg->type) {
  case mw_message_HANDSHAKE_ACK:
    recv_HANDSHAKE_ACK(session, (MwMsgHandshakeAck *) msg);
    break;
  case mw_message_LOGIN_REDIRECT:
    recv_LOGIN_REDIRECT(session, (MwMsgLoginRedirect *) msg);
    break;
  case mw_message_LOGIN_ACK:
    recv_LOGIN_ACK(session, (MwMsgLoginAck *) msg);
    break;

  case mw_message_CHANNEL_CREATE:
    recv_CHANNEL_CREATE(session, (MwMsgChannelCreate *) msg);
    break;
  case mw_message_CHANNEL_CLOSE:
    recv_CHANNEL_CLOSE(session, (MwMsgChannelClose *) msg);
    break;
  case mw_message_CHANNEL_SEND:
    recv_CHANNEL_SEND(session, (MwMsgChannelSend *) msg);
    break;
  case mw_message_CHANNEL_ACCEPT:
    recv_CHANNEL_ACCEPT(session, (MwMsgChannelAccept *) msg);
    break;

  case mw_message_ONE_TIME:
    recv_ONE_TIME(session, (MwMsgOneTime *) msg);
    break;
    
  case mw_message_STATUS:
    recv_STATUS(session, (MwMsgStatus *) msg);
    break;
  case mw_message_PRIVACY:
    recv_PRIVACY(session, (MwMsgPrivacy *) msg);
    break;

  case mw_message_SENSE_SERVICE:
    recv_SENSE_SERVICE(session, (MwMsgSenseService *) msg);
    break;
  case mw_message_ADMIN:
    recv_ADMIN(session, (MwMsgAdmin *) msg);
    break;
  case mw_message_ANNOUNCE:
    recv_ANNOUNCE(session, (MwMsgAnnounce *) msg);
    break;
    
  default:
    ; /* todo: log it */
  }

  MwMessage_free(msg);
  
  mw_debug_exit();
}


static void mw_start(MwSession *self) {
  MwMsgHandshake *msg;
  guint major = 0, minor = 0, type = 0;
  gchar *local_host = NULL;

  g_object_set(G_OBJECT(self), "state", mw_session_starting, NULL);

  g_object_get(G_OBJECT(self),
	       "client-ver-major", &major,
	       "client-ver-minor", &minor,
	       "client-type", &type,
	       "client-host", &local_host,
	       NULL);

  msg = MwMessage_new(mw_message_HANDSHAKE);

  msg->major = major;
  msg->minor = minor;
  msg->client_type = type;

  /* these values are just guestimates */
  if(major >= 0x001e && minor >= 0x001d) {
    msg->unknown_a = 0x0100;
    msg->local_host = local_host;
  }

  MwSession_sendMessage(self, MW_MESSAGE(msg));
  MwMessage_free(MW_MESSAGE(msg));

  g_object_set(G_OBJECT(self), "state", mw_session_handshake, NULL);
}


static void mw_chan_close(gpointer chan, gpointer duh) {
  guint chan_state;
  g_object_get(G_OBJECT(chan), "state", &chan_state, NULL);
  if(chan_state == mw_channel_open) {
    MwChannel_close(chan, 0x00, NULL);
  }
}


static void mw_stop(MwSession *self, guint32 reason) {
  MwSessionPrivate *priv;

  g_debug("stopping session %p: reason 0x%x", self, reason);

  priv = MW_SESSION_GET_PRIVATE(self);

  if(reason) {
    g_object_set(G_OBJECT(self),
		 "error", reason,
		 "state", mw_session_error,
		 NULL);
  }

  g_object_set(G_OBJECT(self), "state", mw_session_stopping, NULL);

  /* close all channels not already closed */
  MwSession_foreachChannel(self, mw_chan_close, NULL);

  /* clear out session queue */
  MwQueue_clear(priv->queue, (GDestroyNotify) MwOpaque_clear);

  /* clear out channel queue */
  MwMetaQueue_clear(priv->chan_queue, (GDestroyNotify) MwOpaque_clear);

  /* and we're stopped */
  g_object_set(G_OBJECT(self), "state", mw_session_stopped, NULL);
}


static void mw_feed(MwSession *self, const guchar *buf, gsize len) {
  MwSessionPrivate *priv;
  MwParser *parser;
  
  priv = MW_SESSION_GET_PRIVATE(self);
  parser = priv->parser;
  MwParser_feed(parser, buf, len);
}


static guint mw_pending(MwSession *self) {
  MwSessionPrivate *priv;
  MwQueue *q;
  MwMetaQueue *mq;
  guint a, b, c;
  
  priv = MW_SESSION_GET_PRIVATE(self);

  q = priv->queue;
  mq = priv->chan_queue;

  a = MwQueue_size(q);
  b = MwMetaQueue_size(mq);
  c = a + b;

  g_debug("session queue: %u, channel queue: %u, total %u", a, b, c);

  return c;
}


static void mw_flush(MwSession *self) {
  MwSessionPrivate *priv;
  MwQueue *q;
  MwMetaQueue *mq;
  MwOpaque *o;
  
  priv = MW_SESSION_GET_PRIVATE(self);

  q = priv->queue;
  mq = priv->chan_queue;

  /* get the next session message */
  o = MwQueue_next(q);

  /* ... or the next channel message */
  if(!o) o = MwMetaQueue_next(mq);

  if(o) {
    gint sig = MW_SESSION_GET_CLASS(self)->signal_write;
    g_signal_emit(self, sig, 0, o->data, o->len, NULL);
    MwOpaque_clear(o);
  }
}


__attribute__((__used__))
static void mw_write_msg(MwSession *self, const MwOpaque *msg) {
  MwSessionPrivate *priv;
  MwPutBuffer *pb;
  MwQueue *q;
  gint sig;
  gboolean ret = FALSE;

  priv = MW_SESSION_GET_PRIVATE(self);
  q = priv->queue;

  pb = MwPutBuffer_new();
  MwOpaque_put(pb, msg);
  MwPutBuffer_free(pb, MwQueue_push(q));

  sig = MW_SESSION_GET_CLASS(self)->signal_pending;
  g_signal_emit(self, sig, 0, &ret);
}


static void mw_send_message(MwSession *self, const MwMessage *msg) {
  MwSessionPrivate *priv;
  MwOpaque tmp;
  MwPutBuffer *pb;
  MwQueue *q;
  gint sig;
  gboolean ret = FALSE;

  priv = MW_SESSION_GET_PRIVATE(self);
  q = priv->queue;

  /* render the message */
  pb = MwPutBuffer_new();
  MwMessage_put(pb, msg);
  MwPutBuffer_free(pb, &tmp);

  /* wrap the rendered message */
  pb = MwPutBuffer_new();
  MwOpaque_put(pb, &tmp);
  MwOpaque_clear(&tmp);
  MwPutBuffer_free(pb, MwQueue_push(q));

  sig = MW_SESSION_GET_CLASS(self)->signal_pending;
  g_signal_emit(self, sig, 0, &ret);
}


static void mw_send_keepalive(MwSession *self) {
  MwSessionPrivate *priv;
  guchar poke = 0x80;
  MwQueue *q;
  MwOpaque *o;
  gint sig;
  gboolean ret = FALSE;

  priv = MW_SESSION_GET_PRIVATE(self);

  q = priv->queue;
  o = MwQueue_push(q);

  o->len = 1;
  o->data = g_memdup(&poke, 1);

  sig = MW_SESSION_GET_CLASS(self)->signal_pending;
  g_signal_emit(self, sig, 0, &ret);
}


static void mw_send_announce(MwSession *self, gboolean may_reply,
			     const gchar **rcpt, const gchar *text) {
  MwMsgAnnounce *msg;
  gchar **r = (gchar **) rcpt;

  msg = MwMessage_new(mw_message_ANNOUNCE);
  msg->may_reply = may_reply;
  msg->text = (gchar *) text;

  msg->rcpt_count = g_strv_length(r);
  msg->recipients = r;

  {
    guint c = msg->rcpt_count;
    g_debug("sending announce to %u recipients", c);
    while(c--) g_debug(" rcpt: %s", r[c]);
  }

  /* send our newly composed message */
  MwSession_sendMessage(self, MW_MESSAGE(msg));
  
  /* didn't make a copy, so steal it back before freeing msg */
  msg->text = NULL;

  /* didn't make a copy, so steal it back */
  msg->recipients = NULL;

  MwMessage_free(MW_MESSAGE(msg));
}


static void mw_force_login(MwSession *self) {
  MwMsgLoginForce *msg;
  gint state;

  g_object_get(G_OBJECT(self), "state", &state, NULL);

  g_return_if_fail(state == mw_session_login_redirect);

  msg = MwMessage_new(mw_message_LOGIN_FORCE);
  MwSession_sendMessage(self, MW_MESSAGE(msg));
  MwMessage_free(MW_MESSAGE(msg));  

  g_object_set(G_OBJECT(self), "state", mw_session_login_force, NULL);
}


static void mw_sense_service(MwSession *self, guint32 id) {
  MwMsgSenseService *msg;
  
  msg = MwMessage_new(mw_message_SENSE_SERVICE);
  msg->service = id;

  MwSession_sendMessage(self, MW_MESSAGE(msg));

  MwMessage_free(MW_MESSAGE(msg));
}


guint mw_send_one_time(MwSession *self,
		       const MwIdentity *target,
		       guint32 service,
		       guint32 proto_type, guint32 proto_ver,
		       guint16 type, const MwOpaque *data) {

  MwSessionPrivate *priv;
  MwMsgOneTime *otm;
  guint32 id;

  priv = MW_SESSION_GET_PRIVATE(self);

  id = ++(priv->otm_counter);

  otm = MwMessage_new(mw_message_ONE_TIME);
  otm->id = id;
  MwIdentity_clone(&otm->target, target, TRUE);
  otm->service = service;
  otm->proto_type = proto_type;
  otm->proto_ver = proto_ver;
  otm->type = type;
  MwOpaque_clone(&otm->data, data);
  
  MwSession_sendMessage(self, MW_MESSAGE(otm));

  MwMessage_free(MW_MESSAGE(otm));

  return id;
}			   


static guint32 mw_next_channel_id(guint32 *cur) {
  guint32 c = *cur;
  c = (c + 1) % 0x80000000;
  return (*cur = c);
}


static MwChannel *mw_new_channel(MwSession *self) {
  MwSessionPrivate *priv;
  MwChannel *chan;
  guint32 id;

  priv = MW_SESSION_GET_PRIVATE(self);
  
  id = mw_next_channel_id(&priv->channel_counter);
  chan = g_object_new(MW_TYPE_CHANNEL, "session", self, "id", id, NULL);

  mw_chan_setup(self, chan);

  return chan;
}


static MwChannel *mw_get_channel(MwSession *self, guint32 id) {
  MwSessionPrivate *priv;
  GHashTable *ht;

  priv = MW_SESSION_GET_PRIVATE(self);
  ht = priv->channels;

  return g_hash_table_lookup(ht, GUINT_TO_POINTER(id));
}


static void mw_foreach_channel(MwSession *self, GFunc func, gpointer d) {
  MwSessionPrivate *priv;
  GHashTable *ht;
  
  priv = MW_SESSION_GET_PRIVATE(self);
  ht = priv->channels;

  mw_map_foreach_val(ht, func, d);
}


static gboolean mw_add_cipher(MwSession *self, MwCipherClass *klass) {
  MwSessionPrivate *priv;
  GHashTable *htid, *htpol;
  guint id, pol;

  priv = MW_SESSION_GET_PRIVATE(self);

  htid = priv->ciphers;
  htpol = priv->ciphers_by_pol;

  pol = MwCipherClass_getPolicy(klass);
  id = MwCipherClass_getIdentifier(klass);

  if(g_hash_table_lookup(htid, GUINT_TO_POINTER(id)))
    return FALSE;

  if(g_hash_table_lookup(htpol, GUINT_TO_POINTER(pol)))
    return FALSE;
  
  g_debug("making available cipher by id 0x%x and policy 0x%x", id, pol);

  g_hash_table_insert(htid, GUINT_TO_POINTER(id), klass);
  g_hash_table_insert(htpol, GUINT_TO_POINTER(pol), klass);

  return TRUE;
}


static MwCipherClass *mw_get_cipher(MwSession *self, guint16 id) {
  MwSessionPrivate *priv;
  GHashTable *ht;
  guint key = (guint) id;
  MwCipherClass *cc;

  g_debug("looking for cipher id 0x%x", key);

  priv = MW_SESSION_GET_PRIVATE(self);
  ht = priv->ciphers;

  cc = g_hash_table_lookup(ht, GUINT_TO_POINTER(key));
  g_debug("found cipher %p", cc);

  return cc;
}


static MwCipherClass *mw_get_cipher_by_pol(MwSession *self, guint16 pol) {
  MwSessionPrivate *priv;
  GHashTable *ht;
  guint key = (guint) pol;

  priv = MW_SESSION_GET_PRIVATE(self);
  ht = priv->ciphers_by_pol;

  return g_hash_table_lookup(ht, GUINT_TO_POINTER(key));
}


static void mw_remove_cipher(MwSession *self, MwCipherClass *klass) {
  MwSessionPrivate *priv;
  GHashTable *htid, *htpol;
  guint id, pol;

  priv = MW_SESSION_GET_PRIVATE(self);

  htid = priv->ciphers;
  htpol = priv->ciphers_by_pol;

  id = MwCipherClass_getPolicy(klass);
  pol = MwCipherClass_getIdentifier(klass);
  
  if(g_hash_table_lookup(htid, GUINT_TO_POINTER(id)))
    g_hash_table_remove(htid, GUINT_TO_POINTER(id));
  
  if(g_hash_table_lookup(htpol, GUINT_TO_POINTER(pol)))
    g_hash_table_remove(htpol, GUINT_TO_POINTER(pol));
}


static void mw_foreach_cipher(MwSession *self, GFunc func, gpointer d) {
  MwSessionPrivate *priv;
  GHashTable *ht;
  
  priv = MW_SESSION_GET_PRIVATE(self);
  ht = priv->ciphers;

  mw_map_foreach_val(ht, func, d);
}


static GObject *
mw_session_constructor(GType type, guint props_count,
		       GObjectConstructParam *props) {

  MwSessionClass *klass;

  GObject *obj;
  MwSession *self;
  MwSessionPrivate *priv;
  
  klass = MW_SESSION_CLASS(g_type_class_peek(MW_TYPE_SESSION));

  obj = parent_class->constructor(type, props_count, props);
  self = (MwSession *) obj;

  priv = MW_SESSION_GET_PRIVATE(self);

  priv->channel_counter = 0x00;
  priv->channels = g_hash_table_new(g_direct_hash, g_direct_equal);

  priv->ciphers = g_hash_table_new(g_direct_hash, g_direct_equal);
  priv->ciphers_by_pol = g_hash_table_new(g_direct_hash, g_direct_equal);

  /* queues for outgoing */
  priv->queue = MwQueue_new(sizeof(MwOpaque), 8);
  priv->chan_queue = MwMetaQueue_new(sizeof(MwOpaque), 8);

  /* parser for incoming */
  priv->parser = MwParser_new(recv_msg, self);

  /* set initial values for properties */
  g_object_set(obj,
	       "state", mw_session_stopped,
	       "auth-type", mw_auth_type_DH_RC2,
	       "client-ver-major", MW_PROTOCOL_VERSION_MAJOR,
	       "client-ver-minor", MW_PROTOCOL_VERSION_MINOR,
	       "client-type", mw_client_MEANWHILE,
	       NULL);

  return obj;
}


static void mw_set_property(GObject *object,
			    guint property_id, const GValue *value,
			    GParamSpec *pspec) {

  MwSession *self = MW_SESSION(object);
  MwSessionPrivate *priv;

  priv = MW_SESSION_GET_PRIVATE(self);

  switch(property_id) {
  case property_state:
    MwObject_setState(MW_OBJECT(object), value);
    break;

  case property_auth_user:
    g_free(priv->login.id.user);
    priv->login.id.user = g_value_dup_string(value);
    break;
  case property_auth_type:
    priv->prop_auth_type = g_value_get_uint(value);
    break;
  case property_auth_password:
    g_free(priv->prop_auth_password);
    priv->prop_auth_password = g_value_dup_string(value);
    break;
  case property_auth_token:
    g_boxed_free(MW_TYPE_OPAQUE, priv->prop_auth_token);
    priv->prop_auth_token = g_value_dup_boxed(value);
    break;
  case property_client_type:
    priv->login.client = g_value_get_uint(value);
    break;
  case property_client_host:
    g_free(priv->prop_client_host);
    priv->prop_client_host = g_value_dup_string(value);
    break;
  case property_client_ver_major:
    priv->prop_client_ver_major = g_value_get_uint(value);
    break;
  case property_client_ver_minor:
    priv->prop_client_ver_minor = g_value_get_uint(value);
    break;
  }
}


static void mw_get_property(GObject *object,
			    guint property_id, GValue *value,
			    GParamSpec *pspec) {

  MwSession *self = MW_SESSION(object);
  MwSessionPrivate *priv;

  priv = MW_SESSION_GET_PRIVATE(self);

  switch(property_id) {
  case property_state:
    MwObject_getState(MW_OBJECT(self), value);
    break;

  case property_master_channel:
    g_value_set_uint(value, priv->master_channel);
    break;

  case property_error:
    g_value_set_uint(value, priv->error_code);
    break;
  case property_redirect_host:
    g_value_set_string(value, priv->redir_host);
    break;

  case property_auth_user:
    g_value_set_string(value, priv->login.id.user);
    break;
  case property_auth_type:
    g_value_set_uint(value, priv->prop_auth_type);
    break;
  case property_auth_password:
    g_value_set_string(value, priv->prop_auth_password);
    break;
  case property_auth_token:
    g_value_set_boxed(value, priv->prop_auth_token);
    break;

  case property_login_community:
    g_value_set_string(value, priv->login.id.community);
    break;
  case property_login_name:
    g_value_set_string(value, priv->login.name);
    break;
  case property_login_id:
    g_value_set_string(value, priv->login.login_id);
    break;
  case property_login_desc:
    g_value_set_string(value, priv->login.desc);
    break;
  case property_login_ip:
    g_value_set_uint(value, priv->login.ip_addr);
    break;

  case property_status:
    g_value_set_uint(value, priv->status.status);
    break;
  case property_status_idle:
    g_value_set_uint(value, priv->status.time);
    break;
  case property_status_desc:
    g_value_set_string(value, priv->status.desc);
    break;

  case property_client_type:
    g_value_set_uint(value, priv->login.client);
    break;
  case property_client_host:
    g_value_set_string(value, priv->prop_client_host);
    break;
  case property_client_ver_major:
    g_value_set_uint(value, priv->prop_client_ver_major);
    break;
  case property_client_ver_minor:
    g_value_set_uint(value, priv->prop_client_ver_minor);
    break;

  case property_server_id:
    g_value_set_string(value, priv->login.server_id);
    break;
  case property_server_ver_major:
    g_value_set_uint(value, priv->prop_server_ver_major);
    break;
  case property_server_ver_minor:
    g_value_set_uint(value, priv->prop_server_ver_minor);
    break;
  }

}


static void mw_session_dispose(GObject *obj) {
  MwSession *self = (MwSession *) obj;
  MwSessionPrivate *priv;

  mw_debug_enter();

  priv = MW_SESSION_GET_PRIVATE(self);

  g_hash_table_destroy(priv->channels);
  g_hash_table_destroy(priv->ciphers);
  MwQueue_free(priv->queue);
  MwMetaQueue_free(priv->chan_queue);
  MwParser_free(priv->parser);

  parent_class->dispose(obj);

  mw_debug_exit();
}


static guint mw_signal_pending() {
  return g_signal_new("pending",
		      MW_TYPE_SESSION,
		      G_SIGNAL_RUN_LAST,
		      G_STRUCT_OFFSET(MwSessionClass, handle_pending),
		      g_signal_accumulator_true_handled, NULL,
		      mw_marshal_BOOLEAN__VOID,
		      G_TYPE_BOOLEAN,
		      0);
}


static guint mw_signal_write() {
  return g_signal_new("write",
		      MW_TYPE_SESSION,
		      0,
		      0,
		      NULL, NULL,
		      mw_marshal_VOID__POINTER_UINT,
		      G_TYPE_NONE,
		      2,
		      G_TYPE_POINTER,
		      G_TYPE_UINT);
}


static guint mw_signal_channel() {
  return g_signal_new("channel",
		      MW_TYPE_SESSION,
		      G_SIGNAL_RUN_LAST,
		      G_STRUCT_OFFSET(MwSessionClass, handle_channel),
		      g_signal_accumulator_true_handled, NULL,
		      mw_marshal_BOOLEAN__POINTER,
		      G_TYPE_BOOLEAN,
		      1,
		      MW_TYPE_CHANNEL);
}


static guint mw_signal_got_status() {
  return g_signal_new("got-status",
		      MW_TYPE_SESSION,
		      0,
		      0,
		      NULL, NULL,
		      mw_marshal_VOID__VOID,
		      G_TYPE_NONE,
		      0);
}


static guint mw_signal_got_privacy() {
  return g_signal_new("got-privacy",
		      MW_TYPE_SESSION,
		      0,
		      0,
		      NULL, NULL,
		      mw_marshal_VOID__VOID,
		      G_TYPE_NONE,
		      0); 
}


static guint mw_signal_got_admin() {
  return g_signal_new("got-admin",
		      MW_TYPE_SESSION,
		      0,
		      0,
		      NULL, NULL,
		      mw_marshal_VOID__POINTER,
		      G_TYPE_NONE,
		      0);
}


static guint mw_signal_got_announce() {
  return g_signal_new("got-announce",
		      MW_TYPE_SESSION,
		      0,
		      0,
		      NULL, NULL,
		      mw_marshal_VOID__BOOLEAN_POINTER_POINTER,
		      G_TYPE_NONE,
		      3,
		      G_TYPE_BOOLEAN,
		      G_TYPE_POINTER,  /* sender */
		      G_TYPE_POINTER); /* text */
}


static guint mw_signal_got_sense_service() {
  return g_signal_new("got-sense-service",
		      MW_TYPE_SESSION,
		      0,
		      0,
		      NULL, NULL,
		      mw_marshal_VOID__UINT,
		      G_TYPE_NONE,
		      1,
		      G_TYPE_UINT);
}


static guint mw_signal_got_one_time() {
  return g_signal_new("got-one-time",
		      MW_TYPE_SESSION,
		      0,
		      0,
		      NULL, NULL,
		      mw_marshal_VOID__UINT_UINT_UINT_UINT_UINT_POINTER,
		      G_TYPE_NONE,
		      6,
		      G_TYPE_UINT, /* id */
		      G_TYPE_UINT, /* service */
		      G_TYPE_UINT, /* protocol type */
		      G_TYPE_UINT, /* protocol version */
		      G_TYPE_UINT, /* type */
		      MW_TYPE_OPAQUE);
}


static void mw_session_class_init(gpointer g_class, gpointer g_class_data) {
  GObjectClass *gobject_class;
  MwObjectClass *mwobject_class;
  MwSessionClass *klass;

  gobject_class = G_OBJECT_CLASS(g_class);
  mwobject_class = MW_OBJECT_CLASS(g_class);
  klass = MW_SESSION_CLASS(g_class);

  parent_class = g_type_class_peek_parent(gobject_class);
  
  gobject_class->constructor = mw_session_constructor;
  gobject_class->dispose = mw_session_dispose;
  gobject_class->set_property = mw_set_property;
  gobject_class->get_property = mw_get_property;

  g_type_class_add_private(klass, sizeof(MwSessionPrivate));

  mw_prop_uint(gobject_class, property_master_channel,
	       "master-channel-id", "get the master channel ID",
	       G_PARAM_READABLE);

  mw_prop_uint(gobject_class, property_error,
	       "error", "get/set the error code",
	       G_PARAM_READWRITE);
  
  mw_prop_str(gobject_class, property_redirect_host,
	      "redirect-host", "get/set the redirect host",
	      G_PARAM_READWRITE);

  mw_prop_str(gobject_class, property_auth_user,
	      "auth-user", "get/set authentication user id",
	      G_PARAM_READWRITE);

  mw_prop_uint(gobject_class, property_auth_type,
	       "auth-type", "get/set authentication type",
	       G_PARAM_READWRITE);
  
  mw_prop_str(gobject_class, property_auth_password,
	      "auth-password", "get/set authentication password",
	      G_PARAM_READWRITE);
  
  mw_prop_boxed(gobject_class, property_auth_token,
		"auth-token", "get/set authentication token Opaque",
		MW_TYPE_OPAQUE, G_PARAM_READWRITE);  
  
  mw_prop_str(gobject_class, property_login_community,
	      "login-community", "get session community",
	      G_PARAM_READABLE);

  mw_prop_str(gobject_class, property_login_name,
	      "login-name", "get session full name",
	      G_PARAM_READABLE);
  
  mw_prop_str(gobject_class, property_login_id,
	      "login-id", "get session identifier ",
	      G_PARAM_READABLE);

  mw_prop_str(gobject_class, property_login_desc,
	      "login-desc", "get session description",
	      G_PARAM_READABLE);

  mw_prop_uint(gobject_class, property_login_ip,
	       "login-ip", "get session login IP address ",
	       G_PARAM_READABLE);

  mw_prop_uint(gobject_class, property_status,
	       "status", "get session status",
	       G_PARAM_READABLE);

  mw_prop_uint(gobject_class, property_status_idle,
	       "status-idle-since", "get session idle timestamp in seconds",
	       G_PARAM_READABLE);

  mw_prop_str(gobject_class, property_status_desc,
	      "status-message", "get session status message",
	      G_PARAM_READABLE);

  mw_prop_uint(gobject_class, property_client_type,
	       "client-type", "get/set client type identifier number",
	       G_PARAM_READWRITE);
  
  mw_prop_str(gobject_class, property_client_host,
	      "client-host", "get/set client hostname",
	      G_PARAM_READWRITE);

  mw_prop_uint(gobject_class, property_client_ver_major,
	       "client-ver-major", "get/set client major version number",
	       G_PARAM_READWRITE);

  mw_prop_uint(gobject_class, property_client_ver_minor,
	       "client-ver-minor", "get/set client minor version number",
	       G_PARAM_READWRITE);

  mw_prop_str(gobject_class, property_server_id,
	      "server-id", "get server identifier",
	      G_PARAM_READABLE);

  mw_prop_uint(gobject_class, property_server_ver_major,
	       "server-ver-major", "get server major version number",
	       G_PARAM_READABLE);

  mw_prop_uint(gobject_class, property_server_ver_minor,
	       "server-ver-minor", "get server minor version number",
	       G_PARAM_READABLE);

  mw_prop_enum(gobject_class, property_state,
	       "state", "get session state",
	       MW_TYPE_SESSION_STATE_ENUM, G_PARAM_READWRITE);

  klass->signal_pending = mw_signal_pending();
  klass->signal_write = mw_signal_write();
  klass->signal_channel = mw_signal_channel();
  klass->signal_got_status = mw_signal_got_status();
  klass->signal_got_privacy = mw_signal_got_privacy();
  klass->signal_got_admin = mw_signal_got_admin();
  klass->signal_got_announce = mw_signal_got_announce();
  klass->signal_got_sense_service = mw_signal_got_sense_service();
  klass->signal_got_one_time = mw_signal_got_one_time();

  klass->handle_pending = MwSession_handlePending;
  klass->handle_channel = MwSession_handleChannel;
  
  klass->start = mw_start;
  klass->stop = mw_stop;
  klass->feed = mw_feed;
  klass->pending = mw_pending;
  klass->flush = mw_flush;
  klass->send_message = mw_send_message;
  klass->send_keepalive = mw_send_keepalive;
  klass->send_announce = mw_send_announce;
  klass->force_login = mw_force_login;
  klass->sense_service = mw_sense_service;
  klass->send_one_time = mw_send_one_time;

  klass->new_channel = mw_new_channel;
  klass->get_channel = mw_get_channel;
  klass->foreach_channel = mw_foreach_channel;
  
  klass->add_cipher = mw_add_cipher;
  klass->get_cipher = mw_get_cipher;
  klass->get_cipher_by_pol = mw_get_cipher_by_pol;
  klass->foreach_cipher = mw_foreach_cipher;
  klass->remove_cipher = mw_remove_cipher;
}


static const GTypeInfo info = {
  .class_size = sizeof(MwSessionClass),
  .base_init = NULL,
  .base_finalize = NULL,
  .class_init = mw_session_class_init,
  .class_finalize = NULL,
  .class_data = NULL,
  .instance_size = sizeof(MwSession),
  .n_preallocs = 0,
  .instance_init = NULL,
  .value_table = NULL,
};


GType MwSession_getType() {
  static GType type = 0;

  if(type == 0) {
    type = g_type_register_static(MW_TYPE_OBJECT, "MwSessionType",
				  &info, 0);
  }

  return type;
}


MwSession *MwSession_new() {
  return g_object_new(MwSession_getType(), NULL);
}


void MwSession_start(MwSession *session) {
  void (*fn)(MwSession *);
  
  g_return_if_fail(session != NULL);

  fn = MW_SESSION_GET_CLASS(session)->start;
  fn(session);
}


void MwSession_stop(MwSession *session, guint32 info) {
  void (*fn)(MwSession *, guint32);

  g_return_if_fail(session != NULL);

  fn = MW_SESSION_GET_CLASS(session)->stop;
  fn(session, info);
}


void MwSession_feed(MwSession *session, const guchar *buf, gsize len) {
  void (*fn)(MwSession *, const guchar *, gsize);

  g_return_if_fail(session != NULL);
  if(! buf || ! len) return;

  fn = MW_SESSION_GET_CLASS(session)->feed;
  fn(session, buf, len);
}


guint MwSession_pending(MwSession *session) {
  guint (*fn)(MwSession *);

  g_return_val_if_fail(session != NULL, 0);

  fn = MW_SESSION_GET_CLASS(session)->pending;
  return fn(session);
}


void MwSession_flush(MwSession *session) {
  void (*fn)(MwSession *);

  g_return_if_fail(session != NULL);

  fn = MW_SESSION_GET_CLASS(session)->flush;
  fn(session);
}


void MwSession_sendMessage(MwSession *session, const MwMessage *msg) {
  void (*fn)(MwSession *, const MwMessage *);

  g_return_if_fail(session != NULL);
  g_return_if_fail(msg != NULL);

  fn = MW_SESSION_GET_CLASS(session)->send_message;
  fn(session, msg);
}


void MwSession_sendKeepalive(MwSession *session) {
  void (*fn)(MwSession *);

  g_return_if_fail(session != NULL);

  fn = MW_SESSION_GET_CLASS(session)->send_keepalive;
  fn(session);
}


void MwSession_sendAnnounce(MwSession *session, gboolean may_reply,
			    const gchar **rcpt, const gchar *text) {

  void (*fn)(MwSession *, gboolean, const gchar **, const gchar *);

  g_return_if_fail(session != NULL);

  fn = MW_SESSION_GET_CLASS(session)->send_announce;
  fn(session, may_reply, rcpt, text);
}


void MwSession_forceLogin(MwSession *session) {
  void (*fn)(MwSession *);

  g_return_if_fail(session != NULL);

  fn = MW_SESSION_GET_CLASS(session)->force_login;
  fn(session);
}


void MwSession_senseService(MwSession *session, guint32 id) {
  void (*fn)(MwSession *, guint32);

  g_return_if_fail(session != NULL);

  fn = MW_SESSION_GET_CLASS(session)->sense_service;
  fn(session, id);
}


guint MwSession_sendOneTime(MwSession *session,
			    const MwIdentity *target,
			    guint32 service,
			    guint32 proto_type, guint32 proto_ver,
			    guint16 type, const MwOpaque *data) {
  
  guint (*fn)(MwSession *, const MwIdentity *,
	      guint32, guint32, guint32, guint16, const MwOpaque *);
  
  g_return_val_if_fail(session != NULL, 0x00);

  fn = MW_SESSION_GET_CLASS(session)->send_one_time;
  return fn(session, target, service, proto_type, proto_ver, type, data);
}


MwChannel *MwSession_newChannel(MwSession *session) {
  MwChannel *(*fn)(MwSession *);

  g_return_val_if_fail(session != NULL, NULL);

  fn = MW_SESSION_GET_CLASS(session)->new_channel;
  return fn(session);
}


MwChannel *MwSession_getChannel(MwSession *session, guint32 id) {
  MwChannel *(*fn)(MwSession *, guint32);

  g_return_val_if_fail(session != NULL, NULL);

  fn = MW_SESSION_GET_CLASS(session)->get_channel;
  return fn(session, id);
}


void MwSession_foreachChannel(MwSession *session,
			      GFunc func, gpointer data) {

  void (*fn)(MwSession *, GFunc, gpointer);

  g_return_if_fail(session != NULL);

  fn = MW_SESSION_GET_CLASS(session)->foreach_channel;
  fn(session, func, data);
}


void MwSession_foreachChannelClosure(MwSession *session,
				     GClosure *closure) {

  MwSession_foreachChannel(session, mw_closure_gfunc_obj, closure);
}


GList *MwSession_getChannels(MwSession *session) {
  GList *list = NULL;
  MwSession_foreachChannel(session, mw_collect_gfunc, &list);
  return list;
}


gboolean MwSession_addCipher(MwSession *session, MwCipherClass *klass) {
  gboolean (*fn)(MwSession *, MwCipherClass *);

  g_return_val_if_fail(session != NULL, FALSE);
  g_return_val_if_fail(klass != NULL, FALSE);

  fn = MW_SESSION_GET_CLASS(session)->add_cipher;
  return fn(session, klass);
}


MwCipherClass *MwSession_getCipher(MwSession *session, guint16 id) {
  MwCipherClass *(*fn)(MwSession *, guint16);

  g_return_val_if_fail(session != NULL, NULL);

  fn = MW_SESSION_GET_CLASS(session)->get_cipher;
  return fn(session, id);
}


MwCipherClass *MwSession_getCipherByPolicy(MwSession *session,
					   guint16 policy) {

  MwCipherClass *(*fn)(MwSession *, guint16);

  g_return_val_if_fail(session != NULL, NULL);

  fn = MW_SESSION_GET_CLASS(session)->get_cipher_by_pol;
  return fn(session, policy);
}


void MwSession_foreachCipher(MwSession *session,
			     GFunc func, gpointer data) {
  
  void (*fn)(MwSession *, GFunc, gpointer);
  
  g_return_if_fail(session != NULL);
  
  fn = MW_SESSION_GET_CLASS(session)->foreach_cipher;
  fn(session, func, data);
}


void MwSession_foreachCipherClosure(MwSession *session,
				    GClosure *closure) {

  MwSession_foreachCipher(session, mw_closure_gfunc_obj, closure);
}


static gint mw_cipher_comp(MwCipherClass *a, MwCipherClass *b) {
  gint a_id, b_id;
  a_id = (gint) MwCipherClass_getIdentifier(a);
  b_id = (gint) MwCipherClass_getIdentifier(b);
  return a_id - b_id;
}


GList *MwSession_getCiphers(MwSession *session) {
  GList *list = NULL;
  MwSession_foreachCipher(session, mw_collect_gfunc, &list);
  return g_list_sort(list, (GCompareFunc) mw_cipher_comp);;
}


void MwSession_removeCipher(MwSession *session, MwCipherClass *klass) {
  void (*fn)(MwSession *, MwCipherClass *);

  g_return_if_fail(session != NULL);
  g_return_if_fail(klass != NULL);

  fn = MW_SESSION_GET_CLASS(session)->remove_cipher;
  fn(session, klass);
}


const MwLogin *MwSession_getLogin(MwSession *session) {
  MwSessionPrivate *priv;

  g_return_val_if_fail(session != NULL, NULL);

  priv = MW_SESSION_GET_PRIVATE(session);
  return &priv->login;
}


void MwSession_setLogin(MwSession *session, const MwLogin *login) {
  MwSessionPrivate *priv;
  MwLogin interim;

  g_return_if_fail(session != NULL);

  priv = MW_SESSION_GET_PRIVATE(session);

  MwLogin_clone(&interim, login, TRUE);
  MwLogin_clear(&priv->login, TRUE);
  MwLogin_clone(&priv->login, &interim, FALSE);
}


const MwPrivacy *MwSession_getPrivacy(MwSession *session) {
  MwSessionPrivate *priv;

  g_return_val_if_fail(session != NULL, NULL);

  priv = MW_SESSION_GET_PRIVATE(session);
  return &priv->privacy;
}


void MwSession_setPrivacy(MwSession *session, const MwPrivacy *privacy) {
  MwMsgPrivacy *msg;

  g_return_if_fail(session != NULL);

  msg = MwMessage_new(mw_message_PRIVACY);
  MwPrivacy_clone(&msg->privacy, privacy, FALSE);
  MwSession_sendMessage(session, MW_MESSAGE(msg));
  MwPrivacy_clear(&msg->privacy, FALSE);
  MwMessage_free(MW_MESSAGE(msg));
}


void MwSession_setStatus(MwSession *session, const MwStatus *status) {
  MwMsgStatus *msg;

  g_return_if_fail(session != NULL);

  msg = MwMessage_new(mw_message_STATUS);
  MwStatus_clone(&msg->status, status, FALSE);
  MwSession_sendMessage(session, MW_MESSAGE(msg));
  MwStatus_clear(&msg->status, FALSE);
  MwMessage_free(MW_MESSAGE(msg));
}


gboolean MwSession_handlePending(MwSession *session) {
  mw_debug_enter();
  MwSession_flush(session);
  mw_debug_return_val(FALSE);
}


gboolean MwSession_handleChannel(MwSession *session, MwChannel *chan) {
  mw_debug_enter();
  MwChannel_close(chan, ERR_SERVICE_NO_SUPPORT, NULL);
  mw_debug_return_val(FALSE);
}


#define enum_val(val, alias) { val, #val, alias }


static const GEnumValue values[] = {
  enum_val(mw_session_stopped, "stopped"),
  enum_val(mw_session_starting, "starting"),
  enum_val(mw_session_handshake, "handshake"),
  enum_val(mw_session_handshake_ack, "handshake_ack"),
  enum_val(mw_session_login, "login"),
  enum_val(mw_session_login_redirect, "login_redirect"),
  enum_val(mw_session_login_force, "login_force"),
  enum_val(mw_session_login_ack, "login_ack"),
  enum_val(mw_session_started, "started"),
  enum_val(mw_session_stopping, "stopping"),
  enum_val(mw_session_error, "error"),
  { 0, NULL, NULL },
};


GType MwSessionStateEnum_getType() {
  static GType type = 0;
   
  if(type == 0) {
    type = g_enum_register_static("MwSessionStateEnumType", values);
  }

  return type;
}


/* The end. */
