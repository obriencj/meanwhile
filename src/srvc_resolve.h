
#ifndef _MW_SRVC_RESOLVE_H_
#define _MW_SRVC_RESOLVE_H_


#include <glib.h>
#include "common.h"


struct mwSession;

struct mwServiceResolve;

typedef void (*mwResolveHandler)(struct mwServiceResolve *,
				 gpointer data, guint32 id, guint32 count,
				 struct mwResolveResult *);

enum mwResolveFlags {
  mwResolve_UNIQUE    = 0x00000001,
  mwResolve_FIRST     = 0x00000002,
  mwResolve_ALL_DIRS  = 0x00000004,
  mwResolve_USERS     = 0x00000008
};


struct mwServiceResolve *mwServiceResolve_new(struct mwSession *);

guint32 mwServiceResolve_search(struct mwServiceResolve *,
				const char *, enum mwResolveFlags,
				mwResolveHandler, gpointer, GDestroyNotify);

void mwServiceResolve_cancelSearch(struct mwServiceResolve *, guint32);


#endif
