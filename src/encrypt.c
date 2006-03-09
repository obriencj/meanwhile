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


#include "mw_channel.h"
#include "mw_common.h"
#include "mw_debug.h"
#include "mw_encrypt.h"
#include "mw_mpi.h"
#include "mw_object.h"


static GObjectClass *cipher_parent_class;
static GObjectClass *rc2_parent_class;
static GObjectClass *dh_rc2_parent_class;


/* used in MwRC2Cipher and MwDHRC2Cipher to override propertes in the
   MwCipher interface */
enum properties {
  property_channel = 1,
};


/* MwCipher interface method */
static void mw_cipher_offer(MwCipher *ci, MwOpaque *info) {
  g_info("Cipher offer");
}


/* MwCipher interface method */
static void mw_cipher_accepted(MwCipher *ci, const MwOpaque *info) {
  g_info("Cipher accepted");
}


/* MwCipher interface method */
static void mw_cipher_offered(MwCipher *ci, const MwOpaque *info) {
  g_info("Cipher offered");
}


/* MwCipher interface method */
static void mw_cipher_accept(MwCipher *ci, MwOpaque *info) {
  g_info("Cipher accept");
}


/* MwCipher interface method */
static void mw_cipher_encrypt(MwCipher *ci,
			      const MwOpaque *in, MwOpaque *out) {

  g_warning("Cipher does not support encrypting data");
}


/* MwCipher interface method */
static void mw_cipher_decrypt(MwCipher *ci,
			      const MwOpaque *in, MwOpaque *out) {

  g_warning("Cipher does not support decrypting data");
}


static guint16 mw_cipher_get_id() {
  return 0xffff;
}


static guint16 mw_cipher_get_policy() {
  return 0x0000;
}


static const gchar *mw_cipher_get_name() {
  return "Meanwhile Cipher Interface";
}


static const gchar *mw_cipher_get_desc() {
  return "Generic interface for all cipher implementations";
}


static void mw_set_property(GObject *object,
			    guint property_id, const GValue *value,
			    GParamSpec *pspec) {
  
  MwCipher *self = MW_CIPHER(object);
  
  switch(property_id) {
  case property_channel:
    mw_gobject_set_weak_pointer(g_value_get_object(value),
				(gpointer *) &self->channel);
    break;
  default:
    ;
  }
}


static void mw_get_property(GObject *object,
			    guint property_id, GValue *value,
			    GParamSpec *pspec) {
  
  MwCipher *self = MW_CIPHER(object);
  
  switch(property_id) {
  case property_channel:
    g_value_set_object(value, self->channel);
    break;
  default:
    ;
  }
}


static void mw_cipher_dispose(GObject *object) {
  MwCipher *self = MW_CIPHER(object);

  mw_gobject_clear_weak_pointer((gpointer *) &self->channel);

  cipher_parent_class->dispose(object);
}


void mw_cipher_init(gpointer gclass,
		    gpointer gclass_data) {

  GObjectClass *gobject_class = gclass;
  MwCipherClass *klass = gclass;

  gobject_class->set_property = mw_set_property;
  gobject_class->get_property = mw_get_property;
  gobject_class->dispose = mw_cipher_dispose;

  cipher_parent_class = g_type_class_peek_parent(gobject_class);

  mw_prop_obj(gclass, property_channel,
	      "channel", "get/set the MwChannel for this cipher",
	      MW_TYPE_CHANNEL, G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY);

  klass->get_id = mw_cipher_get_id;
  klass->get_policy = mw_cipher_get_policy;
  klass->get_name = mw_cipher_get_name;
  klass->get_desc = mw_cipher_get_desc;

  klass->offer = mw_cipher_offer;
  klass->accepted = mw_cipher_accepted;
  klass->offered = mw_cipher_offered;
  klass->accept = mw_cipher_accept;
  klass->encrypt = mw_cipher_encrypt;
  klass->decrypt = mw_cipher_decrypt;
}


static const GTypeInfo mw_cipher_info = {
  .class_size = sizeof(MwCipherClass),
  .base_init = NULL,
  .base_finalize = NULL,
  .class_init = mw_cipher_init,
  .class_finalize = NULL,
  .class_data = NULL,
  .instance_size = sizeof(MwCipher),
  .n_preallocs = 0,
  .instance_init = NULL,
  .value_table = NULL,
};


GType MwCipher_getType() {
  static GType type = 0;

  if(type == 0) {
    type = g_type_register_static(MW_TYPE_OBJECT, "MwCipherType",
				  &mw_cipher_info, 0);
  }

  return type;
}


guint16 MwCipherClass_getIdentifier(MwCipherClass *klass) {
  g_return_val_if_fail(klass != NULL, 0xffff);
  return klass->get_id();
}


guint16 MwCipherClass_getPolicy(MwCipherClass *klass) {
  g_return_val_if_fail(klass != NULL, 0x0000);
  return klass->get_policy();
}


const gchar *MwCipherClass_getName(MwCipherClass *klass) {
  g_return_val_if_fail(klass != NULL, NULL);
  return klass->get_name();
}


const gchar *MwCipherClass_getDescription(MwCipherClass *klass) {
  g_return_val_if_fail(klass != NULL, NULL);
  return klass->get_desc();
}


MwCipher *MwCipherClass_newInstance(MwCipherClass *klass, MwChannel *chan) {
  g_return_val_if_fail(klass != NULL, NULL);
  return g_object_new(G_TYPE_FROM_CLASS(klass), "channel", chan, NULL);
}


void MwCipher_offer(MwCipher *ci, MwOpaque *info) {
  void (*fn)(MwCipher *, MwOpaque *);

  g_return_if_fail(ci != NULL);
  g_return_if_fail(info != NULL);

  fn = MW_CIPHER_GET_CLASS(ci)->offer;
  fn(ci, info);
}


void MwCipher_accepted(MwCipher *ci, const MwOpaque *info) {
  void (*fn)(MwCipher *, const MwOpaque *);

  g_return_if_fail(ci != NULL);
  g_return_if_fail(info != NULL);

  fn = MW_CIPHER_GET_CLASS(ci)->accepted;
  fn(ci, info);
}


void MwCipher_offered(MwCipher *ci, const MwOpaque *info) {
  void (*fn)(MwCipher *, const MwOpaque *);

  g_return_if_fail(ci != NULL);
  g_return_if_fail(info != NULL);

  fn = MW_CIPHER_GET_CLASS(ci)->offered;
  fn(ci, info);
}


void MwCipher_accept(MwCipher *ci, MwOpaque *info) {
  void (*fn)(MwCipher *, MwOpaque *);

  g_return_if_fail(ci != NULL);
  g_return_if_fail(info != NULL);

  fn = MW_CIPHER_GET_CLASS(ci)->accept;
  fn(ci, info);
}


void MwCipher_encrypt(MwCipher *ci, const MwOpaque *in, MwOpaque *out) {
  MwCipherClass *klass;
  void (*fn)(MwCipher *, const MwOpaque *, MwOpaque *);

  g_return_if_fail(ci != NULL);
  g_return_if_fail(in != NULL);
  g_return_if_fail(out != NULL);

  klass = MW_CIPHER_GET_CLASS(ci);
  fn = klass->encrypt;
  fn(ci, in, out);
}


void MwCipher_decrypt(MwCipher *ci, const MwOpaque *in, MwOpaque *out) {
  MwCipherClass *klass;
  void (*fn)(MwCipher *, const MwOpaque *, MwOpaque *);

  g_return_if_fail(ci != NULL);
  g_return_if_fail(in != NULL);
  g_return_if_fail(out != NULL);

  klass = MW_CIPHER_GET_CLASS(ci);
  fn = klass->decrypt;
  fn(ci, in, out);
}


/* ----- RC2 ----- */


/** From RFC2268 */
static guchar RC2_PT[] = {
  0xD9, 0x78, 0xF9, 0xC4, 0x19, 0xDD, 0xB5, 0xED,
  0x28, 0xE9, 0xFD, 0x79, 0x4A, 0xA0, 0xD8, 0x9D,
  0xC6, 0x7E, 0x37, 0x83, 0x2B, 0x76, 0x53, 0x8E,
  0x62, 0x4C, 0x64, 0x88, 0x44, 0x8B, 0xFB, 0xA2,
  0x17, 0x9A, 0x59, 0xF5, 0x87, 0xB3, 0x4F, 0x13,
  0x61, 0x45, 0x6D, 0x8D, 0x09, 0x81, 0x7D, 0x32,
  0xBD, 0x8F, 0x40, 0xEB, 0x86, 0xB7, 0x7B, 0x0B,
  0xF0, 0x95, 0x21, 0x22, 0x5C, 0x6B, 0x4E, 0x82,
  0x54, 0xD6, 0x65, 0x93, 0xCE, 0x60, 0xB2, 0x1C,
  0x73, 0x56, 0xC0, 0x14, 0xA7, 0x8C, 0xF1, 0xDC,
  0x12, 0x75, 0xCA, 0x1F, 0x3B, 0xBE, 0xE4, 0xD1,
  0x42, 0x3D, 0xD4, 0x30, 0xA3, 0x3C, 0xB6, 0x26,
  0x6F, 0xBF, 0x0E, 0xDA, 0x46, 0x69, 0x07, 0x57,
  0x27, 0xF2, 0x1D, 0x9B, 0xBC, 0x94, 0x43, 0x03,
  0xF8, 0x11, 0xC7, 0xF6, 0x90, 0xEF, 0x3E, 0xE7,
  0x06, 0xC3, 0xD5, 0x2F, 0xC8, 0x66, 0x1E, 0xD7,
  0x08, 0xE8, 0xEA, 0xDE, 0x80, 0x52, 0xEE, 0xF7,
  0x84, 0xAA, 0x72, 0xAC, 0x35, 0x4D, 0x6A, 0x2A,
  0x96, 0x1A, 0xD2, 0x71, 0x5A, 0x15, 0x49, 0x74,
  0x4B, 0x9F, 0xD0, 0x5E, 0x04, 0x18, 0xA4, 0xEC,
  0xC2, 0xE0, 0x41, 0x6E, 0x0F, 0x51, 0xCB, 0xCC,
  0x24, 0x91, 0xAF, 0x50, 0xA1, 0xF4, 0x70, 0x39,
  0x99, 0x7C, 0x3A, 0x85, 0x23, 0xB8, 0xB4, 0x7A,
  0xFC, 0x02, 0x36, 0x5B, 0x25, 0x55, 0x97, 0x31,
  0x2D, 0x5D, 0xFA, 0x98, 0xE3, 0x8A, 0x92, 0xAE,
  0x05, 0xDF, 0x29, 0x10, 0x67, 0x6C, 0xBA, 0xC9,
  0xD3, 0x00, 0xE6, 0xCF, 0xE1, 0x9E, 0xA8, 0x2C,
  0x63, 0x16, 0x01, 0x3F, 0x58, 0xE2, 0x89, 0xA9,
  0x0D, 0x38, 0x34, 0x1B, 0xAB, 0x33, 0xFF, 0xB0,
  0xBB, 0x48, 0x0C, 0x5F, 0xB9, 0xB1, 0xCD, 0x2E,
  0xC5, 0xF3, 0xDB, 0x47, 0xE5, 0xA5, 0x9C, 0x77,
  0x0A, 0xA6, 0x20, 0x68, 0xFE, 0x7F, 0xC1, 0xAD
};


static void mw_rc2_iv_init(guchar *iv) {
  iv[0] = 0x01;
  iv[1] = 0x23;
  iv[2] = 0x45;
  iv[3] = 0x67;
  iv[4] = 0x89;
  iv[5] = 0xab;
  iv[6] = 0xcd;
  iv[7] = 0xef;
}


static void mw_rc2_key_expand(gint *ekey, const guchar *key, gsize keylen) {
  guchar tmp[128];
  gint i, j;
  
  g_return_if_fail(keylen > 0);
  g_return_if_fail(key != NULL);

  if(keylen > 128) keylen = 128;

  /* fill the first chunk with what key bytes we have */
  for(i = keylen; i--; tmp[i] = key[i]);
  /* memcpy(tmp, key, keylen); */

  /* build the remaining key from the given data */
  for(i = 0; keylen < 128; i++) {
    tmp[keylen] = RC2_PT[ (tmp[keylen - 1] + tmp[i]) & 0xff ];
    keylen++;
  }

  tmp[0] = RC2_PT[ tmp[0] & 0xff ];

  /* fill ekey from tmp, two guchar per gint */
  for(i = 0, j = 0; i < 64; i++) {
    ekey[i] = (tmp[j] & 0xff) | (tmp[j+1] << 8);
    j += 2;
  }
}


static void mw_rc2_enc_block(const gint *ekey, guchar *block) {
  int a, b, c, d;
  int i, j;

  a = (block[7] << 8) | (block[6] & 0xff);
  b = (block[5] << 8) | (block[4] & 0xff);
  c = (block[3] << 8) | (block[2] & 0xff);
  d = (block[1] << 8) | (block[0] & 0xff);

  for(i = 0; i < 16; i++) {
    j = i * 4;

    d += ((c & (a ^ 0xffff)) + (b & a) + ekey[j++]);
    d = (d << 1) | (d >> 15 & 0x0001);

    c += ((b & (d ^ 0xffff)) + (a & d) + ekey[j++]);
    c = (c << 2) | (c >> 14 & 0x0003);

    b += ((a & (c ^ 0xffff)) + (d & c) + ekey[j++]);
    b = (b << 3) | (b >> 13 & 0x0007);
    
    a += ((d & (b ^ 0xffff)) + (c & b) + ekey[j++]);
    a = (a << 5) | (a >> 11 & 0x001f);

    if(i == 4 || i == 10) {
      d += ekey[a & 0x003f];
      c += ekey[d & 0x003f];
      b += ekey[c & 0x003f];
      a += ekey[b & 0x003f];
    }    
  }

  *block++ = d & 0xff;
  *block++ = (d >> 8) & 0xff;
  *block++ = c & 0xff;
  *block++ = (c >> 8) & 0xff;
  *block++ = b & 0xff;
  *block++ = (b >> 8) & 0xff;
  *block++ = a & 0xff;
  *block++ = (a >> 8) & 0xff;
}


static void mw_rc2_enc(const gint *ekey, guchar *iv,
		       const MwOpaque *in, MwOpaque *out) {
  
  guchar *i = in->data;
  gsize i_len = in->len;

  guchar *o;
  gsize o_len;

  gint x, y;

  /* pad upwards to a multiple of 8 */
  o_len = i_len + (8 - (i_len % 8));
  o = g_malloc(o_len);

  /* this will be our output */
  out->data = o;
  out->len = o_len;

  /* figure out the amount of padding */
  y = o_len - i_len;

  /* copy in to out */
  for(x = i_len; x--; o[x] = i[x]);

  /* fill the padding */
  for(x = i_len; x < o_len; o[x++] = y);

  /* encrypt o in place, block by block */
  for(x = o_len; x > 0; x -= 8) {

    /* apply IV */
    for(y = 8; y--; o[y] ^= iv[y]);

    mw_rc2_enc_block(ekey, o);

    /* update IV */
    for(y = 8; y--; iv[y] = o[y]);

    /* advance */
    o += 8;
  }
}


static void mw_rc2_dec_block(const gint *ekey, guchar *block) {
  int a, b, c, d;
  int i, j;

  a = (block[7] << 8) | (block[6] & 0xff);
  b = (block[5] << 8) | (block[4] & 0xff);
  c = (block[3] << 8) | (block[2] & 0xff);
  d = (block[1] << 8) | (block[0] & 0xff);

  for(i = 16; i--; ) {
    j = i * 4 + 3;

    a = (a << 11) | (a >> 5 & 0x07ff);
    a -= ((d & (b ^ 0xffff)) + (c & b) + ekey[j--]);

    b = (b << 13) | (b >> 3 & 0x1fff);
    b -= ((a & (c ^ 0xffff)) + (d & c) + ekey[j--]);

    c = (c << 14) | (c >> 2 & 0x3fff);
    c -= ((b & (d ^ 0xffff)) + (a & d) + ekey[j--]);

    d = (d << 15) | (d >> 1 & 0x7fff);
    d -= ((c & (a ^ 0xffff)) + (b & a) + ekey[j--]);

    if(i == 5 || i == 11) {
      a -= ekey[b & 0x003f];
      b -= ekey[c & 0x003f];
      c -= ekey[d & 0x003f];
      d -= ekey[a & 0x003f];
    }
  }

  *block++ = d & 0xff;
  *block++ = (d >> 8) & 0xff;
  *block++ = c & 0xff;
  *block++ = (c >> 8) & 0xff;
  *block++ = b & 0xff;
  *block++ = (b >> 8) & 0xff;
  *block++ = a & 0xff;
  *block++ = (a >> 8) & 0xff;
}


void mw_rc2_dec(const gint *ekey, guchar *iv,
		const MwOpaque *in, MwOpaque *out) {
  
  guchar *i = in->data;
  gsize i_len = in->len;
  
  guchar *o;
  gsize o_len;

  gint x, y;

  if(i_len % 8) {
    g_warning("attempting decryption of mis-sized data, %u bytes",
	      (guint) i_len);
    return;
  }

  o = g_malloc(i_len);
  o_len = i_len;
  for(x = i_len; x--; o[x] = i[x]);
  /* memcpy(o, i, i_len); */

  out->data = o;
  out->len = o_len;

  for(x = o_len; x > 0; x -= 8) {
    mw_rc2_dec_block(ekey, o);

    /* apply IV */
    for(y = 8; y--; o[y] ^= iv[y]);

    /* update IV */
    for(y = 8; y--; iv[y] = i[y]);

    /* advance */
    i += 8;
    o += 8;
  }

  /* shorten the length by the value of the filler in the padding
     bytes (effectively chopping off the padding) */
  out->len -= *(o - 1);
}


struct mw_rc2_cipher {
  MwCipher mwcipher;
  
  gint outgoing_key[64];
  gint incoming_key[64];
  guchar outgoing_iv[8];
  guchar incoming_iv[8];
};


static void mw_rc2_setup(MwRC2Cipher *self) {
  /* setup outgoing and incoming key based on channel target and
     session login id */

  MwChannel *chan = NULL;
  MwSession *session = NULL;
  gchar *chan_id = NULL, *sess_id = NULL;

  g_object_get(G_OBJECT(self), "channel", &chan, NULL);
  g_return_if_fail(chan != NULL);

  g_object_get(G_OBJECT(chan),
	       "session", &session,
	       "remote-login-id", &chan_id,
	       NULL);
  
  g_object_get(G_OBJECT(session), "login-id", &sess_id, NULL);

  mw_rc2_key_expand(self->outgoing_key, (guchar *) sess_id, 5);
  mw_rc2_key_expand(self->incoming_key, (guchar *) chan_id, 5);

  /* clean up after every g_object_get */
  mw_gobject_unref(session);
  mw_gobject_unref(chan);
  g_free(chan_id);
  g_free(sess_id);
}


/* MwCipher interface method */
static void mw_rc2_accepted(MwCipher *ci, const MwOpaque *info) {
  mw_rc2_setup(MW_RC2_CIPHER(ci));
}


/* MwCipher interface method */
static void mw_rc2_accept(MwCipher *ci, MwOpaque *info) {
  mw_rc2_setup(MW_RC2_CIPHER(ci));
}


/* MwCipher interface method */
static void mw_rc2_encrypt(MwCipher *ci, const MwOpaque *in, MwOpaque *out) {
  MwRC2Cipher *self = MW_RC2_CIPHER(ci);
  mw_rc2_enc(self->outgoing_key, self->outgoing_iv, in, out);
}


/* MwCipher interface method */
static void mw_rc2_decrypt(MwCipher *ci, const MwOpaque *in, MwOpaque *out) {
  MwRC2Cipher *self = MW_RC2_CIPHER(ci);
  mw_rc2_dec(self->incoming_key, self->incoming_iv, in, out);
}


/* MwRC2Cipher method */
static void mw_rc2_set_encrypt_key(MwRC2Cipher *self,
				   const guchar *key, gsize len) {

  mw_debug_data(key, len, "unexpanded encrypt key");
  mw_rc2_key_expand(self->outgoing_key, key, len);
}


/* MwRC2Cipher method */
static void mw_rc2_set_decrypt_key(MwRC2Cipher *self,
				   const guchar *key, gsize len) {

  mw_rc2_key_expand(self->incoming_key, key, len);
}


static GObject *
mw_rc2_constructor(GType type, guint props_count,
		   GObjectConstructParam *props) {

  MwRC2CipherClass *klass;

  GObject *obj;
  MwRC2Cipher *self;

  klass = MW_RC2_CIPHER_CLASS(g_type_class_peek(MW_TYPE_RC2_CIPHER));

  obj = rc2_parent_class->constructor(type, props_count, props);
  self = (MwRC2Cipher *) obj;

  mw_rc2_iv_init(self->outgoing_iv);
  mw_rc2_iv_init(self->incoming_iv);

  return obj;
}


static guint16 mw_rc2_cipher_get_id() {
  return 0x0000;
}


static guint16 mw_rc2_cipher_get_policy() {
  return 0x1000;
}


static const gchar *mw_rc2_cipher_get_name() {
  return "RC2";
}


static const gchar *mw_rc2_cipher_get_desc() {
  return "Meanwhile RC2 Cipher";
}


static void mw_rc2_class_init(gpointer g_class, gpointer gclass_data) {
  GObjectClass *gobject_class;
  MwCipherClass *cipher_class;
  MwRC2CipherClass *klass;

  gobject_class = G_OBJECT_CLASS(g_class);
  cipher_class = MW_CIPHER_CLASS(g_class);
  klass = MW_RC2_CIPHER_CLASS(g_class);

  rc2_parent_class = g_type_class_peek_parent(gobject_class);

  gobject_class->constructor = mw_rc2_constructor;

  cipher_class->get_id = mw_rc2_cipher_get_id;
  cipher_class->get_policy = mw_rc2_cipher_get_policy;
  cipher_class->get_name = mw_rc2_cipher_get_name;
  cipher_class->get_desc = mw_rc2_cipher_get_desc;

  cipher_class->accepted = mw_rc2_accepted;
  cipher_class->accept = mw_rc2_accept;
  cipher_class->encrypt = mw_rc2_encrypt;
  cipher_class->decrypt = mw_rc2_decrypt;

  klass->set_encrypt_key = mw_rc2_set_encrypt_key;
  klass->set_decrypt_key = mw_rc2_set_decrypt_key;
}


static void mw_rc2_init(GTypeInstance *instance, gpointer g_class) {
  ;
}


static const GTypeInfo mw_rc2_info = {
  .class_size = sizeof(MwRC2CipherClass),
  .base_init = NULL,
  .base_finalize = NULL,
  .class_init = mw_rc2_class_init,
  .class_finalize = NULL,
  .class_data = NULL,
  .instance_size = sizeof(MwRC2Cipher),
  .n_preallocs = 0,
  .instance_init = mw_rc2_init,
  .value_table = NULL,
};


GType MwRC2Cipher_getType() {
  static GType type = 0;

  if(type == 0) {
    type = g_type_register_static(MW_TYPE_CIPHER, "MwRC2CipherType",
				  &mw_rc2_info, 0);
  }

  return type;
}


MwRC2CipherClass *MwRC2Cipher_getCipherClass() {
  return MW_RC2_CIPHER_CLASS(g_type_class_ref(MW_TYPE_RC2_CIPHER));
}


void MwRC2Cipher_setEncryptKey(MwRC2Cipher *ci,
			       const guchar *key, gsize len) {

  void (*fn)(MwRC2Cipher *, const guchar *, gsize);
  
  g_return_if_fail(ci != NULL);
  g_return_if_fail(key != NULL);
  
  fn = MW_RC2_CIPHER_GET_CLASS(ci)->set_encrypt_key;
  fn(ci, key, len);
}


void MwRC2Cipher_setDecryptKey(MwRC2Cipher *ci,
			       const guchar *key, gsize len) {  

  void (*fn)(MwRC2Cipher *, const guchar *, gsize);
  
  g_return_if_fail(ci != NULL);
  g_return_if_fail(key != NULL);
  
  fn = MW_RC2_CIPHER_GET_CLASS(ci)->set_decrypt_key;
  fn(ci, key, len);
}


/* ---- DH RC2 ----- */


struct mw_dh_rc2_cipher {
  MwCipher mwcipher;

  MwMPI *private_key;
  MwMPI *public_key;
  MwMPI *remote_key;
  
  gint shared_key[64];
  guchar outgoing_iv[8];
  guchar incoming_iv[8];
};


/* MwCipher interface */
static void mw_dh_rc2_offer(MwCipher *ci, MwOpaque *info) {
  MwDHRC2Cipher *self = MW_DH_RC2_CIPHER(ci);
  const MwMPI *public;

  public = MwDHRC2Cipher_getLocalPublicKey(self);
  MwMPI_export(public, info);
}


/* MwCipher interface */
static void mw_dh_rc2_accepted(MwCipher *ci, const MwOpaque *info) {
  MwDHRC2Cipher *self = MW_DH_RC2_CIPHER(ci);
  MwMPI *remote = MwMPI_new();

  MwMPI_import(remote, info);
  MwDHRC2Cipher_generateSharedKey(self, remote);

  MwMPI_free(remote);
}


/* MwCipher interface */
static void mw_dh_rc2_offered(MwCipher *ci, const MwOpaque *info) {
  MwDHRC2Cipher *self = MW_DH_RC2_CIPHER(ci);
  MwMPI *remote = MwMPI_new();

  MwMPI_import(remote, info);
  MwDHRC2Cipher_generateSharedKey(self, remote);
  
  MwMPI_free(remote);
}


/* MwCipher interface */
static void mw_dh_rc2_accept(MwCipher *ci, MwOpaque *info) {
  MwDHRC2Cipher *self = MW_DH_RC2_CIPHER(ci);
  const MwMPI *public;

  public = MwDHRC2Cipher_getLocalPublicKey(self);
  MwMPI_export(public, info);
}


/* MwCipher interface */
static void mw_dh_rc2_encrypt(MwCipher *ci,
			      const MwOpaque *in, MwOpaque *out) {

  MwDHRC2Cipher *self = MW_DH_RC2_CIPHER(ci);
  mw_rc2_enc(self->shared_key, self->outgoing_iv, in, out);
}


/* MwCipher interface */
static void mw_dh_rc2_decrypt(MwCipher *ci,
			      const MwOpaque *in, MwOpaque *out) {

  MwDHRC2Cipher *self = MW_DH_RC2_CIPHER(ci);
  mw_rc2_dec(self->shared_key, self->incoming_iv, in, out);
}


/* MwDHRC2Cipher method */
static void mw_dh_rc2_generate_local(MwDHRC2Cipher *self) {
  if(! self->private_key) self->private_key = MwMPI_new();
  if(! self->public_key) self->public_key = MwMPI_new();

  MwMPI_randDHKeypair(self->private_key, self->public_key);
}


/* MwDHRC2Cipher method */
static const MwMPI *mw_dh_rc2_get_public(MwDHRC2Cipher *self) {

  if(! self->public_key) {
    MwDHRC2Cipher_generateLocalKeys(self);
  }

  return self->public_key;
}


/* MwDHRC2Cipher method */
static void mw_dh_rc2_generate_shared(MwDHRC2Cipher *self,
				      const MwMPI *remote) {
  MwMPI *shared;
  MwOpaque o;

  if(! self->private_key) {
    MwDHRC2Cipher_generateLocalKeys(self);
  }

  if(! self->remote_key) self->remote_key = MwMPI_new();
  MwMPI_set(self->remote_key, remote);

  shared = MwMPI_new();
  MwMPI_calculateDHShared(shared, self->remote_key, self->private_key);

  MwMPI_export(shared, &o);
  MwMPI_free(shared);

  mw_rc2_key_expand(self->shared_key, o.data+(o.len-16), 16);

  MwOpaque_clear(&o);
}


static GObject *
mw_dh_rc2_constructor(GType type, guint props_count,
		      GObjectConstructParam *props) {

  MwDHRC2CipherClass *klass;

  GObject *obj;
  MwDHRC2Cipher *self;

  klass = MW_DH_RC2_CIPHER_CLASS(g_type_class_peek(MW_TYPE_DH_RC2_CIPHER));

  obj = dh_rc2_parent_class->constructor(type, props_count, props);
  self = (MwDHRC2Cipher *) obj;

  mw_rc2_iv_init(self->outgoing_iv);
  mw_rc2_iv_init(self->incoming_iv);

  return obj;
}


static void mw_dh_rc2_dispose(GObject *obj) {
  MwDHRC2Cipher *self = (MwDHRC2Cipher *) obj;

  if(self->private_key) {
    MwMPI_free(self->private_key);
    self->private_key = NULL;
  }

  if(self->public_key) {
    MwMPI_free(self->public_key);
    self->private_key = NULL;
  }

  if(self->remote_key) {
    MwMPI_free(self->remote_key);
    self->remote_key = NULL;
  }

  dh_rc2_parent_class->dispose(obj);
}


static guint16 mw_dh_rc2_cipher_get_id() {
  return 0x0001;
}


static guint16 mw_dh_rc2_cipher_get_policy() {
  return 0x2000;
}


static const gchar *mw_dh_rc2_cipher_get_name() {
  return "DH RC2";
}


static const gchar *mw_dh_rc2_cipher_get_desc() {
  return "Meanwhile Diffie/Hellman RC2 Cipher";
}


static void mw_dh_rc2_class_init(gpointer g_class, gpointer g_class_data) {
  GObjectClass *gobject_class;
  MwCipherClass *cipher_class;
  MwDHRC2CipherClass *klass;

  gobject_class = G_OBJECT_CLASS(g_class);
  cipher_class = MW_CIPHER_CLASS(g_class);
  klass = MW_DH_RC2_CIPHER_CLASS(g_class);

  dh_rc2_parent_class = g_type_class_peek_parent(gobject_class);

  gobject_class->constructor = mw_dh_rc2_constructor;
  gobject_class->dispose = mw_dh_rc2_dispose;

  cipher_class->get_id = mw_dh_rc2_cipher_get_id;
  cipher_class->get_policy = mw_dh_rc2_cipher_get_policy;
  cipher_class->get_name = mw_dh_rc2_cipher_get_name;
  cipher_class->get_desc = mw_dh_rc2_cipher_get_desc;

  cipher_class->offer = mw_dh_rc2_offer;
  cipher_class->accepted = mw_dh_rc2_accepted;
  cipher_class->offered = mw_dh_rc2_offered;
  cipher_class->accept = mw_dh_rc2_accept;
  cipher_class->encrypt = mw_dh_rc2_encrypt;
  cipher_class->decrypt = mw_dh_rc2_decrypt;

  klass->generate_local = mw_dh_rc2_generate_local;
  klass->get_public = mw_dh_rc2_get_public;
  klass->generate_shared = mw_dh_rc2_generate_shared;
}


static void mw_dh_rc2_init(GTypeInstance *instance, gpointer g_class) {
  ;
}


static const GTypeInfo mw_dh_rc2_info = {
  .class_size = sizeof(MwDHRC2CipherClass),
  .base_init = NULL,
  .base_finalize = NULL,
  .class_init = mw_dh_rc2_class_init,
  .class_finalize = NULL,
  .class_data = NULL,
  .instance_size = sizeof(MwDHRC2Cipher),
  .n_preallocs = 0,
  .instance_init = mw_dh_rc2_init,
  .value_table = NULL,
};


GType MwDHRC2Cipher_getType() {
  static GType type = 0;

  if(type == 0) {
    type = g_type_register_static(MW_TYPE_CIPHER, "MwDHRC2CipherType",
				  &mw_dh_rc2_info, 0);
  }

  return type;
}


MwDHRC2CipherClass *MwDHRC2Cipher_getCipherClass() {
  MwDHRC2CipherClass *klass = NULL;
  gpointer k = g_type_class_ref(MW_TYPE_DH_RC2_CIPHER);
  klass = MW_DH_RC2_CIPHER_CLASS(k);
  return klass;
}


void MwDHRC2Cipher_generateLocalKeys(MwDHRC2Cipher *ci) {
  void (*fn)(MwDHRC2Cipher *);
  
  g_return_if_fail(ci != NULL);
  
  fn = MW_DH_RC2_CIPHER_GET_CLASS(ci)->generate_local;
  fn(ci);
}


const MwMPI *MwDHRC2Cipher_getLocalPublicKey(MwDHRC2Cipher *ci) {
  const MwMPI *(*fn)(MwDHRC2Cipher *);

  g_return_val_if_fail(ci != NULL, NULL);

  fn = MW_DH_RC2_CIPHER_GET_CLASS(ci)->get_public;
  return fn(ci);
}


void MwDHRC2Cipher_generateSharedKey(MwDHRC2Cipher *ci,
				     const MwMPI *remote_key) {

  void (*fn)(MwDHRC2Cipher *, const MwMPI *);
  
  g_return_if_fail(ci != NULL);
  g_return_if_fail(remote_key != NULL);
  
  fn = MW_DH_RC2_CIPHER_GET_CLASS(ci)->generate_shared;
  fn(ci, remote_key);
}


/* The end. */
