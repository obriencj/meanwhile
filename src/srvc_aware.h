

#ifndef _MW_SRVC_AWARE_H_
#define _MW_SRVC_AWARE_H_


#include <glib/ghash.h>
#include "common.h"


struct mwService;
struct mwSession;


struct mwServiceAware {
  struct mwService service;

  void (*got_aware)(struct mwServiceAware *,
		    struct mwSnapshotAwareIdBlock *, unsigned int);

  GHashTable *buddy_text;
};


struct mwServiceAware *mwServiceAware_new(struct mwSession *);


int mwServiceAware_add(struct mwServiceAware *srvc,
		       struct mwIdBlock *list, unsigned int count);


int mwServiceAware_remove(struct mwServiceAware *srvc,
			  struct mwIdBlock *list, unsigned int count);


/* trigger a got_aware event constructed from the passed user and
   status information. Useful for adding false users and having the
   getText function work for them */
void mwServiceAware_setStatus(struct mwServiceAware *srvc,
			      struct mwAwareIdBlock *user,
			      struct mwUserStatus *stat);


/* look up the status description for a user */
const char *mwServiceAware_getText(struct mwServiceAware *srvc,
				   struct mwIdBlock *user);


#endif


