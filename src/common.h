

#ifndef _MW_COMMON_H_
#define _MW_COMMON_H_


#include <glib.h>


/** @file common.h

    Functions in this file all fit into similar naming conventions of
    <code>TYPE_ACTION</code> as per the activity they perform. The
    following actions are available:

    <code>void TYPE_put(struct mwPutBuffer *b, TYPE *val)</code>
    marshalls val onto the buffer b. The buffer will grow as necessary
    to fit all the data put into it. For guint16, guint32, and
    gboolean, <code>TYPE val</code> is used instead of <code>TYPE
    *val</code>.

    <code>void TYPE_get(struct mwGetBuffer *b, TYPE *val)</code>
    unmarshals val from the buffer b. Failure (due to lack of
    insufficient remaining buffer) is indicated in the buffer's error
    field. A call to a _get function with a buffer in an error state
    has to effect.

    <code>void TYPE_clear(TYPE *val)</code>
    zeros and frees internal members of val, but does not free val
    itself. Needs to be called before free-ing any complex types which
    have been unmarshalled from a TYPE_get or populated from a
    TYPE_clone call to prevent memory leaks.

    <code>void TYPE_clone(TYPE *to, TYPE *from)</code>
    copies/clones members of from into to. May result in memory
    allocation for some types. Note that to is not cleared
    before-hand, it must already be in a pristine condition.

    <code>gboolean TYPE_equal(TYPE *y, TYPE *z)</code>
    simple equality test.
*/


/** @struct mwPutBuffer a buffer to be written to */
struct mwPutBuffer;

/** @struct mwGetBuffer a buffer to be read from */
struct mwGetBuffer;


/** A length of binary data, not null-terminated. */
struct mwOpaque {
  gsize len;   /**< length of data. */
  char *data;  /**< data, normally with no NULL termination */
};


/* 8.3.6 Login Types */

/** The type of login. Normally meaning the type of client code being
    used to login with. */
enum mwLoginType {
  mwLogin_LIB       = 0x1000,  /**< official Lotus binary library */
  mwLogin_JAVA_WEB  = 0x1001,  /**< official Lotus Java applet */
  mwLogin_BINARY    = 0x1002,  /**< official Lotus binary application */
  mwLogin_JAVA_APP  = 0x1003,  /**< official Lotus Java application */
  mwLogin_MEANWHILE = 0x1700   /**< Meanwhile library */
};


/* 8.2 Common Structures */
/* 8.2.1 Login Info block */

struct mwLoginInfo {
  char *login_id;   /**< community-unique ID of the login */
  guint16 type;     /**< @see mwLoginType */
  char *user_id;    /**< community-unique ID of the user */
  char *user_name;  /**< name of user (nick name, full name, etc) */
  char *community;  /**< community name (usually domain name) */
  gboolean full;    /**< if FALSE, following fields non-existant */
  char *desc;       /**< implementation defined description */
  guint32 ip_addr;  /**< ip addr of the login */
  char *server_id;  /**< unique ID of login's server */
};


/* 8.2.2 Private Info Block */

struct mwUserItem {
  gboolean full;  /**< if FALSE, don't include name */
  char *id;       /**< user id */
  char *name;     /**< user name */
};


struct mwPrivacyInfo {
  guint16 reserved;          /**< reserved for internal use */
  gboolean deny;             /**< deny (true) or allow (false) users */
  guint32 count;             /**< count of users */
  struct mwUserItem *users;  /**< the users list */
};


/* 8.3.5 User Status Types */

enum mwStatusType {
  mwStatus_ACTIVE  = 0x0020,
  mwStatus_IDLE    = 0x0040,
  mwStatus_AWAY    = 0x0060,
  mwStatus_BUSY    = 0x0080
};


/* 8.2.3 User Status Block */

struct mwUserStatus {
  guint16 status;  /**< @see mwStatusType */
  guint32 time;    /**< last status change time in seconds */
  char *desc;      /**< status description */
};


/* 8.2.4 ID Block */

struct mwIdBlock {
  char *user;       /**< user id (login id or empty for some services) */
  char *community;  /**< community id (NULL for same community) */
};


/* 8.3.8.2 Awareness Presence Types */

/* @todo move mwAwareType, mwAwareIdBlock and mwAwareSnapshot into the
   aware service and out of common */

/** type codes for mwAwareIdBlock */
enum mwAwareType {
  mwAware_USER  = 0x0002,  /**< a single user */
  mwAware_GROUP = 0x0003,  /**< a group */
};


/* 8.4.2 Awareness Messages */
/* 8.4.2.1 Awareness ID Block */

struct mwAwareIdBlock {
  guint16 type;     /**< @see mwAwareType */
  char *user;       /**< user id */
  char *community;  /**< community id (NULL for same community) */
};


/* 8.4.2.4 Snapshot */

struct mwAwareSnapshot {
  struct mwAwareIdBlock id;
  char *group;                 /**< group this id belongs to */
  gboolean online;             /**< is this user online? */
  char *alt_id;                /**< alternate ID, often same as id.user */
  struct mwUserStatus status;  /**< status of this user */
  char *name;                  /**< Formatted version of ID */
};


/* 8.3.1.5 Resolve error codes */
/* @todo move into the resolve service */

enum mwResolveCode {
  mwResolve_SUCCESS    = 0x00000000,
  mwResolve_PARTIAL    = 0x00010000,
  mwResolve_MULTIPLE   = 0x80020000,
  mwResolve_BAD_FORMAT = 0x80030000,
};


/* 8.4.4.2 Resolve Response */
/* @todo move into the resolve service */

struct mwResolveMatch {
  char *id;    /**< user id */
  char *name;  /**< user name */
  char *desc;  /**< description */
};


struct mwResolveResult {
  guint32 code;   /**< @see mwResolveCode */
  char *name;     /**< name of the result */
  guint32 count;  /**< count of matches */
  struct mwResolveMatch *matches;
};


/** encryption blocks */
struct mwEncryptItem {
  guint16 id;            /**< cipher identifier */
  struct mwOpaque info;  /**< cipher information */
};


/** @name buffer utility functions */
/*@{*/


/** allocate a new empty buffer */
struct mwPutBuffer *mwPutBuffer_new();


/** destroy the buffer */
void mwPutBuffer_free(struct mwPutBuffer *b);


/** move the buffer's data into an opaque, destroy the buffer */
void mwPutBuffer_finalize(struct mwOpaque *to, struct mwPutBuffer *from);


/** allocate a new buffer with a copy of the given data */
struct mwGetBuffer *mwGetBuffer_new(struct mwOpaque *data);


/** allocate a new buffer backed by the given data. Calling
    mwGetBuffer_free will not result in the underlying data being
    freed */
struct mwGetBuffer *mwGetBuffer_wrap(struct mwOpaque *data);


/** destroy the buffer */
void mwGetBuffer_free(struct mwGetBuffer *b);


/** reset the buffer to the very beginning. Also clears the buffer's
    error flag. */
void mwGetBuffer_reset(struct mwGetBuffer *b);


/** count of remaining available bytes */
gsize mwGetBuffer_remaining(struct mwGetBuffer *b);


/** TRUE if an error occurred while reading a basic type from this
    buffer */
gboolean mwGetBuffer_error(struct mwGetBuffer *b);


/*@}*/


/** @name Basic Data Type Marshalling
    The basic types are combined to construct the complex types.
 */
/*@{*/


void guint16_put(struct mwPutBuffer *b, guint16 val);

void guint16_get(struct mwGetBuffer *b, guint16 *val);

guint16 guint16_peek(struct mwGetBuffer *b);


void guint32_put(struct mwPutBuffer *b, guint32 val);

void guint32_get(struct mwGetBuffer *b, guint32 *val);

guint32 guint32_peek(struct mwGetBuffer *b);


void gboolean_put(struct mwPutBuffer *b, gboolean val);

void gboolean_get(struct mwGetBuffer *b, gboolean *val);

gboolean gboolean_peek(struct mwGetBuffer *b);


void mwString_put(struct mwPutBuffer *b, const char *str);

void mwString_get(struct mwGetBuffer *b, char **str);


void mwOpaque_put(struct mwPutBuffer *b, struct mwOpaque *o);

void mwOpaque_get(struct mwGetBuffer *b, struct mwOpaque *o);

void mwOpaque_clear(struct mwOpaque *o);

void mwOpaque_clone(struct mwOpaque *to, struct mwOpaque *from);


/*@}*/


/** @name Complex Data Type Marshalling */
/*@{*/


void mwLoginInfo_put(struct mwPutBuffer *b, struct mwLoginInfo *info);

void mwLoginInfo_get(struct mwGetBuffer *b, struct mwLoginInfo *info);

void mwLoginInfo_clear(struct mwLoginInfo *info);

void mwLoginInfo_clone(struct mwLoginInfo *to, struct mwLoginInfo *from);


void mwUserItem_put(struct mwPutBuffer *b, struct mwUserItem *user);

void mwUserItem_get(struct mwGetBuffer *b, struct mwUserItem *user);

void mwUserItem_clear(struct mwUserItem *user);


void mwPrivacyInfo_put(struct mwPutBuffer *b, struct mwPrivacyInfo *info);

void mwPrivacyInfo_get(struct mwGetBuffer *b, struct mwPrivacyInfo *info);

void mwPrivacyInfo_clear(struct mwPrivacyInfo *info);


void mwUserStatus_put(struct mwPutBuffer *b, struct mwUserStatus *stat);

void mwUserStatus_get(struct mwGetBuffer *b, struct mwUserStatus *stat);

void mwUserStatus_clear(struct mwUserStatus *stat);

void mwUserStatus_clone(struct mwUserStatus *to, struct mwUserStatus *from);


void mwIdBlock_put(struct mwPutBuffer *b, struct mwIdBlock *id);

void mwIdBlock_get(struct mwGetBuffer *b, struct mwIdBlock *id);

void mwIdBlock_clear(struct mwIdBlock *id);

void mwIdBlock_clone(struct mwIdBlock *to, struct mwIdBlock *from);

gboolean mwIdBlock_equal(struct mwIdBlock *a, struct mwIdBlock *b);


void mwAwareIdBlock_put(struct mwPutBuffer *b, struct mwAwareIdBlock *idb);

void mwAwareIdBlock_get(struct mwGetBuffer *b, struct mwAwareIdBlock *idb);

void mwAwareIdBlock_clear(struct mwAwareIdBlock *idb);

void mwAwareIdBlock_clone(struct mwAwareIdBlock *to,
			  struct mwAwareIdBlock *from);

gboolean mwAwareIdBlock_equal(struct mwAwareIdBlock *a,
			      struct mwAwareIdBlock *b);


void mwAwareSnapshot_get(struct mwGetBuffer *b,
			 struct mwAwareSnapshot *idb);

void mwAwareSnapshot_clear(struct mwAwareSnapshot *idb);

void mwAwareSnapshot_clone(struct mwAwareSnapshot *to,
			   struct mwAwareSnapshot *from);


void mwEncryptItem_put(struct mwPutBuffer *b, struct mwEncryptItem *item);

void mwEncryptItem_get(struct mwGetBuffer *b, struct mwEncryptItem *item);

void mwEncryptItem_clear(struct mwEncryptItem *item);


/*@}*/


#endif
