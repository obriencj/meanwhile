#ifndef _MW_COMMON_H
#define _MW_COMMON_H


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


/** @file mw_common.h

    Common data types and functions for handling those types.

    Functions in this file all fit into similar naming conventions of
    <code>TYPE_ACTION</code> as per the activity they perform. The
    following actions are available:

    <code>void TYPE_put(struct mwPutBuffer *b, TYPE *val)</code>
    - marshalls val onto the buffer b. The buffer will grow as necessary
    to fit all the data put into it. For guint16, guint32, and
    gboolean, <code>TYPE val</code> is used instead of <code>TYPE
    \*val</code>.

    <code>void TYPE_get(struct mwGetBuffer *b, TYPE *val)</code>
    - unmarshals val from the buffer b. Failure (due to lack of
    insufficient remaining buffer) is indicated in the buffer's error
    field. A call to a _get function with a buffer in an error state
    has to effect.

    <code>void TYPE_clear(TYPE *val)</code>
    - zeros and frees internal members of val, but does not free val
    itself. Needs to be called before free-ing any complex types which
    have been unmarshalled from a TYPE_get or populated from a
    TYPE_clone call to prevent memory leaks.

    <code>void TYPE_clone(TYPE *to, TYPE *from)</code>
    - copies/clones members of from into to. May result in memory
    allocation for some types. Note that to is not cleared
    before-hand, it must already be in a pristine condition.

    <code>gboolean TYPE_equal(TYPE *y, TYPE *z)</code>
    - simple equality test.
*/


#include <glib.h>
#include <glib-object.h>


G_BEGIN_DECLS


typedef struct mw_put_buffer MwPutBuffer;


/** @struct mwPutBuffer
    buffer to be written to */
struct mw_put_buffer;


typedef struct mw_get_buffer MwGetBuffer;


/** @struct mwGetBuffer
    buffer to be read from */
struct mw_get_buffer;


typedef struct mw_opaque MwOpaque;


/** A length of binary data, not null-terminated. */
struct mw_opaque {
  gsize len;     /**< length of data. */
  guchar *data;  /**< data, normally with no NULL termination */
};


typedef struct mw_identity MwIdentity;


struct mw_identity {
  gchar *user;
  gchar *community;
};


/** The type of login. Normally meaning the type of client code being
    used to login with.

    If you know of any additional client identifiers, please add them
    below or submit an RFE to the meanwhile tracker.
*/
enum mw_client_type {
  mw_client_LIB       = 0x1000,  /**< official Lotus binary library */
  mw_client_JAVA_WEB  = 0x1001,  /**< official Lotus Java applet */
  mw_client_BINARY    = 0x1002,  /**< official Lotus binary application */
  mw_client_JAVA_APP  = 0x1003,  /**< official Lotus Java application */
  mw_client_LINKS     = 0x100a,  /**< official Sametime Links toolkit */

  /* now we're getting crazy */
  mw_client_NOTES_6_5        = 0x1200,
  mw_client_NOTES_6_5_3      = 0x1203,
  mw_client_NOTES_7_0_beta   = 0x1210,
  mw_client_NOTES_7_0        = 0x1214,
  mw_client_ICT              = 0x1300,
  mw_client_ICT_1_7_8_2      = 0x1302,
  mw_client_ICT_SIP          = 0x1303,
  mw_client_NOTESBUDDY_4_14  = 0x1400,  /**< 0xff00 mask? */
  mw_client_NOTESBUDDY_4_15  = 0x1405,
  mw_client_NOTESBUDDY_4_16  = 0x1406,
  mw_client_SANITY           = 0x1600,
  mw_client_ST_PERL          = 0x1625,
  mw_client_PMR_ALERT        = 0x1650,
  mw_client_TRILLIAN         = 0x16aa,  /**< http://sf.net/st-plugin/ */
  mw_client_TRILLIAN_IBM     = 0x16bb,
  mw_client_MEANWHILE        = 0x1700,  /**< Meanwhile library */
};


typedef struct mw_login MwLogin;


struct mw_login {
  MwIdentity id;     /**< identity */
  gchar *name;       /**< user's full name */

  gchar *login_id;   /**< community-unique ID of the login */
  guint16 client;    /**< @see mw_client_type */

  gboolean full;     /**< if FALSE, following fields non-existant */
  gchar *desc;       /**< implementation defined description */
  guint32 ip_addr;   /**< ip addr of the login */
  gchar *server_id;  /**< unique ID of login's server */
};


typedef struct mw_privacy MwPrivacy;


struct mw_privacy {
  gboolean deny;      /**< deny (true) or allow (false) users */
  guint32 count;      /**< count of users */
  MwIdentity *users;  /**< the users list */
};


enum mw_status_type {
  mw_status_ACTIVE  = 0x0020,
  mw_status_IDLE    = 0x0040,
  mw_status_AWAY    = 0x0060,
  mw_Status_BUSY    = 0x0080,
};


typedef struct mw_status MwStatus;


struct mw_status {
  guint16 status;  /**< @see mw_status_type */
  guint32 time;    /**< last status change time in seconds */
  gchar *desc;     /**< status description */
};


/** @name Reading and Writing Values for Output

    The commonly used structures specified in this header are normally
    utilized in the construction of an outgoing Sametime message, or
    are constructed from an incoming Sametime message. The MwPutBuffer
    and MwGetBuffer types and their assorted functions provide this
    functionality.
    
    Values which are intended to be written to a message can be fed into
    a MwPutBuffer.

    Values can be read from message data via a MwGetBuffer.
 */
/*@{*/


/** allocate a new empty buffer */
MwPutBuffer *MwPutBuffer_new();


/** write raw data to the put buffer */
void MwPutBuffer_write(MwPutBuffer *b, gconstpointer data, gsize len);


/** destroy @p b, moving its data into an opaque. If @p is NULL, the
    internal buffer data will be free'd and lost. */
void MwPutBuffer_free(MwPutBuffer *b, MwOpaque *o);


/** allocate a new buffer with a copy of the given data */
MwGetBuffer *MwGetBuffer_new(const MwOpaque *data);


/** allocate a new buffer backed by the given data. Calling
    MwGetBuffer_free will NOT result in the underlying data being
    free'd */
MwGetBuffer *MwGetBuffer_wrap(const MwOpaque *data);


/** read len bytes of raw data from the get buffer into mem. If len is
    greater than the count of bytes remaining in the buffer, the
    buffer's error flag will NOT be set.

    @returns count of bytes successfully copied to mem */
gsize MwGetBuffer_read(MwGetBuffer *b, gpointer mem, gsize len);


/** skip len bytes in the get buffer. If len is greater than the count
    of bytes remaining in the buffer, the buffer's error flag will NOT
    be set.

    @returns count of bytes successfully skipped */
gsize MwGetBuffer_skip(MwGetBuffer *b, gsize len);


/** reset the buffer to the very beginning. Also clears the buffer's
    error flag. */
void MwGetBuffer_reset(MwGetBuffer *b);


/** count of remaining available bytes */
gsize MwGetBuffer_remaining(MwGetBuffer *b);


/** TRUE if an error occurred while reading a basic type from this
    buffer */
gboolean MwGetBuffer_error(MwGetBuffer *b);


/** destroy the buffer */
void MwGetBuffer_free(MwGetBuffer *b);


/*@}*/


/** @name Basic Data Types
    The basic types are combined to construct the compound types.
 */
/*@{*/


void mw_uint16_put(MwPutBuffer *b, guint16 val);


void mw_uint16_get(MwGetBuffer *b, guint16 *val);


guint16 mw_uint16_peek(MwGetBuffer *b);


void mw_uint16_skip(MwGetBuffer *b);


void mw_uint32_put(MwPutBuffer *b, guint32 val);


void mw_uint32_get(MwGetBuffer *b, guint32 *val);


guint32 mw_uint32_peek(MwGetBuffer *b);


void mw_uint32_skip(MwGetBuffer *b);


void mw_boolean_put(MwPutBuffer *b, gboolean val);


void mw_boolean_get(MwGetBuffer *b, gboolean *val);


gboolean mw_boolean_peek(MwGetBuffer *b);


void mw_boolean_skip(MwGetBuffer *b);


void mw_str_put(MwPutBuffer *b, const gchar *str);


void mw_str_putn(MwPutBuffer *b, const gchar *str, gsize len);


void mw_str_get(MwGetBuffer *b, gchar **str);


void mw_str_skip(MwGetBuffer *b);


/** tests for string equality, permitting NULL values */
gboolean mw_str_equal(const gchar *stra, const gchar *strb);


void MwOpaque_put(MwPutBuffer *b, const MwOpaque *o);


/** copies opaque data from @p b into &p o, which will need to be
    cleaned up later via MwOpaque_clear */
void MwOpaque_get(MwGetBuffer *b, MwOpaque *o);


/** like MwOpaque_get, but rather than duplicating the data from &p b
    into &p o, the data field of &p o is set to point to the same
    chunk of memory. This is useful where the opaque needs to only be
    used briefly, and would normally be cleaned immediately
    afterwards, as this opaque doesn't need to be cleaned later.
*/
void MwOpaque_steal(MwGetBuffer *b, MwOpaque *o);


void MwOpaque_skip(MwGetBuffer *b);


/** free the contents of @p o and set the buffer pointer to NULL and
    the length to zero */
void MwOpaque_clear(MwOpaque *o);


/** call MwOpaque_clear, and then g_free @p o */
void MwOpaque_free(MwOpaque *o);


/** duplicate the internal buffer from one opaque to another */
void MwOpaque_clone(MwOpaque *to, const MwOpaque *from);


/** allocate a new opaque and duplicate the internal buffer from @p o */
MwOpaque *MwOpaque_copy(const MwOpaque *o);


/** boxed type for MwOpaque */
GType MwOpaque_getType();


#define MW_TYPE_OPAQUE (MwOpaque_getType())


/*@}*/


/** @name Compound Data Types */
/*@{*/


void MwIdentity_put(MwPutBuffer *b, const MwIdentity *id);


void MwIdentity_get(MwGetBuffer *b, MwIdentity *id);


void MwIdentity_clear(MwIdentity *id, gboolean deep);


void MwIdentity_free(MwIdentity *id, gboolean deep);


void MwIdentity_clone(MwIdentity *to, const MwIdentity *from, gboolean deep);


MwIdentity *MwIdentity_copy(const MwIdentity *orig);


guint MwIdentity_hash(const MwIdentity *id);


gboolean MwIdentity_equal(const MwIdentity *ida, const MwIdentity *idb);


/** create a new GHashTable that can use a MwIdentity* for a key */
#define MwIdentity_newTable(keyfree, valfree)			\
  (g_hash_table_new_full(MwIdentity_hash, MwIdentity_equal,	\
			 (keyfree), (valfree)))


/** boxed type for MwIdentity */
GType MwIdentity_getType();


#define MW_TYPE_IDENTITY (MwIdentity_getType())


void MwLogin_put(MwPutBuffer *b, const MwLogin *login);


void MwLogin_get(MwGetBuffer *b, MwLogin *login);


void MwLogin_clear(MwLogin *login, gboolean deep);


void MwLogin_clone(MwLogin *to, const MwLogin *from, gboolean deep);


void MwPrivacy_put(MwPutBuffer *b, const MwPrivacy *info);


void MwPrivacy_get(MwGetBuffer *b, MwPrivacy *info);


void MwPrivacy_clear(MwPrivacy *info, gboolean deep);


void MwPrivacy_clone(MwPrivacy *to, const MwPrivacy *from, gboolean deep);


void MwStatus_put(MwPutBuffer *b, const MwStatus *stat);


void MwStatus_get(MwGetBuffer *b, MwStatus *stat);


void MwStatus_clear(MwStatus *stat, gboolean deep);


void MwStatus_clone(MwStatus *to, const MwStatus *from, gboolean deep);


/*@}*/


/** @section Library Version Information

    The following functions can be used to get the version information
    for the in-use version of the Meanwhile library.
*/
/* @{ */


const gchar *mw_version();


guint mw_version_major();


guint mw_version_minor();


guint mw_version_micro();


const gchar *mw_version_release();


/* @} */


G_END_DECLS


#endif /* _MW_COMMON_H */
