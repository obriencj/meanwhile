

#ifndef _MW_SRVC_CONF_H
#define _MW_SRVC_CONF_H


#include <glib/glist.h>
#include "mw_common.h"


/* place-holder */
struct mwChannel;
struct mwSession;


/** Type identifier for the conference service */
#define SERVICE_CONFERENCE  0x00000010


enum mwConferenceState {
  mwConference_NEW,      /**< new conference */
  mwConference_PENDING,  /**< outgoing conference pending creation */
  mwConference_INVITED,  /**< invited to incoming conference */
  mwConference_OPEN,     /**< conference open and active */
  mwConference_CLOSING,  /**< conference is closing */
  mwConference_ERROR,    /**< conference is closing due to error */
  mwConference_UNKNOWN,  /**< unable to determine conference state */
};


/** @struct mwServiceConference
    Instance of the multi-user conference service */
struct mwServiceConference;


/** @struct mwConference
    A multi-user chat */
struct mwConference;


/** Handler structure used to provide callbacks for an instance of the
    conferencing service */
struct mwConferenceHandler {

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
  void (*conf_closed)(struct mwConference *, guint32 reason);

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


/** Allocate a new conferencing service, attaching the given handler
    @param sess     owning session
    @param handler  handler providing call-back functions for the service
 */
struct mwServiceConference *
mwServiceConference_new(struct mwSession *sess,
			struct mwConferenceHandler *handler);


/** a mwConference list of the conferences in this service. The GList
    will need to be destroyed with g_list_free after use */
GList *mwServiceConference_conferences(struct mwServiceConference *srvc);


/** Allocate a new conference, in state NEW with the given title.
    @see mwConference_create */
struct mwConference *mwConference_new(struct mwServiceConference *srvc,
				      const char *title);


/** @returns unique conference name */
const char *mwConference_getName(struct mwConference *conf);


/** @returns conference title */
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
int mwConference_destroy(struct mwConference *conf,
			 guint32 reason, const char *text);


#define mwConference_reject(c,r,t) \
  mwConference_destroy((c),(r),(t))


/** accept a conference invitation. Conference must be in the state
    INVITED. */
int mwConference_accept(struct mwConference *conf);


/** invite another user to an ACTIVE conference
    @param who   user to invite
    @param text  invitation message
 */
int mwConference_invite(struct mwConference *conf,
			struct mwIdBlock *who, const char *text);


/** send a text message over an open conference */
int mwConference_sendText(struct mwConference *conf, const char *text);


/** send typing notification over an open conference */
int mwConference_sendTyping(struct mwConference *conf, gboolean typing);


/** associate arbitrary client data and an optional cleanup function
    with a conference. If there is already client data with a clear
    function, it will not be called. */
void mwConference_setClientData(struct mwConference *conf,
				gpointer data, GDestroyNotify clear);


/** reference associated client data */
gpointer mwConference_getClientData(struct mwConference *conf);


/** remove associated client data if any, and call the cleanup
    function on the data as necessary */
void mwConference_removeClientData(struct mwConference *conf);
				    

#endif

