

#include <glib.h>
#include <glib/glist.h>
#include <string.h>

#include "channel.h"
#include "error.h"
#include "message.h"
#include "mw_debug.h"
#include "service.h"
#include "session.h"
#include "srvc_im.h"


/** @todo:
    - service structure should not be public
    - map mwIdBlock to chat structure
    - start/stop 
*/


enum send_actions {
  mwChannelSend_CHAT_MESSAGE    = 0x0064, /* im */
};


enum mwIMType {
  mwIM_TEXT  = 0x00000001, /* text message */
  mwIM_DATA  = 0x00000002  /* status message (usually) */
};


enum mwIMDataType {
  mwIMData_TYPING   = 0x00000001,
  mwIMData_MESSAGE  = 0x00000004
};


static int send_create(struct mwChannel *chan) {
  struct mwMsgChannelCreate *msg;
  char *b;
  gsize n;

  int ret;

  msg = (struct mwMsgChannelCreate *)
    mwMessage_new(mwMessage_CHANNEL_CREATE);

  mwIdBlock_clone(&msg->target, &chan->user);
  msg->channel = chan->id;
  msg->service = chan->service;
  msg->proto_type = chan->proto_type;
  msg->proto_ver = chan->proto_ver;

  msg->addtl.len = n = 8;
  msg->addtl.data = b = (char *) g_malloc(n);
  guint32_put(&b, &n, 0x01);
  guint32_put(&b, &n, 0x01);

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

  if(ret) mwChannel_destroy(chan, NULL);

  return ret;
}


static struct mwChannel *make_channel(struct mwChannelSet *cs,
				      struct mwIdBlock *user) {

  int ret = 0;
  struct mwChannel *chan = mwChannel_newOutgoing(cs);

  chan->status = mwChannel_INIT;

  mwIdBlock_clone(&chan->user, user);

  chan->service = mwService_IM;
  chan->proto_type = mwProtocol_IM;
  chan->proto_ver = 0x03;
  chan->encrypt.type = mwEncrypt_RC2_40;

  ret = send_create(chan);
  return ret? NULL: chan;
}


static struct mwChannel *find_channel(struct mwChannelSet *cs,
				      struct mwIdBlock *user) {
  GList *l;

  for(l = cs->incoming; l; l = l->next) {
    struct mwChannel *c = (struct mwChannel *) l->data;
    if( (c->service == mwService_IM) && mwIdBlock_equal(user, &c->user) )
      return c;
  }

  for(l = cs->outgoing; l; l = l->next) {
    struct mwChannel *c = (struct mwChannel *) l->data;
    if( (c->service == mwService_IM) && mwIdBlock_equal(user, &c->user) )
      return c;
  }

  g_message(" failed to find_channel for (%s, %s)\n",
	    user->user, user->community);

  return NULL;
}


static int send_accept(struct mwChannel *chan) {

  struct mwSession *s = chan->session;
  struct mwMsgChannelAccept *msg;

  char *b;
  unsigned int n;
  int ret;

  msg = (struct mwMsgChannelAccept *)
    mwMessage_new(mwMessage_CHANNEL_ACCEPT);

  msg->head.channel = chan->id;
  msg->service = chan->service;
  msg->proto_type = chan->proto_type;
  msg->proto_ver = chan->proto_ver;

  msg->addtl.len = n = 12 + mwUserStatus_buflen(&s->status);
  msg->addtl.data = b = (char *) g_malloc(n);

  guint32_put(&b, &n, 0x01);
  guint32_put(&b, &n, 0x01);
  guint32_put(&b, &n, 0x02);
  mwUserStatus_put(&b, &n, &s->status);

  msg->encrypt.type = mwEncrypt_RC2_40;
  msg->encrypt.opaque.len = n = 9; /* 1 + 4 + 4 */
  msg->encrypt.opaque.data = b = (char *) g_malloc(n);

  gboolean_put(&b, &n, 0);
  guint32_put(&b, &n, 0x00000000);
  guint32_put(&b, &n, 0x00100000);

  ret = mwChannel_accept(chan, msg);
  mwMessage_free(MW_MESSAGE(msg));

  return ret;
}


static void recv_channelCreate(struct mwService *srvc, struct mwChannel *chan,
			       struct mwMsgChannelCreate *msg) {

  /* - ensure it's the right service,proto,and proto ver
     - check the opaque for the right opaque junk
     - if not, close channel
     - compose & send a channel accept
  */

  struct mwSession *s;
  struct mwChannelSet *cs;
  unsigned int a, b;

  char *buf = msg->addtl.data;
  gsize n = msg->addtl.len;
  
  s = chan->session;
  cs = s->channels;

  if( (msg->service != mwService_IM) ||
      (msg->proto_type != mwProtocol_IM) ||
      (msg->proto_ver != 0x03) ) {

    g_warning(" unacceptable service/proto/ver, 0x%04x, 0x%04x, 0x%04x",
	      msg->service, msg->proto_type, msg->proto_ver);
    mwChannel_destroyQuick(chan, ERR_SERVICE_NO_SUPPORT);

  } else if( guint32_get(&buf, &n, &a) ||
	     guint32_get(&buf, &n, &b) ) {

    g_warning(" bad/malformed addtl");
    mwChannel_destroyQuick(chan, ERR_SERVICE_NO_SUPPORT);

  } else if(a != 0x01) {
    g_message(" unknown params: param = 0x%08x, sub param = 0x%08x", a, b);
    mwChannel_destroyQuick(chan, ERR_IM_NOT_REGISTERED);

  } else if(b == 0x19) {
    g_message(" rejecting pre-conference");
    mwChannel_destroyQuick(chan, ERR_IM_NOT_REGISTERED);

  } else if(s->status.status == mwStatus_BUSY) {
    g_message(" rejecting chat due to DND status");
    mwChannel_destroyQuick(chan, ERR_CLIENT_USER_DND);

  } else {
    g_message(" accepting: param = 0x01, sub_param = 0x%08x", b);

    if(send_accept(chan)) {
      g_message(" sending accept failed");
      mwChannel_destroyQuick(chan, ERR_FAILURE);
    }
  }
}


static void recv_channelAccept(struct mwService *srvc, struct mwChannel *chan,
			       struct mwMsgChannelAccept *msg) {
  ; /* nuttin' */
}


static void recv_channelDestroy(struct mwService *srvc, struct mwChannel *chan,
				struct mwMsgChannelDestroy *msg) {

  struct mwServiceIM *srvc_im = (struct mwServiceIM *) srvc;

  if(chan->status == mwChannel_WAIT) {
    /* we were trying to create a channel to this person, but it failed. */
    if(srvc_im->got_error)
      srvc_im->got_error(srvc_im, &chan->user, msg->reason);
    
  } else {
    if(srvc_im->got_typing)
      srvc_im->got_typing(srvc_im, &chan->user, FALSE);
  }
}


static int recv_text(struct mwServiceIM *srvc, struct mwChannel *chan,
		     const char *b, unsigned int n) {

  char *text;
  if( mwString_get((char **) &b, &n, &text) )
    return -1;

  /* sometimes we receive a zero-length string. Let's ignore those */
  if(!text || !*text) return 0;

  /* we're active again */
  mwChannel_markActive(chan, TRUE);

  if(srvc->got_text)
    srvc->got_text(srvc, &chan->user, text);

  g_free(text);
  return 0;
}


static int recv_data(struct mwServiceIM *srvc, struct mwChannel *chan,
		     const char *b, unsigned int n) {

  unsigned int t, st;
  struct mwOpaque o;
  char *x;

  if( guint32_get((char **) &b, &n, &t) ||
      guint32_get((char **) &b, &n, &st) ||
      mwOpaque_get((char **) &b, &n, &o) )
    return -1;

  switch(t) {
  case mwIMData_TYPING:
    if(srvc->got_typing)
      srvc->got_typing(srvc, &chan->user, !st);
    break;

  case mwIMData_MESSAGE:
    /* a data message with a text message in it. what client is
       sending these, and why? Could this be Notes Buddy sending html
       messages? */

    if(o.len) {
      mwChannel_markActive(chan, TRUE);

      if(srvc->got_text) {
	x = (char *) g_malloc(o.len + 1);
	x[o.len] = '\0';
	memcpy(x, o.data, o.len);
	srvc->got_text(srvc, &chan->user, x);
	g_free(x);
      }
    }
    break;

  default:
    g_warning("unknown data message subtype 0x%04x for im service\n", t);
  }

  mwOpaque_clear(&o);
  return 0;
}


static void recv(struct mwService *srvc, struct mwChannel *chan,
		 guint16 type, const char *b, gsize n) {

  /* - ensure message type is something we want
     - parse message type into either mwIMText or mwIMData
     - handle
  */

  unsigned int mt;
  int ret = 0;

  g_return_if_fail( type == mwChannelSend_CHAT_MESSAGE );
  g_return_if_fail( ! (ret = guint32_get((char **) &b, &n, &mt)) );

  switch(mt) {
  case mwIM_TEXT:
    ret = recv_text((struct mwServiceIM *) srvc, chan, b, n);
    break;

  case mwIM_DATA:
    ret = recv_data((struct mwServiceIM *) srvc, chan, b, n);
    break;

  default:
    g_warning("unknown message type 0x%04x for im service", mt);
  }

  if(ret) {
    g_warning("failed to parse message of type 0x%04x for im service", mt);
    /* consider closing the channel */
  }
}


static void clear(struct mwService *srvc) {
  ; /* no special cleanup required */
}


static const char *name() {
  return "Basic Instant Messaging";
}


static const char *desc() {
  return "A simple IM service, with typing notification";
}


struct mwServiceIM *mwServiceIM_new(struct mwSession *session) {
  struct mwServiceIM *srvc_im = g_new0(struct mwServiceIM, 1);
  struct mwService *srvc = &srvc_im->service;

  srvc->session = session;
  srvc->type = mwService_IM; /* one of the BaseSericeTypes */

  srvc->recv_channelCreate = recv_channelCreate;
  srvc->recv_channelAccept = recv_channelAccept;
  srvc->recv_channelDestroy = recv_channelDestroy;
  srvc->recv = recv;
  srvc->clear = clear;
  srvc->get_name = name;
  srvc->get_desc = desc;

  return srvc_im;
}


int mwServiceIM_sendText(struct mwServiceIM *srvc,
			 struct mwIdBlock *target, const char *text) {
  char *buf, *b;
  gsize len, n;

  int ret;

  struct mwChannelSet *cs;
  struct mwChannel *chan;

  g_return_val_if_fail(srvc && srvc->service.session, -1);

  cs = srvc->service.session->channels;
  chan = find_channel(cs, target);

  if(! chan) chan = make_channel(cs, target);
  g_return_val_if_fail(chan, -1);

  len = n = 4 + mwString_buflen(text);
  buf = b = (char *) g_malloc(len);

  if( guint32_put(&b, &n, mwIM_TEXT) ||
      mwString_put(&b, &n, text) )
    return -1;

  ret = mwChannel_send(chan, mwChannelSend_CHAT_MESSAGE, buf, len);
  g_free(buf);
  return ret;
}


int mwServiceIM_sendTyping(struct mwServiceIM *srvc,
			   struct mwIdBlock *target, gboolean typing) {
  char *buf, *b;
  gsize len, n;

  int ret;

  struct mwChannelSet *cs;
  struct mwChannel *chan;

  g_return_val_if_fail(srvc && srvc->service.session, -1);

  cs = srvc->service.session->channels;
  chan = find_channel(cs, target);

  /* don't bother creating a channel just to send a typing notificiation */
  if(! chan) return 0;

  len = n = 16; /* im type + data type + data subtype + opaque len */
  buf = b = (char *) g_malloc(len);

  if( guint32_put(&b, &n, mwIM_DATA) ||
      guint32_put(&b, &n, mwIMData_TYPING) ||
      guint32_put(&b, &n, !typing) ||
      guint32_put(&b, &n, 0x00000000) ) /* an opaque with nothing in it */
    return -1;

  ret = mwChannel_send(chan, mwChannelSend_CHAT_MESSAGE, buf, len);
  g_free(buf);
  return ret;
}


void mwServiceIM_closeChat(struct mwServiceIM *srvc,
			   struct mwIdBlock *target) {

  struct mwChannelSet *cs;
  struct mwChannel *chan;

  g_return_if_fail(srvc && srvc->service.session && target);

  cs = srvc->service.session->channels;
  chan = find_channel(cs, target);

  if(chan) chan->inactive = time(NULL);
  /* may want to ensure that a stopped-typing message is sent */
}
