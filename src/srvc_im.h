

#ifndef _MW_SRVC_IM_H_
#define _MW_SRVC_IM_H_


#include <glib.h>
#include "common.h"


struct mwService;
struct mwSession;


/* this service can be adapted later to allow for more than just the two
   understood types of events */

struct mwServiceIM {
  struct mwService service;

  void (*got_text)(struct mwServiceIM *,
		   struct mwIdBlock *, const char *);

  void (*got_typing)(struct mwServiceIM *,
		     struct mwIdBlock *, gboolean);

  void (*got_error)(struct mwServiceIM *,
		    struct mwIdBlock *, guint32 err);
};


struct mwServiceIM *mwServiceIM_new(struct mwSession *);


int mwServiceIM_sendText(struct mwServiceIM *srvc,
			 struct mwIdBlock *target, const char *text);


int mwServiceIM_sendTyping(struct mwServiceIM *srvc,
			   struct mwIdBlock *target, gboolean typing);


void mwServiceIM_closeChat(struct mwServiceIM *srvc,
			   struct mwIdBlock *target);


#endif

