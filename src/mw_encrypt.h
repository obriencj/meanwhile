#ifndef _MW_ENCRYPT_H
#define _MW_ENCRYPT_H


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


/** @file mw_encrypt.h
    
*/


#include <glib.h>
#include "mw_mpi.h"
#include "mw_object.h"
#include "mw_typedef.h"


G_BEGIN_DECLS


/**
   @section Cipher Interface
   
   @{
*/


#define MW_TYPE_CIPHER  (MwCipher_getType())


#define MW_CIPHER(obj)						\
  (G_TYPE_CHECK_INSTANCE_CAST((obj), MW_TYPE_CIPHER, MwCipher))


#define MW_CIPHER_CLASS(klass)						\
  (G_TYPE_CHECK_CLASS_CAST((klass), MW_TYPE_CIPHER, MwCipherClass))


#define MW_IS_CIPHER(obj)				\
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), MW_TYPE_CIPHER))


#define MW_IS_CIPHER_CLASS(klass)			\
  (G_TYPE_CHECK_CLASS_TYPE((klass), MW_TYPE_CIPHER))


#define MW_CIPHER_GET_CLASS(obj)					\
  (G_TYPE_INSTANCE_GET_CLASS((obj), MW_TYPE_CIPHER, MwCipherClass))


typedef struct mw_cipher_class MwCipherClass;


struct mw_cipher_class {
  MwObjectClass mwobject_class;
  
  guint16 (*get_id)();
  guint16 (*get_policy)();
  const gchar *(*get_name)();
  const gchar *(*get_desc)();
  
  /* for outgoing */
  void (*offer)(MwCipher *self, MwOpaque *info);
  void (*accepted)(MwCipher *self, const MwOpaque *info);
  
  /* for incoming */
  void (*offered)(MwCipher *self, const MwOpaque *info);
  void (*accept)(MwCipher *self, MwOpaque *info);
  
  void (*encrypt)(MwCipher *self, const MwOpaque *in, MwOpaque *out);
  void (*decrypt)(MwCipher *self, const MwOpaque *in, MwOpaque *out);
};


struct mw_cipher {
  MwObject mwobject;
  MwChannel *channel;
};


GType MwCipher_getType();


guint16 MwCipherClass_getIdentifier(MwCipherClass *klass);


guint16 MwCipherClass_getPolicy(MwCipherClass *klass);


const gchar *MwCipherClass_getName(MwCipherClass *klass);


const gchar *MwCipherClass_getDescription(MwCipherClass *klass);


MwCipher *MwCipherClass_newInstance(MwCipherClass *klass, MwChannel *chan);


void MwCipher_offer(MwCipher *ci, MwOpaque *info);


void MwCipher_accepted(MwCipher *ci, const MwOpaque *info);


void MwCipher_offered(MwCipher *ci, const MwOpaque *info);


void MwCipher_accept(MwCipher *ci, MwOpaque *info);


void MwCipher_encrypt(MwCipher *ci, const MwOpaque *in, MwOpaque *out);


void MwCipher_decrypt(MwCipher *ci, const MwOpaque *in, MwOpaque *out);


/* @} */


/**
   @section RC2 Cipher

   This cipher maintains two keys; one for incoming traffic and one
   for outgoing traffic.

   When used with a MwChannel, the keys are limited to an effective 40
   bits in length; using only the first five bytes worth of the
   unexpanded incoming and outgoing keys.
  
   @{
*/


#define MW_TYPE_RC2_CIPHER  (MwRC2Cipher_getType())


#define MW_RC2_CIPHER(obj)						\
  (G_TYPE_CHECK_INSTANCE_CAST((obj), MW_TYPE_RC2_CIPHER, MwRC2Cipher))


#define MW_RC2_CIPHER_CLASS(klass)					\
  (G_TYPE_CHECK_CLASS_CAST((klass), MW_TYPE_RC2_CIPHER, MwRC2CipherClass))


#define MW_IS_RC2_CIPHER(obj)					\
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), MW_TYPE_RC2_CIPHER))


#define MW_IS_RC2_CIPHER_CLASS(klass)				\
  (G_TYPE_CHECK_CLASS_TYPE((klass), MW_TYPE_RC2_CIPHER))


#define MW_RC2_CIPHER_GET_CLASS(obj)					\
  (G_TYPE_INSTANCE_GET_CLASS((obj), MW_TYPE_RC2_CIPHER, MwRC2CipherClass))


typedef struct mw_rc2_cipher_class MwRC2CipherClass;


struct mw_rc2_cipher_class {
  MwCipherClass cipher_class;

  void (*set_encrypt_key)(MwRC2Cipher *self, const guchar *key, gsize len);
  void (*set_decrypt_key)(MwRC2Cipher *self, const guchar *key, gsize len);
};


struct mw_rc2_cipher;


GType MwRC2Cipher_getType();


MwRC2CipherClass *MwRC2Cipher_getCipherClass();


/** Provide an unexpanded key for an instance, for use in encrypting
    information. */
void MwRC2Cipher_setEncryptKey(MwRC2Cipher *c, const guchar *key, gsize len);


/** Provide an unexpanded for for an instance, for use in decrypting
    information. */
void MwRC2Cipher_setDecryptKey(MwRC2Cipher *c, const guchar *key, gsize len);


/* @} */


/**
   @section RC2 Cipher with Diffie/Hellman key exchange

   This cipher keeps a single shared key for both incoming and
   outgoing data. This shared key is expanded from the value
   calculated via the D/H key exchange method, using built-in Prime
   and Base constants. The cipher can generate random(ish)
   private/public keys for the beginning of negotiations, and will
   calculate the shared key once the remote public key has been
   provided.

   This cipher has an effective key-length of 128 bits, using only
   the first (high) 16 bytes from the D/H shared key.

   @{
*/


#define MW_TYPE_DH_RC2_CIPHER  (MwDHRC2Cipher_getType())


#define MW_DH_RC2_CIPHER(obj)						\
  (G_TYPE_CHECK_INSTANCE_CAST((obj),					\
			      MW_TYPE_DH_RC2_CIPHER,			\
			      MwDHRC2Cipher))


#define MW_DH_RC2_CIPHER_CLASS(klass)					\
  (G_TYPE_CHECK_CLASS_CAST((klass),					\
			   MW_TYPE_DH_RC2_CIPHER,			\
			   MwDHRC2CipherClass))


#define MW_IS_DH_RC2_CIPHER(obj)				\
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), MW_TYPE_DH_RC2_CIPHER))


#define MW_IS_DH_RC2_CIPHER_CLASS(klass)			\
  (G_TYPE_CHECK_CLASS_TYPE((klass), MW_TYPE_DH_RC2_CIPHER))


#define MW_DH_RC2_CIPHER_GET_CLASS(obj)					\
  (G_TYPE_INSTANCE_GET_CLASS((obj),					\
			     MW_TYPE_DH_RC2_CIPHER,			\
			     MwDHRC2CipherClass))


typedef struct mw_dh_rc2_cipher_class MwDHRC2CipherClass;


struct mw_dh_rc2_cipher_class {
  MwCipherClass cipher_class;

  void (*generate_local)(MwDHRC2Cipher *self);
  const MwMPI *(*get_public)(MwDHRC2Cipher *self);
  void (*generate_shared)(MwDHRC2Cipher *self, const MwMPI *remote);
};


struct mw_dh_rc2_cipher;


GType MwDHRC2Cipher_getType();


MwDHRC2CipherClass *MwDHRC2Cipher_getCipherClass();


/** Generate a random Diffie/Hellman private key and a matching public
    key, using the built-in Prime and Base values. The private key
    will me stored internally.
 */
void MwDHRC2Cipher_generateLocalKeys(MwDHRC2Cipher *ci);


/** Get the local public key for a cipher instance. If a
    private/public keypair has not already been generated, returns
    NULL */
const MwMPI *MwDHRC2Cipher_getLocalPublicKey(MwDHRC2Cipher *ci);


/** Generate the Diffie/Hellman shared key internally, using the
    specified public remote key, and either an existing generated
    private key or, if one has not been generated, a newly generated
    private key. After this, the cipher instance can be used to either
    encrypt or decrypt data. */
void MwDHRC2Cipher_generateSharedKey(MwDHRC2Cipher *ci,
				     const MwMPI *remote_key);


/* @} */


G_END_DECLS


#endif /* _MW_ENCRYPT_H */
