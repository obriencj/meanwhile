
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

#ifndef _MW_SRVC_IM_H
#define _MW_SRVC_IM_H


#include <glib.h>
#include "mw_common.h"


/** @file srvc_im.h
 */


/** Type identifier for the IM service */
#define SERVICE_IM  0x00001000


/** @struct mwServiceIm

    An instance of the IM service. This service provides simple
    instant messaging functionality */
struct mwServiceIm;


/** @struct mwConversation

    A conversation between the local service and a single other user */
struct mwConversation;


enum mwImClientType {
  mwImClient_PLAIN       = 0x00000001,  /**< text, typing */
  mwImClient_NOTESBUDDY  = 0x00033453,  /**< adds html, subject, mime */
  mwImClient_UNKNOWN     = 0xffffffff,  /**< trouble determining type */
};


/**
   Types of supported messages. When a conversation is created, the
   least common denominator of features between either side of the
   conversation (based on what features are available in the IM
   service itself) becomes the set of supported features for that
   conversation. At any point, the feature set for the service may
   change, without affecting any existing conversations.

   @relates mwServiceIm_supports
   @relates mwServiceIm_setSupported
   @relates mwConversation_supports
   @relates mwConversation_send
   @relates mwServiceImHandler::conversation_recv
 */
enum mwImSendType {
  mwImSend_PLAIN,   /**< char *, plain-text message */
  mwImSend_TYPING,  /**< gboolean, typing status */
  mwImSend_HTML,    /**< char *, HTML formatted message (NOTESBUDDY) */
  mwImSend_SUBJECT, /**< char *, conversation subject (NOTESBUDDY) */
  mwImSend_MIME,    /**< mwOpaque *, MIME-encoded message (NOTESBUDDY) */
};



/** @relates mwConversation_getState
    @relates MW_CONVO_IS_CLOSED
    @relates MW_CONVO_IS_PENDING
    @relates MW_CONVO_IS_OPEN */
enum mwConversationState {
  mwConversation_CLOSED,   /**< conversation is not open */
  mwConversation_PENDING,  /**< conversation is opening */
  mwConversation_OPEN,     /**< conversation is open */
  mwConversation_UNKNOWN,  /**< unknown state */
};


#define MW_CONVO_IS_STATE(conv, state) \
  (mwConversation_getState(conv) == (state))

#define MW_CONVO_IS_CLOSED(conv) \
  MW_CONVO_IS_STATE((conv), mwConversation_CLOSED)

#define MW_CONVO_IS_PENDING(conv) \
  MW_CONVO_IS_STATE((conv), mwConversation_PENDING)

#define MW_CONVO_IS_OPEN(conv) \
  MW_CONVO_IS_STATE((conv), mwConversation_OPEN)



/** IM Service Handler. Provides functions for events triggered from an
    IM service instance. */
struct mwImHandler {

  /** A conversation has been successfully opened */
  void (*conversation_opened)(struct mwConversation *conv);

  /** A conversation has been closed */
  void (*conversation_closed)(struct mwConversation *conv, guint32 err);
  
  /** A message has been received on a conversation */
  void (*conversation_recv)(struct mwConversation *conv,
			    enum mwImSendType type, gconstpointer msg);

  /** optional. called from mwService_free */
  void (*clear)(struct mwServiceIm *srvc);
};


struct mwServiceIm *mwServiceIm_new(struct mwSession *session,
				    struct mwImHandler *handler);


struct mwImHandler *mwServiceIm_getHandler(struct mwServiceIm *srvc);


/** reference an existing conversation to target, or create a new
    conversation to target if one does not already exist */
struct mwConversation *mwServiceIm_getConversation(struct mwServiceIm *srvc,
						   struct mwIdBlock *target);


/** reference an existing conversation to target */
struct mwConversation *mwServiceIm_findConversation(struct mwServiceIm *srvc,
						    struct mwIdBlock *target);


/** determine if the conversations created from this service will
    support a given send type */
gboolean mwServiceIm_supports(struct mwServiceIm *srvc,
			      enum mwImSendType type);


/** Set the default client type for the service. Newly created
    conversations will attempt to meet this level of functionality
    first.

    @param srvc       the IM service
    @param type       the send type to enable/disable
*/
void mwServiceIm_setClientType(struct mwServiceIm *srvc,
			       enum mwImClientType type);


enum mwImClientType mwServiceIm_getClientType(struct mwServiceIm *srvc);


/** attempt to open a conversation. If the conversation was not
    already open and it is accepted,
    mwServiceImHandler::conversation_opened will be triggered. Upon
    failure, mwServiceImHandler::conversation_closed will be
    triggered */
void mwConversation_open(struct mwConversation *conv);


/** close a conversation. If the conversation was not already closed,
    mwServiceImHandler::conversation_closed will be triggered */
void mwConversation_close(struct mwConversation *conv, guint32 err);


/** determine whether a conversation supports the given message type */
gboolean mwConversation_supports(struct mwConversation *conv,
				 enum mwImSendType type);


enum mwImClientType mwConversation_getClientType(struct mwConversation *conv);


/** get the state of a conversation

    @relates MW_CONVO_IS_OPEN
    @relates MW_CONVO_IS_CLOSED
    @relates MW_CONVO_IS_PENDING
*/
enum mwConversationState mwConversation_getState(struct mwConversation *conv);


/** send a message over an open conversation */
int mwConversation_send(struct mwConversation *conv,
			enum mwImSendType type, gconstpointer send);


/** @returns owning service for a conversation */
struct mwServiceIm *mwConversation_getService(struct mwConversation *conv);


/** login information for conversation partner. returns NULL if conversation 
    is not OPEN */
struct mwLoginInfo *mwConversation_getTargetInfo(struct mwConversation *conv);


/** ID for conversation partner */
struct mwIdBlock *mwConversation_getTarget(struct mwConversation *conv);


/** set whether outgoing messages should be encrypted using the
    negotiated cipher, if any */
void mwConversation_setEncrypted(struct mwConversation *conv,
				 gboolean useCipher);


/** determine whether outgoing messages are being encrypted */
gboolean mwConversation_isEncrypted(struct mwConversation *conv);


/** Associates client data with a conversation. If there is existing data,
    it will not have its cleanup function called. */
void mwConversation_setClientData(struct mwConversation *conv,
				  gpointer data, GDestroyNotify clean);


/** Reference associated client data */
gpointer mwConversation_getClientData(struct mwConversation *conv);


void mwConversation_removeClientData(struct mwConversation *conv);


/** close and destroy the conversation and its backing channel, and
    call the optional client data cleanup function */
void mwConversation_free(struct mwConversation *conv);


#endif

