

#include <glib.h>
#include <string.h>

#include "common.h"


#define MW16_PUT(b, val) \
  *(b)++ = ((val) >> 0x08) & 0xff; \
  *(b)++ = (val) & 0xff;


#define MW16_GET(b, val) \
  val = (*(b)++ & 0xff) << 8; \
  val = val | (*(b)++ & 0xff);


#define MW32_PUT(b, val) \
  *(b)++ = ((val) >> 0x18) & 0xff; \
  *(b)++ = ((val) >> 0x10) & 0xff; \
  *(b)++ = ((val) >> 0x08) & 0xff; \
  *(b)++ = (val) & 0xff;


#define MW32_GET(b, val) \
  val = (*(b)++ & 0xff) << 0x18; \
  val = val | (*(b)++ & 0xff) << 0x10; \
  val = val | (*(b)++ & 0xff) << 0x08; \
  val = val | (*(b)++ & 0xff);


#define guint16_buflen(v) 2


int guint16_put(char **b, gsize *n, guint val) {
  if(2 > *n)
    return 2 - *n;

  MW16_PUT(*b, val);
  *n -= 2;
  return 0;
}


int guint16_get(char **b, gsize *n, guint *val) {
  if(2 > *n)
    return 2 - *n;
  
  MW16_GET(*b, *val);
  *n -= 2;
  return 0;
}


guint guint16_peek(const char *b, gsize n) {
  char *buf = (char *) b;
  gsize len = n;
  guint r = 0;

  guint16_get(&buf, &len, &r);

  return r;
}


#define guint32_buflen(v) 4


int guint32_put(char **b, gsize *n, guint val) {
  if(4 > *n)
    return 4 - *n;

  MW32_PUT(*b, val);
  *n -= 4;
  return 0;
}


int guint32_get(char **b, gsize *n, guint *val) {
  if(4 > *n)
    return 4 - *n;

  MW32_GET(*b, *val);
  *n -= 4;
  return 0;
}


guint guint32_peek(const char *b, gsize n) {
  char *buf = (char *) b;
  gsize len = n;
  guint r = 0;

  guint32_get(&buf, &len, &r);

  return r;
}


#define gboolean_buflen(v) 1


int gboolean_put(char **b, gsize *n, gboolean val) {
  if(*n < 1)
    return 1;

  *(*b)++ = !! val;
  (*n)--;
  return 0;
}


int gboolean_get(char **b, gsize *n, gboolean *val) {
  if(*n < 1)
    return 1;

  *val = !! *(*b)++;
  (*n)--;
  return 0;
}


gboolean gboolean_peek(const char *b, gsize n) {
  gboolean v = FALSE;
  gboolean_get((char **) &b, &n, &v);
  return v;
}


gsize mwString_buflen(const char *str) {
  /* add a two byte header, but no NULL termination */
  return (str)? 2 + strlen(str): 2;
}


int mwString_put(char **b, gsize *n, const char *val) {
  guint16 len = 0;

  if(val != NULL)
    len = strlen(val);

  if(guint16_put(b, n, len))
    return *n;

  if(len > 0) {
    if(len > *n)
      return len - *n;
    
    memcpy(*b, val, len);
    *b += len;
    *n -= len;
  }
  
  return 0;
}


gboolean mw_streq(const char *a, const char *b) {
  return (a == b) || (a && b && !strcmp(a, b));
}


int mwString_get(char **b, gsize *n, char **val) {
  guint len = 0;

  if(guint16_get(b, n, &len))
    return *n;

  *val = NULL;

  if(len > 0) {
    if(len > *n)
      return len - *n;
    
    *val = g_strndup(*b, len);
    *b += len;
    *n -= len;
  }

  return 0;
}


gsize mwOpaque_buflen(struct mwOpaque *o) {
  return 4 + o->len;
}


int mwOpaque_put(char **b, gsize *n, struct mwOpaque *o) {
  if(guint32_put(b, n, o->len))
    return *n - o->len;

  if(o->len > 0) {
    if(o->len > *n)
      return o->len - *n;
    
    memcpy(*b, o->data, o->len);
    *b += o->len;
    *n -= o->len;
  }

  return 0;
}


int mwOpaque_get(char **b, gsize *n, struct mwOpaque *o) {
  if(guint32_get(b, n, &o->len))
    return *n;

  o->data = NULL;

  if(o->len > 0) {
    if(o->len > *n)
      return o->len - *n;

    o->data = (char *) g_memdup(*b, o->len);
    *b += o->len;
    *n -= o->len;
  }

  return 0;
}


void mwOpaque_clear(struct mwOpaque *o) {
  g_free(o->data);
  memset(o, 0x00, sizeof(struct mwOpaque));
}


void mwOpaque_clone(struct mwOpaque *to, struct mwOpaque *from) {
  if( (to->len = from->len) ) {
    to->data = (char *) g_memdup(from->data, from->len);
  } else {
    to->data = NULL;
  }
}


/* 8.2 Common Structures */
/* 8.2.1 Login Info block */

gsize mwLoginInfo_buflen(struct mwLoginInfo *login) {
  gsize len = mwString_buflen(login->login_id)
    + 2
    + mwString_buflen(login->user_id)
    + mwString_buflen(login->user_name)
    + mwString_buflen(login->community)
    + 1;
  
  if(login->full) {
    len += mwString_buflen(login->desc);
    len += 4;
    len += mwString_buflen(login->server_id);
  }
  return len;
}


int mwLoginInfo_put(char **b, gsize *n, struct mwLoginInfo *login) {
  if( mwString_put(b, n, login->login_id) ||
      guint16_put(b, n, login->type) ||
      mwString_put(b, n, login->user_id) ||
      mwString_put(b, n, login->user_name) ||
      mwString_put(b, n, login->community) ||
      gboolean_put(b, n, login->full) )
    return 1;

  if(login->full) {
    if( mwString_put(b, n, login->desc) ||
	guint32_put(b, n, login->ip_addr) ||
	mwString_put(b, n, login->server_id) )
      return 1;
  }

  return 0;
}


int mwLoginInfo_get(char **b, gsize *n, struct mwLoginInfo *login) {
  if( mwString_get(b, n, &login->login_id) ||
      guint16_get(b, n,  &login->type) ||
      mwString_get(b, n, &login->user_id) ||
      mwString_get(b, n, &login->user_name) ||
      mwString_get(b, n, &login->community) ||
      gboolean_get(b, n, &login->full) )
    return 1;
  
  if(login->full) {
    if( mwString_get(b, n, &login->desc) ||
	guint32_get(b, n, &login->ip_addr) ||
	mwString_get(b, n, &login->server_id) )
      return 1;
  }

  return 0;
}


void mwLoginInfo_clear(struct mwLoginInfo *login) {
  g_free(login->login_id);
  g_free(login->user_id);
  g_free(login->user_name);
  g_free(login->community);

  g_free(login->desc);
  g_free(login->server_id);

  memset(login, 0x00, sizeof(struct mwLoginInfo));
}


void mwLoginInfo_clone(struct mwLoginInfo *to, struct mwLoginInfo *from) {
  to->login_id= g_strdup(from->login_id);
  to->type = from->type;
  to->user_id = g_strdup(from->user_id);
  to->user_name = g_strdup(from->user_name);
  to->community = g_strdup(from->community);

  if( (to->full = from->full) ) {
    to->desc = g_strdup(from->desc);
    to->ip_addr = from->ip_addr;
    to->server_id = g_strdup(from->server_id);
  }
}


/* 8.2.2 Private Info Block */

gsize mwUserItem_buflen(struct mwUserItem *user) {
  gsize len = 1 + mwString_buflen(user->id);
  if(user->full) len += mwString_buflen(user->name);
  return len;
}


int mwUserItem_put(char **b, gsize *n, struct mwUserItem *user) {
  if( gboolean_put(b, n, user->full) ||
      mwString_put(b, n, user->id) )
    return 1;
  
  if(user->full) {
    if( mwString_put(b, n, user->name) )
      return 1;
  }
  return 0;
}


int mwUserItem_get(char **b, gsize *n, struct mwUserItem *user) {
  if( gboolean_get(b, n, &user->full) ||
      mwString_get(b, n, &user->id) )
    return 1;

  if(user->full) {
    if( mwString_get(b, n, &user->name) )
      return 1;
  }
  return 0;
}


void mwUserItem_clear(struct mwUserItem *user) {
  g_free(user->id);
  g_free(user->name);
  memset(user, 0x00, sizeof(struct mwUserItem));
}


gsize mwPrivacyInfo_buflen(struct mwPrivacyInfo *info) {
  guint32 c = info->count;
  gsize ret = 7; /* guint16, gboolean, guint32 */

  while(c--)
    ret += mwUserItem_buflen(info->users + c);

  return ret;
}


int mwPrivacyInfo_put(char **b, gsize *n, struct mwPrivacyInfo *info) {
  guint32 c = info->count;

  if( guint16_put(b, n, info->reserved) ||
      gboolean_put(b, n, info->deny) ||
      guint32_put(b, n, c) )
    return -1;

  while(c--)
    if(mwUserItem_put(b, n, info->users + c))
      return -1;

  return 0;
}


int mwPrivacyInfo_get(char **b, gsize *n, struct mwPrivacyInfo *info) {
  if( guint16_get(b, n, &info->reserved) ||
      gboolean_get(b, n, &info->deny) ||
      guint32_get(b, n, &info->count) )
    return -1;

  if(info->count) {
    guint32 c = info->count;

    info->users = (struct mwUserItem *) g_new0(struct mwUserItem, c);
    while(c--)
      if(mwUserItem_get(b, n, info->users + c))
	return -1;
  }

  return 0;
}


void mwPrivacyInfo_clear(struct mwPrivacyInfo *info) {
  struct mwUserItem *u = info->users;
  guint32 c = info->count;

  while(c--) mwUserItem_clear(u + c);
  g_free(u);

  memset(info, 0x00, sizeof(struct mwUserItem));
}


/* 8.2.3 User Status Block */

gsize mwUserStatus_buflen(struct mwUserStatus *stat) {
  return 2 + 4 + mwString_buflen(stat->desc);
}


int mwUserStatus_put(char **b, gsize *n, struct mwUserStatus *stat) {
  if( guint16_put(b, n, stat->status) ||
      guint32_put(b, n, stat->time) ||
      mwString_put(b, n, stat->desc) )
    return 1;

  return 0;
}


int mwUserStatus_get(char **b, gsize *n, struct mwUserStatus *stat) {
  if( guint16_get(b, n, &stat->status) ||
      guint32_get(b, n, &stat->time) ||
      mwString_get(b, n, &stat->desc) )
    return 1;

  return 0;
}


void mwUserStatus_clear(struct mwUserStatus *stat) {
  g_free(stat->desc);
  memset(stat, 0x00, sizeof(struct mwUserStatus));
}


void mwUserStatus_clone(struct mwUserStatus *to, struct mwUserStatus *from) {
  to->status = from->status;
  to->time = from->time;
  to->desc = g_strdup(from->desc);
}


/* 8.2.4 ID Block */

gsize mwIdBlock_buflen(struct mwIdBlock *id) {
  return mwString_buflen(id->user) + mwString_buflen(id->community);
}


int mwIdBlock_put(char **b, gsize *n, struct mwIdBlock *id) {
  if( mwString_put(b, n, id->user) ||
      mwString_put(b, n, id->community) )
    return *n;

  return 0;
}


int mwIdBlock_get(char **b, gsize *n, struct mwIdBlock *id) {
  if( mwString_get(b, n, &id->user) ||
      mwString_get(b, n, &id->community) )
    return *n;

  return 0;
}


void mwIdBlock_clear(struct mwIdBlock *id) {
  g_free(id->user);
  g_free(id->community);
  id->user = NULL;
  id->community = NULL;
}


void mwIdBlock_clone(struct mwIdBlock *to, struct mwIdBlock *from) {
  to->user = g_strdup(from->user);
  to->community = g_strdup(from->community);
}


gboolean  mwIdBlock_equal(struct mwIdBlock *a, struct mwIdBlock *b) {
  gboolean ret = ( mw_streq(a->user, b->user) &&
		   mw_streq(a->community, b->community) );
  return ret;
}


/* 8.2.5 Encryption Block */

gsize mwEncryptBlock_buflen(struct mwEncryptBlock *eb) {
  return 2 + mwOpaque_buflen(&eb->opaque);
}


int mwEncryptBlock_put(char **b, gsize *n, struct mwEncryptBlock *eb) {
  if( guint16_put(b, n, eb->type) ||
      mwOpaque_put(b, n, &eb->opaque) )
    return *n;

  return 0;
}


int mwEncryptBlock_get(char **b, gsize *n, struct mwEncryptBlock *eb) {
  if( guint16_get(b, n, &eb->type) ||
      mwOpaque_get(b, n, &eb->opaque) )
    return *n;

  return 0;
}


void mwEncryptBlock_clear(struct mwEncryptBlock *eb) {
  eb->type = 0x00;
  mwOpaque_clear(&eb->opaque);
}


void mwEncryptBlock_clone(struct mwEncryptBlock *to,
			  struct mwEncryptBlock *from) {
  to->type = from->type;
  mwOpaque_clone(&to->opaque, &from->opaque);
}


/* 8.4.2.1 Awareness ID Block */

gsize mwAwareIdBlock_buflen(struct mwAwareIdBlock *idb) {
  return 2 + mwString_buflen(idb->user) + mwString_buflen(idb->community);
}


int mwAwareIdBlock_put(char **b, gsize *n, struct mwAwareIdBlock *idb) {
  if( guint16_put(b, n, idb->type) ||
      mwString_put(b, n, idb->user) ||
      mwString_put(b, n, idb->community) )
    return *n;

  return 0; 
}


int mwAwareIdBlock_get(char **b, gsize *n, struct mwAwareIdBlock *idb) {
  if( guint16_get(b, n, &idb->type) ||
      mwString_get(b, n, &idb->user) ||
      mwString_get(b, n, &idb->community) )
    return *n;

  return 0;
}


void mwAwareIdBlock_clone(struct mwAwareIdBlock *to,
			  struct mwAwareIdBlock *from) {
  to->type = from->type;
  to->user = g_strdup(from->user);
  to->community = g_strdup(from->community);
}


void mwAwareIdBlock_clear(struct mwAwareIdBlock *idb) {
  g_free(idb->user);
  g_free(idb->community);
  memset(idb, 0x00, sizeof(struct mwAwareIdBlock));
}


gboolean mwAwareIdBlock_equal(struct mwAwareIdBlock *a,
			      struct mwAwareIdBlock *b) {
  
  gboolean ret = ( (a->type == b->type) &&
		   mw_streq(a->user, b->user) &&
		   mw_streq(a->community, b->community) );
  return ret;
}


/* 8.4.2.4 Snapshot */

int mwSnapshotAwareIdBlock_get(char **b, gsize *n,
			       struct mwSnapshotAwareIdBlock *idb) {

  guint junk;
  char *empty = NULL;
  int ret = 0;

  if( guint32_get(b, n, &junk) ||
      mwAwareIdBlock_get(b, n, &idb->id) ||
      mwString_get(b, n, &empty) ||
      gboolean_get(b, n, &idb->online) )
    ret = *n;

  g_free(empty);

  if((!ret) && idb->online) {
    if( mwString_get(b, n, &idb->alt_id) ||
	mwUserStatus_get(b, n, &idb->status) ||
	mwString_get(b, n, &idb->wtf) )
      ret = *n;
  }

  return ret;
}


void mwSnapshotAwareIdBlock_clone(struct mwSnapshotAwareIdBlock *to,
				  struct mwSnapshotAwareIdBlock *from) {

  mwAwareIdBlock_clone(&to->id, &from->id);
  if( (to->online = from->online) ) {
    to->alt_id = g_strdup(from->alt_id);
    mwUserStatus_clone(&to->status, &from->status);
    /* doesn't copy ::wtf */
  }
}


void mwSnapshotAwareIdBlock_clear(struct mwSnapshotAwareIdBlock *idb) {
  mwAwareIdBlock_clear(&idb->id);
  mwUserStatus_clear(&idb->status);
  g_free(idb->alt_id);
  g_free(idb->wtf);
  memset(idb, 0x00, sizeof(struct mwSnapshotAwareIdBlock));
}


