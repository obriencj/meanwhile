
#ifndef _MW_SRVC_RESOLVE_H
#define _MW_SRVC_RESOLVE_H


#include <glib.h>
#include <glib/glist.h>


/* place-holder */
struct mwSession;


/** Type identifier for the conference service */
#define SERVICE_RESOLVE  0x00000015


/** Return value of mwServiceResolve_search indicating an error */
#define SEARCH_ERROR  0x00


/** @struct mwServiceResolve
    User lookup service */
struct mwServiceResolve;


enum mwResolveFlag {
  /** return unique results or none at all */
  mwResolveFlag_UNIQUE    = 0x00000001,

  /** return only the first result */
  mwResolveFlag_FIRST     = 0x00000002,

  /** search all directories, not just the first with a match */
  mwResolveFlag_ALL_DIRS  = 0x00000004,

  /** search for users. This should be set for all user searches */
  mwResolveFlag_USERS     = 0x00000008,
};


/** @see mwResolveResult */
enum mwResolveCode {
  /** successful search */
  mwResolveCode_SUCCESS     = 0x00000000,

  /** only some of the nested searches were successful */
  mwResolveCode_PARTIAL     = 0x00010000,

  /** more than one result (occurs when mwResolveFlag_UNIQUE is used
      and more than one result would have been otherwise returned) */
  mwResolveCode_MULTIPLE    = 0x80020000,

  /** the name is not resolvable due to its format */
  mwResolveCode_BAD_FORMAT  = 0x80030000,
};


struct mwResolveMatch {
  char *id;    /**< user id */
  char *name;  /**< user name */
  char *desc;  /**< description */
};


struct mwResolveResult {
  guint32 code;    /**< @see mwResolveCode */
  char *name;      /**< name of the result */
  GList *matches;  /**< list of mwResolveMatch */
};


/** Handle the results of a resolve request. If there was a cleanup
    function specified to mwServiceResolve_search, it will be called
    upon the user data after this callback returns.

    @param srvc     the resolve service
    @param id       the resolve request ID
    @param results  list of mwResolveResult
    @param data     optional user data attached to the request
*/
typedef void (*mwResolveHandler)(struct mwServiceResolve *srvc,
				 guint32 id, GList *results, gpointer data);


/** Allocate a new resolve service */
struct mwServiceResolve *mwServiceResolve_new(struct mwSession *);


/** Inisitate a resolve request.

    @param query    NULL terminated array of query strings
    @param flags    search flags
    @param handler  result handling function
    @param data     optional user data attached to the request
    @param cleanup  optional function to clean up user data
    @return         generated ID for the search request, or SEARCH_ERROR
*/
guint32 mwServiceResolve_search(struct mwServiceResolve *srvc,
				const char *query[], enum mwResolveFlag flags,
				mwResolveHandler handler,
				gpointer data, GDestroyNotify cleanup);


/** Cancel a resolve request by its generated ID. The handler function
    will not be called, and the optional cleanup function will be
    called upon the optional user data for the request */
void mwServiceResolve_cancelSearch(struct mwServiceResolve *, guint32);


#endif
