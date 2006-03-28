#ifndef _MW_SRVC_NBIM_H
#define _MW_SRVC_NBIM_H


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
   @file mw_srvc_nbim.h

   An extension of the IM Service to provide additional features, such
   as HTML messages, conversation subjects, and multipart MIME
   messages (to allow for inline attachments such as images)

   These protocol extensions are all compatible with their original
   implementation in the Alphaworks NotesBuddy sametime client

   As these extensions are not part of the standard client's IM
   service, it is possible and likely that conversations initiated in
   either direction (from a user or to a user) may not support these
   features. In that case there is an optional compatability mode
   property for the session (to allow it to accept vanilla incoming
   conversation channels, not just NotesBuddy extension channels), and
   a compatability mode for individual conversations (to allow them to
   fall-back to standard-only features if the target user does not
   support the extensions). If neither compatability mode is used,
   then client code needs to fall back to an instance of the vanilla
   MwIMService in order to provide the standard features.
*/


#include <glib.h>
#include "mw_srvc_im.h"


G_BEGIN_DECLS


#define MW_TYPE_NB_IM_SERVICE  (MwNBIMService_getType())


#define MW_NB_IM_SERVICE(obj)					\
  (G_TYPE_CHECK_INSTANCE_CAST((obj), MW_TYPE_NB_IM_SERVICE,	\
			      MwNBIMService))


#define MW_NB_IM_SERVICE_CLASS(klass)				\
  (G_TYPE_CHECK_CLASS_CAST((klass), MW_TYPE_NB_IM_SERVICE,	\
			   MwNBIMServiceClass))


#define MW_IS_NB_IM_SERVICE(obj)				\
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), MW_TYPE_NB_IM_SERVICE))


#define MW_IS_NB_IM_SERVICE_CLASS(klass)			\
  (G_TYPE_CHECK_CLASS_TYPE((klass), MW_TYPE_NB_IM_SERVICE))


#define MW_NB_IM_SERVICE_GET_CLASS(obj)				\
  (G_TYPE_INSTANCE_GET_CLASS((obj), MW_TYPE_NB_IM_SERVICE,	\
			     MwNBIMServiceClass))


typedef struct mw_nb_im_service_private MwNBIMServicePrivate;


struct mw_nb_im_service {
  MwIMService mwimservice;

  MwNBIMServicePrivate *private;
};


typedef struct mw_nb_im_service_class MwNBIMServiceClass;


struct mw_nb_im_service_class {
  MwIMServiceClass mwimservice_class;
};


GType MwNBIMService_getType();


MwNBIMService *MwNBIMService_new(MwSession *session);


#define MW_TYPE_NB_CONVERSATION  (MwNBConversation_getType())


#define MW_NB_CONVERSATION(obj)						\
  (G_TYPE_CHECK_INSTANCE_CAST((obj), MW_TYPE_NB_CONVERSATION,		\
			      MwNBConversation))


#define MW_NB_CONVERSATION_CLASS(klass)					\
  (G_TYPE_CHECK_CLASS_CAST((klass), MW_TYPE_NB_CONVERSATION,		\
			   MwNBConversationClass))


#define MW_IS_NB_CONVERSATION(obj)				\
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), MW_TYPE_NB_CONVERSATION))


#define MW_IS_NB_CONVERSATION_CLASS(klass)			\
  (G_TYPE_CHECK_CLASS_TYPE((klass), MW_TYPE_NB_CONVERSATION))


#define MW_NB_CONVERSATION_GET_CLASS(obj)				\
  (G_TYPE_INSTANCE_GET_CLASS((obj), MW_TYPE_NB_CONVERSATION,		\
			     MwNBConversationClass))


typedef struct mw_nb_conversation_private MwNBConversationPrivate;


struct mw_nb_conversation_private;


struct mw_nb_conversation {
  MwConversation mwconv;

  MwNBConversationPrivate *private;
};


typedef struct mw_nb_conversation_class MwNBConversationClass;


struct mw_nb_conversation_class {
  MwConversationClass mwconvclass;

  guint signal_got_subject;
  guint signal_got_html;
  guint signal_got_mime;
  guint signal_got_stamp;

  void (*send_subject)(MwNBConversation *self, const gchar *subject);
  void (*send_html)(MwNBConversation *self, const gchar *html);
  void (*send_mime)(MwNBConversation *self, const gchar *mime);
  void (*send_stamp)(MwNBConversation *self, guint timestamp);
};


GType MwNBConversation_getType();


void MwNBConversation_sendSubject(MwNBConversation *self,
				  const gchar *subject);


void MwNBConversation_sendHTML(MwNBConversation *self,
			       const gchar *html);


void MwNBConversation_sendMIME(MwNBConversation *self,
			       const gchar *mime);


void MwNBConversation_sendStamp(MwNBConversation *self,
				guint timestamp);


G_END_DECLS


#endif /* _MW_SRVC_NBIM_H */
