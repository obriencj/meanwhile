

#include <glib.h>
#include <glib/ghash.h>
#include <glib/glist.h>

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "srvc_conf.h"

#include "channel.h"
#include "compat.h"
#include "error.h"
#include "message.h"
#include "mw_debug.h"
#include "service.h"
#include "session.h"


/* This thing needs a re-write. More than anything else, I need to
   re-examine the conferencing service protocol from more modern
   clients
*/


/** @see mwMsgChannelSend::type
    @see recv */
enum msg_type {
  msg_WELCOME  = 0x0000,  /**< welcome message */
  msg_INVITE   = 0x0001,  /**< received invitation */
  msg_JOIN     = 0x0002,  /**< someone joined */
  msg_PART     = 0x0003,  /**< someone left */
  msg_MESSAGE  = 0x0004,  /**< conference message */
};


/** the conferencing service */
struct mwServiceConf {
  struct mwService service;

  /** call-back handler for this service */
  struct mwServiceConfHandler *handler;

  /** collection of conferences in this service */
  GList *confs;
};


/** a conference and its members */
struct mwConference {
  enum mwConferenceState state;   /**< state of the conference */
  struct mwServiceConf *service;  /**< owning service */
  struct mwChannel *channel;      /**< conference's channel */

  char *name;   /**< server identifier for the conference */
  char *topic;  /**< topic for the conference */

  struct mwLoginInfo owner;  /**< person who created this conference */
  GHashTable *members;       /**< mapping MEMBER_KEY(member):member */
};


/** sametime conferences send messages in relation to a conference member
    ID. This structure relates that id to a user id block */
struct mwConfMember {
  struct mwConference *conf;
  struct mwLoginInfo user;
  guint16 id;
};


#define MEMBER_KEY(cm)  GUINT_TO_POINTER((guint) (cm)->id)


#define MEMBER_FIND(conf, id) \
  g_hash_table_lookup(conf->members, GUINT_TO_POINTER((guint) id))


/** free a conference structure */
static void conference_free(gpointer c) {
  struct mwConference *conf = c;
  if(! conf) return;

  if(conf->members)
    g_hash_table_destroy(conf->members);

  g_free(conf->name);
  g_free(conf->topic);

  g_free(conf);
}


static struct mwConference *conference_find(struct mwServiceConf *srvc,
					    struct mwChannel *chan) {
  GList *l;
  for(l = srvc->confs; l; l = l->next) {
    struct mwConference *conf = l->data;
    if(conf->channel == chan) return conf;
  }

  return NULL;
}


/** free a conference member structure */
static void member_free(struct mwConfMember *member) {
  mwLoginInfo_clear(&member->user);
  g_free(member);
}


static void recv_channelCreate(struct mwService *srvc,
			       struct mwChannel *chan,
			       struct mwMsgChannelCreate *msg) {

  /* - this is how we really receive invitations
     - create a conference and associate it with the channel
     - obtain the invite data from the msg addtl info
     - mark the conference as INVITED
     - trigger the got_invite event
  */

  struct mwServiceConf *srvc_conf = (struct mwServiceConf *) srvc;
  struct mwConference *conf = mwConference_new(srvc_conf);

  struct mwGetBuffer *b = mwGetBuffer_wrap(&msg->addtl);

  char *invite = NULL;
  guint tmp;

  guint32_get(b, &tmp);
  mwString_get(b, &conf->name);
  mwString_get(b, &conf->topic);
  guint32_get(b, &tmp);
  mwLoginInfo_get(b, &conf->owner);
  guint32_get(b, &tmp);
  mwString_get(b, &invite);

  if(mwGetBuffer_error(b)) {
    g_warning("failure parsing addtl for conference invite");
    mwConference_destroy(conf, ERR_FAILURE, NULL);

  } else {
    struct mwServiceConfHandler *h = srvc_conf->handler;

    conf->state = mwConference_INVITED;

    if(h->got_invite)
      h->got_invite(conf, &conf->owner, invite);
  }

  mwGetBuffer_free(b);
  g_free(invite);
}


static void recv_channelAccept(struct mwService *srvc,
			       struct mwChannel *chan,
			       struct mwMsgChannelAccept *msg) {

  /* Since a conference should always send a welcome message immediately
     after acceptance, we'll do nothing here. */
}


static void recv_channelDestroy(struct mwService *srvc,
				struct mwChannel *chan,
				struct mwMsgChannelDestroy *msg) {

  /* - find conference from channel
     - trigger got_closed
     - remove conference, dealloc
  */

  struct mwServiceConf *srvc_conf = (struct mwServiceConf *) srvc;
  struct mwConference *conf = conference_find(srvc_conf, chan);
  struct mwServiceConfHandler *h = srvc_conf->handler;

  /* if there's no such conference, then I guess there's nothing to worry
     about. Except of course for the fact that we should never receive a
     channel destroy for a conference that doesn't exist. */
  if(! conf) return;

  if(h->got_closed)
    h->got_closed(conf);

  conf->channel = NULL;
  mwConference_destroy(conf, ERR_SUCCESS, NULL);
}


static void WELCOME_recv(struct mwServiceConf *srvc,
			 struct mwConference *conf,
			 struct mwGetBuffer *b) {

  struct mwServiceConfHandler *h;
  guint16 tmp16;
  guint32 tmp32;
  guint32 count;
  GList *l = NULL;

  /* re-read name and title */
  g_free(conf->name);
  g_free(conf->topic);
  mwString_get(b, &conf->name);
  mwString_get(b, &conf->topic);

  /* some numbers we don't care about, then a count of members */
  guint16_get(b, &tmp16);
  guint32_get(b, &tmp32);
  guint32_get(b, &count);

  if(mwGetBuffer_error(b)) {
    g_warning("error parsing welcome message for conference");
    mwConference_destroy(conf, ERR_FAILURE, NULL);
    return;
  }
  
  while(count--) {
    struct mwConfMember *cm = g_new0(struct mwConfMember, 1);
    cm->conf = conf;

    guint16_get(b, &cm->id);
    mwLoginInfo_get(b, &cm->user);

    if(mwGetBuffer_error(b)) {
      member_free(cm);
      break;
    }

    /* this list is for triggering the event */
    l = g_list_append(l, &cm->user);
  
    /* populate the conference */
    g_hash_table_insert(conf->members, MEMBER_KEY(cm), cm);
  }

  conf->state = mwConference_ACTIVE;

  h = srvc->handler;
  if(h->got_welcome)
    h->got_welcome(conf, l);
}


static void JOIN_recv(struct mwServiceConf *srvc,
		      struct mwConference *conf,
		      struct mwGetBuffer *b) {

  struct mwServiceConfHandler *h;
  struct mwConfMember *m;
  
  m = g_new0(struct mwConfMember, 1);
  m->conf = conf;

  guint16_get(b, &m->id);
  mwLoginInfo_get(b, &m->user);

  if(mwGetBuffer_error(b)) {
    g_warning("failed parsing JOIN message in conference");
    member_free(m);
    return;
  }

  g_hash_table_insert(conf->members, MEMBER_KEY(m), m);

  h = srvc->handler;
  if(h->got_join)
    h->got_join(conf, &m->user);
}


static void PART_recv(struct mwServiceConf *srvc,
		      struct mwConference *conf,
		      struct mwGetBuffer *b) {

  /* - parse who left
     - look up their membership
     - remove them from the members list
     - trigger the event
  */

  struct mwServiceConfHandler *h;
  guint16 id = 0;
  struct mwConfMember *m;

  guint16_get(b, &id);

  if(mwGetBuffer_error(b)) return;

  m = MEMBER_FIND(conf, id);
  if(! m) return;

  g_hash_table_remove(conf->members, GUINT_TO_POINTER((guint) id));

  h = srvc->handler;
  if(h->got_part)
    h->got_part(conf, &m->user);

  member_free(m);
}


static void text_recv(struct mwServiceConf *srvc,
		      struct mwConfMember *who,
		      struct mwGetBuffer *b) {

  /* this function acts a lot like receiving an IM Text message. The text
     message contains only a string */

  char *text = NULL;
  struct mwServiceConfHandler *h;

  mwString_get(b, &text);

  if(mwGetBuffer_error(b)) {
    g_warning("failed to parse text message in conference");
    return;
  }

  h = srvc->handler;
  if(h->got_text)
    h->got_text(who->conf, &who->user, text);

  g_free(text);
}


static void data_recv(struct mwServiceConf *srvc,
		      struct mwConfMember *who,
		      struct mwGetBuffer *b) {

  /* this function acts a lot like receiving an IM Data message. The
     data message has a type, a subtype, and an opaque. We only
     support typing notification though. */

  /** @todo it's possible that some clients send text in a data
      message, as we've seen rarely in the IM service. Have to add
      support for that here */

  guint32 type, subtype;
  struct mwServiceConfHandler *h;

  guint32_get(b, &type);
  guint32_get(b, &subtype);

  /* don't know how to deal with any others yet */
  if(type != 0x01) return;

  h = srvc->handler;
  if(h->got_typing)
    h->got_typing(who->conf, &who->user, !subtype);
}


static void MESSAGE_recv(struct mwServiceConf *srvc,
			 struct mwConference *conf,
			 struct mwGetBuffer *b) {

  /* - look up who send the message by their id
     - trigger the event
  */

  guint16 id;
  guint32 type;
  struct mwConfMember *m;

  /* an empty buffer isn't an error, just ignored */
  if(! mwGetBuffer_remaining(b)) return;

  guint16_get(b, &id);
  guint32_get(b, &type); /* reuse type variable */
  guint32_get(b, &type);

  if(mwGetBuffer_error(b)) return;

  m = MEMBER_FIND(conf, id);
  if(! m) {
    g_warning("received message type 0x%04x for"
	      " unknown conference member %u", type, id);
    return;
  }
  
  switch(type) {
  case 0x01:  /* type is text */
    text_recv(srvc, m, b);
    break;

  case 0x02:  /* type is data */
    data_recv(srvc, m, b);
    break;

  default:
    g_warning("unknown message type 0x%4x received in conference", type);
  }
}


static void recv(struct mwService *srvc, struct mwChannel *chan,
		 guint16 type, struct mwOpaque *data) {

  struct mwServiceConf *srvc_conf = (struct mwServiceConf *) srvc;
  struct mwConference *conf = conference_find(srvc_conf, chan);
  struct mwGetBuffer *b;

  g_return_if_fail(conf != NULL);

  b = mwGetBuffer_wrap(data);

  switch(type) {
  case msg_WELCOME:
    WELCOME_recv(srvc_conf, conf, b);
    break;

  case msg_JOIN:
    JOIN_recv(srvc_conf, conf, b);
    break;

  case msg_PART:
    PART_recv(srvc_conf, conf, b);
    break;

  case msg_MESSAGE:
    MESSAGE_recv(srvc_conf, conf, b);
    break;

  default:
    ; /* hrm. should log this. TODO */
  }
}


static void clear(struct mwService *srvc) {
  struct mwServiceConf *srvc_conf = (struct mwServiceConf *) srvc;
  GList *l;

  for(l = srvc_conf->confs; l; l = l->next)
    conference_free(l->data);

  g_list_free(srvc_conf->confs);
  srvc_conf->confs = NULL;
}


static const char *name(struct mwService *srvc) {
  return "Basic Conferencing";
}


static const char *desc(struct mwService *srvc) {
  return "A simple multi-user conference service";
}


struct mwServiceConf *mwServiceConf_new(struct mwSession *session,
					struct mwServiceConfHandler *handler) {

  struct mwServiceConf *srvc_conf = g_new0(struct mwServiceConf, 1);
  struct mwService *srvc = &srvc_conf->service;

  mwService_init(srvc, session, mwService_CONF);
  srvc->recv_channelCreate = recv_channelCreate;
  srvc->recv_channelAccept = recv_channelAccept;
  srvc->recv_channelDestroy = recv_channelDestroy;
  srvc->recv = recv;
  srvc->clear = clear;
  srvc->get_name = name;
  srvc->get_desc = desc;

  srvc_conf->handler = handler;

  return srvc_conf;
}


struct mwConference *mwConference_new(struct mwServiceConf *srvc) {
  struct mwConference *conf;

  conf = g_new0(struct mwConference, 1);
  conf->service = srvc;
  conf->state = mwConference_NEW;

  srvc->confs = g_list_prepend(srvc->confs, conf);

  return conf;
}


static char *make_conf_name(const char *user_id) {
  char c[16]; /* limited space. Used only to hold sprintf output */

  guint a = time(NULL);
  guint b;

  srand(clock());
  b = ((rand() & 0xff) << 8) | (rand() & 0xff);
  g_sprintf(c, "(%08x,%04x)", a, b);

  return g_strconcat(user_id, c, NULL);
}


int mwConference_create(struct mwConference *conf) {
  struct mwSession *session;
  struct mwChannel *chan;
  struct mwPutBuffer *b;

  g_return_val_if_fail(conf != NULL, -1);
  g_return_val_if_fail(conf->service != NULL, -1);
  g_return_val_if_fail(conf->state == mwConference_NEW, -1);

  session = mwService_getSession(MW_SERVICE(conf->service));
  g_assert(session != NULL);

  if(! conf->name) {
    struct mwLoginInfo *login = mwSession_getLoginInfo(session);
    conf->name = make_conf_name(login->user_id);
    g_debug("generated random conference name: '%s'", NSTR(conf->name));
  }

  chan = mwChannel_newOutgoing(mwSession_getChannels(session));
  mwChannel_setService(chan, MW_SERVICE(conf->service));
  mwChannel_setProtoType(chan, mwProtocol_CONF);
  mwChannel_setProtoVer(chan, 0x02);

  /* this is wrong, I think */
  b = mwPutBuffer_new();
  mwString_put(b, conf->name);
  mwString_put(b, conf->topic);
  guint32_put(b, 0x00);
  guint32_put(b, 0x00);

  return mwChannel_create(chan);
}


int mwConference_destroy(struct mwConference *conf,
			 guint32 reason, const char *text) {

  struct mwServiceConf *srvc;
  struct mwOpaque info = { .len = 0, .data = NULL };
  int ret = 0;

  g_return_val_if_fail(conf != NULL, -1);

  srvc = conf->service;
  g_assert(srvc != NULL);

  srvc->confs = g_list_remove_all(srvc->confs, conf);

  if(conf->channel) {
    if(text) {
      info.len = strlen(text);
      info.data = (char *) text;
    }

    ret = mwChannel_destroy(conf->channel, reason, &info);
  }
  
  conference_free(conf);
  return ret;
}


int mwConference_accept(struct mwConference *conf) {
  /* - if conference is not INVITED, return -1
     - accept the conference channel
     - send an empty JOIN message
  */

  struct mwChannel *chan = conf->channel;
  struct mwOpaque o = { .len = 0, .data = NULL };
  int ret;

  g_return_val_if_fail(conf->state == mwConference_INVITED, -1);

  ret = mwChannel_accept(chan);

  if(! ret)
    ret = mwChannel_send(chan, msg_JOIN, &o);

  return ret;
}


int mwConference_invite(struct mwConference *conf,
			struct mwIdBlock *who,
			const char *text) {

  struct mwPutBuffer *b;
  struct mwOpaque o;
  int ret;

  g_return_val_if_fail(conf->state != mwConference_ACTIVE, -1);

  b = mwPutBuffer_new();

  mwIdBlock_put(b, who);
  mwString_put(b, who->user);
  mwString_put(b, who->user); /* yes. TWICE! */
  mwString_put(b, text);
  gboolean_put(b, FALSE);

  mwPutBuffer_finalize(&o, b);
  ret = mwChannel_send(conf->channel, msg_INVITE, &o);
  mwOpaque_clear(&o);

  return ret;
}


int mwConference_sendText(struct mwConference *conf, const char *text) {
  struct mwPutBuffer *b;
  struct mwOpaque o;
  int ret;

  g_return_val_if_fail(conf != NULL, -1);
  g_return_val_if_fail(conf->channel != NULL, -1);

  b = mwPutBuffer_new();

  guint32_put(b, 0x01);
  mwString_put(b, text);

  mwPutBuffer_finalize(&o, b);
  ret = mwChannel_send(conf->channel, msg_MESSAGE, &o);
  mwOpaque_clear(&o);

  return ret;
}


int mwConference_sendTyping(struct mwConference *conf, gboolean typing) {
  struct mwPutBuffer *b;
  struct mwOpaque o = { .len = 0, .data = NULL };
  int ret;

  g_return_val_if_fail(conf != NULL, -1);
  g_return_val_if_fail(conf->channel != NULL, -1);

  if(conf->state != mwConference_ACTIVE) return 0;

  b = mwPutBuffer_new();

  guint32_put(b, 0x02);
  guint32_put(b, 0x01);
  guint32_put(b, !typing);
  mwOpaque_put(b, NULL);

  mwPutBuffer_finalize(&o, b);
  ret = mwChannel_send(conf->channel, msg_MESSAGE, &o);
  mwOpaque_clear(&o);

  return ret;
}

