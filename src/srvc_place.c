

enum incoming_msg {
  msg_in_JOIN_RESPONSE   = 0x0000, /* ? */
  msg_in_WELCOME         = 0x0002,
  msg_in_MESSAGE         = 0x0004,
  msg_in_STATUS          = 0x0014,
  msg_in_UNKNOWNa        = 0x0015,
};


enum in_status_subtype {
  msg_in_STATUS_RESPONSE     = 0x0000,
  msg_in_STATUS_PEER         = 0x0001,
  msg_in_STATUS_PEER_PARTED  = 0x0003,
};


enum in_status_peer_subtype {
  msg_in_STATUS_PEER_JOINED        = 0x0000,
  msg_in_STATUS_PEER_PART_SECTION  = 0x0001,
  msg_in_STATUS_PEER_CLEAR_ATTR    = 0x0003,
  msg_in_STATUS_PEER_SET_ATTR      = 0x0004,
};


enum outgoing_msg {
  msg_out_JOIN_PLACE     = 0x0000, /* ? */
  msg_out_JOIN_SECTION   = 0x0002, /* ? */
  msg_out_MESSAGE        = 0x0003,
  msg_out_OLD_INVITE     = 0x0005,
  msg_out_SET_ATTR       = 0x000a,
  msg_out_CLEAR_ATTR     = 0x000b,
  msg_out_STATUS         = 0x0014,
  msg_out_ENTER_SECTION  = 0x001e, /* ? maybe enter stage ? */
};


enum out_status_subtype {
  msg_out_STATUS_REQUEST       = 0x0002,
  msg_out_STATUS_PART_SECTION  = 0x0003,
};


/* for joining a place on our own
  : create channel
  : msg_out_JOIN_PLACE  (maybe create?)
  : msg_in_JOIN_RESPONSE (contains our place member ID)
  : msg_in_WELCOME
  : msg_in_UNKNOWNa
  : msg_out_ENTER_SECTION
  : msg_out_STATUS_REQUEST
  : msg_out_ENTER_SECTION
  : msg_in_STATUS_RESPONSE
  : msg_in_STATUS_PERR_JOINED (empty)
  : msg_in_UNKNOWNa

  all set to go
*/

/* for joining a place from an invitation
   : 
*/


struct mwServicePlace {
  struct mwService service;
  GList *places;
};


struct mwPlace {
  struct mwServicePlace *service;

  struct mwPlaceHandler *handler;

  enum mwPlaceState state;
  struct mwChannel *channel;

  char *name;
  char *title;
  GHashTable *members;
  guint32 our_id;
  guint32 section;

  struct mw_datum userdata;
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


static void place_member_free(struct place_member *p) {
  g_free(p->id);
  g_free(p->community);
  g_free(p->login_id);
  g_free(p->name);
  g_free(p);
}


static const struct mwIdBlock *
member_as_id_block(struct place_member *p) {
  static struct mwIdBlock idb;

  idb.name = p->id;
  idb.community = p->community;

  return &idb;
}


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
  case mwPlace_OPEN:     return "open";
  case mwPlace_JOINED:   return "joined";
  case mwPlace_CLOSING:  return "closing";
  case mwPlace_ERROR:    return "error";

  case mwPlace_UNKNOWN:  /* fall-through */
  default:               return "UNKNOWN";
  }
}


static void place_state(struct mwPlace *place, enum mwPlaceState s) {
  g_return_if_fail(place != NULL);
  
  if(place->state == s) return;

  place->state = state;
  g_message("place %s state: %s", NSTR(place->name), place_state_str(s));
}


static void recv_channelAccept(struct mwService *service,
			       struct mwChannel *chan,
			       struct mwMsgChannelAccept *msg) {

  struct mwServicePlace *srvc = (struct mwServicePlace *) service;
  struct mwPlace *place;

  place = mwChannel_getServiceData(chan);
  g_return_if_fail(place != NULL);

  place_state(place, mwPlace_OPEN);
  if(place->handler && place->handler->place_opened)
    place->handler->place_opened(place);
}


static void recv_JOIN_RESPONSE(struct mwPlace *place,
			       struct mwGetBuffer *b) {

  struct mwServicePlace *srvc;
  struct mwSession *session;
  struct mwLoginInfo *me;
  
  struct place_member *pm;
  guint32 our_id, section;

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
}


static void recv_MESSAGE(struct mwPlace *place,
			 struct mwGetBuffer *b) {
  
  guint32 pm_id;
  guint32 unkn_a, unkn_b, ign;
  struct place_member *pm;
  const struct mwIdBlock *idb;
  char *msg = NULL;

  /* regarding unkn_a and unkn_b:

     they're probably a section indicator and a message count, I'm
     just not sure which is which. Until this implementation supports
     place sections in the API, it really doesn't matter. */
  
  guint32_get(b, &pm_id);
  pm = GET_MEMBER(place, pm_id);
  g_return_if_fail(pm != NULL);

  guint32_get(b, &unkn_a);
  guint32_get(b, &ign);     /* actually an opaque length */
  
  if(! ign) return;

  guint32_get(b, &unkn_b);
  mwString_get(b, &msg);

  idb = member_as_id_block(pm);

  if(place->handler && place->handler->place_message)
    place->handler->place_message(place, idb, msg);

  g_free(msg);
}


static void recv_STATUS_RESPONSE(struct mwPlace *place,
				 struct mwGetBuffer *b) {

}


static void recv_STATUS_PEER_JOINED(struct mwPlace *place,
				    struct mwGetBuffer *b) {
  guint32 count;
  guint32 unknowna, unknownb;

  guint32_get(b, &count);
  while(count--) {
    struct place_member *pm = g_new0(struct place_member, 1);
    guint32_get(b, &pm->place_id);
    mwGetBuffer_skip(b, 4);
    guint16_get(b, &pm->unknown);
    mwString_get(b, &pm->id);
    mwString_get(b, &pm->community);
    mwString_get(b, &pm->login_id);
    mwString_get(b, &pm->name);
    guint16_get(b, &pm->type);

    PUT_MEMBER(place, pm);
    if(place->handler && place->handler->peerJoined)
      place->handler->peerJoined(place, member_as_id_block(pm));
  }

  guint32_get(b, &unknowna);
  guint32_get(b, &unknownb);
}


static void recv_STATUS_PEER_PART_SECTION(struct mwPlace *place,
					  struct mwGetBuffer *b) {
  
}


static void recv_STATUS_PEER_CLEAR_ATTR(struct mwPlace *place,
					struct mwGetBuffer *b) {

}


static void recv_STATUS_PEER_SET_ATTR(struct mwPlace *place,
				      struct mwGetBuffer *b) {

}


static void recv_STATUS_PEER(struct mwPlace *place,
			     struct mwGetBuffer *b) {
  guint16 subtype;
  mwGetBuffer_get(b, &subtype);

  g_return_if_fail(! mwGetBuffer_error());

  switch(subtype) {
  case msg_in_STATUS_PEER_JOINED:
    recv_STATUS_PEER_JOINED(place, b);
    break;

  case msg_in_STATUS_PEER_PART_SECTION:
    recv_STATUS_PEER_PART_SECTION(place, b);
    break;

  case msg_in_STATUS_PEER_CLEAR_ATTR:
    recv_STATUS_PEER_CLEAR_ATTR(place, b);
    break;

  case msg_in_STATUS_PEER_SET_ATTR:
    recv_STATUS_PEER_SET_ATTR(place, b);
    break;

  default:
    ;
  }
}


static void recv_STATUS_PEER_PARTED(struct mwPlace *place,
				    struct mwGetBuffer *b) {
  /* look up user in place
     remove user from place
     trigger event */

  guint32 pm_id;
  struct place_member *pm;
  const struct mwIdBlock *idb;

  guint32_get(b, &pm_id);

  pm = g_hash_table_lookup(place->members, GUINT_TO_POINTER(pm_id));
  g_return_if_fail(pm != NULL);

  idb = member_as_id_block(pm);
  if(place->handler && place->handler->place_peerParted)
    place->handler->place_peerParted(place, idb);

  g_hash_table_remove(place->members, GUINT_TO_POINTER(pm_id));
}


static void recv_STATUS(struct mwPlace *place, struct mwGetBuffer *b) {
  guint16 subtype;
  mwGetBuffer_get(b, &subtype);

  g_return_if_fail(! mwGetBuffer_error());

  switch(subtype) {
  case msg_in_STATUS_RESPONSE:
    recv_STATUS_RESPONSE(place, b);
    break;

  case msg_in_STATUS_PEER:
    recv_STATUS_PEER(place, b);
    break;

  case msg_in_STATUS_PEER_PARTED:
    recv_STATUS_PEER_PARTED(place, b);
    break;

  default:
    ;
  }
}


static void recv(struct mwService *service, struct mwChannel *chan,
		 guint16 type, struct mwOpaque *data) {

  struct mwServicePlace *srvc = (struct mwServicePlace *) service;
  struct mwPlace *place;
  struct mwGetBuffer *b;

  place = mwChannel_getServiceData(chan);
  g_return_if_fail(place != NULL);

  b = mwGetBuffer_wrap(data);
  switch(type) {
  case msg_in_JOIN_RESPONSE:
    recv_JOIN_RESPONSE(place, b);
    break;

  case msg_in_MESSAGE:
    recv_MESSAGE(place, b);
    break;

  case msg_in_STATUS:
    recv_STATUS(place, b);
    break;

  case msg_in_WELCOME:
  case msg_in_UNKNOWNa:
    break;

  default:
    mw_mailme_opaque(data, "Received unknown message type 0x%x on place %s",
		     type, NSTR(place->name));
  }

  mwGetBuffer_free(b);
}


struct mwServicePlace *mwServicePlace_new(struct mwSession *session) {
  struct mwServicePlace *srvc_place;
  struct mwService *srvc;

  g_return_val_if_fail(session != NULL);

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
  srvc->get_name = name;
  srvc->get_desc = desc;

  return srvc_place;
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

  return place;
}


int mwPlace_open(struct mwPlace *p) {

}


int mwPlace_join(struct mwPlace *p) {

}


