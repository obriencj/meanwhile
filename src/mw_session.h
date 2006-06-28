#ifndef _MW_SESSION_H
#define _MW_SESSION_H


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


/**
   @file mw_session.h

   A client session with a Sametime server is encapsulated in the
   MwSession class. The session controls channels and provides
   encryption ciphers.

   A MwSession instance does not directly communicate with a socket or
   stream. Instead, client code is required to connect to the "write"
   signal in order to catch outgoing data.

   In order to pass incoming data to a MwSession instance for
   processing, client code must call the MwSession_feed method. The
   session will buffer and parse fed data, performing activities based
   on the messages as they arrive.
*/


#include <glib.h>
#include "mw_channel.h"
#include "mw_common.h"
#include "mw_encrypt.h"
#include "mw_message.h"
#include "mw_object.h"
#include "mw_typedef.h"


G_BEGIN_DECLS


/** default protocol major version */
#define MW_PROTOCOL_VERSION_MAJOR  0x001e


/** default protocol minor version */
#define MW_PROTOCOL_VERSION_MINOR  0x001d


enum mw_auth_type {
  mw_auth_type_PLAIN   = 0x0000,
  mw_auth_type_TOKEN   = 0x0001,
  mw_auth_type_RC2     = 0x0002,
  mw_auth_type_DH_RC2  = 0x0004,
};


#define MW_TYPE_SESSION  (MwSession_getType())


#define MW_SESSION(obj)							\
  (G_TYPE_CHECK_INSTANCE_CAST((obj), MW_TYPE_SESSION, MwSession))


#define MW_SESSION_CLASS(klass)						\
  (G_TYPE_CHECK_CLASS_CAST((klass), MW_TYPE_SESSION, MwSessionClass))


#define MW_IS_SESSION(obj)				\
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), MW_TYPE_SESSION))


#define MW_IS_SESSION_CLASS(klass)			\
  (G_TYPE_CHECK_CLASS_TYPE((klass), MW_TYPE_SESSION))


#define MW_SESSION_GET_CLASS(obj)					\
  (G_TYPE_INSTANCE_GET_CLASS((obj), MW_TYPE_SESSION, MwSessionClass))


struct mw_session {
  MwObject mwobject;
};


typedef struct mw_session_class MwSessionClass;


struct mw_session_class {
  MwObjectClass mwobjectclass;

  guint signal_pending;            /**< session has data to be written */
  guint signal_write;              /**< must be handled to write data */
  guint signal_channel;            /**< new incoming channel */
  guint signal_got_otm;            /**< incoming one time message */
  guint signal_got_status;         /**< user status changed */
  guint signal_got_privacy;        /**< user privacy changed */
  guint signal_got_admin;          /**< incoming admin message */
  guint signal_got_announce;       /**< incoming announcement */
  guint signal_got_sense_service;  /**< service is available */
  guint signal_got_one_time;       /**< incoming one time message */

  /** default signal handler for "pending" */
  gboolean (*handle_pending)(MwSession *self);

  /** default signal handler for "channel-incoming" */
  gboolean (*handle_channel)(MwSession *self, MwChannel *channel);

  /* methods */
  void (*start)(MwSession *self);
  void (*stop)(MwSession *self, guint32 reason);
  void (*feed)(MwSession *self, const guchar *buf, gsize len);
  guint (*pending)(MwSession *self);
  void (*flush)(MwSession *self);
  void (*send_message)(MwSession *self, const MwMessage *msg);
  void (*send_keepalive)(MwSession *self);
  void (*send_announce)(MwSession *self, gboolean may_reply,
			const gchar **recipients, const gchar *text);
  void (*force_login)(MwSession *self);
  void (*sense_service)(MwSession *self, guint32 id);

  guint (*send_one_time)(MwSession *self, const MwIdentity *target,
			 guint32 srvc, guint32 proto_type, guint32 proto_ver,
			 guint16 type, const MwOpaque *message);

  MwChannel *(*new_channel)(MwSession *self);
  MwChannel *(*get_channel)(MwSession *self, guint32 id);
  void (*foreach_channel)(MwSession *self, GFunc func, gpointer data);
  
  gboolean (*add_cipher)(MwSession *self, MwCipherClass *klass);
  MwCipherClass *(*get_cipher)(MwSession *self, guint16 id);
  MwCipherClass *(*get_cipher_by_pol)(MwSession *self, guint16 pol);
  void (*foreach_cipher)(MwSession *self, GFunc func, gpointer data);
  void (*remove_cipher)(MwSession *self, MwCipherClass *klass);
};


GType MwSession_getType();


MwSession *MwSession_new();


void MwSession_start(MwSession *session);


void MwSession_stop(MwSession *session, guint32 reason);


/** Feed data to a session's internal message parser */
void MwSession_feed(MwSession *session, const guchar *buf, gsize len);


/** Feed data to a session's internal message parser */
void MwSession_feedOpaque(MwSession *session, const MwOpaque *data);


/**
   Count of pending outgoing messages. Pending messages can be written
   to the write signal handler via a call to MwSession_flush, one at a
   time.
*/
guint MwSession_pending(MwSession *session);


/**
   Cause the write signal to be emitted upon the session, passing the
   first pending outgoing message.

   Any code that connects to the session's write-ready signal and
   returns TRUE should ensure that this method is eventually
   called. The default behavior for a session instance will cause this
   method to be called automatically if no write-ready signal handler
   returns TRUE.
*/
void MwSession_flush(MwSession *session);


void MwSession_sendMessage(MwSession *self, const MwMessage *msg);


/** Cause the write signal to be emitted with a keepalive byte */
void MwSession_sendKeepalive(MwSession *self);


/** Send an announcement message to the specified recipients.  The
    recipient list is an array of strings in the following format

    @li "\@U userid" to indicate a user on your community
    @li "\@L loginid" to indicate a login on your community
    @li "\@G groupid" to indicate all users in a Notes Address Book group
    @li "\@E externaluserid" to indicate a user via a SIP gateway
*/
void MwSession_sendAnnounce(MwSession *session, gboolean may_reply,
			    const gchar **recipients, const gchar *text);


/** Force a session to continue the authentication process against the
    current server connection. This is only valid when the session
    state is mw_session_LOGIN_REDIR. */
void MwSession_forceLogin(MwSession *session);


void MwSession_senseService(MwSession *session, guint32 type);


/** Sends a One Time Message (OTM) to the target */
guint MwSession_sendOneTime(MwSession *session,
			    const MwIdentity *target,
			    guint32 service,
			    guint32 proto_type, guint32 proto_ver,
			    guint16 type, const MwOpaque *data);


/** allocate a new outgoing channel */
MwChannel *MwSession_newChannel(MwSession *session);


/** find a channel by its ID */
MwChannel *MwSession_getChannel(MwSession *session, guint32 id);


void MwSession_foreachChannel(MwSession *session,
			      GFunc func, gpointer data);


void MwSession_foreachChannelClosure(MwSession *session,
				     GClosure *closure);


/** Uses MwSession_foreachChannel to construct a GList of MwChannel
    instanciated by this session. Remember to g_list_free when you're
    done. */
GList *MwSession_getChannels(MwSession *session);


gboolean MwSession_addCipher(MwSession *session, MwCipherClass *klass);


void MwSession_removeCipher(MwSession *session, MwCipherClass *klass);


MwCipherClass *MwSession_getCipher(MwSession *session, guint16 id);


MwCipherClass *MwSession_getCipherByPolicy(MwSession *session,
					   guint16 policy);


void MwSession_foreachCipher(MwSession *session,
			     GFunc func, gpointer data);


void MwSession_foreachCipherClosure(MwSession *session,
				    GClosure *closure);


/** Uses MwSession_foreachCipher to construct a GList of
    MwCipherClass, sorted by their identifiers. Remember to call
    g_list_free when you're done. */
GList *MwSession_getCiphers(MwSession *session);


const MwLogin *MwSession_getLogin(MwSession *session);


void MwSession_setLogin(MwSession *session, const MwLogin *login);


const MwPrivacy *MwSession_getPrivacy(MwSession *session);


void MwSession_setPrivacy(MwSession *session, const MwPrivacy *privacy);


const MwStatus *MwSession_getStatus(MwSession *session);


void MwSession_setStatus(MwSession *session, const MwStatus *status);


/**
   Default handler for the MW_SESSION_PENDING signal
 */
gboolean MwSession_handlePending(MwSession *session);


/**
   Default handler for the MW_SESSION_CHANNEL signal
*/
gboolean MwSession_handleChannel(MwSession *session, MwChannel *chan);


#define MW_TYPE_SESSION_STATE_ENUM  (MwSessionStateEnum_getType())


enum mw_session_state {
  mw_session_stopped   = mw_object_stopped,
  mw_session_starting  = mw_object_starting,
  mw_session_started   = mw_object_started,
  mw_session_stopping  = mw_object_stopping,
  mw_session_error     = mw_object_error,
  mw_session_handshake,      /**< session has sent handshake */
  mw_session_handshake_ack,  /**< session has received handshake ack */
  mw_session_login,          /**< session has sent login */
  mw_session_login_redirect, /**< session has been redirected */
  mw_session_login_force,    /**< session has sent a login continue */
  mw_session_login_ack,      /**< session has received login ack */
};


GType MwSessionStateEnum_getType();


G_END_DECLS


#endif /* _MW_SESSION_H */

