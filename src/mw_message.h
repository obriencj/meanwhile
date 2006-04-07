#ifndef _MW_MESSAGE_H
#define _MW_MESSAGE_H


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

*/


#include <glib.h>
#include "mw_common.h"


G_BEGIN_DECLS


/** Cast a pointer to a message subtype (eg, MwMsgHandshake,
    MwMsgAdmin) into a pointer to a MwMessage */
#define MW_MESSAGE(msg) (&(msg)->head)


/** Indicates the type of a message. */
enum mw_message_type {
  mw_message_HANDSHAKE        = 0x0000,  /**< MwMsgHandshake */
  mw_message_HANDSHAKE_ACK    = 0x8000,  /**< MwMsgHandshakeAck */
  mw_message_LOGIN            = 0x0001,  /**< MwMsgLogin */
  mw_message_LOGIN_REDIRECT   = 0x0018,  /**< MwMsgLoginRedirect */
  mw_message_LOGIN_FORCE      = 0x0016,  /**< MwMsgLoginForce */
  mw_message_LOGIN_ACK        = 0x8001,  /**< MwMsgLoginAck */

  mw_message_CHANNEL_CREATE   = 0x0002,  /**< MwMsgChannelCreate */
  mw_message_CHANNEL_CLOSE    = 0x0003,  /**< MwMsgChannelClose */
  mw_message_CHANNEL_SEND     = 0x0004,  /**< MwMsgChannelSend */
  mw_message_CHANNEL_ACCEPT   = 0x0006,  /**< MwMsgChannelAccept */

  mw_message_ONE_TIME         = 0x0007,  /**< MwMsgOneTime */

  mw_message_STATUS           = 0x0009,  /**< MwMsgStatus */
  mw_message_PRIVACY          = 0x000b,  /**< MwMsgPrivacy */

  mw_message_SENSE_SERVICE    = 0x0011,  /**< MwMsgSenseService */
  mw_message_ADMIN            = 0x0019,  /**< MwMsgAdmin */
  mw_message_ANNOUNCE         = 0x0022,  /**< MwMsgAnnounce */
};


enum mw_message_ption {
  mw_message_option_ENCRYPT  = 0x4000,  /**< message data is encrypted */
  mw_message_option_ATTRIBS  = 0x8000,  /**< message has attributes */
};


/** @see mw_message_option */
#define MW_MESSAGE_HAS_OPTION(msg, opt) \
  ((msg)->options & (opt))


typedef struct mw_message MwMessage;


struct mw_message {
  guint16 type;      /**< @see mw_message_type */
  guint16 options;   /**< @see mw_message_option */
  guint32 channel;   /**< ID of channel message is intended for */
  MwOpaque attribs;  /**< optional message attributes */
};


/** Allocate and initialize a new MwMessage of the specified type */
gpointer MwMessage_new(enum mw_message_type type);


/** build a message from its representation */
gpointer MwMessage_get(MwGetBuffer *b);


/** write a message to a buffer */
void MwMessage_put(MwPutBuffer *b, const MwMessage *msg);


/** destroy a message */
void MwMessage_free(MwMessage *msg);


typedef struct mw_msg_handshake MwMsgHandshake;


struct mw_msg_handshake {
  MwMessage head;
  guint16 major;          /**< client's major version number */
  guint16 minor;          /**< client's minor version number */
  guint32 srvrcalc_addr;  /**< 0.0.0.0 */
  guint16 client_type;    /**< @see mw_client_type */
  guint32 loclcalc_addr;  /**< local public IP */
  guint16 unknown_a;      /**< normally 0x0100 */
  guint32 unknown_b;      /**< normally 0x00000000 */
  gchar *local_host;      /**< name of client host */
};


typedef struct mw_msg_handshake_ack MwMsgHandshakeAck;


struct mw_msg_handshake_ack {
  MwMessage head;
  guint16 major;          /**< server's major version number */
  guint16 minor;          /**< server's minor version number */
  guint32 srvrcalc_addr;  /**< server-calculated address */
  guint32 magic;          /**< four bytes of something */
  MwOpaque data;          /**< server's DH public key for auth */
};


typedef struct mw_msg_login MwMsgLogin;


struct mw_msg_login  {
  MwMessage head;
  guint16 client_type;  /**< @see mw_client_type */
  gchar *name;          /**< user identification */
  guint16 auth_type;    /**< @see mw_auth_type */
  MwOpaque auth_data;   /**< authentication data */
};


typedef struct mw_msg_login_ack MwMsgLoginAck;


struct mw_msg_login_ack {
  MwMessage head;
  MwLogin login;
  MwPrivacy privacy;
  MwStatus status;
};


typedef struct mw_msg_login_redirect MwMsgLoginRedirect;


struct mw_msg_login_redirect {
  MwMessage head;
  gchar *host;
  gchar *server_id;
};


typedef struct mw_msg_login_force MwMsgLoginForce;


struct mw_msg_login_force {
  MwMessage head;
};


typedef struct mw_enc_item MwEncItem;


struct mw_enc_item {
  guint16 cipher;
  MwOpaque info;
};


typedef struct mw_msg_channel_create MwMsgChannelCreate;


struct mw_msg_channel_create {
  MwMessage head;

  guint32 reserved;       /**< unknown reserved data */
  guint32 channel;        /**< intended ID for new channel */
  MwIdentity target;      /**< User ID. for service use */
  guint32 service;        /**< ID for the target service */
  guint32 proto_type;     /**< protocol type for the service */
  guint32 proto_ver;      /**< protocol version for the service */
  guint32 options;        /**< options */
  MwOpaque addtl;         /**< service-specific additional data */
  gboolean creator_flag;  /**< indicate presence of creator information */
  MwLogin creator;

  guint16 enc_mode;      /**< offered encryption mode */
  guint32 enc_count;     /**< count of offered items */
  MwEncItem *enc_items;  /**< offered encryption items */
  guint16 enc_extra;     /**< encryption mode again? */
  gboolean enc_flag;     /**< unknown flag */
};


typedef struct mw_msg_channel_accept MwMsgChannelAccept;


struct mw_msg_channel_accept {
  MwMessage head;
  guint32 service;         /**< ID for the channel's service */
  guint32 proto_type;      /**< protocol type for the service */
  guint32 proto_ver;       /**< protocol version for the service */
  MwOpaque addtl;          /**< service-specific additional data */
  gboolean acceptor_flag;  /**< indicate presence of acceptor information */
  MwLogin acceptor;

  guint16 enc_mode;    /**< accepted encryption mode */
  MwEncItem enc_item;  /**< accepted encryption item (if enc_mode != 0) */
  guint16 enc_extra;   /**< originally offered encryption mode */
  gboolean enc_flag;   /**< unknown flag */
};


typedef struct mw_msg_channel_send MwMsgChannelSend;


struct mw_msg_channel_send {
  MwMessage head;

  /** message type. each service defines its own send types. Type IDs
      are only necessarily unique within a given service. */
  guint16 type;

  /** protocol data to be interpreted by the handling service */
  MwOpaque data;
};


typedef struct mw_msg_channel_close MwMsgChannelClose;


struct mw_msg_channel_close {
  MwMessage head;
  guint32 reason;  /**< reason for closing the channel. */
  MwOpaque data;   /**< additional information */
};


typedef struct mw_msg_one_time MwMsgOneTime;


struct mw_msg_one_time {
  MwMessage head;

  guint32 id;
  MwIdentity target;
  guint32 service;
  guint32 proto_type;
  guint32 proto_ver;

  guint16 type;
  MwOpaque data;
};


typedef struct mw_msg_status MwMsgStatus;


struct mw_msg_status {
  MwMessage head;
  MwStatus status;
};


typedef struct mw_msg_privacy MwMsgPrivacy;


struct mw_msg_privacy {
  MwMessage head;
  MwPrivacy privacy;
};


typedef struct mw_msg_sense_service MwMsgSenseService;


/** Sent to the server to request the presense of a service by its
    ID. Sent to the client to indicate the presense of such a
    service */
struct mw_msg_sense_service {
  MwMessage head;
  guint32 service;
};


typedef struct mw_msg_admin MwMsgAdmin;


/** An administrative broadcast message */
struct mw_msg_admin {
  MwMessage head;
  gchar *text;
};


typedef struct mw_msg_announce MwMsgAnnounce;


/** An announcement between users */
struct mw_msg_announce {
  MwMessage head;

  gboolean sender_flag;  /**< indicates presence of sender data */
  MwLogin sender;        /**< who sent the announcement */

  guint16 unknown_a;   /**< unknown A. Usually 0x00 */
  gboolean may_reply;  /**< replies allowed */
  gchar *text;         /**< text of message */

  guint32 rcpt_count;  /**< count of recipients */

  /** list of (char *) indicating recipients. Recipient users are in
      the format "@U username" and recipient NAB groups are in the
      format "@G groupname" */
  gchar **recipients;
};


G_END_DECLS


#endif /* _MW_MESSAGE_H */
