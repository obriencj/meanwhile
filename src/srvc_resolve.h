
#ifndef _MW_SRVC_RESOLVE_H
#define _MW_SRVC_RESOLVE_H


#include <glib.h>
#include "common.h"


struct mwSession;

struct mwServiceResolve;

/**
   @param srvc   the resolve service
   @param id     the resolve request ID
   @param count  the count of results
   @param data   user data attached to the request
*/
typedef void (*mwResolveHandler)(struct mwServiceResolve *srvc,
				 guint32 id, guint32 count,
				 struct mwResolveResult *results,
				 gpointer data);


enum mwResolveFlags {
  mwResolve_UNIQUE    = 0x00000001,
  mwResolve_FIRST     = 0x00000002,
  mwResolve_ALL_DIRS  = 0x00000004,
  mwResolve_USERS     = 0x00000008
};


struct mwServiceResolve *mwServiceResolve_new(struct mwSession *);

/**
   @param query    the query string
   @param flags    search flags
   @param handler  result handling function
   @param data     user data attached to the request
   @param cleanup  function to clean up user data
   @return         the generated id for the search request
*/
guint32 mwServiceResolve_search(struct mwServiceResolve *srvc,
				const char *query, enum mwResolveFlags flags,
				mwResolveHandler handler,
				gpointer data, GDestroyNotify cleanup);

void mwServiceResolve_cancelSearch(struct mwServiceResolve *, guint32);


#endif
