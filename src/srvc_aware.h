

#ifndef _MW_SRVC_AWARE_H_
#define _MW_SRVC_AWARE_H_


#include <glib/ghash.h>
#include "common.h"


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
    @param list mwAwareList emiting the signal
    @param id awareness status information
    @param data user-specified data
*/
typedef void (*mwAwareList_onAwareHandler)
     (struct mwAwareList *list,
      struct mwSnapshotAwareIdBlock *id,
      gpointer data);


struct mwServiceAware *mwServiceAware_new(struct mwSession *);


/** Allocate and initialize an aware list. */
struct mwAwareList *mwAwareList_new(struct mwServiceAware *);


/** Clean and free an aware list. Will remove all signal subscribers */
void mwAwareList_free(struct mwAwareList *list);


/** Add a collection of user IDs to an aware list.
    @param list mwAwareList to add user ID to
    @param id_list array of user IDs to add
    @param count number of members in id_list
    @return zero for success, non-zero to indicate an error.
*/
int mwAwareList_addAware(struct mwAwareList *list,
			 struct mwAwareIdBlock *id_list, guint count);


/** Remove a collection of user IDs from an aware list.
    @param list mwAwareList to add user ID to
    @param id_list array of user IDs to add
    @param count number of members in id_list
    @return zero for success, non-zero to indicate an error.
*/
int mwAwareList_removeAware(struct mwAwareList *list,
			    struct mwAwareIdBlock *id_list, guint count);


/** Utility function for registering a subscriber to the on-aware signal
    emitted by an aware list.
    @param list mwAwareList to listen for
    @param cb callback function
    @param data user-specific data to be passed along to cb
 */
void mwAwareList_setOnAware(struct mwAwareList *list,
			    mwAwareList_onAwareHandler cb, gpointer data);


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


