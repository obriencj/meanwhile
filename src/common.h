

#ifndef _MW_COMMON_H_
#define _MW_COMMON_H_


#include <glib.h>


/* 8.1 Basic Data Types */

int guint16_put(gchar **b, gsize *n, guint val);

int guint16_get(gchar **b, gsize *n, guint *val);

guint guint16_peek(const gchar *b, gsize n);


int guint32_put(gchar **b, gsize *n, guint val);

int guint32_get(gchar **b, gsize *n, guint *val);

guint guint32_peek(const gchar *b, gsize n);


int gboolean_put(gchar **b, gsize *n, gboolean val);

int gboolean_get(gchar **b, gsize *n, gboolean *val);

gboolean gboolean_peek(const gchar *b, gsize n);


/* strings are normally NULL-terminated, but when written to a message then
   are not. Instead, they have a two-byte length prefix. */
gsize mwString_buflen(const gchar *str);

int mwString_put(gchar **b, gsize *n, const gchar *str);

int mwString_get(gchar **b, gsize *n, gchar **str);


struct mwOpaque {
  gsize len;    /* four byte unsigned integer: length of data. */
  gchar *data;  /* data. normally no NULL termination */
};

gsize mwOpaque_buflen(struct mwOpaque *o);

int mwOpaque_put(gchar **b, gsize *n, struct mwOpaque *o);

int mwOpaque_get(gchar **b, gsize *n, struct mwOpaque *o);

void mwOpaque_clear(struct mwOpaque *o);

void mwOpaque_clone(struct mwOpaque *to, struct mwOpaque *from);


/* 8.3.6 Login Types */

enum mwLoginType {
  mwLogin_LIB       = 0x1000,
  mwLogin_JAVA_WEB  = 0x1001,
  mwLogin_BINARY    = 0x1002,
  mwLogin_JAVA_APP  = 0x1003
};


/* 8.2 Common Structures */
/* 8.2.1 Login Info block */

struct mwLoginInfo {
  gchar *login_id;        /* community-unique ID of the login */
  enum mwLoginType type;  /* type of login (see 8.3.6) */
  gchar *user_id;         /* community-unique ID of the user */
  gchar *user_name;       /* name of user (nick name, full name, etc) */
  gchar *community;       /* community name (usually domain name) */
  gboolean full;          /* if FALSE, following fields non-existant */
  gchar *desc;            /* implementation defined description */
  guint ip_addr;          /* ip addr of the login */
  gchar *server_id;       /* unique ID of login's server */
};

gsize mwLoginInfo_buflen(struct mwLoginInfo *info);

int mwLoginInfo_put(gchar **b, gsize *n, struct mwLoginInfo *info);

int mwLoginInfo_get(gchar **b, gsize *n, struct mwLoginInfo *info);

void mwLoginInfo_clear(struct mwLoginInfo *info);

void mwLoginInfo_clone(struct mwLoginInfo *to, struct mwLoginInfo *from);


/* 8.2.2 Private Info Block */

struct mwUserItem {
  gboolean full;  /* if FALSE, don't include name */
  gchar *id;      /* user id */
  gchar *name;    /* user name */
};

gsize mwUserItem_buflen(struct mwUserItem *user);

int mwUserItem_put(gchar **b, gsize *n, struct mwUserItem *user);

int mwUserItem_get(gchar **b, gsize *n, struct mwUserItem *user);

void mwUserItem_clear(struct mwUserItem *user);


struct mwPrivacyInfo {
  guint reserved;            /* reserved for internal use */
  gboolean deny;             /* deny (true) or allow (false) users */
  guint count;               /* count of following users list */
  struct mwUserItem *users;  /* the users list */
};

gsize mwPrivacyInfo_buflen(struct mwPrivacyInfo *info);

int mwPrivacyInfo_put(gchar **b, gsize *n, struct mwPrivacyInfo *info);

int mwPrivacyInfo_get(gchar **b, gsize *n, struct mwPrivacyInfo *info);

void mwPrivacyInfo_clear(struct mwPrivacyInfo *info);


/* 8.3.5 User Status Types */

enum mwStatusType {
  mwStatus_ACTIVE  = 0x0020,
  mwStatus_IDLE    = 0x0040,
  mwStatus_AWAY    = 0x0060,
  mwStatus_BUSY    = 0x0080
};


/* 8.2.3 User Status Block */

struct mwUserStatus {
  enum mwStatusType status;  /* status of user (see 8.3.5) */
  guint time;                /* last status change time in seconds */
  gchar *desc;               /* status description */
};

gsize mwUserStatus_buflen(struct mwUserStatus *stat);

int mwUserStatus_put(gchar **b, gsize *n, struct mwUserStatus *stat);

int mwUserStatus_get(gchar **b, gsize *n, struct mwUserStatus *stat);

void mwUserStatus_clear(struct mwUserStatus *stat);

void mwUserStatus_clone(struct mwUserStatus *to, struct mwUserStatus *from);


/* 8.2.4 ID Block */

struct mwIdBlock {
  gchar *user;       /* user id (login id or empty for some services) */
  gchar *community;  /* community name (empty for same community) */
};

gsize mwIdBlock_buflen(struct mwIdBlock *id);

int mwIdBlock_put(gchar **b, gsize *n, struct mwIdBlock *id);

int mwIdBlock_get(gchar **b, gsize *n, struct mwIdBlock *id);

void mwIdBlock_clear(struct mwIdBlock *id);

void mwIdBlock_clone(struct mwIdBlock *to, struct mwIdBlock *from);

int mwIdBlock_equal(struct mwIdBlock *a, struct mwIdBlock *b);


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

gsize mwEncryptBlock_buflen(struct mwEncryptBlock *eb);

int mwEncryptBlock_put(gchar **b, gsize *n, struct mwEncryptBlock *eb);

int mwEncryptBlock_get(gchar **b, gsize *n, struct mwEncryptBlock *eb);

void mwEncryptBlock_clear(struct mwEncryptBlock *enc);

void mwEncryptBlock_clone(struct mwEncryptBlock *to,
			  struct mwEncryptBlock *from);


/* 8.3.8.2 Awareness Presence Types */

enum mwAwareType {
  mwAware_USER  = 0x0002
};


/* 8.4.2 Awareness Messages */
/* 8.4.2.1 Awareness ID Block */

struct mwAwareIdBlock {
  enum mwAwareType type;
  gchar *user;
  gchar *community;
};

gsize mwAwareIdBlock_buflen(struct mwAwareIdBlock *idb);

int mwAwareIdBlock_put(gchar **b, gsize *n, struct mwAwareIdBlock *idb);

int mwAwareIdBlock_get(gchar **b, gsize *n, struct mwAwareIdBlock *idb);

void mwAwareIdBlock_clear(struct mwAwareIdBlock *idb);


/* 8.4.2.4 Snapshot */

struct mwSnapshotAwareIdBlock {
  struct mwAwareIdBlock id;
  gboolean online;
  gchar *alt_id;
  struct mwUserStatus status;
  gchar *wtf;  /* wtf is this? */
};

int mwSnapshotAwareIdBlock_get(gchar **b, gsize *n,
			       struct mwSnapshotAwareIdBlock *idb);

void mwSnapshotAwareIdBlock_clear(struct mwSnapshotAwareIdBlock *idb);


/* 8.3.1.5 Resolve error codes */

enum mwResultCode {
  mwResult_SUCCESS     = 0x00000000,
  mwResult_PARTIAL     = 0x00010000,
  mwResult_MULTIPLE    = 0x80020000,
  mwResult_BAD_FORMAT  = 0x80030000
};


/* 8.4.4.2 Resolve Response */

struct mwResolveMatch {
  gchar *id;
  gchar *name;
  gchar *desc;
};


struct mwResolveResult {
  enum mwResultCode code;
  gchar *name;
  guint count;
  struct mwResolveMatch *matches;
};


#endif
