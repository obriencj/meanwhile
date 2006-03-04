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


#include "mw_mpi.h"
#include "mpi/mpi.h"


struct mw_mpi {
  mw_mp_int i;
};


/** prime number used in DH exchange */
static guchar dh_prime[] = {
  0xCF, 0x84, 0xAF, 0xCE, 0x86, 0xDD, 0xFA, 0x52,
  0x7F, 0x13, 0x6D, 0x10, 0x35, 0x75, 0x28, 0xEE,
  0xFB, 0xA0, 0xAF, 0xEF, 0x80, 0x8F, 0x29, 0x17,
  0x4E, 0x3B, 0x6A, 0x9E, 0x97, 0x00, 0x01, 0x71,
  0x7C, 0x8F, 0x10, 0x6C, 0x41, 0xC1, 0x61, 0xA6,
  0xCE, 0x91, 0x05, 0x7B, 0x34, 0xDA, 0x62, 0xCB,
  0xB8, 0x7B, 0xFD, 0xC1, 0xB3, 0x5C, 0x1B, 0x91,
  0x0F, 0xEA, 0x72, 0x24, 0x9D, 0x56, 0x6B, 0x9F
};


/** base used in DH exchange */
#define DH_BASE  3


MwMPI *MwMPI_new() {
  MwMPI *i = g_new0(MwMPI, 1);
  mw_mp_init(&i->i);
  return i;
}


void MwMPI_free(MwMPI *i) {
  if(! i) return;
  mw_mp_clear(&i->i);
  g_free(i);
}


void MwMPI_import(MwMPI *i, const MwOpaque *o) {
  g_return_if_fail(i != NULL);
  g_return_if_fail(o != NULL);

  mw_mp_read_unsigned_bin(&i->i, o->data, o->len);
}


void MwMPI_export(const MwMPI *i, MwOpaque *o) {
  g_return_if_fail(i != NULL);
  g_return_if_fail(o != NULL);
  
  o->len = mw_mp_unsigned_bin_size(&((MwMPI *)i)->i);
  o->data = g_malloc0(o->len);
  mw_mp_to_unsigned_bin(&((MwMPI *)i)->i, o->data);
}


void MwMPI_set(MwMPI *to, const MwMPI *from) {
  g_return_if_fail(to != NULL);
  g_return_if_fail(from != NULL);

  mw_mp_copy((mw_mp_int *) &from->i, &to->i);
}


static mw_mp_int *get_prime() {
  static mw_mp_int prime_mpi;
  static gboolean prime_ready = FALSE;

  if(! prime_ready) {
    prime_ready = TRUE;
    mw_mp_init(&prime_mpi);
    mw_mp_read_unsigned_bin(&prime_mpi, dh_prime, 64);
  }

  return &prime_mpi;
}


void MwMPI_setDHPrime(MwMPI *i) {
  g_return_if_fail(i != NULL);
  mw_mp_copy(get_prime(), &i->i);
}


static mw_mp_int *get_base() {
  static mw_mp_int base_mpi;
  static gboolean base_ready = FALSE;

  if(! base_ready) {
    base_ready = TRUE;
    mw_mp_init(&base_mpi);
    mw_mp_set_int(&base_mpi, DH_BASE);
  }

  return &base_mpi;
}


void MwMPI_setDHBase(MwMPI *i) {
  g_return_if_fail(i != NULL);
  mw_mp_copy(get_base(), &i->i);
}


void MwMPI_randDHKeypair(MwMPI *private, MwMPI *public) {
  g_return_if_fail(private != NULL);
  g_return_if_fail(public != NULL);

  mw_mp_set_rand(&private->i, 512);
  mw_mp_exptmod(get_base(), &private->i, get_prime(), &public->i);
}


void MwMPI_calculateDHShared(MwMPI *shared,
			     const MwMPI *remote,
			     const MwMPI *private) {

  g_return_if_fail(shared != NULL);
  g_return_if_fail(remote != NULL);
  g_return_if_fail(private != NULL);

  mw_mp_exptmod(&((MwMPI *)remote)->i, &((MwMPI *)private)->i,
		get_prime(), &shared->i);
}


/* The end. */
