

#ifndef _MW_SRVC_CONF_H
#define _MW_SRVC_CONF_H


#include <glib/glist.h>
#include "mw_common.h"


/* place-holder */
struct mwChannel;
struct mwSession;


/** Type identifier for the conference service */
#define SERVICE_CONFERENCE  0x80000010


enum mwConferenceState {
  mwConference_NEW      = 0x00,  /**< new conference */
  mwConference_PENDING  = 0x01,  /**< conference pending creation */
  mwConference_INVITED  = 0x02,  /**< invited to conference */
  mwConference_ACTIVE   = 0x08,  /**< conference active */
  mwConference_ERROR    = 0x80,  /**< conference error, needs to be closed */
};


/** @struct mwServiceConference
    Instance of the multi-user conference service */
struct mwServiceConference;


/** @struct mwConference
    A multi-user chat */
struct mwConference;


/** Handler structure used to provide callbacks for an instance of the
    conferencing service */
struct mwServiceConfHandler {

  /** triggered when we receive a conference invitation. Call
      mwConference_accept to accept the invitation and join the
      conference, or mwConference_close to reject the invitation.

      @param conf     the newly created conference
      @param inviter  the indentity of the user who sent the invitation
      @param invite   the invitation text
   */
  void (*on_invited)(struct mwConference *conf,
		     struct mwLoginInfo *inviter, const char *invite);

  /** triggered when we enter the conference. Provides the initial
      conference membership list as a GList of mwLoginInfo structures

      @param conf     the conference just joined
      @param members  mwLoginInfo list of existing conference members
  */
  void (*conf_opened)(struct mwConference *conf, GList *members);

  /** triggered when a conference is closed. This is typically when
      we've left it */
  void (*conf_closed)(struct mwConference *);

  /** triggered when someone joins the conference */
  void (*on_peer_joined)(struct mwConference *, struct mwLoginInfo *);

  /** triggered when someone leaves the conference */
  void (*on_peer_parted)(struct mwConference *, struct mwLoginInfo *);

  /** triggered when someone says something */
  void (*on_text)(struct mwConference *conf,
		  struct mwLoginInfo *who, const char *what);

  /** typing notification */
  void (*on_typing)(struct mwConference *conf,
		    struct mwLoginInfo *who, gboolean typing);

  /** optional. called from mwService_free */
  void (*clear)(struct mwServiceConference *srvc);
};


/** Allocate a new conferencing service, attaching the given handler */
struct mwServiceConference
*mwServiceConference_new(struct mwSession *,
			 struct mwServiceConfHandler *);


/** a mwConference list of the conferences in this service. The GList
    will need to be free'd after use */
GList *mwServiceConference_conferences(struct mwServiceConference *srvc);


/** Allocate a new conference. Conference will be in state NEW.
    @see mwConference_create */
struct mwConference *mwConference_new(struct mwServiceConference *srvc,
				      const char *name, const char *title);


const char *mwConference_getName(struct mwConference *conf);


const char *mwConference_getTitle(struct mwConference *conf);


/** a mwIdBlock list of the members of the conference. The GList will
    need to be free'd after use */
GList *mwConference_getMembers(struct mwConference *conf);


/** Initiate a conference. Conference must be in state NEW. If no name
    or title for the conference has been set, they will be
    generated. Conference will be placed into state PENDING. */
int mwConference_open(struct mwConference *conf);


/** Leave and close an existing conference, or reject an invitation.
    Triggers mwServiceConfHandler::conf_closed and free's the
    conference.
 */
int mwConference_close(struct mwConference *conf,
		       guint32 reason, const char *text);


/** accept a conference invitation. Conference must be in the state
    INVITED. */
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

