

#include <glib.h>
#include <string.h>

#include "channel.h"
#include "cipher.h"
#include "error.h"
#include "message.h"
#include "mw_debug.h"
#include "service.h"
#include "session.h"


struct mwSession *mwSession_new() {
  struct mwSession *s = g_new0(struct mwSession, 1);

  s->channels = g_new0(struct mwChannelSet, 1);
  s->channels->session = s;
  
  return s;
}


void mwSession_free(struct mwSession **session) {
  struct mwSession *s = *session;

  *session = NULL;

  while(s->services) {
    struct mwService *srv = (struct mwService *) s->services->data;
    mwSession_removeService(s, srv->type);
    mwService_free(&srv);
  }

  mwChannelSet_clear(s->channels);
  g_free(s->channels);

  g_free(s->buf);
  g_free(s->auth.password);

  mwLoginInfo_clear(&s->login);
  mwUserStatus_clear(&s->status);
  mwPrivacyInfo_clear(&s->privacy);

  g_free(s);
}


void mwSession_initConnect(struct mwSession *s) {
  if(s->on_initConnect)
    s->on_initConnect(s);
}


void initConnect_sendHandshake(struct mwSession *s) {
  struct mwMsgHandshake *msg;
  msg = (struct mwMsgHandshake *) mwMessage_new(mwMessage_HANDSHAKE);

  msg->major = PROTOCOL_VERSION_MAJOR;
  msg->minor = PROTOCOL_VERSION_MINOR;
  msg->login_type = mwLogin_JAVA_APP; /* what else can we use? */

  mwSession_send(s, MESSAGE(msg));
  mwMessage_free(MESSAGE(msg));
}


void mwSession_closeConnect(struct mwSession *s, guint32 reason) {
  
  struct mwMsgChannelDestroy *msg = (struct mwMsgChannelDestroy *)
    mwMessage_new(mwMessage_CHANNEL_DESTROY);

  msg->head.channel = MASTER_CHANNEL_ID;
  msg->reason = reason;

  /* don't care if this fails, we're closing the connection anyway */
  mwSession_send(s, MESSAGE(msg));
  mwMessage_free(MESSAGE(msg));

  /* let the listener know what's about to happen */
  if(s->on_closeConnect)
    s->on_closeConnect(s, reason);

  /* close the connection */
  if(s->handler)
    s->handler->close(s->handler);
}


static void HANDSHAKE_recv(struct mwSession *s, struct mwMsgHandshake *msg) {
  if(s->on_handshake)
    s->on_handshake(s, msg);
}


static void HANDSHAKE_ACK_recv(struct mwSession *s,
			       struct mwMsgHandshakeAck *msg) {
  if(s->on_handshakeAck)
    s->on_handshakeAck(s, msg);
}


static void compose_auth(struct mwOpaque *auth, const char *pass) {
  struct mwOpaque a, b;
  char iv[8], key[5];
  char *buf;
  unsigned int len;

  /* get an IV and a random five-byte key */
  mwIV_init((char *) iv);
  rand_key((char *) key, 5);

  /* the opaque with the key */
  a.len = 5;
  a.data = key;

  b.len = 0;
  b.data = NULL;

  /* the opaque with the encrypted pass */
  mwEncrypt(a.data, a.len, iv, pass, strlen(pass), &b.data, &b.len);

  /* and opaque containing the other two opaques */
  len = auth->len = mwOpaque_buflen(&a) + mwOpaque_buflen(&b);
  buf = auth->data = (char *) g_malloc(len);
  mwOpaque_put(&buf, &len, &a);
  mwOpaque_put(&buf, &len, &b);

  /* this is the only one to clear, as the Encrypt malloc'd the buffer */
  g_free(b.data);
}


void handshakeAck_sendLogin(struct mwSession *s,
			    struct mwMsgHandshakeAck *msg) {

  struct mwMsgLogin *log = (struct mwMsgLogin *)
    mwMessage_new(mwMessage_LOGIN);

  log->type = mwLogin_JAVA_APP; /* bleh. Find something else to use */
  log->name = g_strdup(s->login.user_id);
  log->auth_type = mwAuthType_ENCRYPT;

  compose_auth(&log->auth_data, s->auth.password);
  
  mwSession_send(s, MESSAGE(log));
  mwMessage_free(MESSAGE(log));
}


static void LOGIN_recv(struct mwSession *s, struct mwMsgLogin *msg) {
  if(s->on_login)
    s->on_login(s, msg);
}


/*
static void LOGIN_REDIRECT_recv(struct mwSession *s,
				struct mwMsgLoginRedirect *msg) {
  if(s->on_loginRedirect)
    s->on_loginRedirect(s, msg);
}
*/


static void LOGIN_ACK_recv(struct mwSession *s, struct mwMsgLoginAck *msg) {

  debug_printf(" --> LOGIN_ACK_recv\n");

  /* store the login information in the session */
  mwLoginInfo_clear(&s->login);
  mwLoginInfo_clone(&s->login, &msg->login);

  /* expand the five-byte login id into a key */
  mwKeyExpand((int *) s->session_key, msg->login.login_id, 5);

  if(s->on_loginAck)
    s->on_loginAck(s, msg);
}


static void CHANNEL_CREATE_recv(struct mwSession *s,
				struct mwMsgChannelCreate *msg) {
  /* - look up the service
     - if there's no such service, close the channel, return
     - create an incoming channel with the appropriate id, status WAIT
     - recv_createChannel on the service
  */

  struct mwChannel *chan;
  struct mwService *srvc;

  srvc = mwSession_getService(s, msg->service);
  if(! srvc) {
    /* seems other clients send ERR_IM_NOT_REGISTERED ? */
    mwChannel_destroyQuick(s->channels, msg->channel,
			   ERR_SERVICE_NO_SUPPORT, NULL);
    return;
  }

  /* create the channel */
  chan = mwChannel_newIncoming(s->channels, msg->channel);
  mwChannel_create(chan, msg);

  /* notify the service */
  mwService_recvChannelCreate(srvc, chan, msg);
}


static void CHANNEL_DESTROY_recv(struct mwSession *s,
				 struct mwMsgChannelDestroy *msg) {
  /* - look up the channel
     - if there's no such channel, return
     - look up channel's service
     - if there's a service, recv_destroyChannel on it
     - remove the channel
  */

  struct mwChannel *chan;
  struct mwService *srvc;

  debug_printf(" --> CHANNEL_DESTROY_recv\n");

  /* This doesn't really work out right. But later it will! */
  if(msg->head.channel == MASTER_CHANNEL_ID) {
    mwSession_closeConnect(s, msg->reason);
    return;
  }

  chan = mwChannel_find(s->channels, msg->head.channel);
  g_return_if_fail(chan);

  srvc = mwSession_getService(s, chan->service);
  mwService_recvChannelDestroy(srvc, chan, msg);

  /* don't send the message back. annoying design feature */
  mwChannel_destroy(chan, NULL);
}


static void CHANNEL_SEND_recv(struct mwSession *s,
			      struct mwMsgChannelSend *msg) {
  /* - look up the channel
     - if there's no such channel, return
     - look up the channel's service
     - recv or queue, depending on channel state
  */

  struct mwChannel *chan = mwChannel_find(s->channels, msg->head.channel);
  g_return_if_fail(chan);
  mwChannel_recv(chan, msg);
}


static void CHANNEL_ACCEPT_recv(struct mwSession *s,
				struct mwMsgChannelAccept *msg) {
  /* - look up the channel
     - if the channel isn't outgoing, close it, return
     - if the channel isn't status WAIT, close it, return
     - look up the channel's service
     - if there's no such service, close the channel, return
     - set the channel's status to OPEN
     - recv_channelAccept on the service
     - trigger session listener
  */

  unsigned int chan_id = msg->head.channel;
  struct mwChannel *chan;
  struct mwService *srvc;

  if(CHAN_ID_IS_INCOMING(chan_id)) {
    mwChannel_destroyQuick(s->channels, chan_id, ERR_REQUEST_INVALID, NULL);
    debug_printf("CHANNEL_ACCEPT_recv, bad channel id: 0x%x\n", chan_id);
    return;
  }

  chan = mwChannel_find(s->channels, chan_id);
  g_return_if_fail(chan);

  if(chan->status != mwChannel_WAIT) {
    mwChannel_destroyQuick(s->channels, chan->id, ERR_REQUEST_INVALID, NULL);
    debug_printf("CHANNEL_ACCEPT_recv, channel status not WAIT\n");
    return;
  }
  
  srvc = mwSession_getService(s, chan->service);
  if(! srvc) {
    mwChannel_destroyQuick(s->channels, chan->id,
			   ERR_SERVICE_NO_SUPPORT, NULL);
    debug_printf("CHANNEL_ACCEPT_recv, no service: 0x%x\n", chan->service);
    return;
  }
  
  /* let the service know */
  mwService_recvChannelAccept(srvc, chan, msg);

  mwChannel_accept(chan, msg);
}


static void SET_USER_STATUS_recv(struct mwSession *s,
				 struct mwMsgSetUserStatus *msg) {
  debug_printf(" --> SET_USER_STATUS_recv\n");

  if(s->on_setUserStatus)
    s->on_setUserStatus(s, msg);

  mwUserStatus_clone(&s->status, &msg->status);
}


static void ADMIN_recv(struct mwSession *s, struct mwMsgAdmin *msg) {
  if(s->on_admin)
    s->on_admin(s, msg);
}


#define CASE(var, type) \
case mwMessage_ ## var: \
  var ## _recv(s, (struct type *) msg); \
  break;


static void session_process(struct mwSession *s,
			    const char *b, gsize n) {

  struct mwMessage *msg = NULL;

  g_assert(s);

  pretty_print(b, n);

  /* attempt to parse the message. */
  msg = mwMessage_get(b, n);
  g_return_if_fail(msg);

  /* handle each of the appropriate types of mwMessage */
  switch(msg->type) {
    CASE(HANDSHAKE, mwMsgHandshake);
    CASE(HANDSHAKE_ACK, mwMsgHandshakeAck);
    CASE(LOGIN, mwMsgLogin);
    /* CASE(LOGIN_REDIRECT, mwMsgLoginRedirect); */
    CASE(LOGIN_ACK, mwMsgLoginAck);
    CASE(CHANNEL_CREATE, mwMsgChannelCreate);
    CASE(CHANNEL_DESTROY, mwMsgChannelDestroy);
    CASE(CHANNEL_SEND, mwMsgChannelSend);
    CASE(CHANNEL_ACCEPT, mwMsgChannelAccept);
    CASE(SET_USER_STATUS, mwMsgSetUserStatus);
    CASE(ADMIN, mwMsgAdmin);
    
  default:
    g_warning("unknown message type %x, no handler\n", msg->type);
  }

  mwMessage_free(msg);
}


#undef CASE


#define ADVANCE(b, n, count) (b += count, n -= count)


void mwSession_recv(struct mwSession *s, const char *b, gsize n) {
  /* This is messy and kind of confusing. I'd like to simplify it at some
     point, but the constraints are as follows:

      - buffer up to a single full message on the session buffer
      - buffer must contain the four length bytes
      - the four length bytes indicate how much we'll need to buffer
      - the four length bytes might not arrive all at once, so it's possible
        that we'll need to buffer to get them.
      - since our buffering includes the length bytes, we know we still have an
        incomplete length if the buffer length is only four.
  */
  
  /* TODO: we should allow a compiled-in upper limit to message sizes, and just
     drop messages over that size. However, to do that we'd need to keep track
     of the size of a message and keep dropping bytes until we'd fulfilled the
     entire length. eg: if we receive a message size of 10MB, we need to pass
     up exactly 10MB before it's safe to start processing the rest as a new
     message.
  */

  gsize x;
  
  if(n && (s->buf_len == 0) && (*b & 0x80)) {
    /* keep-alive bytes are ignored */
    ADVANCE(b, n, 1);
  }

  if(! n) return;

  /* finish off anything on the session buffer */
  if(s->buf_len > 0) {

    /* determine how many bytes still required */
    x = s->buf_len - s->buf_used;

    if(n < x) {
      /* now quite enough */
      memcpy(s->buf+s->buf_used, b, n);
      s->buf_used += n;
      x = s->buf_len - s->buf_used;
      return;

    } else {
      /* enough to finish the buffer, at least */
      memcpy(s->buf+s->buf_used, b, x);
      ADVANCE(b, n, x);

      if(s->buf_len == 4) {
	/* if only the length bytes were being buffered... */
	x = guint32_peek(s->buf, 4);

	if(n < x) {
	  /* if there isn't enough to meet the demands of the length, buffer
	     it for next time */
	  char *t;
	  x += 4;
	  t = (char *) g_malloc0(x);
	  memcpy(t, s->buf, 4);
	  memcpy(t+4, b, n);
	  g_free(s->buf);
	  s->buf = t;
	  s->buf_len = x;
	  return;

	} else {
	  /* there's enough (maybe more) for a full message */

	  /* don't need the old session buffer any more */
	  g_free(s->buf);
	  s->buf = NULL;
	  s->buf_len = s->buf_used = 0;

	  session_process(s, b, x);
	  ADVANCE(b, n, x);
	}

      } else {
	/* skip the first four bytes of session buffer, since they're just the
	   size bytes */
	session_process(s, s->buf+4, s->buf_len-4);
	g_free(s->buf);
	s->buf = NULL;
	s->buf_len = s->buf_used = 0;
      }
    }
  }

  /* if we've gotten this far, then the session buffer is empty */
  /* get as many full messages out of buf as possible */
  while(n) {

    if(n < 4) {
      /* uh oh. less than four bytes means we've got an incomplete length
	 indicator. Have to buffer to get the rest of it. */
      s->buf = (char *) g_malloc0(4);
      memcpy(s->buf, b, n);
      s->buf_len = 4;
      s->buf_used = n;
      return;
    }

    /* peek at the length indicator */
    x = guint32_peek(b, n);
    if(! x) return; /* started seeing zero in those four bytes. weird */

    if(n < (x + 4)) {
      /* if the amount of data remaining isn't enough to cover the length bytes
	 and the length indicated by those bytes, then we'll need to buffer */
      x += 4;
      break;

    } else {
      /* advance past length bytes */
      ADVANCE(b, n, 4);

      /* process and advance */
      session_process(s, b, x);
      ADVANCE(b, n, x);
    }
  }

  /* alright, done with all the full messages */
  /* buffer the leftovers on the session buffer */
  if(n) {
    char *t = (char *) g_malloc0(x);
    memcpy(t, b, n);
    
    s->buf = t;
    s->buf_len = x;
    s->buf_used = n;
  }
}


#undef ADVANCE


int mwSession_send(struct mwSession *s, struct mwMessage *msg) {
  /* - allocate buflen of msg
     - serialize msg into buffer
     - tell the handler to send buffer
     - free buffer
     - for certain types of messages, trigger call-back on successful send
  */

  gsize len, n;
  char *buf, *b;
  int ret = 0;

  debug_printf(" --> mwSession_send\n");

  g_return_val_if_fail(s->handler, -1);

  /* ensure we could determine the required buflen */
  n = len = mwMessage_buflen(msg);
  g_return_val_if_fail(len, -1);

  n = len = len + 4;
  b = buf = g_malloc(len);
  
  guint32_put(&b, &n, len - 4);
  ret = mwMessage_put(&b, &n, msg);

  /* ensure we could correctly serialize the message */
  if(! ret) {
    s->handler->write(s->handler, buf, len);
    debug_printf("mwSession_send, sent %u bytes\n", len);

    /* trigger these outgoing events */
    switch(msg->type) {
    case mwMessage_HANDSHAKE:
      HANDSHAKE_recv(s, (struct mwMsgHandshake *) msg);
      break;
    case mwMessage_LOGIN:
      LOGIN_recv(s, (struct mwMsgLogin *) msg);
      break;
    case mwMessage_SET_USER_STATUS:
      SET_USER_STATUS_recv(s, (struct mwMsgSetUserStatus *) msg);
      break;
    default:
      ; /* only want to worry about those types. */
    }
  }

  g_free(buf);

  return ret;
}


int mwSession_setUserStatus(struct mwSession *s, struct mwUserStatus *stat) {
  int ret;
  struct mwMsgSetUserStatus *msg;

  debug_printf(" --> mwSession_setUserStatus\n");

  msg = (struct mwMsgSetUserStatus *)
    mwMessage_new(mwMessage_SET_USER_STATUS);

  mwUserStatus_clone(&msg->status, stat);

  ret = mwSession_send(s, MESSAGE(msg));
  mwMessage_free(MESSAGE(msg));

  return ret;
}


int mwSession_putService(struct mwSession *s, struct mwService *srv) {
  g_return_val_if_fail(s && srv, -1);

  if(mwSession_getService(s, srv->type)) {
    return 1;

  } else {
    s->services = g_list_prepend(s->services, srv);
    return 0;
  }
}


struct mwService *mwSession_getService(struct mwSession *s, guint32 srv) {
  GList *l;

  g_return_val_if_fail(s, NULL);

  for(l = s->services; l; l = l->next) {
    struct mwService *svc = (struct mwService *) l->data;
    if(svc->type == srv) return svc;
  }
  return NULL;
}


int mwSession_removeService(struct mwSession *s, guint32 srv) {
  struct mwService *svc;
  int ret = 1;

  g_return_val_if_fail(s, -1);

  while((svc = mwSession_getService(s, srv))) {
    ret = 0;
    s->services = g_list_remove_all(s->services, svc);
  }
  return ret;
}


