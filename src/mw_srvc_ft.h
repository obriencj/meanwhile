
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


/** @file mw_srvc_ft.h

    File transfer service
*/


struct mwServiceFileTransfer;


struct mwFileTransfer;


struct mwFileTransferHandler {

  void (*ft_offered)(struct mwFileTransfer *ft);

  void (*ft_opened)(struct mwFileTransfer *ft);

  void (*ft_closed)(struct mwFileTransfer *ft, guint32 reason);

  void (*ft_recv)(struct mwFileTransfer *ft, struct mwOpaque *data);

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
		   struct mwIdBlock *who, const char *msg,
		   const char *filename);


int mwFileTransfer_send(struct mwFileTransfer *ft,
			struct mwOpaque *data, gboolean done);


void mwFileTransfer_setClientData(struct mwFileTransfer *ft,
				  gpointer data, GDestroyNotify clean);


gpointer mwFileTransfer_getClientData(struct mwFileTransfer *ft);


void mwFileTransfer_removeClientData(struct mwFileTransfer *ft);


int mwFileTransfer_accept(struct mwFileTransfer *ft);


int mwFileTransfer_reject(struct mwFileTransfer *ft);


#endif