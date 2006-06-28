/*
  Meanwhile - Unofficial Lotus Sametime Community Client Library
  Copyright (C) 2004  Christopher (siege) O'Brien
  
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.
  
  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.
  
  You should have received a copy of the GNU Library General Public
  License along with this library; if not, write to the Free
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/


#include <glib.h>
#include <string.h>

#include "mw_common.h"
#include "mw_config.h"


/** @todo the *_get functions should make sure to clear their
    structures in the event of failure, to prevent memory leaks */


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


struct mw_put_buffer {
  guchar *buf;  /**< head of buffer */
  gsize len;    /**< length of buffer */

  guchar *ptr;  /**< offset to first unused byte */
  gsize rem;    /**< count of unused bytes remaining */
};


struct mw_get_buffer {
  guchar *buf;  /**< head of buffer */
  gsize len;    /**< length of buffer */

  guchar *ptr;  /**< offset to first unused byte */
  gsize rem;    /**< count of unused bytes remaining */

  gboolean wrap;   /**< TRUE to indicate buf shouldn't be freed */
  gboolean error;  /**< TRUE to indicate an error */
};


#define BUFFER_USED(buffer) \
  ((buffer)->len - (buffer)->rem)


/** ensure that there's at least enough space remaining in the put
    buffer to fit needed. */
static void ensure_buffer(MwPutBuffer *b, gsize needed) {
  if(b->rem < needed) {
    gsize len = b->len, use = BUFFER_USED(b);
    guchar *buf;

    /* newly created buffers are empty until written to, and then they
       have 1024 available */
    if(! len) len = MW_DEFAULT_BUFLEN;

    /* increase len until it's large enough to fit needed */
    while( (len - use) < needed )
      len += MW_DEFAULT_BUFLEN;

    /* create the new buffer. if there was anything in the old buffer,
       copy it into the new buffer and free the old copy */
    buf = g_malloc(len);
    if(b->buf) {
      memcpy(buf, b->buf, use);
      g_free(b->buf);
    }

    /* put the new buffer into b */
    b->buf = buf;
    b->len = len;
    b->ptr = buf + use;
    b->rem = len - use;
  }
}


/** determine if there are at least needed bytes available in the
    buffer. sets the error flag if there's not at least needed bytes
    left in the buffer

    @returns true if there's enough data, false if not */
static gboolean check_buffer(MwGetBuffer *b, gsize needed) {
  if(! b->error)  b->error = (b->rem < needed);
  return ! b->error;
}


MwPutBuffer *MwPutBuffer_new() {
  return g_new0(MwPutBuffer, 1);
}


void MwPutBuffer_write(MwPutBuffer *b, gconstpointer data, gsize len) {
  g_return_if_fail(b != NULL);
  g_return_if_fail(data != NULL);

  if(! len) return;

  ensure_buffer(b, len);
  memcpy(b->ptr, data, len);
  b->ptr += len;
  b->rem -= len;
}


void MwPutBuffer_free(MwPutBuffer *b, MwOpaque *o) {
  g_return_if_fail(b != NULL);

  if(o) {
    o->len = BUFFER_USED(b);
    o->data = b->buf;

  } else {
    g_free(b->buf);
  }

  g_free(b);
}


MwGetBuffer *MwGetBuffer_new(const MwOpaque *o) {
  MwGetBuffer *b = g_new0(MwGetBuffer, 1);

  if(o && o->len) {
    b->buf = b->ptr = g_memdup(o->data, o->len);
    b->len = b->rem = o->len;
  }

  return b;
}


MwGetBuffer *MwGetBuffer_wrap(const MwOpaque *o) {
  MwGetBuffer *b = g_new0(MwGetBuffer, 1);

  if(o && o->len) {
    b->buf = b->ptr = o->data;
    b->len = b->rem = o->len;
  }
  b->wrap = TRUE;

  return b;
}


gsize MwGetBuffer_read(MwGetBuffer *b, gpointer data, gsize len) {
  g_return_val_if_fail(b != NULL, 0);
  g_return_val_if_fail(data != NULL, 0);

  if(b->error) return 0;
  if(! len) return 0;

  if(b->rem < len)
    len = b->rem;

  memcpy(data, b->ptr, len);
  b->ptr += len;
  b->rem -= len;

  return len;
}


gsize MwGetBuffer_skip(MwGetBuffer *b, gsize len) {
  g_return_val_if_fail(b != NULL, 0);

  if(b->error) return 0;
  if(! len) return 0;

  if(b->rem < len)
    len = b->rem;

  b->ptr += len;
  b->rem -= len;

  return len;
}


void MwGetBuffer_reset(MwGetBuffer *b) {
  g_return_if_fail(b != NULL);

  b->rem = b->len;
  b->ptr = b->buf;
  b->error = FALSE;
}


gsize MwGetBuffer_remaining(MwGetBuffer *b) {
  g_return_val_if_fail(b != NULL, 0);
  return b->rem;
}


gboolean MwGetBuffer_error(MwGetBuffer *b) {
  g_return_val_if_fail(b != NULL, FALSE);
  return b->error;
}


void MwGetBuffer_free(MwGetBuffer *b) {
  if(! b) return;
  if(! b->wrap) g_free(b->buf);
  g_free(b);
}


#define mw_uint16_len  2


void mw_uint16_put(MwPutBuffer *b, guint16 val) {
  g_return_if_fail(b != NULL);

  ensure_buffer(b, mw_uint16_len);
  MW16_PUT(b->ptr, val);
  b->rem -= mw_uint16_len;
}


void mw_uint16_get(MwGetBuffer *b, guint16 *val) {
  g_return_if_fail(b != NULL);
  g_return_if_fail(val != NULL);

  if(b->error) return;
  g_return_if_fail(check_buffer(b, mw_uint16_len));

  MW16_GET(b->ptr, *val);
  b->rem -= mw_uint16_len;
}


guint16 mw_uint16_peek(MwGetBuffer *b) {
  guchar *buf;
  guint16 r = 0;
  
  g_return_val_if_fail(b != NULL, 0);

  buf = b->buf;

  if(b->rem >= mw_uint16_len)
    MW16_GET(buf, r);

  return r;
}


void mw_uint16_skip(MwGetBuffer *b) {
  g_return_if_fail(b != NULL);
  MwGetBuffer_skip(b, mw_uint16_len);
}


#define mw_uint32_len  4


void mw_uint32_put(MwPutBuffer *b, guint32 val) {
  g_return_if_fail(b != NULL);

  ensure_buffer(b, mw_uint32_len);
  MW32_PUT(b->ptr, val);
  b->rem -= mw_uint32_len;
}


void mw_uint32_get(MwGetBuffer *b, guint32 *val) {
  g_return_if_fail(b != NULL);
  g_return_if_fail(val != NULL);

  if(b->error) return;
  g_return_if_fail(check_buffer(b, mw_uint32_len));

  MW32_GET(b->ptr, *val);
  b->rem -= mw_uint32_len;
}


guint32 mw_uint32_peek(MwGetBuffer *b) {
  guchar *buf;
  guint32 r = 0;

  g_return_val_if_fail(b != NULL, 0);

  buf = b->buf;

  if(b->rem >= mw_uint32_len)
    MW32_GET(buf, r);

  return r;
}


void mw_uint32_skip(MwGetBuffer *b) {
  g_return_if_fail(b != NULL);
  MwGetBuffer_skip(b, mw_uint32_len);
}


#define mw_boolean_len  1


void mw_boolean_put(MwPutBuffer *b, gboolean val) {
  g_return_if_fail(b != NULL);

  ensure_buffer(b, mw_boolean_len);
  *(b->ptr) = !! val;
  b->ptr++;
  b->rem--;
}


void mw_boolean_get(MwGetBuffer *b, gboolean *val) {
  g_return_if_fail(b != NULL);
  g_return_if_fail(val != NULL);

  if(b->error) return;
  g_return_if_fail(check_buffer(b, mw_boolean_len));

  *val = !! *(b->ptr);
  b->ptr++;
  b->rem--;
}


gboolean mw_boolean_peek(MwGetBuffer *b) {
  gboolean v = FALSE;

  g_return_val_if_fail(b != NULL, FALSE);

  if(b->rem >= mw_boolean_len)
    v = !! *(b->ptr);

  return v;
}


void mw_boolean_skip(MwGetBuffer *b) {
  g_return_if_fail(b != NULL);
  MwGetBuffer_skip(b, mw_boolean_len);
}


void mw_str_put(MwPutBuffer *b, const char *val) {
  gsize len = 0;
  if(val) len = strlen(val);
  mw_str_putn(b, val, len);
}


void mw_str_putn(MwPutBuffer *b, const gchar *str, gsize len) {
  g_return_if_fail(b != NULL);

  mw_uint16_put(b, (guint16) len);

  if(len) {
    ensure_buffer(b, len);
    memcpy(b->ptr, str, len);
    b->ptr += len;
    b->rem -= len;
  }
}


void mw_str_get(MwGetBuffer *b, char **val) {
  guint16 len = 0;

  g_return_if_fail(b != NULL);
  g_return_if_fail(val != NULL);

  *val = NULL;

  if(b->error) return;
  mw_uint16_get(b, &len);

  g_return_if_fail(check_buffer(b, (gsize) len));

  if(len) {
    *val = g_malloc0(len + 1);
    memcpy(*val, b->ptr, len);
    b->ptr += len;
    b->rem -= len;
  }
}


void mw_str_skip(MwGetBuffer *b) {
  guint16 len = 0;

  g_return_if_fail(b != NULL);

  mw_uint16_get(b, &len);
  MwGetBuffer_skip(b, len);
}


gboolean mw_str_equal(const gchar *a, const gchar *b) {
  return (a == b) || (a && b && !strcmp(a, b));
}


/** simple macro for deep/shallow clones of a string */
#define mw_str_dup(str, copy) \
  ((copy)? (g_strdup((str))): (str))


void MwOpaque_put(MwPutBuffer *b, const MwOpaque *o) {
  gsize len;

  g_return_if_fail(b != NULL);

  if(! o) {
    /* treat a null opaque the same as an empty one */
    mw_uint32_put(b, 0x00);
    return;
  }

  len = o->len;
  if(len) {
    g_return_if_fail(o->data != NULL);
  }

  mw_uint32_put(b, (guint32) len);

  if(len) {
    ensure_buffer(b, len);
    memcpy(b->ptr, o->data, len);
    b->ptr += len;
    b->rem -= len;
  }
}


void MwOpaque_get(MwGetBuffer *b, MwOpaque *o) {
  g_return_if_fail(b != NULL);
  g_return_if_fail(o != NULL);

  MwOpaque_steal(b, o);
  o->data = g_memdup(o->data, o->len);
}


void MwOpaque_steal(MwGetBuffer *b, MwOpaque *o) {
  guint32 tmp = 0;

  g_return_if_fail(b != NULL);
  g_return_if_fail(o != NULL);

  o->len = 0;
  o->data = NULL;
  
  if(b->error) return;

  mw_uint32_get(b, &tmp);
  g_return_if_fail(check_buffer(b, (gsize) tmp));

  if(tmp) {
    o->len = (gsize) tmp;
    o->data = b->ptr;

    b->ptr += tmp;
    b->rem -= tmp;
  }
}


void MwOpaque_skip(MwGetBuffer *b) {
  guint32 len = 0;

  g_return_if_fail(b != NULL);

  mw_uint32_get(b, &len);
  MwGetBuffer_skip(b, len);
}


void MwOpaque_clear(MwOpaque *o) {
  if(! o) return;
  g_free(o->data);
  o->data = NULL;
  o->len = 0;
}


void MwOpaque_free(MwOpaque *o) {
  if(! o) return;
  g_free(o->data);
  g_free(o);
}


void MwOpaque_clone(MwOpaque *to, const MwOpaque *from) {
  g_return_if_fail(to != NULL);

  to->len = 0;
  to->data = NULL;

  if(from) {
    to->len = from->len;
    if(to->len)
      to->data = g_memdup(from->data, to->len);
  }
}


MwOpaque *MwOpaque_copy(const MwOpaque *o) {
  MwOpaque *ret;

  if(! o) return NULL;

  ret = g_new(MwOpaque, 1);
  MwOpaque_clone(ret, o);
  return ret;
}


GType MwOpaque_getType() {
  static GType type = 0;

  if(type == 0) {
    type = g_boxed_type_register_static("MwOpaqueType",
					(GBoxedCopyFunc) MwOpaque_copy,
					(GBoxedFreeFunc) MwOpaque_free);
  }

  return type;
}


void MwIdentity_put(MwPutBuffer *b, const MwIdentity *id) {
  g_return_if_fail(b != NULL);
  g_return_if_fail(id != NULL);

  mw_str_put(b, id->user);
  mw_str_put(b, id->community);
}


void MwIdentity_get(MwGetBuffer *b, MwIdentity *id) {
  g_return_if_fail(b != NULL);
  g_return_if_fail(id != NULL);

  id->user = NULL;
  id->community = NULL;
  
  if(b->error) return;

  mw_str_get(b, &id->user);
  mw_str_get(b, &id->community);
}


void MwIdentity_clear(MwIdentity *id, gboolean deep) {
  if(! id) return;

  if(deep) {
    g_free(id->user);
    g_free(id->community);
  }

  id->user = NULL;
  id->community = NULL;
}


void MwIdentity_free(MwIdentity *id, gboolean deep) {
  if(! id) return;
  MwIdentity_clear(id, deep);
  g_free(id);
}


void MwIdentity_clone(MwIdentity *to, const MwIdentity *from, gboolean deep) {
  g_return_if_fail(to != NULL);
  g_return_if_fail(from != NULL);

  to->user = mw_str_dup(from->user, deep);
  to->community = mw_str_dup(from->community, deep);
}


MwIdentity *MwIdentity_copy(const MwIdentity *orig) {
  MwIdentity *id;

  if(! orig) return NULL;

  id = g_new(MwIdentity, 1);
  MwIdentity_clone(id, orig, TRUE);
  return id;
}


guint MwIdentity_hash(const MwIdentity *idb) {
  return (idb)? g_str_hash(idb->user): 0;
}


gboolean MwIdentity_equal(const MwIdentity *a, const MwIdentity *b) {

  return ( (a == b) ||
	   (a && b &&
	    (mw_str_equal(a->user, b->user)) &&
	    (mw_str_equal(a->community, b->community))) );
}



GType MwIdentity_getType() {
  static GType type = 0;

  if(type == 0) {
    type = g_boxed_type_register_static("MwIdentityType",
					(GBoxedCopyFunc) MwIdentity_copy,
					(GBoxedFreeFunc) MwIdentity_free);
  }

  return type;
}


void MwLogin_put(MwPutBuffer *b, const MwLogin *login) {
  g_return_if_fail(b != NULL);
  g_return_if_fail(login != NULL);

  mw_str_put(b, login->login_id);
  mw_uint16_put(b, login->client);
  mw_str_put(b, login->id.user);
  mw_str_put(b, login->name);
  mw_str_put(b, login->id.community);
  mw_boolean_put(b, login->full);

  if(login->full) {
    mw_str_put(b, login->desc);
    mw_uint32_put(b, login->ip_addr);
    mw_str_put(b, login->server_id);
  }
}


void MwLogin_get(MwGetBuffer *b, MwLogin *login) {
  g_return_if_fail(b != NULL);
  g_return_if_fail(login != NULL);

  login->id.user = NULL;
  login->id.community = NULL;
  login->name = NULL;
  login->login_id = NULL;
  login->client = 0;
  login->full = FALSE;
  login->desc = NULL;
  login->ip_addr = 0;
  login->server_id = NULL;

  if(b->error) return;

  mw_str_get(b, &login->login_id);
  mw_uint16_get(b, &login->client);
  mw_str_get(b, &login->id.user);
  mw_str_get(b, &login->name);
  mw_str_get(b, &login->id.community);
  mw_boolean_get(b, &login->full);
  
  if(login->full) {
    mw_str_get(b, &login->desc);
    mw_uint32_get(b, &login->ip_addr);
    mw_str_get(b, &login->server_id);
  }
}


void MwLogin_clear(MwLogin *login, gboolean deep) {
  if(! login) return;

  MwIdentity_clear(&login->id, deep);

  if(deep) {
    g_free(login->name);
    g_free(login->login_id);
    g_free(login->desc);
    g_free(login->server_id);
  }

  login->name = NULL;
  login->login_id = NULL;
  login->client = 0;
  login->desc = NULL;
  login->ip_addr = 0;
  login->server_id = NULL;
}


void MwLogin_clone(MwLogin *to, const MwLogin *from, gboolean deep) {
  g_return_if_fail(to != NULL);
  g_return_if_fail(from != NULL);

  MwIdentity_clone(&to->id, &from->id, deep);

  to->name = mw_str_dup(from->name, deep);

  to->login_id= mw_str_dup(from->login_id, deep);
  to->client = from->client;

  if( (to->full = from->full) ) {
    to->desc = mw_str_dup(from->desc, deep);
    to->ip_addr = from->ip_addr;
    to->server_id = mw_str_dup(from->server_id, deep);
  }
}


void MwPrivacy_put(MwPutBuffer *b, const MwPrivacy *info) {
  guint32 c;

  g_return_if_fail(b != NULL);
  g_return_if_fail(info != NULL);

  mw_boolean_put(b, info->deny);
  mw_uint32_put(b, info->count);

  for(c = info->count; c--; ) {
    mw_boolean_put(b, FALSE);
    MwIdentity_put(b, info->users + c);
  }
}


void MwPrivacy_get(MwGetBuffer *b, MwPrivacy *info) {
  g_return_if_fail(b != NULL);
  g_return_if_fail(info != NULL);

  info->deny = FALSE;
  info->count = 0;
  info->users = NULL;

  if(b->error) return;

  mw_boolean_get(b, &info->deny);
  mw_uint32_get(b, &info->count);

  if(info->count) {
    guint32 c = info->count;
    info->users = g_new0(MwIdentity, c);

    while(c--) {
      gboolean f = FALSE;
      mw_boolean_get(b, &f);
      MwIdentity_get(b, info->users + c);
      if(f) mw_str_skip(b);
    }
  }
}


void MwPrivacy_clear(MwPrivacy *info, gboolean deep) {
  guint32 c;

  g_return_if_fail(info != NULL);

  for(c = info->count; c--; ) {
    MwIdentity_clear(info->users + c, deep);
  }

  if(deep) {
    g_free(info->users);
  }

  info->users = NULL;
  info->count = 0;
}


void MwPrivacy_clone(MwPrivacy *to, const MwPrivacy *from, gboolean deep) {
  guint32 c;

  g_return_if_fail(to != NULL);
  g_return_if_fail(from != NULL);

  to->deny = from->deny;
  c = to->count = from->count;

  to->users = g_new0(MwIdentity, c);

  while(c--) {
    MwIdentity_clone(to->users+c, from->users+c, deep);
  }
}


void MwStatus_put(MwPutBuffer *b, const MwStatus *stat) {
  g_return_if_fail(b != NULL);
  g_return_if_fail(stat != NULL);

  mw_uint16_put(b, stat->status);
  mw_uint32_put(b, stat->time);
  mw_str_put(b, stat->desc);
}


void MwStatus_get(MwGetBuffer *b, MwStatus *stat) {
  g_return_if_fail(b != NULL);
  g_return_if_fail(stat != NULL);

  stat->status = 0;
  stat->time = 0;
  stat->desc = NULL;

  if(b->error) return;

  mw_uint16_get(b, &stat->status);
  mw_uint32_get(b, &stat->time);
  mw_str_get(b, &stat->desc);
}


void MwStatus_clear(MwStatus *stat, gboolean deep) {
  if(! stat) return;

  if(deep) {
    g_free(stat->desc);
  }

  stat->status = 0;
  stat->time = 0;
  stat->desc = NULL;
}


void MwStatus_clone(MwStatus *to, const MwStatus *from, gboolean deep) {
  g_return_if_fail(to != NULL);
  g_return_if_fail(from != NULL);

  to->status = from->status;
  to->time = from->time;
  to->desc = mw_str_dup(from->desc, deep);
}


const gchar *mw_version() {
  return MW_VERSION "-" MW_VER_RELEASE;
}


guint mw_version_major() {
  return MW_VER_MAJOR;
}


guint mw_version_minor() {
  return MW_VER_MINOR;
}


guint mw_version_micro() {
  return MW_VER_MICRO;
}


const gchar *mw_version_release() {
  return MW_VER_RELEASE;
}


/* The end. */
