

#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cipher.h"


/** From RFC2268 */


static char PT[] = {
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


void rand_key(char *key, unsigned int keylen) {
  srand(clock());
  while(keylen--) key[keylen] = rand() & 0xff;
}


void mwIV_init(char *iv) {
  static char normal_iv[] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef
  };
  memcpy(iv, normal_iv, 8);

  /*
  *iv++ = 0x01; *iv++ = 0x23;
  *iv++ = 0x45; *iv++ = 0x67;
  *iv++ = 0x89; *iv++ = 0xab;
  *iv++ = 0xcd; *iv   = 0xef;
  */
}


/* This does not seem to produce the same results as normal RC2 key
   expansion would, but it works, so eh. It might be smart to farm
   this out to mozilla or openssl */
void mwKeyExpand(int *ekey, const char *key, gsize keylen) {
  char tmp[128];
  int i, j;

  if(keylen > 128) keylen = 128;
  memcpy(tmp, key, keylen);

  for(i = 0; keylen < 128; i++) {
    tmp[keylen] = PT[ (tmp[keylen - 1] + tmp[i]) & 0xff ];
    keylen++;
  }

  tmp[0] = PT[ tmp[0] & 0xff ];

  for(i = 0, j = 0; i < 64; i++) {
    ekey[i] = (tmp[j] & 0xff) | (tmp[j+1] << 8);
    j += 2;
  }
}


/* normal RC2 encryption given a full 128-byte (as 64 ints) key */
static void mwEncryptBlock(const int *ekey, char *out) {
  int a = (out[7] << 8) | (out[6] & 0xff);
  int b = (out[5] << 8) | (out[4] & 0xff);
  int c = (out[3] << 8) | (out[2] & 0xff);
  int d = (out[1] << 8) | (out[0] & 0xff);

  int i, j;

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

  *out++ = d & 0xff;
  *out++ = (d >> 8) & 0xff;
  *out++ = c & 0xff;
  *out++ = (c >> 8) & 0xff;
  *out++ = b & 0xff;
  *out++ = (b >> 8) & 0xff;
  *out++ = a & 0xff;
  *out++ = (a >> 8) & 0xff;
}


void mwEncryptExpanded(const int *ekey, char *iv,
		       const char *in, gsize i_len,
		       char **out, gsize *o_len) {
  int x, y;
  char *o;
  int o_l;

  if(*out == NULL) {
    o_l = *o_len = (i_len & -8) + 8;
    o = *out = (char *) g_malloc(o_l);
    
  } else {
    o_l = *o_len;
    o = *out;
  }

  y = o_l - i_len;

  /* copy in to out, and add padding bytes */
  memcpy(o, in, i_len);
  memset(o + i_len, y, y);

  /* encrypt in blocks */
  for(x = o_l; x > 0; x -= 8) {
    for(y = 8; y--; o[y] ^= iv[y]);
    mwEncryptBlock(ekey, o);
    memcpy(iv, o, 8);
    o += 8;
  }
}


void mwEncrypt(const char *key, gsize keylen, char *iv,
	       const char *in, gsize i_len,
	       char **out, gsize *o_len) {

  int ekey[64];
  mwKeyExpand(ekey, key, keylen);
  mwEncryptExpanded(ekey, iv, in, i_len, out, o_len);
}


static void mwDecryptBlock(const int *ekey, char *out) {
  int a = (out[7] << 8) | (out[6] & 0xff);
  int b = (out[5] << 8) | (out[4] & 0xff);
  int c = (out[3] << 8) | (out[2] & 0xff);
  int d = (out[1] << 8) | (out[0] & 0xff);

  int i, j;

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

  *out++ = d & 0xff;
  *out++ = (d >> 8) & 0xff;
  *out++ = c & 0xff;
  *out++ = (c >> 8) & 0xff;
  *out++ = b & 0xff;
  *out++ = (b >> 8) & 0xff;
  *out++ = a & 0xff;
  *out++ = (a >> 8) & 0xff;
}


void mwDecryptExpanded(const int *ekey, char *iv,
		       const char *in, gsize i_len,
		       char **out, gsize *o_len) {
  int x, y;
  char *o;
  int o_l;

  if(*out == NULL) {
    o_l = *o_len = i_len;
    o = *out = (char *) g_malloc(o_l);
    
  } else {
    o_l = *o_len;
    o = *out;
  }

  memcpy(o, in, i_len);

  for(x = o_l; x > 0; x -= 8) {
    /* decrypt a block */
    mwDecryptBlock(ekey, o);
    for(y = 8; y--; o[y] ^= iv[y]);

    /* modify the initialization vector */
    memcpy(iv, in, 8);
    in += 8;

    /* move ahead along the buffer, but only while there's more */
    o += 8;
  }

  /* remove the padding from the length count */
  x = *(o-1);
  *o_len -= x;
}


void mwDecrypt(const char *key, gsize keylen, char *iv,
	       const char *in, gsize i_len,
	       char **out, gsize *o_len) {

  int ekey[64];
  mwKeyExpand(ekey, key, keylen);
  mwDecryptExpanded(ekey, iv, in, i_len, out, o_len);
}


