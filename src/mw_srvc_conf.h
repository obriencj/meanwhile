

#ifndef _MW_SRVC_CONF_H
#define _MW_SRVC_CONF_H


#include <glib/glist.h>
#include "mw_common.h"


/* place-holder */
struct mwChannel;
struct mwSession;


/** Type identifier for the conference service */
#define SERVICE_CONF  0x80000010


enum mwConferenceState {
  mwConference_NEW      = 0x00,  /**< new conference */
  mwConference_PENDING  = 0x01,  /**< conference pending creation */
  mwConference_INVITED  = 0x02,  /**< invited to conference */
  mwConference_ACTIVE   = 0x08,  /**< conference active */
  mwConference_ERROR    = 0x80,  /**< conference error, needs to be closed */
};


/** @struct mwServiceConf
    Instance of the conference service */
struct mwServiceConf;


/** @struct mwConference
    A multi-user conference */
struct mwConference;


/** Handler structure used to provide callbacks for an instance of the
    conferencing service */
struct mwServiceConfHandler {

  /** triggered when we receive a conference invitation
      @param conf     the newly created conference
      @param inviter  the indentity of the user who sent the invitation
      @param invite   the invitation text
   */
  void (*got_invite)(struct mwConference *conf,
		     struct mwLoginInfo *inviter, const char *invite);

  /** triggered when we enter the conference. Provides the initial
      conference membership list
      @param conf     the conference just joined
      @param members  mwLoginInfo list of existing conference members
  */
  void (*got_welcome)(struct mwConference *conf, GList *members);

  /** triggered when we leave the conference */
  void (*got_closed)(struct mwConference *);

  /** triggered when someone joins the conference */
  void (*got_join)(struct mwConference *, struct mwLoginInfo *);

  /** triggered when someone leaves the conference */
  void (*got_part)(struct mwConference *, struct mwLoginInfo *);

  /** triggered when someone says something */
  void (*got_text)(struct mwConference *conf,
		   struct mwLoginInfo *who, const char *what);

  /** triggered when someone says something with formatting
      @todo not implemented */
  void (*got_html)(struct mwConference *conf,
		   struct mwLoginInfo *who, const char *what);

  /** typing notification */
  void (*got_typing)(struct mwConference *conf,
		     struct mwLoginInfo *who, gboolean typing);

  /** triggered from mwService_free, should be used to clean up before
      service is free'd */
  void (*clear)(struct mwServiceConf *srvc);
};


/** Allocate a new conferencing service, attaching the given handler */
struct mwServiceConf *mwServiceConf_new(struct mwSession *,
					struct mwServiceConfHandler *);


/** a mwConference list of the conferences in this service. The GList
    will need to be freed after user */
GList *mwServiceConf_conferences(struct mwServiceConf *srvc);


/** Allocate a new conference. Conference will be in state NEW.
    @see mwConference_create */
struct mwConference *mwConference_new(struct mwServiceConf *srvc);


const char *mwConference_getName(struct mwConference *conf);


void mwConference_setName(struct mwConference *conf, const char *name);


const char *mwConference_getTitle(struct mwConference *conf);


void mwConference_setTitle(struct mwConference *conf, const char *title);


/** a mwIdBlock list of the members of the conference. The GList will
    need to be freed after use */
GList *mwConference_getMembers(struct mwConference *conf);


/** Initiate a conference. Conference must be in state NEW. If no name
    or title for the conference has been set, they will be
    generated. Conference will be placed into state PENDING */
int mwConference_create(struct mwConference *conf);


/** Leave and close an existing conference, or reject an invitation */
int mwConference_destroy(struct mwConference *conf,
			 guint32 reason, const char *text);


/** accept a conference invitation. Conference must be in the state
    INVITED. This will result in the got_welcome callback of the
    service handler being triggered */
int mwConference_accept(struct mwConference *conf);


/** invite another user to an ACTIVE conference
    @param who   user to invite
    @param text  invitation message
 */
int mwConference_invite(struct mwConference *conf,
			struct mwIdBlock *who, const char *text);


int mwConference_sendText(struct mwConference *conf, const char *text);


int mwConference_sendTyping(struct mwConference *conf, gboolean typing);


#endif

