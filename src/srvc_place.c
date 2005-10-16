

enum incoming_msg {
  msg_in_JOIN_RESPONSE   = 0x0000,
  msg_in_WELCOME         = 0x0002,
  msg_in_MESSAGE         = 0x0004,
  msg_in_STATUS          = 0x0014,
  msg_in_UNKNOWNa        = 0x0015,
};


enum in_status_subtype {
  msg_in_STATUS_RESPONSE     = 0x0000,
  msg_in_STATUS_PEER         = 0x0001,
  msg_in_STATUS_PEER_PARTED  = 0x0002,
};


enum in_status_peer_subtype {
  msg_in_STATUS_PEER_JOINED      = 0x0000,
  msg_in_STATUS_PEER_UNKNOWNa    = 0x0001,
  msg_in_STATUS_PEER_CLEAR_ATTR  = 0x0003,
  msg_in_STATUS_PEER_SET_ATTR    = 0x0004,
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
  msg_out_STATUS_REQUEST  = 0x0002,
  msg_out_STATUS_PART     = 0x0003,
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
  struct mwPlaceHandler *handler;

  enum mwPlaceState state;
  struct mwChannel *channel;

  char *name;
  char *title;
  GHashTable *members;
  guint32 our_id;

  struct mw_datum userdata;
};


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


static void recv_JOIN_RESPONSE(struct mwPlace *place, struct mwGetBuffer *b) {

}


static void recv_STATUS_RESPONSE(struct mwPlace *place,
				 struct mwGetBuffer *b) {

}


static void recv_STATUS_PEER_JOINED(struct mwPlace *place,
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

  case msg_in_STATUS_PEER_UNKNOWNa:
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

}


static void recv_STATUS(struct mwPlace *place, struct mwGetBuffer *b) {

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



struct mwServicePlace_new(struct mwSession *s) {

}


struct mwPlace_new(struct mwServicePlace *srvc,
		   struct mwPlaceHandler *handler,
		   const char *name, const char *title) {
  
}


int mwPlace_open(struct mwPlace *p) {

}


int mwPlace_join(struct mwPlace *p) {

}


