

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

  s->channels = mwChannelSet_new(s);
  s->login.type = mwLogin_MEANWHILE;
  
  return s;
}


static void session_buf_free(struct mwSession *s) {
  g_free(s->buf);
  s->buf = NULL;
  s->buf_len = s->buf_used = 0;
}


void mwSession_free(struct mwSession *s) {
  g_return_if_fail(s != NULL);

  session_buf_free(s);

  /* seems a bad idea to free services with the session */
  /*
  while(s->services) {
    struct mwService *srv = (struct mwService *) s->services->data;
    mwSession_removeService(s, srv->type);
    mwService_free(srv);
  }
  */
  g_list_free(s->services);

  mwChannelSet_free(s->channels);

  g_free(s->auth.password);

  mwLoginInfo_clear(&s->login);
  mwUserStatus_clear(&s->status);
  mwPrivacyInfo_clear(&s->privacy);

  g_free(s);
}


void mwSession_start(struct mwSession *s) {
  g_return_if_fail(s != NULL);

  if(s->on_start)
    s->on_start(s);
}


void onStart_sendHandshake(struct mwSession *s) {
  struct mwMsgHandshake *msg;
  msg = (struct mwMsgHandshake *) mwMessage_new(mwMessage_HANDSHAKE);

  msg->major = PROTOCOL_VERSION_MAJOR;
  msg->minor = PROTOCOL_VERSION_MINOR;
  msg->login_type = s->login.type;

  mwSession_send(s, MW_MESSAGE(msg));
  mwMessage_free(MW_MESSAGE(msg));
}


void mwSession_stop(struct mwSession *s, guint32 reason) {
  GList *l = NULL;
  struct mwMsgChannelDestroy *msg;

  g_return_if_fail(s != NULL);
  g_message("stopping meanwhile session, reason: 0x%08x", reason);

  for(l = s->services; l; l = l->next)
    mwService_stop(MW_SERVICE(l->data));

  msg = (struct mwMsgChannelDestroy *)
    mwMessage_new(mwMessage_CHANNEL_DESTROY);
  
  msg->head.channel = MASTER_CHANNEL_ID;
  msg->reason = reason;

  /* don't care if this fails, we're closing the connection anyway */
  mwSession_send(s, MW_MESSAGE(msg));
  mwMessage_free(MW_MESSAGE(msg));

  /* let the listener know what's about to happen */
  if(s->on_stop)
    s->on_stop(s, reason);

  session_buf_free(s);

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


void onHandshakeAck_sendLogin(struct mwSession *s,
			      struct mwMsgHandshakeAck *msg) {

  struct mwMsgLogin *log = (struct mwMsgLogin *)
    mwMessage_new(mwMessage_LOGIN);

  log->type = s->login.type;
  log->name = g_strdup(s->login.user_id);
  log->auth_type = mwAuthType_ENCRYPT;

  compose_auth(&log->auth_data, s->auth.password);
  
  mwSession_send(s, MW_MESSAGE(log));
  mwMessage_free(MW_MESSAGE(log));
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

  /* create the channel */
  chan = mwChannel_newIncoming(s->channels, msg->channel);
  mwChannel_create(chan, msg);

  srvc = mwSession_getService(s, msg->service);
  if(srvc) {
    /* notify the service */
    mwService_recvChannelCreate(srvc, chan, msg);

  } else {
    /* seems other clients send ERR_IM_NOT_REGISTERED ? */
    mwChannel_destroyQuick(chan, ERR_SERVICE_NO_SUPPORT);
  }
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

  /* This doesn't really work out right. But later it will! */
  if(msg->head.channel == MASTER_CHANNEL_ID) {
    mwSession_stop(s, msg->reason);
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

  guint chan_id = msg->head.channel;
  struct mwChannel *chan;
  struct mwService *srvc;

  chan = mwChannel_find(s->channels, chan_id);
  g_return_if_fail(chan != NULL);

  if(CHAN_IS_INCOMING(chan)) {
    g_warning("bad channel id: 0x%x", chan_id);
    mwChannel_destroyQuick(chan, ERR_REQUEST_INVALID);
    return;
  }

  if(chan->status != mwChannel_WAIT) {
    g_warning("channel status not WAIT");
    mwChannel_destroyQuick(chan, ERR_REQUEST_INVALID);
    return;
  }
  
  srvc = mwSession_getService(s, chan->service);
  if(! srvc) {
    g_warning("no service: 0x%x", chan->service);
    mwChannel_destroyQuick(chan, ERR_SERVICE_NO_SUPPORT);
    return;
  }

  mwChannel_accept(chan, msg);
  
  /* let the service know */
  mwService_recvChannelAccept(srvc, chan, msg);
}


static void SET_USER_STATUS_recv(struct mwSession *s,
				 struct mwMsgSetUserStatus *msg) {
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

  struct mwMessage *msg;

  g_assert(s != NULL);

  /* attempt to parse the message. */
  msg = mwMessage_get(b, n);
  g_return_if_fail(msg != NULL);

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
    g_warning("unknown message type 0x%04x, no handler", msg->type);
  }

  mwMessage_free(msg);
}


#undef CASE


#define ADVANCE(b, n, count) (b += count, n -= count)


/* handle input to complete an existing buffer */
static gsize session_recv_cont(struct mwSession *s, const char *b, gsize n) {

  /* determine how many bytes still required */
  gsize x = s->buf_len - s->buf_used;
  
  if(n < x) {
    /* not quite enough; still need some more */
    memcpy(s->buf+s->buf_used, b, n);
    s->buf_used += n;
    return 0;
    
  } else {
    /* enough to finish the buffer, at least */
    memcpy(s->buf+s->buf_used, b, x);
    ADVANCE(b, n, x);
    
    if(s->buf_len == 4) {
      /* if only the length bytes were being buffered, we'll now try
       to complete an actual message */
      
      x = guint32_peek(s->buf, 4);
      if(n < x) {
	/* there isn't enough to meet the demands of the length, so
	   we'll buffer it for next time */

	char *t;
	x += 4;
	t = (char *) g_malloc(x);
	memcpy(t, s->buf, 4);
	memcpy(t+4, b, n);
	
	session_buf_free(s);
	
	s->buf = t;
	s->buf_len = x;
	s->buf_used = n + 4;
	return 0;
	
      } else {
	/* there's enough (maybe more) for a full message. don't
	   need the old session buffer (which recall, was only the
	   length bytes) any more */
	
	session_buf_free(s);
	session_process(s, b, x);
	ADVANCE(b, n, x);
      }
      
    } else {
      /* process the now-complete buffer. remember to skip the first
	 four bytes, since they're just the size count */
      session_process(s, s->buf+4, s->buf_len-4);
      session_buf_free(s);
    }
  }

  return n;
}


/* handle input when there's nothing previously buffered */
static gsize session_recv_empty(struct mwSession *s, const char *b, gsize n) {
  gsize x;

  if(n < 4) {
    /* uh oh. less than four bytes means we've got an incomplete length
       indicator. Have to buffer to get the rest of it. */
    s->buf = (char *) g_malloc0(4);
    memcpy(s->buf, b, n);
    s->buf_len = 4;
    s->buf_used = n;
    return 0;
  }
  
  /* peek at the length indicator. if it's a zero length message,
     forget it */
  x = guint32_peek(b, n);
  if(! x) return 0;

  if(n < (x + 4)) {
    /* if the total amount of data isn't enough to cover the length
       bytes and the length indicated by those bytes, then we'll need
       to buffer */

    x += 4;
    s->buf = (char *) g_malloc(x);
    memcpy(s->buf, b, n);
    s->buf_len = x;
    s->buf_used = n;
    return 0;
    
  } else {
    /* advance past length bytes */
    ADVANCE(b, n, 4);
    
    /* process and advance */
    session_process(s, b, x);
    ADVANCE(b, n, x);

    /* return left-over count */
    return n;
  }
}


static gsize session_recv(struct mwSession *s, const char *b, gsize n) {
  /* This is messy and kind of confusing. I'd like to simplify it at
     some point, but the constraints are as follows:

      - buffer up to a single full message on the session buffer
      - buffer must contain the four length bytes
      - the four length bytes indicate how much we'll need to buffer
      - the four length bytes might not arrive all at once, so it's
        possible that we'll need to buffer to get them.
      - since our buffering includes the length bytes, we know we
        still have an incomplete length if the buffer length is only
        four. */
  
  /** @todo we should allow a compiled-in upper limit to message
     sizes, and just drop messages over that size. However, to do that
     we'd need to keep track of the size of a message and keep
     dropping bytes until we'd fulfilled the entire length. eg: if we
     receive a message size of 10MB, we need to pass up exactly 10MB
     before it's safe to start processing the rest as a new
     message. */
  
  if(n && (s->buf_len == 0) && (*b & 0x80)) {
    /* keep-alive and series bytes are ignored */
    ADVANCE(b, n, 1);
  }

  if(n <= 0) {
    return 0;

  } else if(s->buf_len > 0) {
    return session_recv_cont(s, b, n);

  } else {
    return session_recv_empty(s, b, n);
  }
}


#undef ADVANCE


void mwSession_recv(struct mwSession *s, const char *buf, gsize n) {
  char *b = (char *) buf;
  gsize remain = 0;

  while(n > 0) {
    remain = session_recv(s, b, n);
    b += (n - remain);
    n = remain;
  }
}


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

  g_return_val_if_fail(s->handler, -1);

  /* ensure we could determine the required buflen */
  n = len = mwMessage_buflen(msg);
  g_return_val_if_fail(len > 0, -1);

  n = len = len + 4;
  b = buf = g_malloc(len);
  
  guint32_put(&b, &n, len - 4);
  ret = mwMessage_put(&b, &n, msg);

  /* ensure we could correctly serialize the message */
  if(! ret) {
    s->handler->write(s->handler, buf, len);
    g_message("mwSession_send, sent %u bytes", len);

    /* trigger these outgoing events */
    switch(msg->type) {
    case mwMessage_HANDSHAKE:
      HANDSHAKE_recv(s, (struct mwMsgHandshake *) msg);
      break;
    case mwMessage_LOGIN:
      LOGIN_recv(s, (struct mwMsgLogin *) msg);
      break;
    case mwMessage_SET_USER_STATUS:
      /* maybe not necessary if the server always replies */
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

  msg = (struct mwMsgSetUserStatus *)
    mwMessage_new(mwMessage_SET_USER_STATUS);

  mwUserStatus_clone(&msg->status, stat);

  ret = mwSession_send(s, MW_MESSAGE(msg));
  mwMessage_free(MW_MESSAGE(msg));

  return ret;
}


int mwSession_putService(struct mwSession *s, struct mwService *srv) {
  g_return_val_if_fail(s != NULL, -1);
  g_return_val_if_fail(srv != NULL, -1);

  if(mwSession_getService(s, srv->type)) {
    return 1;

  } else {
    s->services = g_list_prepend(s->services, srv);
    return 0;
  }
}


struct mwService *mwSession_getService(struct mwSession *s, guint32 srv) {
  GList *l;

  g_return_val_if_fail(s != NULL, NULL);

  for(l = s->services; l; l = l->next) {
    struct mwService *svc = MW_SERVICE(l->data);
    if(mwService_getServiceType(svc) == srv) return svc;
  }

  return NULL;
}


int mwSession_removeService(struct mwSession *s, guint32 srv) {
  struct mwService *svc;
  int ret = 1;

  g_return_val_if_fail(s != NULL, -1);

  while((svc = mwSession_getService(s, srv))) {
    ret = 0;
    s->services = g_list_remove_all(s->services, svc);
  }
  return ret;
}


