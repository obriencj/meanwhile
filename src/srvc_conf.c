

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "channel.h"
#include "error.h"
#include "message.h"
#include "mw_debug.h"
#include "service.h"
#include "session.h"
#include "srvc_conf.h"


enum send_actions {
  mwChannelSend_CONF_WELCOME    = 0x0000,  /* grr. shouldn't use zero */
  mwChannelSend_CONF_INVITE     = 0x0001,
  mwChannelSend_CONF_JOIN       = 0x0002,
  mwChannelSend_CONF_PART       = 0x0003,
  mwChannelSend_CONF_MESSAGE    = 0x0004   /* conference */
};


/* this structure isn't public, because the "user" never needs to know that
   sametime conferences associate users with a number */
struct mwConfMember {
  unsigned int id;
  char *user;
  char *community;
};


static void member_free(struct mwConfMember *member) {
  g_free(member->user);
  g_free(member->community);
}


static struct mwConfMember *member_find(struct mwConference *conf,
					unsigned int id) {
  GList *l;
  for(l = conf->members; l; l = l->next) {
    struct mwConfMember *m = (struct mwConfMember *) l->data;
    if(m->id == id) return m;
  }
  return NULL;
}


static void chan_clear(struct mwChannel *chan) {
  struct mwConference *conf = (struct mwConference *) chan->addtl;

  /* simply disassociate the channel and conference */
  conf->channel = NULL;
  chan->addtl = NULL;
}


static void chan_conf_associate(struct mwChannel *chan,
				struct mwConference *conf) {
  conf->channel = chan;
  chan->addtl = conf;
  chan->clear = chan_clear;
}


static void conf_clear(struct mwConference *conf) {
  GList *l;

  for(l = conf->members; l; l = l->next) {
    struct mwConfMember *m = (struct mwConfMember *) l->data;
    l->data = NULL;
    member_free(m);
  }
  g_list_free(conf->members);
  
  g_free(conf->name);
  g_free(conf->topic);
}


static void conf_remove(struct mwConference *conf) {
  struct mwServiceConf *srvc = conf->srvc;
  srvc->conferences = g_list_remove_all(srvc->conferences, conf);
  conf_clear(conf);
}


static void recv_channelCreate(struct mwService *srvc, struct mwChannel *chan,
			       struct mwMsgChannelCreate *msg) {

  /* - this is how we really receive invitations
     - create a conference and associate it with the channel
     - obtain the invite data from the msg addtl info
     - mark the conference as INVITED
     - trigger the got_invite event
  */

  struct mwServiceConf *srvc_conf = (struct mwServiceConf *) srvc;
  struct mwConference *conf = mwConference_new(srvc_conf);

  struct mwLoginInfo login;
  struct mwIdBlock idb;

  char *invite = NULL;
  unsigned int tmp;

  char *b = msg->addtl.data;
  unsigned int n = msg->addtl.len;

  chan_conf_associate(chan, conf);

  /* bleh. necessary to prevent later cleanup from blowing up the world */
  memset(&login, 0x00, sizeof(struct mwLoginInfo));

  if( guint32_get(&b, &n, &tmp) ||
      mwString_get(&b, &n, &conf->name) ||
      mwString_get(&b, &n, &conf->topic) ||
      guint32_get(&b, &n, &tmp) ||
      mwLoginInfo_get(&b, &n, &login) ||
      guint32_get(&b, &n, &tmp) ||
      mwString_get(&b, &n, &invite) ) {

    debug_printf(" failure parsing addtl for invite\n");
    
    /* a lot of cleanup on failure */
    mwConference_destroy(conf, ERR_FAILURE, NULL);
    g_free(conf);

  } else {

    conf->status = mwConference_INVITED;

    idb.user = login.user_id;
    idb.community = login.community;

    if(srvc_conf->got_invite)
      srvc_conf->got_invite(conf, &idb, invite);
  }

  mwLoginInfo_clear(&login);
  g_free(invite);
}


static void recv_channelAccept(struct mwService *srvc, struct mwChannel *chan,
			       struct mwMsgChannelAccept *msg) {

  /* Since a conference should always send a welcome message immediately
     after acceptance, we'll do nothing here. */
}


static void recv_channelDestroy(struct mwService *srvc, struct mwChannel *chan,
				struct mwMsgChannelDestroy *msg) {

  /* - find conference from channel
     - trigger got_closed
     - remove conference, dealloc
  */

  struct mwServiceConf *srvc_conf = (struct mwServiceConf *) srvc;
  struct mwConference *conf = mwConference_findByChannel(srvc_conf, chan);

  /* if there's no such conference, then I guess there's nothing to worry
     about. Except of course for the fact that we should never receive a
     channel destroy for a conference that doesn't exist. */
  if(! conf) return;

  if(srvc_conf->got_closed)
    srvc_conf->got_closed(conf);

  conf_remove(conf);
  g_free(conf);
}


static int WELCOME_recv(struct mwServiceConf *srvc,
			struct mwConference *conf,
			const char *b, gsize n) {
  /* - parse conf info
     - mark conference as ACTIVE
     - trigger got_welcome
  */

  struct mwLoginInfo login;
  struct mwIdBlock *members;
  unsigned int count, c;
  int ret = 0;

  if( mwString_get((char **) &b, &n, &conf->name) ||
      mwString_get((char **) &b, &n, &conf->topic) ||
      n < 10 ) {
    g_return_val_if_reached(-1);
  }
  
  /* note, the following two lines are the 10 bytes we checked for above */
  b += 6; n -= 6; /* skip a 16-bit and a 32-bit number */
  guint32_get((char **) &b, &n, &count);

  /* this is to trigger the event. */
  members = g_new0(struct mwIdBlock, count);
  
  for(c = count; c--; ) {
    struct mwConfMember *cm;
    struct mwIdBlock *m;
    unsigned int id;

    /* prep the login info */
    memset(&login, 0x00, sizeof(struct mwLoginInfo));
    
    if( guint16_get((char **) &b, &n, &id) ||
	mwLoginInfo_get((char **) &b, &n, &login) ) {
      
      /* cleanup for failures */
      mwLoginInfo_clear(&login);
      ret = -1;
      break;
    }
  
    /* this is for actually populating the conference */
    cm = g_new0(struct mwConfMember, 1);
    cm->id = id;
    cm->user = g_strdup(login.user_id);
    cm->community = g_strdup(login.community);
    conf->members = g_list_prepend(conf->members, cm);

    /* this is just to trigger the event. Just re-referencing the
       strings should be ok. There could be a problem if the handler
       decides to do something with them, though. */
    m = members + c;
    m->user = cm->user;
    m->community = cm->community;

    /* cleanup the allocated stuff from the parsing */
    mwLoginInfo_clear(&login);
  }
  
  if(! ret) {
    conf->status = mwConference_ACTIVE;
    if(srvc->got_welcome)
      srvc->got_welcome(conf, members, count);
  }

  g_free(members);

  return ret;
}


static int INVITE_recv(struct mwServiceConf *srvc,
		       struct mwConference *conf,
		       const char *b, gsize n) {
  /* TODO:
     - do we ever actually receive one of these? I don't think we do.
  */
  return 0;
}


static int JOIN_recv(struct mwServiceConf *srvc,
		     struct mwConference *conf,
		     const char *b, gsize n) {

  /* - parse who joined
     - add them to the members list
     - trigger the event
  */

  unsigned int id;
  struct mwLoginInfo *login;
  struct mwConfMember *m;
  struct mwIdBlock idb;

  login = g_new0(struct mwLoginInfo, 1);

  if( guint16_get((char **) &b, &n, &id) ||
      mwLoginInfo_get((char **) &b, &n, login) ) {

    mwLoginInfo_clear(login);
    g_free(login);
    g_return_val_if_reached(-1);
  }

  /* we receive a login info block. We store a conf member. We trigger with
     an id block */

  m = g_new0(struct mwConfMember, 1);
  m->id = id;
  m->user = g_strdup(login->user_id);
  m->community = g_strdup(login->community);

  idb.user = m->user;
  idb.community = m->community;

  mwLoginInfo_clear(login);
  g_free(login);

  conf->members = g_list_prepend(conf->members, m);

  if(srvc->got_join)
    srvc->got_join(conf, &idb);

  return 0;
}


static int PART_recv(struct mwServiceConf *srvc,
		     struct mwConference *conf,
		     const char *b, gsize n) {

  /* - parse who left
     - look up their membership
     - remove them from the members list
     - trigger the event
  */

  unsigned int id;
  struct mwConfMember *m;
  struct mwIdBlock idb;

  if( guint16_get((char **) &b, &n, &id) )
    g_return_val_if_reached(-1);

  m = member_find(conf, id);
  g_return_val_if_fail(m, -1);

  conf->members = g_list_remove_all(conf->members, m);

  idb.user = m->user;
  idb.community = m->community;

  if(srvc->got_part)
    srvc->got_part(conf, &idb);

  member_free(m);

  return 0;
}


static int text_recv(struct mwServiceConf *srvc, struct mwConference *conf,
		     unsigned int id, const char *b, gsize n) {

  /* this function acts a lot like receiving an IM Text message. The text
     message contains only a string */

  char *text;

  struct mwConfMember *m;
  struct mwIdBlock idb;

  /* look up the member */
  m = member_find(conf, id);
  g_return_val_if_fail(m, -1);

  if( mwString_get((char **) &b, &n, &text) )
    g_return_val_if_reached(-1);

  idb.user = m->user;
  idb.community = m->community;

  if(srvc->got_text)
    srvc->got_text(conf, &idb, text);

  g_free(text);
  return 0;
}


static int data_recv(struct mwServiceConf *srvc, struct mwConference *conf,
		     unsigned int id, const char *b, gsize n) {

  /* this function acts a lot like receiving an IM Data message. The data
     message has a type, a subtype, and an opaque. we only support typing
     notification */

  /* TODO: it's possible that some clients send text in a data message, as
     we've seen rarely in the IM service. Have to add support for that here */

  unsigned int typing;

  struct mwConfMember *m;
  struct mwIdBlock idb;

  /* look up the member */
  m = member_find(conf, id);
  g_return_val_if_fail(m, -1);

  /* get the type (which should be 0x01) and the subtype (which indicates
     the opposite of whether the user is typing) */
  if( guint32_get((char **) &b, &n, &typing) || (typing != 0x01) ||
      guint32_get((char **) &b, &n, &typing) )
    g_return_val_if_reached(-1);

  idb.user = m->user;
  idb.community = m->community;

  if(srvc->got_typing)
    srvc->got_typing(conf, &idb, !typing);

  return 0;
}


static int MESSAGE_recv(struct mwServiceConf *srvc, struct mwConference *conf,
			const char *b, unsigned int n) {

  /* - look up who send the message by their id
     - trigger the event
  */

  unsigned int id, type;

  /* an empty message isn't an error, it's just ignored */
  if(! n) return 0;

  if( guint16_get((char **) &b, &n, &id) ||
      guint32_get((char **) &b, &n, &type) || /* not a typo, just reusing */
      guint32_get((char **) &b, &n, &type) )
    return -1;

  if(type == 0x01) {
    /* type is text */
    return text_recv(srvc, conf, id, b, n);

  } else if(type == 0x02) {
    /* type is data */
    return data_recv(srvc, conf, id, b, n);

  } else {
    return -1;
  }
}


static void recv(struct mwService *srvc, struct mwChannel *chan,
		 guint32 type, const char *b, gsize n) {

  struct mwServiceConf *srvc_conf = (struct mwServiceConf *) srvc;
  struct mwConference *conf = mwConference_findByChannel(srvc_conf, chan);
  int ret;

  if(! conf) return;

  switch(type) {
  case mwChannelSend_CONF_WELCOME:
    ret = WELCOME_recv(srvc_conf, conf, b, n);
    break;

  case mwChannelSend_CONF_INVITE:
    ret = INVITE_recv(srvc_conf, conf, b, n);
    break;

  case mwChannelSend_CONF_JOIN:
    ret = JOIN_recv(srvc_conf, conf, b, n);
    break;

  case mwChannelSend_CONF_PART:
    ret = PART_recv(srvc_conf, conf, b, n);
    break;

  case mwChannelSend_CONF_MESSAGE:
    ret = MESSAGE_recv(srvc_conf, conf, b, n);
    break;

  default:
    ; /* hrm. */
  }

  if(ret) ; /* handle how? */
}


static void clear(struct mwService *srvc) {
  struct mwServiceConf *srvc_conf = (struct mwServiceConf *) srvc;
  GList *l;
  
  for(l = srvc_conf->conferences; l; l = l->next) {
    struct mwConference *c = (struct mwConference *) l->data;
    l->data = NULL;
    conf_clear(c);
    g_free(c);
  }
  g_list_free(srvc_conf->conferences);
  srvc_conf->conferences = NULL;
}


static const char *name() {
  return "Basic Conferencing";
}


static const char *desc() {
  return "A simple multi-user conference service";
}


struct mwServiceConf *mwServiceConf_new(struct mwSession *session) {
  struct mwServiceConf *srvc_conf = g_new0(struct mwServiceConf, 1);
  struct mwService *srvc = &srvc_conf->service;

  srvc->session = session;
  srvc->type = mwService_CONF; /* one of the BaseServiceTypes */

  srvc->recv_channelCreate = recv_channelCreate;
  srvc->recv_channelAccept = recv_channelAccept;
  srvc->recv_channelDestroy = recv_channelDestroy;
  srvc->recv = recv;
  /* todo: start and stop */
  srvc->clear = clear;
  srvc->get_name = name;
  srvc->get_desc = desc;

  return srvc_conf;
}


struct mwConference *mwConference_new(struct mwServiceConf *srvc) {

  /* - allocate/initialize a conference
     - set the state to NEW
     - add it to the internal list
     - return it
  */

  struct mwConference *conf;

  debug_printf(" --> mwConference_new\n");

  conf = g_new0(struct mwConference, 1);
  conf->srvc = srvc;
  conf->status = mwConference_NEW; /* redundant, it's 0x00 */

  srvc->conferences = g_list_prepend(srvc->conferences, conf);

  debug_printf(" <-- mwConference_new\n");
  return conf;
}


static char *make_conf_name(struct mwSession *s) {
  char c[64]; /* limited space. Used only to hold sprintf output */

  unsigned int a = time(NULL);
  unsigned int b;

  srand(clock());
  b = ((rand() & 0xff) << 8) | (rand() & 0xff);
  sprintf(c, "(%08x,%04x)%c", a, b, '\0');

  return g_strconcat(s->login.user_id, c, NULL);
}


static int send_create(struct mwSession *s, struct mwConference *conf) {
  struct mwChannel *chan = conf->channel;
  struct mwMsgChannelCreate *msg;
  char *b;
  unsigned int n;

  int ret;

  msg = (struct mwMsgChannelCreate *)
    mwMessage_new(mwMessage_CHANNEL_CREATE);

  msg->channel = chan->id;
  msg->service = chan->service;
  msg->proto_type = chan->proto_type;
  msg->proto_ver = chan->proto_ver; 

  msg->addtl.len = n =
    mwString_buflen(conf->name) + mwString_buflen(conf->topic) + 8;
  msg->addtl.data = b = g_malloc0(n);
  mwString_put(&b, &n, conf->name);
  mwString_put(&b, &n, conf->topic);
  
  msg->encrypt.type = chan->encrypt.type;
  msg->encrypt.opaque.len = n = 10;
  msg->encrypt.opaque.data = b = (char *) g_malloc(n);
   
  /* I really want to know what this opaque means. Every client seems
     to send it or something similar, and it doesn't work without it */
  guint32_put(&b, &n, 0x00000001);
  guint32_put(&b, &n, 0x00000000);
  guint16_put(&b, &n, 0x0000);

  ret = mwChannel_create(chan, msg);
  mwMessage_free(MW_MESSAGE(msg));

  return ret;
}


int mwConference_create(struct mwConference *conf) {
  /* - if the state is not NEW, return -1
     - if the name is NULL, create the name
     - set the state to PENDING
     - create a channel and associate it with the conference
     - compose and send the channel create message
  */

  struct mwChannel *chan;
  struct mwSession *session;

  g_return_val_if_fail(conf && conf->srvc, -1);
  g_return_val_if_fail(conf->status == mwConference_NEW, -1);

  session = conf->srvc->service.session;

  if(! conf->name) conf->name = make_conf_name(session);
  g_message("made up conference name: '%s'", conf->name);

  chan = mwChannel_newOutgoing(session->channels);
  chan->status = mwChannel_INIT;

  chan->service = mwService_CONF;
  chan->proto_type = mwProtocol_CONF;
  chan->proto_ver = 0x02;
  chan->encrypt.type = mwEncrypt_RC2_40;

  chan_conf_associate(chan, conf);
  conf->status = mwConference_PENDING;

  return send_create(session, conf);
}


int mwConference_destroy(struct mwConference *conf,
			 guint32 reason, const char *text) {

  /* - close the conference channel (possibly with reason text)
     - trigger got_closed
     - remove the conference from the service
  */

  struct mwServiceConf *srvc = conf->srvc;
  struct mwChannel *chan = conf->channel;
  struct mwMsgChannelDestroy *msg;
  int ret;

  msg = (struct mwMsgChannelDestroy *)
    mwMessage_new(mwMessage_CHANNEL_DESTROY);

  msg->head.channel = chan->id;
  msg->reason = reason;

  if(text) {
    msg->data.len = strlen(text);
    msg->data.data = g_strdup(text);
  }
  
  if(srvc->got_closed)
    srvc->got_closed(conf);

  ret = mwChannel_destroy(chan, msg);
  mwMessage_free(MW_MESSAGE(msg));
  
  conf_remove(conf);
  g_free(conf);
  
  return ret;
}


int mwConference_accept(struct mwConference *conf) {
  /* - if conference is not INVITED, return -1
     - accept the conference channel
     - that should be it. we should receive a welcome message
  */

  struct mwMsgChannelAccept *msg;
  struct mwChannel *chan = conf->channel;

  char *b;
  gsize n;
  int ret;

  g_return_val_if_fail(conf->status == mwConference_INVITED, -1);

  msg = (struct mwMsgChannelAccept *)
    mwMessage_new(mwMessage_CHANNEL_ACCEPT);

  msg->head.channel = chan->id;
  msg->service = chan->service;
  msg->proto_type = chan->proto_type;
  msg->proto_ver = chan->proto_ver;

  msg->encrypt.type = mwEncrypt_RC2_40;
  msg->encrypt.opaque.len = n = 6; /* just judging from what sanity sends */
  msg->encrypt.opaque.data = b = (char *) g_malloc0(n); /* all zero */

  ret = mwChannel_accept(chan, msg);
  mwMessage_free(MW_MESSAGE(msg));

  /* send an empty conf join to let the channel know we're ready to go */
  if(! ret) ret = mwChannel_send(chan, mwChannelSend_CONF_JOIN, NULL, 0x00);

  return ret;
}


int mwConference_invite(struct mwConference *conf, struct mwIdBlock *who,
			const char *text) {

  /* - if the conference is not ACTIVE, return -1
     - compose an invitation, and send it over the channel
  */

  char *buf, *b;
  unsigned int len, n;
  int ret;

  if(conf->status != mwConference_ACTIVE)
    return -1;

  len = n = mwIdBlock_buflen(who) + 4 + 4 +
    mwString_buflen(text) + mwString_buflen(who->user);

  buf = b = (char *) g_malloc0(len);

  mwIdBlock_put(&b, &n, who);
  b += 8; n -= 8; /* four byte number, and a blank string */
  mwString_put(&b, &n, text);
  mwString_put(&b, &n, who->user); /* seems redundant... */

  ret = mwChannel_send(conf->channel, mwChannelSend_CONF_INVITE, buf, len);
  g_free(buf);

  return ret;
}


struct mwConference *mwConference_findByChannel(struct mwServiceConf *srvc,
						struct mwChannel *chan) {
  GList *l;
  for(l = srvc->conferences; l; l = l->next) {
    struct mwConference *conf = (struct mwConference *) l->data;
    if(conf->channel == chan) return conf;
  }

  return NULL;  
}


struct mwConference *mwConference_findByName(struct mwServiceConf *srvc,
					     const char *name) {
  GList *l;
  for(l = srvc->conferences; l; l = l->next) {
    struct mwConference *conf = (struct mwConference *) l->data;
    if(name && conf->name && !strcmp(conf->name, name)) return conf;
  }

  return NULL;
}


int mwConference_sendText(struct mwConference *conf, const char *text) {

  char *buf, *b;
  unsigned int len, n;
  int ret;

  /* we don't care about status, we just want to ensure we can queue the
     message for later if necessary */
  if(! conf->channel) {
    debug_printf("mwConference_sendText, there's no channel for the"
		 " conference to enqueue to\n");
    return -1;
  }

  len = n = 4 + mwString_buflen(text);
  buf = b = (char *) g_malloc0(len);

  guint32_put(&b, &n, 0x01);
  mwString_put(&b, &n, text);

  ret = mwChannel_send(conf->channel, mwChannelSend_CONF_MESSAGE, buf, len);
  g_free(buf);
  return ret;
}


int mwConference_sendTyping(struct mwConference *conf, int typing) {

  char *buf, *b;
  unsigned int len, n;
  int ret;

  if(conf->status != mwConference_ACTIVE) return -1;

  len = n = 4 + 4 + 4 + 4; /* message type, data type, data subtype, opaque */
  buf = b = (char *) g_malloc0(len);

  guint32_put(&b, &n, 0x02);
  guint32_put(&b, &n, 0x01);
  guint32_put(&b, &n, !typing);
  /* skip writing the opaque. it's empty. */

  ret = mwChannel_send(conf->channel, mwChannelSend_CONF_MESSAGE, buf, len);
  g_free(buf);
  return ret;
}

