
#ifndef _MW_SRVC_IM_H
#define _MW_SRVC_IM_H


#include <glib.h>
#include "common.h"


/* place-holders */
struct mwService;
struct mwSession;


/* this service can be adapted later to allow for more than just the two
   understood types of events */


/** @struct mwServiceIm
    An instance of the IM service. This service provides simple
    instant messaging functionality */
struct mwServiceIm;


/** IM Service Handler. Provides functions for events triggered from an
    IM service instance. */
struct mwServiceImHandler {

  /** A plain text message is received */
  void (*got_text)(struct mwServiceIm *srvc,
		   struct mwIdBlock *from, const char *text);

  /** A markup-formatted message is received */
  void (*got_html)(struct mwServiceIm *srvc,
		   struct mwIdBlock *from, const char *html);

  /** Received typing notification */
  void (*got_typing)(struct mwServiceIm *srvc,
		     struct mwIdBlock *from, gboolean typing);
  
  /** An error occured in the conversation to user */
  void (*got_error)(struct mwServiceIm *srvc,
		    struct mwIdBlock *user, guint32 error);

  /** Called from mwService_free */
  void (*clear)(struct mwServiceIm *srvc);


  /** optional member for client data */
  gpointer data;
};


struct mwServiceIm *mwServiceIm_new(struct mwSession *session,
				    struct mwServiceImHandler *handler);


struct mwServiceImHandler *mwServiceIm_getHandler(struct mwServiceIm *srvc);


int mwServiceIm_sendText(struct mwServiceIm *srvc,
			 struct mwIdBlock *target, const char *text);


/** sends an html formatted message to a user. If the user cannot
    accept such a message, got_error of value XXX will be triggered,
    and the message should be manually resent as plain text */
int mwServiceIm_sendHtml(struct mwServiceIm *srvc,
			 struct mwIdBlock *target, const char *html);


int mwServiceIm_sendTyping(struct mwServiceIm *srvc,
			   struct mwIdBlock *target, gboolean typing);


void mwServiceIm_closeChat(struct mwServiceIm *srvc,
			   struct mwIdBlock *target);


#endif

