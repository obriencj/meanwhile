
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


#ifndef _MW_SRVC_FT_H
#define _MW_SRVC_FT_H


#include "mw_common.h"


/** @file mw_srvc_ft.h

    File transfer service
*/


/** @struct mwServiceFileTransfer
    File transfer service
*/
struct mwServiceFileTransfer;


/** @struct mwFileTransfer
    A single file trasfer session
 */
struct mwFileTransfer;


#define mwService_FILE_TRANSFER  0x00000038


enum mwFileTranferCode {
  mwFileTranfer_SUCCESS   = 0x00000000,
  mwFileTranfer_REJECTED  = 0x08000606,
};


struct mwFileTransferHandler {

  /** an incoming file transfer has been offered */
  void (*ft_offered)(struct mwFileTransfer *ft);

  /** a file transfer has been initiated */
  void (*ft_opened)(struct mwFileTransfer *ft);

  /** a file transfer has been terminated */
  void (*ft_closed)(struct mwFileTransfer *ft, guint32 code);

  /** receive a chunk of a file from an inbound file transfer */
  void (*ft_recv)(struct mwFileTransfer *ft,
		  struct mwOpaque *data, gboolean done);

  /** optional. called from mwService_free */
  void (*clear)(struct mwServiceFileTransfer *srvc);
};


struct mwServiceFileTransfer *
mwServiceFileTransfer_new(struct mwSession *session,
			  struct mwFileTransferHandler *handler);


struct mwFileTransferHandler *
mwServiceFileTransfer_getHandler(struct mwServiceFileTransfer *srvc);


struct mwFileTransfer *
mwFileTransfer_new(struct mwServiceFileTransfer *srvc,
		   const struct mwIdBlock *who, const char *msg,
		   const char *filename, guint32 filesize);


/** the user on the other end of the file transfer */
const struct mwIdBlock *
mwFileTransfer_getUser(struct mwFileTransfer *ft);


const char *
mwFileTransfer_getMessage(struct mwFileTransfer *ft);


const char *
mwFileTransfer_getFileName(struct mwFileTransfer *ft);


guint32 mwFileTransfer_getFileSize(struct mwFileTransfer *ft);


guint32 mwFileTransfer_getRemaining(struct mwFileTransfer *ft);


#define mwFileTransfer_getSent(ft) \
  (mwFileTransfer_getFileSize(ft) - mwFileTransfer_getRemaining(ft))


int mwFileTransfer_accept(struct mwFileTransfer *ft);


#define mwFileTransfer_reject(ft) \
  mwFileTransfer_close((ft), mwFileTranfer_REJECTED)


#define mwFileTransfer_cancel(ft) \
  mwFileTransfer_close((ft), mwFileTranfer_SUCCESS);


int mwFileTransfer_close(struct mwFileTransfer *ft, guint32 code);


/** send a chunk of data over an outbound file transfer */
int mwFileTransfer_send(struct mwFileTransfer *ft,
			struct mwOpaque *data, gboolean done);


void mwFileTransfer_setClientData(struct mwFileTransfer *ft,
				  gpointer data, GDestroyNotify clean);


gpointer mwFileTransfer_getClientData(struct mwFileTransfer *ft);


void mwFileTransfer_removeClientData(struct mwFileTransfer *ft);


#endif
