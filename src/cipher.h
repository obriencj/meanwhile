

#ifndef _MW_CIPHER_
#define _MW_CIPHER_


/*
  This set of functions is a broken sort of RC2 implementation. But it works
  with sametime, so we're all happy, right? Primary change to functionality
  appears in the mwKeyExpand function. Hypothetically, using a key expanded
  here (but breaking it into a 128-char array rather than 64 ints), one could
  pass it at that length to openssl and no further key expansion would occur.

  I'm not certain if replacing this with a wrapper for openssl calls is a good
  idea or not. Proven software versus added dependencies.
*/


/** generate some pseudo-random bytes */
void rand_key(char *key, unsigned int keylen);


/** Setup an Initialization Vector */
void mwIV_init(char *iv);


/** Expand a variable-length key into a 128-byte key (represented as an
    an array of 64 ints) */
void mwKeyExpand(int *ekey, const char *key, gsize keylen);


/** Encrypt data using an already-expanded key */
void mwEncryptExpanded(const int *ekey, char *iv,
		       const char *in, gsize i_len,
		       char **out, gsize *o_len);


/** Encrypt data using an expanded form of the given key */
void mwEncrypt(const char *key, gsize keylen, char *iv,
	       const char *in, gsize i_len,
	       char **out, gsize *o_len);


/** Decrypt data using an already expanded key */
void mwDecryptExpanded(const int *ekey, char *iv,
		       const char *in, gsize i_len,
		       char **out, gsize *o_len);


/** Decrypt data using an expanded form of the given key */
void mwDecrypt(const char *key, gsize keylen, char *iv,
	       const char *in, gsize i_len,
	       char **out, gsize *o_len);


#endif


