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


#include <glib.h>
#include "mw_common.h"
#include "mw_debug.h"
#include "mw_message.h"


static void MwMessageHead_put(MwPutBuffer *b, const MwMessage *msg) {
  mw_uint16_put(b, msg->type);
  mw_uint16_put(b, msg->options);
  mw_uint32_put(b, msg->channel);
  
  if(msg->options & mw_message_option_ATTRIBS)
    MwOpaque_put(b, &msg->attribs);
}


static void MwMessageHead_get(MwGetBuffer *b, MwMessage *msg) {

  if(MwGetBuffer_error(b)) return;

  mw_uint16_get(b, &msg->type);
  mw_uint16_get(b, &msg->options);
  mw_uint32_get(b, &msg->channel);

  if(msg->options & mw_message_option_ATTRIBS)
    MwOpaque_get(b, &msg->attribs);
}


static void MwMessageHead_clone(MwMessage *to,
				MwMessage *from) {

  to->type = from->type;
  to->options = from->options;
  to->channel = from->channel;
  MwOpaque_clone(&to->attribs, &from->attribs);
}


static void MwMessageHead_clear(MwMessage *msg) {
  MwOpaque_clear(&msg->attribs);
}


static void MwMsgHandshake_put(MwPutBuffer *b, const MwMsgHandshake *msg) {
  mw_uint16_put(b, msg->major);
  mw_uint16_put(b, msg->minor);
  mw_uint32_put(b, msg->head.channel);
  mw_uint32_put(b, msg->srvrcalc_addr);
  mw_uint16_put(b, msg->client_type);
  mw_uint32_put(b, msg->loclcalc_addr);
  
  if(msg->major >= 0x001e && msg->minor >= 0x001d) {
    mw_uint16_put(b, msg->unknown_a);
    mw_uint32_put(b, msg->unknown_b);
    mw_str_put(b, msg->local_host);
  }
}


static void MwMsgHandshake_get(MwGetBuffer *b, MwMsgHandshake *msg) {
  if(MwGetBuffer_error(b)) return;

  mw_uint16_get(b, &msg->major);
  mw_uint16_get(b, &msg->minor);
  mw_uint32_get(b, &msg->head.channel);
  mw_uint32_get(b, &msg->srvrcalc_addr);
  mw_uint16_get(b, &msg->client_type);
  mw_uint32_get(b, &msg->loclcalc_addr);

  if(msg->major >= 0x001e && msg->minor >= 0x001d) {
    mw_uint16_get(b, &msg->unknown_a);
    mw_uint32_get(b, &msg->unknown_b);
    mw_str_get(b, &msg->local_host);
  }
}


static void MwMsgHandshake_clear(MwMsgHandshake *msg) {
  ; /* nothing to clean up */
}


static void MwMsgHandshakeAck_get(MwGetBuffer *b,
				  MwMsgHandshakeAck *msg) {

  if(MwGetBuffer_error(b)) return;

  mw_uint16_get(b, &msg->major);
  mw_uint16_get(b, &msg->minor);
  mw_uint32_get(b, &msg->srvrcalc_addr);

  /** @todo: get a better handle on what versions support what parts
      of this message. eg: minor version 0x0018 doesn't send the
      following */
  if(msg->major >= 0x1e && msg->minor > 0x18) {
    mw_uint32_get(b, &msg->magic);
    MwOpaque_get(b, &msg->data);
  }
}


static void MwMsgHandshakeAck_put(MwPutBuffer *b,
				  const MwMsgHandshakeAck *msg) {

  mw_uint16_put(b, msg->major);
  mw_uint16_put(b, msg->minor);
  mw_uint32_put(b, msg->srvrcalc_addr);

  if(msg->major >= 0x1e && msg->minor > 0x18) {
    mw_uint32_put(b, msg->magic);
    MwOpaque_put(b, &msg->data);
  }
}


static void MwMsgHandshakeAck_clear(MwMsgHandshakeAck *msg) {
  MwOpaque_clear(&msg->data);
}


static void MwMsgLogin_put(MwPutBuffer *b, const MwMsgLogin *msg) {
  mw_uint16_put(b, msg->client_type);
  mw_str_put(b, msg->name);

  /* ordering reversed from houri draft?? */
  MwOpaque_put(b, &msg->auth_data);
  mw_uint16_put(b, msg->auth_type);

  mw_uint16_put(b, 0x0000); /* unknown */
}


static void MwMsgLogin_get(MwGetBuffer *b, MwMsgLogin *msg) {
  if(MwGetBuffer_error(b)) return;

  mw_uint16_get(b, &msg->client_type);
  mw_str_get(b, &msg->name);
  MwOpaque_get(b, &msg->auth_data);
  mw_uint16_get(b, &msg->auth_type);
}


static void MwMsgLogin_clear(MwMsgLogin *msg) {
  g_free(msg->name);  msg->name = NULL;
  MwOpaque_clear(&msg->auth_data);
}


static void MwMsgLoginAck_get(MwGetBuffer *b, MwMsgLoginAck *msg) {
  guint16 junk;

  if(MwGetBuffer_error(b)) return;

  MwLogin_get(b, &msg->login);
  mw_uint16_get(b, &junk);
  MwPrivacy_get(b, &msg->privacy);
  MwStatus_get(b, &msg->status);
}


static void MwMsgLoginAck_clear(MwMsgLoginAck *msg) {
  MwLogin_clear(&msg->login, TRUE);
  MwPrivacy_clear(&msg->privacy, TRUE);
  MwStatus_clear(&msg->status, TRUE);
}


static void MwMsgLoginForce_put(MwPutBuffer *b,
				const MwMsgLoginForce *msg) {

  ; /* nothing but a message header */
}


static void MwMsgLoginForce_get(MwGetBuffer *b,
				MwMsgLoginForce *msg) {

  ; /* nothing but a message header */
}


static void MwMsgLoginForce_clear(MwMsgLoginForce *msg) {
  ; /* this is a very simple message */
}


static void MwMsgLoginRedirect_get(MwGetBuffer *b,
				   MwMsgLoginRedirect *msg) {

  if(MwGetBuffer_error(b)) return;
  mw_str_get(b, &msg->host);
  mw_str_get(b, &msg->server_id);  
}


static void MwMsgLoginRedirect_put(MwPutBuffer *b,
				   const MwMsgLoginRedirect *msg) {
  mw_str_put(b, msg->host);
  mw_str_put(b, msg->server_id);
}


static void MwMsgLoginRedirect_clear(MwMsgLoginRedirect *msg) {
  g_free(msg->host);
  msg->host = NULL;

  g_free(msg->server_id);
  msg->server_id = NULL;
}


static void enc_offer_put(MwPutBuffer *b, const MwMsgChannelCreate *msg) {
  mw_uint16_put(b, msg->enc_mode);

  if(msg->enc_mode && msg->enc_count) {
    guint32 i, count;
    MwPutBuffer *p;
    MwOpaque o;

    /* write the count, items, extra, and flag into a tmp buffer,
       render that buffer into an opaque, and write it into b */

    p = MwPutBuffer_new();

    count = msg->enc_count;
    mw_uint32_put(p, count);

    for(i = 0; i < count; i++) {
      MwEncItem *ei = msg->enc_items + i;
      mw_uint16_put(p, ei->cipher);
      MwOpaque_put(p, &ei->info);
    }

    mw_uint16_put(p, msg->enc_extra);
    mw_boolean_put(p, msg->enc_flag);

    MwPutBuffer_free(p, &o);
    MwOpaque_put(b, &o);
    MwOpaque_clear(&o);
  }
}


static void MwMsgChannelCreate_put(MwPutBuffer *b,
				   const MwMsgChannelCreate *msg) {

  mw_uint32_put(b, msg->reserved);
  mw_uint32_put(b, msg->channel);
  MwIdentity_put(b, &msg->target);
  mw_uint32_put(b, msg->service);
  mw_uint32_put(b, msg->proto_type);
  mw_uint32_put(b, msg->proto_ver);
  mw_uint32_put(b, msg->options);
  MwOpaque_put(b, &msg->addtl);
  mw_boolean_put(b, msg->creator_flag);

  if(msg->creator_flag)
    MwLogin_put(b, &msg->creator);

  enc_offer_put(b, msg);

  /*
  mw_uint32_put(b, 0x00);
  mw_uint32_put(b, 0x00);
  */

  mw_uint16_put(b, 0x07);
}


static void enc_offer_get(MwGetBuffer *b,
			  MwMsgChannelCreate *msg) {

  if(MwGetBuffer_error(b)) return;

  mw_uint16_get(b, &msg->enc_mode);
  mw_uint32_skip(b);

  if(msg->enc_mode) {
    guint32 i, count;

    mw_uint32_get(b, &count);
    msg->enc_count = count;

    msg->enc_items = g_new0(MwEncItem, count);

    for(i = 0; i < count; i++) {
      MwEncItem *ei = msg->enc_items + i;
      mw_uint16_get(b, &ei->cipher);
      MwOpaque_get(b, &ei->info);
    }

    mw_uint16_get(b, &msg->enc_extra);
    mw_boolean_get(b, &msg->enc_flag);
  }
}


static void MwMsgChannelCreate_get(MwGetBuffer *b,
				   MwMsgChannelCreate *msg) {

  if(MwGetBuffer_error(b)) return;

  mw_uint32_get(b, &msg->reserved);
  mw_uint32_get(b, &msg->channel);
  MwIdentity_get(b, &msg->target);
  mw_uint32_get(b, &msg->service);
  mw_uint32_get(b, &msg->proto_type);
  mw_uint32_get(b, &msg->proto_ver);
  mw_uint32_get(b, &msg->options);
  MwOpaque_get(b, &msg->addtl);
  mw_boolean_get(b, &msg->creator_flag);
  
  if(msg->creator_flag)
    MwLogin_get(b, &msg->creator);
  
  enc_offer_get(b, msg);
}


static void MwMsgChannelCreate_clear(MwMsgChannelCreate *msg) {
  guint32 count;

  MwIdentity_clear(&msg->target, TRUE);
  MwOpaque_clear(&msg->addtl);
  MwLogin_clear(&msg->creator, TRUE);
  
  count = msg->enc_count;
  while(count--) {
    MwEncItem *ei = msg->enc_items + count;
    MwOpaque_clear(&ei->info);
  }
  g_free(msg->enc_items);
}


static void enc_accept_put(MwPutBuffer *b,
			   const MwMsgChannelAccept *msg) {

  mw_uint16_put(b, msg->enc_mode);

  if(msg->enc_mode) {
    MwPutBuffer *p;
    MwOpaque o;

    p = MwPutBuffer_new();

    mw_uint16_put(p, msg->enc_item.cipher);
    MwOpaque_put(p, &msg->enc_item.info);
    mw_uint16_put(p, msg->enc_extra);
    mw_boolean_put(p, msg->enc_flag);

    MwPutBuffer_free(p, &o);
    MwOpaque_put(b, &o);
    MwOpaque_clear(&o);
  }
}


static void MwMsgChannelAccept_put(MwPutBuffer *b,
				   const MwMsgChannelAccept *msg) {
  
  mw_uint32_put(b, msg->service);
  mw_uint32_put(b, msg->proto_type);
  mw_uint32_put(b, msg->proto_ver);
  MwOpaque_put(b, &msg->addtl);
  mw_boolean_put(b, msg->acceptor_flag);
  
  if(msg->acceptor_flag)
    MwLogin_put(b, &msg->acceptor);
  
  enc_accept_put(b, msg);

  mw_uint16_put(b, 0x07);
}


static void enc_accept_get(MwGetBuffer *b,
			   MwMsgChannelAccept *msg) {

  if(MwGetBuffer_error(b)) return;

  mw_uint16_get(b, &msg->enc_mode);
  mw_uint32_skip(b);

  if(msg->enc_mode) {
    mw_uint16_get(b, &msg->enc_item.cipher);
    MwOpaque_get(b, &msg->enc_item.info);
    mw_uint16_get(b, &msg->enc_extra);
    mw_boolean_get(b, &msg->enc_flag);
  }
}


static void MwMsgChannelAccept_get(MwGetBuffer *b,
				   MwMsgChannelAccept *msg) {

  if(MwGetBuffer_error(b)) return;

  mw_uint32_get(b, &msg->service);
  mw_uint32_get(b, &msg->proto_type);
  mw_uint32_get(b, &msg->proto_ver);
  MwOpaque_get(b, &msg->addtl);
  mw_boolean_get(b, &msg->acceptor_flag);

  if(msg->acceptor_flag)
    MwLogin_get(b, &msg->acceptor);

  enc_accept_get(b, msg);
}


static void MwMsgChannelAccept_clear(MwMsgChannelAccept *msg) {
  MwOpaque_clear(&msg->addtl);
  MwLogin_clear(&msg->acceptor, TRUE);
  MwOpaque_clear(&msg->enc_item.info);
}


static void MwMsgChannelSend_put(MwPutBuffer *b,
				 const MwMsgChannelSend *msg) {

  mw_uint16_put(b, msg->type);
  MwOpaque_put(b, &msg->data);
}


static void MwMsgChannelSend_get(MwGetBuffer *b,
				 MwMsgChannelSend *msg) {

  if(MwGetBuffer_error(b)) return;

  mw_uint16_get(b, &msg->type);
  MwOpaque_get(b, &msg->data);
}


static void MwMsgChannelSend_clear(MwMsgChannelSend *msg) {
  MwOpaque_clear(&msg->data);
}


static void MwMsgChannelClose_put(MwPutBuffer *b,
				  const MwMsgChannelClose *msg) {
  mw_uint32_put(b, msg->reason);
  MwOpaque_put(b, &msg->data);
}


static void MwMsgChannelClose_get(MwGetBuffer *b,
				  MwMsgChannelClose *msg) {

  if(MwGetBuffer_error(b)) return;

  mw_uint32_get(b, &msg->reason);
  MwOpaque_get(b, &msg->data);
}


static void MwMsgChannelClose_clear(MwMsgChannelClose *msg) {
  MwOpaque_clear(&msg->data);
}


static void MwMsgOneTime_put(MwPutBuffer *b, const MwMsgOneTime *msg) {
  mw_uint32_put(b, msg->id);
  MwIdentity_put(b, &msg->target);
  mw_uint32_put(b, msg->service);
  mw_uint32_put(b, msg->proto_type);
  mw_uint32_put(b, msg->proto_ver);
  mw_uint16_put(b, msg->type);
  MwOpaque_put(b, &msg->data);
}


static void MwMsgOneTime_get(MwGetBuffer *b, MwMsgOneTime *msg) {
  if(MwGetBuffer_error(b)) return;

  mw_uint32_get(b, &msg->id);
  MwIdentity_get(b, &msg->target);
  mw_uint32_get(b, &msg->service);
  mw_uint32_get(b, &msg->proto_type);
  mw_uint32_get(b, &msg->proto_ver);
  mw_uint16_get(b, &msg->type);
  MwOpaque_get(b, &msg->data);
}


static void MwMsgOneTime_clear(MwMsgOneTime *msg) {
  MwIdentity_clear(&msg->target, TRUE);
  MwOpaque_clear(&msg->data);
}


static void MwMsgStatus_put(MwPutBuffer *b,
			    const MwMsgStatus *msg) {
  MwStatus_put(b, &msg->status);
}


static void MwMsgStatus_get(MwGetBuffer *b,
			    MwMsgStatus *msg) {

  if(MwGetBuffer_error(b)) return;
  MwStatus_get(b, &msg->status);
}


static void MwMsgStatus_clear(MwMsgStatus *msg) {
  MwStatus_clear(&msg->status, TRUE);
}


static void MwMsgPrivacy_put(MwPutBuffer *b,
			     const MwMsgPrivacy *msg) {
  MwPrivacy_put(b, &msg->privacy);
}


static void MwMsgPrivacy_get(MwGetBuffer *b,
			     MwMsgPrivacy *msg) {

  if(MwGetBuffer_error(b)) return;
  MwPrivacy_get(b, &msg->privacy);
}


static void MwMsgPrivacy_clear(MwMsgPrivacy *msg) {
  MwPrivacy_clear(&msg->privacy, TRUE);
}


static void MwMsgSenseService_put(MwPutBuffer *b,
				  const MwMsgSenseService *msg) {
  mw_uint32_put(b, msg->service);
}


static void MwMsgSenseService_get(MwGetBuffer *b,
				  MwMsgSenseService *msg) {

  if(MwGetBuffer_error(b)) return;
  mw_uint32_get(b, &msg->service);
}


static void MwMsgSenseService_clear(MwMsgSenseService *msg) {
  ;
}


static void MwMsgAdmin_get(MwGetBuffer *b, MwMsgAdmin *msg) {
  mw_str_get(b, &msg->text);
}


static void MwMsgAdmin_clear(MwMsgAdmin *msg) {
  g_free(msg->text);
  msg->text = NULL;
}


static void MwMsgAnnounce_get(MwGetBuffer *b, MwMsgAnnounce *msg) {
  MwOpaque o = { 0, 0 };
  MwGetBuffer *gb;
  guint32 i, count;

  mw_boolean_get(b, &msg->sender_flag);
  if(msg->sender_flag)
    MwLogin_get(b, &msg->sender);
  mw_uint16_get(b, &msg->unknown_a);
  
  MwOpaque_get(b, &o);
  gb = MwGetBuffer_wrap(&o);

  mw_boolean_get(gb, &msg->may_reply);
  mw_str_get(gb, &msg->text);

  MwGetBuffer_free(gb);
  MwOpaque_clear(&o);

  mw_uint32_get(b, &count);
  msg->rcpt_count = count;
  msg->recipients = g_new0(gchar *, count);

  for(i = 0; i < count; i++) {
    mw_str_get(b, msg->recipients + i);
  }
}


static void MwMsgAnnounce_put(MwPutBuffer *b, const MwMsgAnnounce *msg) {
  MwOpaque o = { 0, 0 };
  MwPutBuffer *pb;
  guint32 i, count;
  
  mw_boolean_put(b, msg->sender_flag);
  if(msg->sender_flag)
    MwLogin_put(b, &msg->sender);
  mw_uint16_put(b, msg->unknown_a);

  pb = MwPutBuffer_new();
  
  mw_boolean_put(pb, msg->may_reply);
  mw_str_put(pb, msg->text);

  MwPutBuffer_free(pb, &o);
  MwOpaque_put(b, &o);
  MwOpaque_clear(&o);

  count = msg->rcpt_count;
  mw_uint32_put(b, count);
  for(i = 0; i < count; i++) {
    mw_str_put(pb, msg->recipients[i]);
  }
}


static void MwMsgAnnounce_clear(MwMsgAnnounce *msg) {
  guint32 i;
  
  MwLogin_clear(&msg->sender, TRUE);

  g_free(msg->text);
  msg->text = NULL;
  
  for(i = msg->rcpt_count; i--; ) {
    g_free(msg->recipients + i);
  }
  g_free(msg->recipients);
  msg->recipients = NULL;
}


/* general functions */


gpointer MwMessage_new(enum mw_message_type type) {
  MwMessage *msg = NULL;
  
  switch(type) {
  case mw_message_HANDSHAKE:
    msg = MW_MESSAGE(g_new0(MwMsgHandshake, 1));
    break;
  case mw_message_HANDSHAKE_ACK:
    msg = MW_MESSAGE(g_new0(MwMsgHandshakeAck, 1));
    break;
  case mw_message_LOGIN:
    msg = MW_MESSAGE(g_new0(MwMsgLogin, 1));
    break;
  case mw_message_LOGIN_ACK:
    msg = MW_MESSAGE(g_new0(MwMsgLoginAck, 1));
    break;
  case mw_message_LOGIN_REDIRECT:
    msg = MW_MESSAGE(g_new0(MwMsgLoginRedirect, 1));
    break;
  case mw_message_LOGIN_FORCE:
    msg = MW_MESSAGE(g_new0(MwMsgLoginForce, 1));
    break;

  case mw_message_CHANNEL_CREATE:
    msg = MW_MESSAGE(g_new0(MwMsgChannelCreate, 1));
    break;
  case mw_message_CHANNEL_CLOSE:
    msg = MW_MESSAGE(g_new0(MwMsgChannelClose, 1));
    break;
  case mw_message_CHANNEL_SEND:
    msg = MW_MESSAGE(g_new0(MwMsgChannelSend, 1));
    break;
  case mw_message_CHANNEL_ACCEPT:
    msg = MW_MESSAGE(g_new0(MwMsgChannelAccept, 1));
    break;
    
  case mw_message_ONE_TIME:
    msg = MW_MESSAGE(g_new0(MwMsgOneTime, 1));
    break;

  case mw_message_STATUS:
    msg = MW_MESSAGE(g_new0(MwMsgStatus, 1));
    break;
  case mw_message_PRIVACY:
    msg = MW_MESSAGE(g_new0(MwMsgPrivacy, 1));
    break;
  case mw_message_SENSE_SERVICE:
    msg = MW_MESSAGE(g_new0(MwMsgSenseService, 1));
    break;
  case mw_message_ADMIN:
    msg = MW_MESSAGE(g_new0(MwMsgAdmin, 1));
    break;
  case mw_message_ANNOUNCE:
    msg = MW_MESSAGE(g_new0(MwMsgAnnounce, 1));
    break;
    
  default:
    g_warning("unknown message type 0x%02x\n", type);
  }
  
  if(msg) msg->type = type;
    
  return msg;
}


gpointer MwMessage_get(MwGetBuffer *b) {
  gpointer msg = NULL;
  MwMessage head;
  
  g_return_val_if_fail(b != NULL, NULL);

  head.attribs.len = 0;
  head.attribs.data = NULL;

  /* attempt to read the header first */
  MwMessageHead_get(b, &head);

  if(MwGetBuffer_error(b)) {
    MwMessageHead_clear(&head);
    g_warning("problem parsing message head from buffer");
    return NULL;
  }
  
  /* load the rest of the message depending on the header type */
  switch(head.type) {
  case mw_message_HANDSHAKE:
    msg = g_new0(MwMsgHandshake, 1);
    MwMsgHandshake_get(b, msg);
    break;
  case mw_message_HANDSHAKE_ACK:
    msg = g_new0(MwMsgHandshakeAck, 1);
    MwMsgHandshakeAck_get(b, msg);
    break;
  case mw_message_LOGIN:
    msg = g_new0(MwMsgLogin, 1);
    MwMsgLogin_get(b, msg);
    break;
  case mw_message_LOGIN_REDIRECT:
    msg = g_new0(MwMsgLoginRedirect, 1);
    MwMsgLoginRedirect_get(b, msg);
    break;
  case mw_message_LOGIN_FORCE:
    msg = g_new0(MwMsgLoginForce, 1);
    MwMsgLoginForce_get(b, msg);
    break;
  case mw_message_LOGIN_ACK:
    msg = g_new0(MwMsgLoginAck, 1);
    MwMsgLoginAck_get(b, msg);
    break;

  case mw_message_CHANNEL_CREATE:
    msg = g_new0(MwMsgChannelCreate, 1);
    MwMsgChannelCreate_get(b, msg);
    break;
  case mw_message_CHANNEL_CLOSE:
    msg = g_new0(MwMsgChannelClose, 1);
    MwMsgChannelClose_get(b, msg);
    break;
  case mw_message_CHANNEL_SEND:
    msg = g_new0(MwMsgChannelSend, 1);
    MwMsgChannelSend_get(b, msg);
    break;
  case mw_message_CHANNEL_ACCEPT:
    msg = g_new0(MwMsgChannelAccept, 1);
    MwMsgChannelAccept_get(b, msg);
    break;

  case mw_message_ONE_TIME:
    msg = g_new0(MwMsgOneTime, 1);
    MwMsgOneTime_get(b, msg);
    break;

  case mw_message_STATUS:
    msg = g_new0(MwMsgStatus, 1);
    MwMsgStatus_get(b, msg);
    break;
  case mw_message_PRIVACY:
    msg = g_new0(MwMsgPrivacy, 1);
    MwMsgPrivacy_get(b, msg);
    break;
  case mw_message_SENSE_SERVICE:
    msg = g_new0(MwMsgSenseService, 1);
    MwMsgSenseService_get(b, msg);
    break;
  case mw_message_ADMIN:
    msg = g_new0(MwMsgAdmin, 1);
    MwMsgAdmin_get(b, msg);
    break;
  case mw_message_ANNOUNCE:
    msg = g_new0(MwMsgAnnounce, 1);
    MwMsgAnnounce_get(b, msg);
    break;

  default:
    g_warning("unknown message type 0x%02x, no parse handler", head.type);
  }
  
  if(MwGetBuffer_error(b)) {
    g_warning("problem parsing message type 0x%02x, not enough data",
	      head.type);
  }
  
  if(msg) MwMessageHead_clone(msg, &head);
  MwMessageHead_clear(&head);
  
  return msg;
}


void MwMessage_put(MwPutBuffer *b, const MwMessage *msg) {

  /* just to save me from typing the casts */
  gconstpointer mptr = msg;

  g_return_if_fail(b != NULL);
  g_return_if_fail(msg != NULL);

  MwMessageHead_put(b, msg);

  switch(msg->type) {
  case mw_message_HANDSHAKE:
    MwMsgHandshake_put(b, mptr);
    break;
  case mw_message_HANDSHAKE_ACK:
    MwMsgHandshakeAck_put(b, mptr);
    break;
  case mw_message_LOGIN:
    MwMsgLogin_put(b, mptr);
    break;

#if 0
  case mw_message_LOGIN_ACK:
    MwMsgLoginAck_put(b, mptr);
    break;
#endif

  case mw_message_LOGIN_REDIRECT:
    MwMsgLoginRedirect_put(b, mptr);
    break;
  case mw_message_LOGIN_FORCE:
    MwMsgLoginForce_put(b, mptr);
    break;

  case mw_message_CHANNEL_CREATE:
    MwMsgChannelCreate_put(b, mptr);
    break;
  case mw_message_CHANNEL_CLOSE:
    MwMsgChannelClose_put(b, mptr);
    break;
  case mw_message_CHANNEL_SEND:
    MwMsgChannelSend_put(b, mptr);
    break;
  case mw_message_CHANNEL_ACCEPT:
    MwMsgChannelAccept_put(b, mptr);
    break;

  case mw_message_ONE_TIME:
    MwMsgOneTime_put(b, mptr);
    break;

  case mw_message_STATUS:
    MwMsgStatus_put(b, mptr);
    break;
  case mw_message_PRIVACY:
    MwMsgPrivacy_put(b, mptr);
    break;
  case mw_message_SENSE_SERVICE:
    MwMsgSenseService_put(b, mptr);
    break;

#if 0
  case mw_message_ADMIN:
    MwMsgAdmin_put(b, mptr);
    break;
#endif

  case mw_message_ANNOUNCE:
    MwMsgAnnounce_put(b, mptr);
    break;
    
  default:
    ; /* hrm. */
  }
}


void MwMessage_clear(MwMessage *msg) {

  /* just to save me from typing the casts */
  gpointer mptr = msg;
  
  g_return_if_fail(msg != NULL);

  switch(msg->type) {
  case mw_message_HANDSHAKE:
    MwMsgHandshake_clear(mptr);
    break;
  case mw_message_HANDSHAKE_ACK:
    MwMsgHandshakeAck_clear(mptr);
    break;
  case mw_message_LOGIN:
    MwMsgLogin_clear(mptr);
    break;
  case mw_message_LOGIN_ACK:
    MwMsgLoginAck_clear(mptr);
    break;
  case mw_message_LOGIN_REDIRECT:
    MwMsgLoginRedirect_clear(mptr);
    break;
  case mw_message_LOGIN_FORCE:
    MwMsgLoginForce_clear(mptr);
    break;

  case mw_message_CHANNEL_CREATE:
    MwMsgChannelCreate_clear(mptr);
    break;
  case mw_message_CHANNEL_CLOSE:
    MwMsgChannelClose_clear(mptr);
    break;
  case mw_message_CHANNEL_SEND:
    MwMsgChannelSend_clear(mptr);
    break;
  case mw_message_CHANNEL_ACCEPT:
    MwMsgChannelAccept_clear(mptr);
    break;

  case mw_message_ONE_TIME:
    MwMsgOneTime_clear(mptr);
    break;

  case mw_message_STATUS:
    MwMsgStatus_clear(mptr);
    break;
  case mw_message_PRIVACY:
    MwMsgPrivacy_clear(mptr);
    break;
  case mw_message_SENSE_SERVICE:
    MwMsgSenseService_clear(mptr);
    break;
  case mw_message_ADMIN:
    MwMsgAdmin_clear(mptr);
    break;
  case mw_message_ANNOUNCE:
    MwMsgAnnounce_clear(mptr);
    break;
    
  default:
    ; /* hrm */
  }
  
  MwMessageHead_clear(msg);
}


void MwMessage_free(MwMessage *msg) {
  if(! msg) return;
  MwMessage_clear(msg);
  g_free(msg);
}


/* The end. */
