#ifndef _MW_CHANNEL_H
#define _MW_CHANNEL_H


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


/** @file mw_channel.h
    
*/


#include "mw_common.h"
#include "mw_message.h"
#include "mw_object.h"
#include "mw_session.h"
#include "mw_typedef.h"


G_BEGIN_DECLS


#define MW_TYPE_CHANNEL  (MwChannel_getType())


#define MW_CHANNEL(obj)							\
  (G_TYPE_CHECK_INSTANCE_CAST((obj), MW_TYPE_CHANNEL, MwChannel))


#define MW_CHANNEL_CLASS(klass)						\
  (G_TYPE_CHECK_CLASS_CAST((klass), MW_TYPE_CHANNEL, MwChannelClass))


#define MW_IS_CHANNEL(obj)				\
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), MW_TYPE_CHANNEL))


#define MW_IS_CHANNEL_CLASS(klass)			\
  (G_TYPE_CHECK_CLASS_TYPE((klass), MW_TYPE_CHANNEL))


#define MW_CHANNEL_GET_CLASS(obj)					\
  (G_TYPE_INSTANCE_GET_CLASS((obj), MW_TYPE_CHANNEL, MwChannelClass))


typedef struct mw_channel_private MwChannelPrivate;


struct mw_channel_private;


struct mw_channel {
  MwObject mwobject;
  MwSession *session;  /**< weak reference to owning session */

  MwChannelPrivate *private;
};


typedef struct mw_channel_class MwChannelClass;


struct mw_channel_class {
  MwObjectClass mwobject_class;

  guint signal_state_changed;
  guint signal_outgoing;  /**< passes a message to be written */
  guint signal_incoming;  /**< passes a message to be handled */

  void (*open)(MwChannel *self, const MwOpaque *info);
  void (*close)(MwChannel *self, guint32 reason, const MwOpaque *info);

  void (*feed)(MwChannel *self, MwMessage *msg);
  void (*send)(MwChannel *self, guint16 type,
	       const MwOpaque *msg, gboolean enc);
};


GType MwChannel_getType();


/** channel status */
enum mw_channel_state {
  mw_channel_CLOSED,   /**< channel is closed */
  mw_channel_PENDING,  /**< channel is waiting for accept */
  mw_channel_OPEN,     /**< channel is accepted and open */
  mw_channel_ERROR,    /**< channel is closing due to error */
  mw_channel_UNKNOWN,  /**< error determining channel state */
};


/** Encryption policy for a channel, dictating what sort of encryption
    should be used, if any, and when.
*/
enum mw_channel_encrypt {
  mw_channel_encrypt_NONE      = 0x0000,  /**< encrypt none */
  mw_channel_encrypt_WHATEVER  = 0x0001,  /**< encrypt whatever, any cipher */
  mw_channel_encrypt_ANY       = 0x0002,  /**< encrypt all, any cipher */
};


/** Formally open a channel. If the channel is outgoing, this will
    cause a MwMsgChannelCreate message to be composed and sent. If the
    channel is incoming, it will be a MwMsgChannelAccept message
    instead.
*/
void MwChannel_open(MwChannel *channel, const MwOpaque *info);


/**
   Utility function to make checking a channel's state easier. Will use
   g_object_get on the channel to discover the state property, and check
   that it's equal to the specified state
*/
gboolean MwChannel_isState(MwChannel *channel, enum mw_channel_state state);


#define MW_CHANNEL_IS_OPEN(chan)		\
  (MwChannel_isState((chan), mw_channel_OPEN))


/** Close a channel. Sends a MwMsgChannelClose message to the
    server. Decrements the reference count for the channel. If the
    channel is not referenced elsewhere, this may cause the channel to
    be disposed.

    @param chan    the channel to close
    @param reason  the reason code for closing the channel
    @param data    optional additional information 
*/
void MwChannel_close(MwChannel *channel,
		     guint32 reason, const MwOpaque *data);


/** Feed a message to a channel for handling.  MwSession will
    automatically pass messages of the types MwMsgChannelCreate,
    MwMsgChannelAccept, MwMsgChannelClose, and MwMsgChannelSend to
    this method as they arrive.
*/
void MwChannel_feed(MwChannel *chan, MwMessage *message);


/** Compose a send-on-channel message, encrypt it as per the channel's
    specification, and send it */
void MwChannel_send(MwChannel *chan,
		    guint16 msg_type, const MwOpaque *msg,
		    gboolean encrypt);


G_END_DECLS


#endif /* _MW_CHANNEL_H */
