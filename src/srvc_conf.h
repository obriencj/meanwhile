

#ifndef _MW_SRVC_CONF_
#define _MW_SRVC_CONF_


#include <glib/glist.h>
#include "common.h"


enum mwConferenceStatus {
  mwConference_NEW      = 0x00,
  mwConference_PENDING  = 0x01,
  mwConference_INVITED  = 0x02,
  mwConference_ACTIVE   = 0x08
};


struct mwConference {
  enum mwConferenceStatus status;

  struct mwServiceConf *srvc;
  struct mwChannel *channel;

  char *name;
  char *topic;

  /* internally used list of conference members */
  GList *members;
};


struct mwServiceConf {
  struct mwService service;

  /* triggered when we receive a conference invitation. */
  void (*got_invite)(struct mwConference *, struct mwIdBlock *, const char *);

  /* triggered when we enter the conference. Provides initial conference
     membership list */
  void (*got_welcome)(struct mwConference *, struct mwIdBlock *, gsize count);

  /* triggered when we leave the conference, or the conference leaves us */
  void (*got_closed)(struct mwConference *);

  /* when someone joins the conference */
  void (*got_join)(struct mwConference *, struct mwIdBlock *);

  /* when someone leaves the conference */
  void (*got_part)(struct mwConference *, struct mwIdBlock *);

  /* when someone says something */
  void (*got_text)(struct mwConference *, struct mwIdBlock *, const char *);

  /* typing notification */
  void (*got_typing)(struct mwConference *, struct mwIdBlock *, gboolean);

  /* list of mwConference structs */
  GList *conferences;
};


struct mwServiceConf *mwServiceConf_new(struct mwSession *);


/* generate a conference to be used in mwConference_create */
struct mwConference *mwConference_new(struct mwServiceConf *srvc);


/* initiate a new conference */
int mwConference_create(struct mwConference *conf);


/* leave and close an existing conference, or reject an invitation */
int mwConference_destroy(struct mwConference *conf,
			 guint32 reason, const char *text);


/* accept a conference invitation */
int mwConference_accept(struct mwConference *conf);


/* invite another user to an ACTIVE conference */
int mwConference_invite(struct mwConference *conf, struct mwIdBlock *who,
			const char *text);


struct mwConference *mwConference_findByChannel(struct mwServiceConf *srvc,
						struct mwChannel *chan);


struct mwConference *mwConference_findByName(struct mwServiceConf *srvc,
					     const char *name);


int mwConference_sendText(struct mwConference *conf, const char *text);


int mwConference_sendTyping(struct mwConference *conf, gboolean typing);


#endif

