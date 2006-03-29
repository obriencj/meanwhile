#ifndef _MW_SRVC_IM_H
#define _MW_SRVC_IM_H


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
   @file mw_srvc_im.h
*/


#include <glib.h>
#include "mw_channel.h"
#include "mw_object.h"
#include "mw_service.h"
#include "mw_session.h"
#include "mw_typedef.h"


G_BEGIN_DECLS


#define MW_TYPE_IM_SERVICE  (MwIMService_getType())


#define MW_IM_SERVICE(obj)						\
  (G_TYPE_CHECK_INSTANCE_CAST((obj), MW_TYPE_IM_SERVICE, MwIMService))


#define MW_IM_SERVICE_CLASS(klass)					\
  (G_TYPE_CHECK_CLASS_CAST((klass), MW_TYPE_IM_SERVICE, MwIMServiceClass))


#define MW_IS_IM_SERVICE(obj)					\
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), MW_TYPE_IM_SERVICE))


#define MW_IS_IM_SERVICE_CLASS(klass)				\
  (G_TYPE_CHECK_CLASS_TYPE((klass), MW_TYPE_IM_SERVICE))


#define MW_IM_SERVICE_GET_CLASS(obj)					\
  (G_TYPE_INSTANCE_GET_CLASS((obj), MW_TYPE_IM_SERVICE, MwIMServiceClass))


typedef struct mw_im_service_private MwIMServicePrivate;


struct mw_im_service_private;


struct mw_im_service {
  MwService mwservice;

  MwIMServicePrivate *private;
};


typedef struct mw_im_service_class MwIMServiceClass;


struct mw_im_service_class {
  MwServiceClass mwservice_class;

  guint signal_incoming_conversation;

  /**
     @see MwIMService_getConversation
     
     This method can be overridden to provide a subclass of
     conversation for extensions of the IM service, without having to
     override the rest of the related methods herein.
   */
  MwConversation *(*new_conv)(MwIMService *srvc,
			      const gchar *user, const gchar *community);
  
  /** @see MwIMService_getConversation */ 
  MwConversation *(*get_conv)(MwIMService *srvc,
			      const gchar *user, const gchar *community);

  /** @see MwIMService_findConversation */
  MwConversation *(*find_conv)(MwIMService *srvc,
			       const gchar *user, const gchar *community);

  void (*foreach_conv)(MwIMService *srvc, GFunc func, gpointer data);
};


GType MwIMService_getType();


/**
   @return reference to new IM service instance
*/
MwIMService *MwIMService_new(MwSession *session);


/**
   @since 2.0.0
   
   Find or create a conversation to the target. If it's a new conversation,
   it will have a refcount of 1. If it's an existing conversation, the
   refcount will be incremented before being returned.

   @return reference to MwConversation instance
*/
MwConversation *
MwIMService_getConversation(MwIMService *srvc,
			    const gchar *user, const gchar *community);


/**
   @since 2.0.0
   
   Find a conversation to the target. If an existing conversation can
   be found, the refcount will be incremented before being returned.

   @return reference to MwConversation instance or NULL
*/
MwConversation *
MwIMService_findConversation(MwIMService *srvc,
			     const gchar *user, const gchar *community);


/**
   Borrowed references to all conversations created via this service instance.

   @return list of borrowed references to MwConversation instances
*/
GList *MwIMService_getConversations(MwIMService *srvc);


#define MW_TYPE_CONVERSATION  (MwConversation_getType())


#define MW_CONVERSATION(obj)						\
  (G_TYPE_CHECK_INSTANCE_CAST((obj), MW_TYPE_CONVERSATION, MwConversation))


#define MW_CONVERSATION_CLASS(klass)					\
  (G_TYPE_CHECK_CLASS_CAST((klass), MW_TYPE_CONVERSATION, MwConversationClass))


#define MW_IS_CONVERSATION(obj)					\
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), MW_TYPE_CONVERSATION))


#define MW_IS_CONVERSATION_CLASS(klass)				\
  (G_TYPE_CHECK_CLASS_TYPE((klass), MW_TYPE_CONVERSATION))


#define MW_CONVERSATION_GET_CLASS(obj)					\
  (G_TYPE_INSTANCE_GET_CLASS((obj), MW_TYPE_CONVERSATION, MwConversationClass))


typedef struct mw_conversation_private MwConversationPrivate;


struct mw_conversation_private;


struct mw_conversation {
  MwObject mwobject;

  MwConversationPrivate *private;
};


typedef struct mw_conversation_class MwConversationClass;


struct mw_conversation_class {
  MwObjectClass mwobject_class;

  guint signal_state_changed;

  guint signal_got_text;

  /** while technically a typing notification is a data message, since
      it's a standard feature, it's checked for first */
  guint signal_got_typing;

  /** when an incoming data message is not determined immediately to
      be text, it will be passed on via this signal. Useful for
      extending the basic conversation by adding new message types
      beyond text and typing */
  guint signal_got_data;

  void (*open)(MwConversation *self);
  void (*opened)(MwConversation *self);
  void (*close)(MwConversation *self, guint32 code);
  void (*closed)(MwConversation *self, guint32 code);

  void (*send_text)(MwConversation *self, const gchar *text);
  void (*send_typing)(MwConversation *self, gboolean typing);
  void (*send_data)(MwConversation *self, guint type, guint subtype,
		    const MwOpaque *data);
};


enum mw_conversation_state {
  mw_conversation_closed = mw_channel_closed,
  mw_conversation_pending = mw_channel_pending,
  mw_conversation_open = mw_channel_open,
  mw_conversation_error = mw_channel_error,
};


GType MwConversation_getType();


/**
   open a conversation.
*/
void MwConversation_open(MwConversation *conv);


/**
   close a conversation. May be re-opened 
*/
void MwConversation_close(MwConversation *conv, guint32 code);


gboolean MwConversation_isState(MwConversation *conv,
				enum mw_conversation_state state);


#define MW_CONVERSATION_IS_OPEN(conv) \
  (MwConversation_isState((conv), mw_conversation_OPEN))


void MwConversation_sendText(MwConversation *conv, const gchar *text);


void MwConversation_sendTyping(MwConversation *conv, gboolean typing);


void MwConversation_sendData(MwConversation *conv,
			     guint32 type, guint32 subtype,
			     const MwOpaque *data);


G_END_DECLS


#endif /* _MW_SRVC_IM_H */
