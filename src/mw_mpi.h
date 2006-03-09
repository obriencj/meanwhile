#ifndef _MW_MPI_H
#define _MW_MPI_H


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


/**
   @file mw_mpi.h
   @since 2.0.0
   
   This is a small set of functions wrapping a full multiple-precision
   integer math library. Currently, the math is provided by a copy of
   the public domain libmpi.
   
   For more information on the underlying MPI Library, visit
   http://www.cs.dartmouth.edu/~sting/mpi/
*/


#include <glib.h>
#include "mw_common.h"


G_BEGIN_DECLS


/**
   @since 2.0.0

   Mulitple-precision integer value
*/
typedef struct mw_mpi MwMPI;


/**
   @since 2.0.0

   alocate a new mpi value, set to zero
*/
MwMPI *MwMPI_new();


/**
   @since 2.0.0

   free an mpi value
*/
void MwMPI_free(MwMPI *i);


/**
   @since 2.0.0

   import a mpi from an opaque, most significant byte first
*/
void MwMPI_import(MwMPI *i, const MwOpaque *o);


/**
   @since 2.0.0

   export a mpi to an opaque, most significant byte first
*/
void MwMPI_export(const MwMPI *i, MwOpaque *o);


/**
   @since 2.0.0

   set the value of a MwMPI instance to the value of another MwMPI
   instance
*/
void MwMPI_set(MwMPI *to, const MwMPI *from);


/**
   @since 2.0.0

   set the value of a MwMPI to a random number of the specified number
   of bits in length
*/
void MwMPI_random(MwMPI *to, guint bits);


/**
   @since 2.0.0

   set the passed mpi to the constant Prime value for use in
   Sametime-based encryption
*/
void MwMPI_setDHPrime(MwMPI *i);


/**
   @since 2.0.0
   
   set the passed mpi to the constant Base value for use in
   Sametime-based encryption.
*/
void MwMPI_setDHBase(MwMPI *i);


/**
   @since 2.0.0

   generate a (pseudo) random private key and the related public key

   @param private  the generated random key
   @param public   the generated public key
*/
void MwMPI_randDHKeypair(MwMPI *private, MwMPI *public);


/**
   @since 2.0.0
   
   calculates a Diffie/Hellman shared key given a remove public key and
   a local private key
    
   @param shared   set to the calculated shared key value
   @param remote   remote public key
   @param private  local private key
*/
void MwMPI_calculateDHShared(MwMPI *shared,
			     const MwMPI *remote,
			     const MwMPI *private);


G_END_DECLS


#endif /* _MW_MPI_H */
