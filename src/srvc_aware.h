

#ifndef _MW_SRVC_AWARE_H
#define _MW_SRVC_AWARE_H


#include "common.h"


/* place-holder */
struct mwSession;


/** @struct mwServiceAware
    Instance of an Aware Service. The members of this structure are
    not made available. Accessing the parts of an aware service should
    be performed through the appropriate functions. Note that
    instances of this structure can be safely cast to a mwService.
*/
struct mwServiceAware;


/** @struct mwAwareList
    Instance of an Aware List. The members of this structure are not
    made available. Access to the parts of an aware list should be
    handled through the appropriate functions.
*/
struct mwAwareList;


/** Appropriate function type for the on-aware signal
    @param list  mwAwareList emiting the signal
    @param id    awareness status information
    @param data  user-specified data
*/
typedef void (*mwAwareList_onAwareHandler)
     (struct mwAwareList *list,
      struct mwAwareSnapshot *id,
      gpointer data);


struct mwServiceAware *mwServiceAware_new(struct mwSession *);


/** Allocate and initialize an aware list. */
struct mwAwareList *mwAwareList_new(struct mwServiceAware *);


/** Clean and free an aware list. Will remove all signal subscribers */
void mwAwareList_free(struct mwAwareList *list);


/** Add a collection of user IDs to an aware list.
    @param list     mwAwareList to add user ID to
    @param id_list  mwAwareIdBlock list of user IDs to add
    @return         0 for success, non-zero to indicate an error.
*/
int mwAwareList_addAware(struct mwAwareList *list, GList *id_list);


/** Remove a collection of user IDs from an aware list.
    @param list     mwAwareList to remove user ID from
    @param id_list  mwAwareIdBlock list of user IDs to remove
    @return  0      for success, non-zero to indicate an error.
*/
int mwAwareList_removeAware(struct mwAwareList *list, GList *id_list);


/** Utility function for registering a subscriber to the on-aware signal
    emitted by an aware list.
    @param list       mwAwareList to listen for
    @param cb         callback function
    @param data       optional user data to be passed along to cb
    @param data_free  optional function to cleanup data on mwAwareList_free
*/
void mwAwareList_setOnAware(struct mwAwareList *list,
			    mwAwareList_onAwareHandler cb,
			    gpointer data, GDestroyNotify data_free);


/** trigger a got_aware event constructed from the passed user and
    status information. Useful for adding false users and having the
    getText function work for them */
void mwServiceAware_setStatus(struct mwServiceAware *srvc,
			      struct mwAwareIdBlock *user,
			      struct mwUserStatus *stat);


/** look up the status description for a user */
const char *mwServiceAware_getText(struct mwServiceAware *srvc,
				   struct mwAwareIdBlock *user);


#endif


