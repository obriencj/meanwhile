

#ifndef _MW_COMMON_H_
#define _MW_COMMON_H_


#include <glib.h>


/** @file common.h

    Functions in this file all fit into similar naming conventions of
    <code>TYPE_ACTION</code> as per the activity they perform. The
    following actions are available:

    <code>gsize TYPE_buflen(TYPE val)</code> - calculates the
    necessary length in bytes required to serialize val.

    <code>int TYPE_put(char **b, gsize *n, TYPE *val)</code> -
    marshalls val onto the buffer portion b, which has n bytes
    remaining. b will be incremented forward along the buffer, and n
    will be decremented by the number of bytes written. returns 0 for
    success, and non-zero for failure. Failure is usually attributed
    to an insufficiently large n (indicating not enough buffer
    remaining). For guint16, guint32, and gboolean, <code>TYPE
    val</code> is used instead of <code>TYPE *val</code>.

    <code>int TYPE_get(char **b, gsize *n, TYPE *val)</code> -
    unmarshals val from the buffer portion b, which has n bytes
    remaining. b will be incremented forward along the buffer, and n
    will be decremented by the number of bytes read. returns 0 for
    success, and non-zero for failure. Failure is usually attributed
    to an insufficiently large n (indicating not enough buffer
    remaining for the type to be complete).

    <code>void TYPE_clear(TYPE *val)</code> - zeros and frees internal
    members of val, but does not free val itself. Needs to be called
    before free-ing any complex types which have been unmarshalled
    from a TYPE_get or populated from a TYPE_clone call to prevent
    memory leaks.

    <code>void TYPE_clone(TYPE *to, TYPE *from)</code> - copies/clones
    members of from into to. May result in memory allocation for some
    types. Note that to is not cleared before-hand, it must already be
    in a pristine condition.

    <code>gboolean TYPE_equal(TYPE *y, TYPE *z)</code> - simple
    equality test.

*/


struct mwOpaque {
  gsize len;   /**< four byte unsigned integer: length of data. */
  char *data;  /**< data. normally no NULL termination */
};


/* 8.3.6 Login Types */

enum mwLoginType {
  mwLogin_LIB       = 0x1000,
  mwLogin_JAVA_WEB  = 0x1001,
  mwLogin_BINARY    = 0x1002,
  mwLogin_JAVA_APP  = 0x1003,
  mwLogin_MEANWHILE = 0x1700
};


/* 8.2 Common Structures */
/* 8.2.1 Login Info block */

struct mwLoginInfo {
  char *login_id;         /**< community-unique ID of the login */
  enum mwLoginType type;  /**< type of login (see 8.3.6) */
  char *user_id;          /**< community-unique ID of the user */
  char *user_name;        /**< name of user (nick name, full name, etc) */
  char *community;        /**< community name (usually domain name) */
  gboolean full;          /**< if FALSE, following fields non-existant */
  char *desc;             /**< implementation defined description */
  guint ip_addr;          /**< ip addr of the login */
  char *server_id;        /**< unique ID of login's server */
};


/* 8.2.2 Private Info Block */

struct mwUserItem {
  gboolean full;  /**< if FALSE, don't include name */
  char *id;       /**< user id */
  char *name;     /**< user name */
};


struct mwPrivacyInfo {
  guint reserved;            /**< reserved for internal use */
  gboolean deny;             /**< deny (true) or allow (false) users */
  guint count;               /**< count of following users list */
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
  enum mwStatusType status;  /**< status of user (see 8.3.5) */
  guint time;                /**< last status change time in seconds */
  char *desc;                /**< status description */
};


/* 8.2.4 ID Block */

struct mwIdBlock {
  char *user;       /**< user id (login id or empty for some services) */
  char *community;  /**< community name (empty for same community) */
};


/* 8.2.5 Encryption Block */

enum mwEncryptType {
  mwEncrypt_NONE    = 0x0000,
  mwEncrypt_ANY     = 0x0001,  /* what's the difference between ANY */
  mwEncrypt_ALL     = 0x0002,  /* and ALL ? */
  mwEncrypt_RC2_40  = 0x1000
};

struct mwEncryptBlock {
  enum mwEncryptType type;
  struct mwOpaque opaque;
};


/* 8.3.8.2 Awareness Presence Types */

enum mwAwareType {
  mwAware_USER  = 0x0002
};


/* 8.4.2 Awareness Messages */
/* 8.4.2.1 Awareness ID Block */

struct mwAwareIdBlock {
  enum mwAwareType type;
  char *user;
  char *community;
};


/* 8.4.2.4 Snapshot */

struct mwSnapshotAwareIdBlock {
  struct mwAwareIdBlock id;
  gboolean online;
  char *alt_id;
  struct mwUserStatus status;
  char *wtf;  /* wtf is this? */
};


/* 8.3.1.5 Resolve error codes */

enum mwResultCode {
  mwResult_SUCCESS     = 0x00000000,
  mwResult_PARTIAL     = 0x00010000,
  mwResult_MULTIPLE    = 0x80020000,
  mwResult_BAD_FORMAT  = 0x80030000
};


/* 8.4.4.2 Resolve Response */

struct mwResolveMatch {
  char *id;
  char *name;
  char *desc;
};


struct mwResolveResult {
  enum mwResultCode code;
  char *name;
  guint count;
  struct mwResolveMatch *matches;
};


/** @name Basic Data Type Marshalling
    The basic types are combined to construct the complex types.
 */
/*@{*/


int guint16_put(char **b, gsize *n, guint val);

int guint16_get(char **b, gsize *n, guint *val);

guint guint16_peek(const char *b, gsize n);


int guint32_put(char **b, gsize *n, guint val);

int guint32_get(char **b, gsize *n, guint *val);

guint guint32_peek(const char *b, gsize n);


int gboolean_put(char **b, gsize *n, gboolean val);

int gboolean_get(char **b, gsize *n, gboolean *val);

gboolean gboolean_peek(const char *b, gsize n);


gsize mwString_buflen(const char *str);

int mwString_put(char **b, gsize *n, const char *str);

int mwString_get(char **b, gsize *n, char **str);


gsize mwOpaque_buflen(struct mwOpaque *o);

int mwOpaque_put(char **b, gsize *n, struct mwOpaque *o);

int mwOpaque_get(char **b, gsize *n, struct mwOpaque *o);

void mwOpaque_clear(struct mwOpaque *o);

void mwOpaque_clone(struct mwOpaque *to, struct mwOpaque *from);


/*@}*/

/** @name Complex Data Type Marshalling */
/*@{*/


gsize mwLoginInfo_buflen(struct mwLoginInfo *info);

int mwLoginInfo_put(char **b, gsize *n, struct mwLoginInfo *info);

int mwLoginInfo_get(char **b, gsize *n, struct mwLoginInfo *info);

void mwLoginInfo_clear(struct mwLoginInfo *info);

void mwLoginInfo_clone(struct mwLoginInfo *to, struct mwLoginInfo *from);


gsize mwUserItem_buflen(struct mwUserItem *user);

int mwUserItem_put(char **b, gsize *n, struct mwUserItem *user);

int mwUserItem_get(char **b, gsize *n, struct mwUserItem *user);

void mwUserItem_clear(struct mwUserItem *user);


gsize mwPrivacyInfo_buflen(struct mwPrivacyInfo *info);

int mwPrivacyInfo_put(char **b, gsize *n, struct mwPrivacyInfo *info);

int mwPrivacyInfo_get(char **b, gsize *n, struct mwPrivacyInfo *info);

void mwPrivacyInfo_clear(struct mwPrivacyInfo *info);


gsize mwUserStatus_buflen(struct mwUserStatus *stat);

int mwUserStatus_put(char **b, gsize *n, struct mwUserStatus *stat);

int mwUserStatus_get(char **b, gsize *n, struct mwUserStatus *stat);

void mwUserStatus_clear(struct mwUserStatus *stat);

void mwUserStatus_clone(struct mwUserStatus *to, struct mwUserStatus *from);


gsize mwIdBlock_buflen(struct mwIdBlock *id);

int mwIdBlock_put(char **b, gsize *n, struct mwIdBlock *id);

int mwIdBlock_get(char **b, gsize *n, struct mwIdBlock *id);

void mwIdBlock_clear(struct mwIdBlock *id);

void mwIdBlock_clone(struct mwIdBlock *to, struct mwIdBlock *from);

gboolean mwIdBlock_equal(struct mwIdBlock *a, struct mwIdBlock *b);


gsize mwEncryptBlock_buflen(struct mwEncryptBlock *eb);

int mwEncryptBlock_put(char **b, gsize *n, struct mwEncryptBlock *eb);

int mwEncryptBlock_get(char **b, gsize *n, struct mwEncryptBlock *eb);

void mwEncryptBlock_clear(struct mwEncryptBlock *enc);

void mwEncryptBlock_clone(struct mwEncryptBlock *to,
			  struct mwEncryptBlock *from);


gsize mwAwareIdBlock_buflen(struct mwAwareIdBlock *idb);

int mwAwareIdBlock_put(char **b, gsize *n, struct mwAwareIdBlock *idb);

int mwAwareIdBlock_get(char **b, gsize *n, struct mwAwareIdBlock *idb);

void mwAwareIdBlock_clear(struct mwAwareIdBlock *idb);

void mwAwareIdBlock_clone(struct mwAwareIdBlock *to,
			  struct mwAwareIdBlock *from);

gboolean mwAwareIdBlock_equal(struct mwAwareIdBlock *a,
			      struct mwAwareIdBlock *b);


int mwSnapshotAwareIdBlock_get(char **b, gsize *n,
			       struct mwSnapshotAwareIdBlock *idb);

void mwSnapshotAwareIdBlock_clear(struct mwSnapshotAwareIdBlock *idb);

void mwSnapshotAwareIdBlock_clone(struct mwSnapshotAwareIdBlock *to,
				  struct mwSnapshotAwareIdBlock *from);


/*@}*/


#endif
