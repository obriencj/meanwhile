
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
#include "mw_error.h"
#include "mw_message.h"
#include "mw_service.h"
#include "mw_session.h"
#include "mw_srvc_ft.h"
#include "mw_util.h"


#define PROTOCOL_TYPE  0x00000000
#define PROTOCOL_VER   0x00000001


/** send-on-channel type: FT transfer data */
#define msg_TRANSFER  0x0001


struct mwServiceFileTransfer {
  struct mwService service;

  struct mwFileTransferHandler *handler;
  GList *transfers;
};


struct mwFileTransfer {
  struct mwServiceFileTransfer *service;
  
  struct mwChannel *channel;
  
  struct mwIdBlock who;

  char *filename;
  char *message;

  guint32 size;
  guint32 remaining;

  struct mw_datum client_data;
};


/** momentarily places a mwLoginInfo into a mwIdBlock */
static void login_into_id(struct mwIdBlock *to, struct mwLoginInfo *from) {
  to->user = from->user_id;
  to->community = from->community;
}


static void recv_channelCreate(struct mwServiceFileTransfer *srvc,
			       struct mwChannel *chan,
			       struct mwMsgChannelCreate *msg) {

  struct mwFileTransferHandler *handler;
  struct mwGetBuffer *b;

  char *fnm, *txt;
  guint32 size, junk;
  gboolean b_err;

  g_return_if_fail(srvc->handler != NULL);
  handler = srvc->handler;
  
  b = mwGetBuffer_wrap(&msg->addtl);

  guint32_get(b, &junk); /* unknown */
  mwString_get(b, &fnm); /* offered filename */
  mwString_get(b, &txt); /* offering message */
  guint32_get(b, &size); /* size of offered file */
  guint32_get(b, &junk); /* unknown */
  /* and we just skip an unknown guint16 at the end */

  b_err = mwGetBuffer_error(b);
  mwGetBuffer_free(b);

  if(b_err) {
    g_warning("bad/malformed addtl in File Transfer service");
    mwChannel_destroy(chan, ERR_FAILURE, NULL);

  } else {
    struct mwIdBlock idb;
    struct mwFileTransfer *ft;

    login_into_id(&idb, mwChannel_getUser(chan));
    ft = mwFileTransfer_new(srvc, &idb, txt, fnm, size);

    mwChannel_setServiceData(chan, ft, NULL);

    if(handler->ft_offered)
      handler->ft_offered(ft);
  }

  g_free(fnm);
  g_free(txt);
}


static void recv_channelAccept(struct mwServiceFileTransfer *srvc,
			       struct mwChannel *chan,
			       struct mwMsgChannelAccept *msg) {

  struct mwFileTransferHandler *handler;
  struct mwFileTransfer *ft;

  g_return_if_fail(srvc->handler != NULL);
  handler = srvc->handler;

  ft = mwChannel_getServiceData(chan);
  g_return_if_fail(ft != NULL);

  if(handler->ft_opened)
    handler->ft_opened(ft);
}


static void recv_channelDestroy(struct mwServiceFileTransfer *srvc,
				struct mwChannel *chan,
				struct mwMsgChannelDestroy *msg) {

  struct mwFileTransferHandler *handler;
  struct mwFileTransfer *ft;
  guint32 code;

  code = msg->reason;

  g_return_if_fail(srvc->handler != NULL);
  handler = srvc->handler;

  ft = mwChannel_getServiceData(chan);
  g_return_if_fail(ft != NULL);

  if(handler->ft_closed)
    handler->ft_closed(ft, code);  
}


static void recv(struct mwService *srvc, struct mwChannel *chan,
		 guint16 type, struct mwOpaque *data) {

  struct mwServiceFileTransfer *srvc_ft;
  struct mwFileTransferHandler *handler;
  struct mwFileTransfer *ft;

  g_return_if_fail(type == msg_TRANSFER);

  srvc_ft = (struct mwServiceFileTransfer *) srvc;
  handler = srvc_ft->handler;

  g_return_if_fail(handler != NULL);
  
  ft = mwChannel_getServiceData(chan);
  g_return_if_fail(ft != NULL);

  if(data->len > ft->remaining) {
    /* @todo handle error */

  } else {
    ft->remaining -= data->len;
    
    if(handler->ft_recv)
      handler->ft_recv(ft, data, !ft->remaining);
  }
}


static void clear(struct mwServiceFileTransfer *srvc) {
  struct mwFileTransferHandler *h;
  
  h = srvc->handler;
  if(h && h->clear)
    h->clear(srvc);
  srvc->handler = NULL;
}


static const char *name(struct mwService *srvc) {
  return "File Transfer";
}


static const char *desc(struct mwService *srvc) {
  return "Provides file transfer capabilities through the community server";
}


static void start(struct mwService *srvc) {
  mwService_started(srvc);
}


static void stop(struct mwServiceFileTransfer *srvc) {
  while(srvc->transfers) {
    mwFileTransfer_free(srvc->transfers->data);
  }

  mwService_stopped(MW_SERVICE(srvc));
}


struct mwServiceFileTransfer *
mwServiceFileTransfer_new(struct mwSession *session,
			  struct mwFileTransferHandler *handler) {

  struct mwServiceFileTransfer *srvc_ft;
  struct mwService *srvc;

  g_return_val_if_fail(session != NULL, NULL);
  g_return_val_if_fail(handler != NULL, NULL);

  srvc_ft = g_new0(struct mwServiceFileTransfer, 1);
  srvc = MW_SERVICE(srvc_ft);

  mwService_init(srvc, session, mwService_FILE_TRANSFER);
  srvc->recv_create = (mwService_funcRecvCreate) recv_channelCreate;
  srvc->recv_accept = (mwService_funcRecvAccept) recv_channelAccept;
  srvc->recv_destroy = (mwService_funcRecvDestroy) recv_channelDestroy;
  srvc->recv = recv;
  srvc->clear = (mwService_funcClear) clear;
  srvc->get_name = name;
  srvc->get_desc = desc;
  srvc->start = start;
  srvc->stop = (mwService_funcStop) stop;

  srvc_ft->handler = handler;

  return srvc_ft;
}


struct mwFileTransferHandler *
mwServiceFileTransfer_getHandler(struct mwServiceFileTransfer *srvc) {
  g_return_val_if_fail(srvc != NULL, NULL);
  return srvc->handler;
}


struct mwFileTransfer *
mwFileTransfer_new(struct mwServiceFileTransfer *srvc,
		   const struct mwIdBlock *who, const char *msg,
		   const char *filename, guint32 filesize) {
  
  struct mwFileTransfer *ft;

  g_return_val_if_fail(srvc != NULL, NULL);
  g_return_val_if_fail(who != NULL, NULL);
  
  ft = g_new0(struct mwFileTransfer, 1);
  ft->service = srvc;
  mwIdBlock_clone(&ft->who, who);
  ft->filename = g_strdup(filename);
  ft->message = g_strdup(msg);
  ft->size = ft->remaining = filesize;

  /* stick a reference in the service */
  srvc->transfers = g_list_prepend(srvc->transfers, ft);

  return ft;
}


const struct mwIdBlock *
mwFileTransfer_getTarget(struct mwFileTransfer *ft) {
  g_return_val_if_fail(ft != NULL, NULL);
  return &ft->who;
}


const char *
mwFileTransfer_getMessage(struct mwFileTransfer *ft) {
  g_return_val_if_fail(ft != NULL, NULL);
  return ft->message;
}


const char *
mwFileTransfer_getFileName(struct mwFileTransfer *ft) {
  g_return_val_if_fail(ft != NULL, NULL);
  return ft->filename;
}


guint32 mwFileTransfer_getFileSize(struct mwFileTransfer *ft) {
  g_return_val_if_fail(ft != NULL, 0);
  return ft->size;
}


guint32 mwFileTransfer_getRemaining(struct mwFileTransfer *ft) {
  g_return_val_if_fail(ft != NULL, 0);
  return ft->remaining;
}


int mwFileTransfer_accept(struct mwFileTransfer *ft) {
  struct mwServiceFileTransfer *srvc;
  struct mwFileTransferHandler *handler;
  int ret;

  g_return_val_if_fail(ft != NULL, -1);
  g_return_val_if_fail(ft->channel != NULL, -1);
  g_return_val_if_fail(mwChannel_isIncoming(ft->channel), -1);
  g_return_val_if_fail(mwChannel_isState(ft->channel, mwChannel_WAIT), -1);

  g_return_val_if_fail(ft->service != NULL, -1);
  srvc = ft->service;

  g_return_val_if_fail(srvc->handler != NULL, -1);
  handler = srvc->handler;

  ret = mwChannel_accept(ft->channel);
  if(!ret && handler->ft_opened)
    handler->ft_opened(ft);

  return ret;
}


int mwFileTransfer_close(struct mwFileTransfer *ft, guint32 code) {
  struct mwFileTransferHandler *handler;
  int ret;

  g_return_val_if_fail(ft != NULL, -1);
  g_return_val_if_fail(ft->channel != NULL, -1);

  ret = mwChannel_destroy(ft->channel, code, NULL);
  ft->channel = NULL;

  if(!ret && handler->ft_closed)
    handler->ft_closed(ft, code);

  return ret;
}


void mwFileTransfer_free(struct mwFileTransfer *ft) {
  struct mwServiceFileTransfer *srvc;

  if(! ft) return;

  if(ft->channel)
    mwFileTransfer_cancel(ft);

  mwFileTransfer_removeClientData(ft);

  srvc = ft->service;
  if(srvc) srvc->transfers = g_list_remove(srvc->transfers, ft);

  mwIdBlock_clear(&ft->who);
  g_free(ft->filename);
  g_free(ft->message);
  g_free(ft);
}


int mwFileTransfer_send(struct mwFileTransfer *ft,
			struct mwOpaque *data, gboolean done) {

  struct mwChannel *chan;
  int ret;

  g_return_val_if_fail(ft->channel != NULL, -1);
  chan = ft->channel;

  g_return_val_if_fail(mwChannel_isOutgoing(chan), -1);

  if(data->len > ft->remaining) {
    /* @todo handle error */
    return -1;
  }

  ret = mwChannel_send(chan, msg_TRANSFER, data);
  if(! ret) ft->remaining -= data->len;
  return ret;
}


void mwFileTransfer_setClientData(struct mwFileTransfer *ft,
				  gpointer data, GDestroyNotify clean) {
  g_return_if_fail(ft != NULL);
  mw_datum_set(&ft->client_data, data, clean);
}


gpointer mwFileTransfer_getClientData(struct mwFileTransfer *ft) {
  g_return_val_if_fail(ft != NULL, NULL);
  return mw_datum_get(&ft->client_data);
}


void mwFileTransfer_removeClientData(struct mwFileTransfer *ft) {
  g_return_if_fail(ft != NULL);
  mw_datum_clear(&ft->client_data);
}

