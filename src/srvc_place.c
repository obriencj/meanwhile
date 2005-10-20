
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
#include <glib/ghash.h>
#include <glib/glist.h>

#include <stdio.h>
#include <stdlib.h>

#include "mw_channel.h"
#include "mw_common.h"
#include "mw_debug.h"
#include "mw_error.h"
#include "mw_message.h"
#include "mw_service.h"
#include "mw_session.h"
#include "mw_srvc_place.h"
#include "mw_util.h"


#define PROTOCOL_TYPE  0x00
#define PROTOCOL_VER   0x05


enum incoming_msg {
  msg_in_JOIN_RESPONSE  = 0x0000,  /* ? */
  msg_in_INFO           = 0x0002,
  msg_in_MESSAGE        = 0x0004,
  msg_in_SECTION        = 0x0014,  /* see in_section_subtype */
  msg_in_UNKNOWNa       = 0x0015,
};


enum in_section_subtype {
  msg_in_SECTION_LIST  = 0x0000,  /* list of section members */
  msg_in_SECTION_PEER  = 0x0001,  /* see in_section_peer_subtye */
  msg_in_SECTION_PART  = 0x0003,
};


enum in_section_peer_subtype {
  msg_in_SECTION_PEER_JOIN        = 0x0000,
  msg_in_SECTION_PEER_PART        = 0x0001,  /* after msg_in_SECTION_PART */
  msg_in_SECTION_PEER_CLEAR_ATTR  = 0x0003,
  msg_in_SECTION_PEER_SET_ATTR    = 0x0004,
};


enum outgoing_msg {
  msg_out_JOIN_PLACE  = 0x0000,  /* ? */
  msg_out_PEER_INFO   = 0x0002,  /* ? */
  msg_out_MESSAGE     = 0x0003,
  msg_out_OLD_INVITE  = 0x0005,  /* old-style conf. invitation */
  msg_out_SET_ATTR    = 0x000a,
  msg_out_CLEAR_ATTR  = 0x000b,
  msg_out_SECTION     = 0x0014,  /* see out_section_subtype */
  msg_out_UNKNOWNb    = 0x001e,  /* ? maybe enter stage ? */
};


enum out_section_subtype {
  msg_out_SECTION_LIST  = 0x0002,  /* req list of members */
  msg_out_SECTION_PART  = 0x0003,
};


/*
  : allocate section
  : state = NEW

  : create channel
  : state = PENDING

  : channel accepted
  : msg_out_JOIN_PLACE  (maybe create?)
  : state = JOINING

  : msg_in_JOIN_RESPONSE (contains our place member ID and section ID)
  : msg_in_INFO (for place, not peer)
  : msg_in_UNKNOWNa

  : msg_out_SECTION_LIST (asking for all sections) (optional)
  : msg_in_SECTION_LIST (listing all sections, as requested above)

  : msg_out_PEER_INFO (with our place member ID) (optional)
  : msg_in_INFO (peer info as requested above)

  : msg_out_SECTION_LIST (with our section ID) (sorta optional)
  : msg_in_SECTION_LIST (section listing as requested above)

  : msg_out_UNKNOWNb
  : msg_in_SECTION_PEER_JOINED (empty, with our place member ID)
  : msg_in_UNKNOWNa
  : state = OPENED

  : stuff... (invites, joins, parts, messages, attr)

  : state = CLOSING
  : msg_out_SECTION_PART
  : destroy channel
  : deallocate section
*/


struct mwServicePlace {
  struct mwService service;
  GList *places;
};


enum mwPlaceState {
  mwPlace_NEW,
  mwPlace_PENDING,
  mwPlace_JOINING,
  mwPlace_JOINED,
  mwPlace_OPEN,
  mwPlace_CLOSING,
  mwPlace_ERROR,
  mwPlace_UNKNOWN,
};


struct mwPlace {
  struct mwServicePlace *service;

  struct mwPlaceHandler *handler;

  enum mwPlaceState state;
  struct mwChannel *channel;

  char *name;
  char *title;
  GHashTable *members;  /* mapping of member ID: place_member */
  guint32 our_id;       /* our member ID */
  guint32 section;      /* the section we're using */

  guint32 requests;     /* counter for requests */

  struct mw_datum client_data;
};


struct place_member {
  guint32 place_id;
  guint16 unknown;
  char *id;
  char *community;
  char *login_id;
  char *name;
  guint16 type;
};


#define GET_MEMBER(place, id) \
  (g_hash_table_lookup(place->members, GUINT_TO_POINTER(id)))


#define PUT_MEMBER(place, member) \
  (g_hash_table_insert(place->members, \
                       GUINT_TO_POINTER(member->place_id), member))


#define REMOVE_MEMBER(place, id) \
  (g_hash_table_remove(place->members, GUINT_TO_POINTER(id)))


static void member_free(struct place_member *p) {
  g_free(p->id);
  g_free(p->community);
  g_free(p->login_id);
  g_free(p->name);
  g_free(p);
}


static const struct mwIdBlock *
member_as_id_block(struct place_member *p) {
  static struct mwIdBlock idb;

  idb.user = p->id;
  idb.community = p->community;

  return &idb;
}


__attribute__((used))
static const struct mwLoginInfo *
member_as_login_info(struct place_member *p) {
  static struct mwLoginInfo li;
  
  li.login_id = p->login_id;
  li.type = p->type;
  li.user_id = p->id;
  li.user_name = p->name;
  li.community = p->community;
  li.full = FALSE;

  return &li;
}


static const char *place_state_str(enum mwPlaceState s) {
  switch(s) {
  case mwPlace_NEW:      return "new";
  case mwPlace_PENDING:  return "pending";
  case mwPlace_JOINING:  return "joining";
  case mwPlace_JOINED:   return "joined";
  case mwPlace_OPEN:     return "open";
  case mwPlace_CLOSING:  return "closing";
  case mwPlace_ERROR:    return "error";

  case mwPlace_UNKNOWN:  /* fall-through */
  default:               return "UNKNOWN";
  }
}


static void place_state(struct mwPlace *place, enum mwPlaceState s) {
  g_return_if_fail(place != NULL);
  
  if(place->state == s) return;

  place->state = s;
  g_message("place %s state: %s", NSTR(place->name), place_state_str(s));
}


static void place_free(struct mwPlace *place) {
  struct mwServicePlace *srvc;

  if(! place) return;
  
  srvc = place->service;
  g_return_if_fail(srvc != NULL);

  if(place->handler && place->handler->clear)
    place->handler->clear(place);

  mw_datum_clear(&place->client_data);

  g_hash_table_destroy(place->members);

  g_free(place->name);
  g_free(place->title);
  g_free(place);
}


static int recv_JOIN_RESPONSE(struct mwPlace *place,
			      struct mwGetBuffer *b) {

  struct mwServicePlace *srvc;
  struct mwSession *session;
  struct mwLoginInfo *me;
  
  struct place_member *pm;
  guint32 our_id, section;

  int ret = 0;

  guint32_get(b, &our_id);
  guint32_get(b, &section);

  place->our_id = our_id;
  place->section = section;

  srvc = place->service;
  session = mwService_getSession(MW_SERVICE(srvc));
  me = mwSession_getLoginInfo(session);

  /* compose a place member entry for ourselves using the login info
     block on the session */

  pm = g_new0(struct place_member, 1);
  pm->place_id = our_id;
  pm->id = g_strdup(me->user_id);
  pm->community = g_strdup(me->community);
  pm->login_id = g_strdup(me->login_id);
  pm->name = g_strdup(me->user_name);
  pm->type = me->type;

  PUT_MEMBER(place, pm);

  return ret;
}


static int recv_INFO(struct mwPlace *place,
		     struct mwGetBuffer *b) {

  int ret = 0;

  return ret;
}


static int recv_MESSAGE(struct mwPlace *place,
			struct mwGetBuffer *b) {
  
  guint32 pm_id;
  guint32 unkn_a, unkn_b, ign;
  struct place_member *pm;
  const struct mwIdBlock *idb;
  char *msg = NULL;
  int ret = 0;

  /* regarding unkn_a and unkn_b:

     they're probably a section indicator and a message count, I'm
     just not sure which is which. Until this implementation supports
     place sections in the API, it really doesn't matter. */
  
  guint32_get(b, &pm_id);
  pm = GET_MEMBER(place, pm_id);
  g_return_val_if_fail(pm != NULL, -1);

  guint32_get(b, &unkn_a);
  guint32_get(b, &ign);     /* actually an opaque length */
  
  if(! ign) return ret;

  guint32_get(b, &unkn_b);
  mwString_get(b, &msg);

  idb = member_as_id_block(pm);

  if(place->handler && place->handler->place_message)
    place->handler->place_message(place, idb, msg);

  g_free(msg);

  return ret;
}


static int recv_SECTION_PEER_JOIN(struct mwPlace *place,
				  struct mwGetBuffer *b) {
  guint32 count;
  guint32 unknowna, unknownb;
  int ret = 0;

  guint32_get(b, &count);

  if(! count) {
    if(place->state == mwPlace_PENDING) {
      place_state(place, mwPlace_OPEN);
      return ret;
    }
  }

  while(count--) {
    struct place_member *pm = g_new0(struct place_member, 1);
    guint32_get(b, &pm->place_id);
    mwGetBuffer_advance(b, 4);
    guint16_get(b, &pm->unknown);
    mwString_get(b, &pm->id);
    mwString_get(b, &pm->community);
    mwString_get(b, &pm->login_id);
    mwString_get(b, &pm->name);
    guint16_get(b, &pm->type);

    PUT_MEMBER(place, pm);
    if(place->handler && place->handler->place_peerJoined)
      place->handler->place_peerJoined(place, member_as_id_block(pm));
  }

  guint32_get(b, &unknowna);
  guint32_get(b, &unknownb);

  return ret;
}


static int recv_SECTION_PEER_PART(struct mwPlace *place,
				  struct mwGetBuffer *b) {
  int ret = 0;
  return ret;
}


static int recv_SECTION_PEER_CLEAR_ATTR(struct mwPlace *place,
					struct mwGetBuffer *b) {
  int ret = 0;
  return ret;
}


static int recv_SECTION_PEER_SET_ATTR(struct mwPlace *place,
				      struct mwGetBuffer *b) {
  int ret = 0;
  return ret;
}


static int recv_SECTION_PEER(struct mwPlace *place,
			      struct mwGetBuffer *b) {
  guint16 subtype;
  int res;

  guint16_get(b, &subtype);

  g_return_val_if_fail(! mwGetBuffer_error(b), -1);

  switch(subtype) {
  case msg_in_SECTION_PEER_JOIN:
    res = recv_SECTION_PEER_JOIN(place, b);
    break;

  case msg_in_SECTION_PEER_PART:
    res = recv_SECTION_PEER_PART(place, b);
    break;

  case msg_in_SECTION_PEER_CLEAR_ATTR:
    res = recv_SECTION_PEER_CLEAR_ATTR(place, b);
    break;

  case msg_in_SECTION_PEER_SET_ATTR:
    res = recv_SECTION_PEER_SET_ATTR(place, b);
    break;

  default:
    res = -1;
  }

  return res;
}


static int recv_SECTION_LIST(struct mwPlace *place,
			     struct mwGetBuffer *b) {
  int ret = 0;
  return ret;
}


static int recv_SECTION_PART(struct mwPlace *place,
			     struct mwGetBuffer *b) {
  /* look up user in place
     remove user from place
     trigger event */

  guint32 pm_id;
  struct place_member *pm;
  const struct mwIdBlock *idb;

  guint32_get(b, &pm_id);

  pm = GET_MEMBER(place, pm_id);
  g_return_val_if_fail(pm != NULL, -1);

  idb = member_as_id_block(pm);
  if(place->handler && place->handler->place_peerParted)
    place->handler->place_peerParted(place, idb);

  REMOVE_MEMBER(place, pm_id);

  return 0;
}


static int recv_SECTION(struct mwPlace *place, struct mwGetBuffer *b) {
  guint16 subtype;
  int res;

  guint16_get(b, &subtype);

  g_return_val_if_fail(! mwGetBuffer_error(b), -1);

  switch(subtype) {
  case msg_in_SECTION_LIST:
    res = recv_SECTION_LIST(place, b);
    break;

  case msg_in_SECTION_PEER:
    res = recv_SECTION_PEER(place, b);
    break;

  case msg_in_SECTION_PART:
    res = recv_SECTION_PART(place, b);
    break;

  default:
    res = -1;
  }

  return res;
}


static void recv(struct mwService *service, struct mwChannel *chan,
		 guint16 type, struct mwOpaque *data) {

  struct mwPlace *place;
  struct mwGetBuffer *b;
  int res = 0;

  place = mwChannel_getServiceData(chan);
  g_return_if_fail(place != NULL);

  b = mwGetBuffer_wrap(data);
  switch(type) {
  case msg_in_JOIN_RESPONSE:
    res = recv_JOIN_RESPONSE(place, b);
    break;

  case msg_in_INFO:
    res = recv_INFO(place, b);
    break;

  case msg_in_MESSAGE:
    res = recv_MESSAGE(place, b);
    break;

  case msg_in_SECTION:
    res = recv_SECTION(place, b);
    break;

  case msg_in_UNKNOWNa:
    break;

  default:
    mw_mailme_opaque(data, "Received unknown message type 0x%x on place %s",
		     type, NSTR(place->name));
  }

  if(res) {
    mw_mailme_opaque(data, "Troubling parsing message type 0x0x on place %s",
		     type, NSTR(place->name));
  }

  mwGetBuffer_free(b);
}


static void stop(struct mwServicePlace *srvc) {
  while(srvc->places)
    mwPlace_destroy(srvc->places->data, ERR_SUCCESS);

  mwService_stopped(MW_SERVICE(srvc));
}


static int send_JOIN_PLACE(struct mwPlace *place) {
  struct mwOpaque o = {0, 0};
  struct mwPutBuffer *b;
  int ret;

  b = mwPutBuffer_new();
  gboolean_put(b, FALSE);
  guint16_put(b, 0x01);
  guint16_put(b, 0x01);
  guint16_put(b, 0x00);

  mwPutBuffer_finalize(&o, b);

  ret = mwChannel_send(place->channel, msg_out_JOIN_PLACE, &o);

  mwOpaque_clear(&o);

  if(ret) {
    place_state(place, mwPlace_ERROR);
  } else {
    place_state(place, mwPlace_JOINING);
  }

  return ret;
}


static void recv_channelAccept(struct mwService *service,
			       struct mwChannel *chan,
			       struct mwMsgChannelAccept *msg) {
  struct mwServicePlace *srvc;
  struct mwPlace *place;
  int res;

  srvc = (struct mwServicePlace *) service;
  g_return_if_fail(srvc != NULL);

  place = mwChannel_getServiceData(chan);
  g_return_if_fail(place != NULL);

  res = send_JOIN_PLACE(place);
}


static void recv_channelDestroy(struct mwService *service,
				struct mwChannel *chan,
				struct mwMsgChannelDestroy *msg) {
  struct mwServicePlace *srvc;
  struct mwPlace *place;

  srvc = (struct mwServicePlace *) service;
  g_return_if_fail(srvc != NULL);

  place = mwChannel_getServiceData(chan);
  g_return_if_fail(place != NULL);

  place_state(place, mwPlace_ERROR);

  place->channel = NULL;

  if(place->handler && place->handler->place_closed)
    place->handler->place_closed(place, msg->reason);  

  mwPlace_destroy(place, msg->reason);
}


static void clear(struct mwServicePlace *srvc) {
  while(srvc->places)
    place_free(srvc->places->data);
}


static const char *get_name(struct mwService *srvc) {
  return "Places Conferencing";
}


static const char *get_desc(struct mwService *srvc) {
  return "Barebones conferencing via Places";
}


struct mwServicePlace *mwServicePlace_new(struct mwSession *session) {
  struct mwServicePlace *srvc_place;
  struct mwService *srvc;

  g_return_val_if_fail(session != NULL, NULL);

  srvc_place = g_new0(struct mwServicePlace, 1);

  srvc = MW_SERVICE(srvc_place);
  mwService_init(srvc, session, SERVICE_PLACE);
  srvc->start = NULL;
  srvc->stop = (mwService_funcStop) stop;
  srvc->recv_create = NULL;
  srvc->recv_accept = recv_channelAccept;
  srvc->recv_destroy = recv_channelDestroy;
  srvc->recv = recv;
  srvc->clear = (mwService_funcClear) clear;
  srvc->get_name = get_name;
  srvc->get_desc = get_desc;

  return srvc_place;
}


const char *mwPlace_getName(struct mwPlace *place) {
  g_return_val_if_fail(place != NULL, NULL);
  return place->name;
}


const char *mwPlace_getTitle(struct mwPlace *place) {
  g_return_val_if_fail(place != NULL, NULL);
  return place->title;
}


struct mwPlace *mwPlace_new(struct mwServicePlace *srvc,
			    struct mwPlaceHandler *handler,
			    const char *name, const char *title) {
  struct mwPlace *place;
  
  place = g_new0(struct mwPlace, 1);
  place->service = srvc;
  place->handler = handler;
  place->name = g_strdup(name);
  place->title = g_strdup(title);
  place->state = mwPlace_NEW;

  place->members = g_hash_table_new_full(g_direct_hash, g_direct_equal,
					 NULL, (GDestroyNotify) member_free);
  return place;
}


static char *place_generate_name(const char *user) {
  guint a, b;
  char *ret;
  
  user = user? user: "meanwhile";

  srand(clock() + rand());
  a = ((rand() & 0xff) << 8) | (rand() & 0xff);
  b = time(NULL);

  ret = g_strdup_printf("%s(%08x,%04x)", user, b, a);
  g_debug("generated random conference name: '%s'", ret);
  return ret;
}


static char *place_generate_title(const char *user) {
  char *ret;
  
  user = user? user: "Meanwhile";
  ret = g_strdup_printf("%s's Conference", user);
  g_debug("generated conference title: %s", ret);

  return ret;
}


int mwPlace_open(struct mwPlace *p) {
  struct mwSession *session;
  struct mwChannel *chan;
  struct mwPutBuffer *b;

  struct mwLoginInfo *li;

  int ret;

  session = mwService_getSession(MW_SERVICE(p->service));
  g_return_val_if_fail(session != NULL, -1);

  li = mwSession_getLoginInfo(session);

  if(! p->name) {
    p->name = place_generate_name(li->user_id);
  }

  if(! p->title) {
    p->title = place_generate_title(li->user_name);
  }

  chan = mwChannel_newOutgoing(mwSession_getChannels(session));
  mwChannel_setService(chan, MW_SERVICE(p->service));
  mwChannel_setProtoType(chan, PROTOCOL_TYPE);
  mwChannel_setProtoVer(chan, PROTOCOL_VER);

  mwChannel_populateSupportedCipherInstances(chan);

  b = mwPutBuffer_new();
  mwString_put(b, p->name);
  mwString_put(b, p->title);
  guint32_put(b, 0x00); /* ? */

  mwPutBuffer_finalize(mwChannel_getAddtlCreate(chan), b);

  ret = mwChannel_create(chan);
  if(ret) {
    place_state(p, mwPlace_ERROR);
  } else {
    place_state(p, mwPlace_PENDING);
    p->channel = chan;
    mwChannel_setServiceData(chan, p, NULL);
  }

  return ret;
}


int mwPlace_destroy(struct mwPlace *p, guint32 code) {
  int ret = 0;

  place_state(p, mwPlace_CLOSING);

  if(p->channel) {
    ret = mwChannel_destroy(p->channel, code, NULL);
    p->channel = NULL;
  }

  place_free(p);

  return ret;
}


