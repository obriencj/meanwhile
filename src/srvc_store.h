
#ifndef _MW_SRVC_STORE_H_
#define _MW_SRVC_STORE_H_


#include <glib.h>
#include "common.h"


/** @struct mwServiceStorage
    @see mwServiceStorage_new
    Instance of the storage service handler. */
struct mwServiceStorage;


/** These are some of the common keys determined from packet dumps that
    might be relevant for other clients. */
enum mwStorageKey {
  /** The buddy list, in the Sametime .dat file format. String */
  mwStore_AWARE_LIST      = 0x00000000,

  /** Default text for chat invitations. String */
  mwStore_INVITE_CHAT     = 0x00000006,

  /** Default text for meeting invitations. String */
  mwStore_INVITE_MEETING  = 0x0000000e,
};


struct mwStorageUnit {
  /** key by which data is referenced in service
      @see mwStorageKey */
  guint32 key;

  /** Data associated with key in service */
  struct mwOpaque data;
};


/** Appropriate function type for load and store callbacks.
    @param result the result value of the load or store call
    @param item the storage unit loaded or saved
    @param data user data
 */
typedef void (*mwStorageCallback)
     (guint result, struct mwStorageUnit *item, gpointer data);


/** Allocates and initializes a storage service instance for use on
    the passed session. */
struct mwServiceStorage *mwServiceStorage_new(struct mwSession *);


/** allocated an empty storage unit */
struct mwStorageUnit *mwStorageUnit_new(guint32 key);


/** creates a storage unit with the passed key, and a copy of data. */
struct mwStorageUnit *mwStorageUnit_newData(guint32 key,
					    struct mwOpaque *data);


/** creates a storage unit with the passed key, and an encapsulated
    boolean value */
struct mwStorageUnit *mwStorageUnit_newBoolean(guint32 key,
					       gboolean val);


/** creates a storage unit with the passed key, and an encapsulated
    string value. */
struct mwStorageUnit *mwStorageUnit_newString(guint32 key,
					      const char *str);


/** attempts to obtain a boolean value from a storage unit. If the
    unit is empty, or does not contain the type in a recongnizable
    format, val is returned instead */
gboolean mwStorageUnit_asBoolean(struct mwStorageUnit *, gboolean val);


/** attempts to obtain a string value from a storage unit. If the unit
    is empty, or does not contain the type in a recognizable format,
    NULL is returned instead. Note that string returned is a copy, and
    will need to be deallocated at some point. */
char *mwStorageUnit_asString(struct mwStorageUnit *);


/** clears and frees a storage unit */
void mwStorageUnit_free(struct mwStorageUnit *);

      
/** Initiates a load call to the storage service. If the service is
    not currently available, the call will be cached and processed
    when the service is started.
    @param item storage unit to load
    @param cb callback function when the load call completes
    @param data user data for callback
*/
void mwServiceStorage_load(struct mwServiceStorage *srvc,
			   struct mwStorageUnit *item,
			   mwStorageCallback cb, gpointer data);


/** Initiates a store call to the storage service. If the service is
    not currently available, the call will be cached and processed
    when the service is started.
    @param item storage unit to save
    @param cb callback function when the load call completes
    @param data user data for callback
*/
void mwServiceStorage_save(struct mwServiceStorage *srvc,
			   struct mwStorageUnit *item,
			   mwStorageCallback cb, gpointer data);


#endif
