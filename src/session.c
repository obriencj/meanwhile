

#include <glib.h>
#include <string.h>

#include "channel.h"
#include "cipher.h"
#include "error.h"
#include "message.h"
#include "mw_debug.h"
#include "service.h"
#include "session.h"


/** the hash table key for a service, for mwSession::services */
#define SERVICE_KEY(srvc) GUINT_TO_POINTER(mwService_getType(srvc))

/** the hash table key for a cipher, for mwSession::ciphers */
#define CIPHER_KEY(ciph)  GUINT_TO_POINTER(mwCipher_getType(ciph))


struct mwSession {

  /** provides I/O and callback functions */
  struct mwSessionHandler *handler;

  enum mwSessionState state;  /**< session state */
  guint32 state_info;         /**< additional state info */
  
  char *buf;       /**< buffer for incoming message data */
  gsize buf_len;   /**< length of buf */
  gsize buf_used;  /**< offset to last-used byte of buf */

  char *password;  /**< password authentication data */
  char *token;     /**< token authentication data */
  
  struct mwLoginInfo login;      /**< login information */
  struct mwUserStatus status;    /**< session's user status */
  struct mwPrivacyInfo privacy;  /**< session's privacy list */

  /** the collection of channels */
  struct mwChannelSet *channels;

  /** the collection of services */
  GHashTable *services;

  /** the collection of ciphers */
  GHashTable *ciphers;
};


struct mwSession *mwSession_new(struct mwSessionHandler *handler) {
  struct mwSession *s;

  g_return_val_if_fail(handler != NULL, NULL);

  /* consider io_write and io_close to be absolute necessities */
  g_return_val_if_fail(handler->io_write != NULL, NULL);
  g_return_val_if_fail(handler->io_close != NULL, NULL);

  s = g_new0(struct mwSession, 1);

  s->state = mwSession_STOPPED;

  s->handler = handler;
  s->login.type = mwLogin_MEANWHILE;

  s->channels = mwChannelSet_new(s);
  s->services = g_hash_table_new(g_direct_hash, g_direct_equal);
  s->ciphers = g_hash_table_new(g_direct_hash, g_direct_equal);

  /** XXX TEST HACK */
  mwSession_addCipher(s, mwCipher_new_RC2_40(s));

  return s;
}


/** free and reset the session buffer */
static void session_buf_free(struct mwSession *s) {
  g_return_if_fail(s != NULL);

  g_free(s->buf);
  s->buf = NULL;
  s->buf_len = 0;
  s->buf_used = 0;
}


/** a polite string version of the session state enum */
static const char *state_str(enum mwSessionState state) {
  switch(state) {
  case mwSession_STARTING:      return "starting";
  case mwSession_HANDSHAKE:     return "handshake sent";
  case mwSession_HANDSHAKE_ACK: return "handshake acknowledged";
  case mwSession_LOGIN:         return "login sent";
  case mwSession_LOGIN_REDIR:   return "login redirected";
  case mwSession_LOGIN_ACK:     return "login acknowledged";
  case mwSession_STARTED:       return "started";
  case mwSession_STOPPING:      return "stopping";
  case mwSession_STOPPED:       return "stopped";

  case mwSession_UNKNOWN:       /* fall-through */
  default:                      return "UNKNOWN";
  }
}


void mwSession_free(struct mwSession *s) {
  struct mwSessionHandler *h;

  g_return_if_fail(s != NULL);

  if(! SESSION_IS_STOPPED(s)) {
    g_debug("session is not stopped (state: %s), proceeding with free",
	    state_str(s->state));
  }

  h = s->handler;
  s->handler = NULL;
  if(h) h->clear(s);

  session_buf_free(s);

  mwChannelSet_free(s->channels);
  g_hash_table_destroy(s->services);
  g_hash_table_destroy(s->ciphers);

  g_free(s->password);
  g_free(s->token);

  mwLoginInfo_clear(&s->login);
  mwUserStatus_clear(&s->status);
  mwPrivacyInfo_clear(&s->privacy);

  g_free(s);
}


/** write data to the session handler */
static int io_write(struct mwSession *s, const char *buf, gsize len) {
  g_return_val_if_fail(s != NULL, -1);
  g_return_val_if_fail(s->handler != NULL, -1);
  g_return_val_if_fail(s->handler->io_write != NULL, -1);

  return s->handler->io_write(s, buf, len);
}


/** close the session handler */
static void io_close(struct mwSession *s) {
  g_return_if_fail(s != NULL);
  g_return_if_fail(s->handler != NULL);
  g_return_if_fail(s->handler->io_close != NULL);

  s->handler->io_close(s);
}


/** set the state of the session, and trigger the session handler's
    on_stateChange function. Has no effect if the session is already
    in the specified state (ignores additional state info)

    @param s      the session
    @param state  the state to set
    @param info   additional state info
*/
static void state(struct mwSession *s, enum mwSessionState state,
		  guint32 info) {

  struct mwSessionHandler *sh;

  g_return_if_fail(s != NULL);
  g_return_if_fail(s->handler != NULL);

  if(SESSION_IS_STATE(s, state)) return;

  s->state = state;
  s->state_info = info;

  if(info) {
    g_message("session state: %s (0x%08x)", state_str(state), info);
  } else {
    g_message("session state: %s", state_str(state));
  }

  sh = s->handler;
  if(sh->on_stateChange)
    sh->on_stateChange(s, state, info);
}


void mwSession_start(struct mwSession *s) {
  struct mwMsgHandshake *msg;
  int ret;

  g_return_if_fail(s != NULL);
  g_return_if_fail(SESSION_IS_STOPPED(s));

  state(s, mwSession_STARTING, 0);

  msg = (struct mwMsgHandshake *) mwMessage_new(mwMessage_HANDSHAKE);
  msg->major = PROTOCOL_VERSION_MAJOR;
  msg->minor = PROTOCOL_VERSION_MINOR;
  msg->login_type = s->login.type;

  ret = mwSession_send(s, MW_MESSAGE(msg));
  mwMessage_free(MW_MESSAGE(msg));

  if(ret) {
    mwSession_stop(s, CONNECTION_BROKEN);
  } else {
    state(s, mwSession_HANDSHAKE, 0);
  }
}


void mwSession_stop(struct mwSession *s, guint32 reason) {
  GList *list, *l = NULL;
  struct mwMsgChannelDestroy *msg;

  g_return_if_fail(s != NULL);
  g_return_if_fail(! SESSION_IS_STOPPING(s));
  g_return_if_fail(! SESSION_IS_STOPPED(s));

  state(s, mwSession_STOPPING, reason);

  for(list = l = mwSession_getServices(s); l; l = l->next)
    mwService_stop(MW_SERVICE(l->data));
  g_list_free(list);

  msg = (struct mwMsgChannelDestroy *)
    mwMessage_new(mwMessage_CHANNEL_DESTROY);

  msg->head.channel = MASTER_CHANNEL_ID;
  msg->reason = reason;

  /* don't care if this fails, we're closing the connection anyway */
  mwSession_send(s, MW_MESSAGE(msg));
  mwMessage_free(MW_MESSAGE(msg));

  session_buf_free(s);

  /* close the connection */
  io_close(s);

  state(s, mwSession_STOPPED, reason);
}


void mwSession_setUserId(struct mwSession *s, const char *user) {
  g_return_if_fail(s != NULL);
  g_free(s->login.user_id);
  s->login.user_id = g_strdup(user);
}


void mwSession_setPassword(struct mwSession *s, const char *pass) {
  g_return_if_fail(s != NULL);
  g_free(s->password);
  s->password = g_strdup(pass);
}


/** compose authentication information into an opaque based on the
    password */
static void compose_auth(struct mwOpaque *auth, const char *pass) {
  char iv[8], key[5];
  struct mwOpaque a, b, z;
  struct mwPutBuffer *p;

  /* get an IV and a random five-byte key */
  mwIV_init((char *) iv);
  rand_key((char *) key, 5);

  /* the opaque with the key */
  a.len = 5;
  a.data = key;

  /* the opaque to receive the encrypted pass */
  b.len = 0;
  b.data = NULL;

  /* the plain-text pass dressed up as an opaque */
  z.len = strlen(pass);
  z.data = (char *) pass;

  /* the opaque with the encrypted pass */
  mwEncrypt(a.data, a.len, iv, &z, &b);

  /* an opaque containing the other two opaques */
  p = mwPutBuffer_new();
  mwOpaque_put(p, &a);
  mwOpaque_put(p, &b);
  mwPutBuffer_finalize(auth, p);

  /* this is the only one to clear, as the other uses a static buffer */
  mwOpaque_clear(&b);
}


/** handle the receipt of a handshake_ack message by sending the login
    message */
static void HANDSHAKE_ACK_recv(struct mwSession *s,
			       struct mwMsgHandshakeAck *msg) {
  struct mwMsgLogin *log;
  int ret;
			       
  g_return_if_fail(s != NULL);
  g_return_if_fail(msg != NULL);
  g_return_if_fail(SESSION_IS_STATE(s, mwSession_HANDSHAKE));

  state(s, mwSession_HANDSHAKE_ACK, 0);

  log = (struct mwMsgLogin *) mwMessage_new(mwMessage_LOGIN);
  log->login_type = s->login.type;
  log->name = g_strdup(s->login.user_id);
  log->auth_type = mwAuthType_ENCRYPT;

  /* default to password for now */
  compose_auth(&log->auth_data, s->password);
  
  ret = mwSession_send(s, MW_MESSAGE(log));
  mwMessage_free(MW_MESSAGE(log));
  if(! ret) state(s, mwSession_LOGIN, 0);
}


/* I really need to set up a second server to test this */
/*
static void LOGIN_REDIRECT_recv(struct mwSession *s,
				struct mwMsgLoginRedirect *msg) {
  if(s->on_loginRedirect)
    s->on_loginRedirect(s, msg);
}
*/


/** handle the receipt of a login_ack message. This completes the
    startup sequence for the session */
static void LOGIN_ACK_recv(struct mwSession *s,
			   struct mwMsgLoginAck *msg) {
  GList *ll, *l;

  g_return_if_fail(s != NULL);
  g_return_if_fail(msg != NULL);
  g_return_if_fail(SESSION_IS_STATE(s, mwSession_LOGIN));

  /* store the login information in the session */
  mwLoginInfo_clear(&s->login);
  mwLoginInfo_clone(&s->login, &msg->login);

  state(s, mwSession_LOGIN_ACK, 0);

  /* @todo any further startup stuff? */
  state(s, mwSession_STARTED, 0);

  /* send sense service requests for each added service */
  for(ll = l = mwSession_getServices(s); l; l = l->next)
    mwSession_senseService(s, mwService_getType(MW_SERVICE(l->data)));
  g_list_free(ll);
}


static void CHANNEL_CREATE_recv(struct mwSession *s,
				struct mwMsgChannelCreate *msg) {
  struct mwChannel *chan;
  chan = mwChannel_newIncoming(s->channels, msg->channel);

  mwChannel_recvCreate(chan, msg);
}


static void CHANNEL_ACCEPT_recv(struct mwSession *s,
				struct mwMsgChannelAccept *msg) {
  struct mwChannel *chan;
  chan = mwChannel_find(s->channels, msg->head.channel);

  g_return_if_fail(chan != NULL);

  mwChannel_recvAccept(chan, msg);
}


static void CHANNEL_DESTROY_recv(struct mwSession *s,
				 struct mwMsgChannelDestroy *msg) {

  /* the server can indicate that we should close the session by
     destroying the zero channel */
  if(msg->head.channel == MASTER_CHANNEL_ID) {
    mwSession_stop(s, msg->reason);

  } else {
    struct mwChannel *chan;
    chan = mwChannel_find(s->channels, msg->head.channel);

    g_return_if_fail(chan != NULL);
    
    mwChannel_recvDestroy(chan, msg);
  }
}


static void CHANNEL_SEND_recv(struct mwSession *s,
			      struct mwMsgChannelSend *msg) {
  struct mwChannel *chan;
  chan = mwChannel_find(s->channels, msg->head.channel);

  g_return_if_fail(chan != NULL);

  mwChannel_recv(chan, msg);
}


static void SET_USER_STATUS_recv(struct mwSession *s,
				 struct mwMsgSetUserStatus *msg) {
  struct mwSessionHandler *sh = s->handler;

  if(sh && sh->on_setUserStatus)
    sh->on_setUserStatus(s);

  mwUserStatus_clone(&s->status, &msg->status);
}


static void SENSE_SERVICE_recv(struct mwSession *s,
			       struct mwMsgSenseService *msg) {
  struct mwService *srvc;

  srvc = mwSession_getService(s, msg->service);
  if(srvc) mwService_start(srvc);
}


static void ADMIN_recv(struct mwSession *s, struct mwMsgAdmin *msg) {
  struct mwSessionHandler *sh = s->handler;

  if(sh && sh->on_admin)
    sh->on_admin(s, msg->text);
}


static void LOGIN_REDIRECT_recv(struct mwSession *s,
				struct mwMsgLoginRedirect *msg) {
  struct mwSessionHandler *sh = s->handler;

  if(sh && sh->on_loginRedirect)
    sh->on_loginRedirect(s, msg->host);
}



#define CASE(var, type) \
case mwMessage_ ## var: \
  var ## _recv(s, (struct type *) msg); \
  break;


static void session_process(struct mwSession *s,
			    const char *buf, gsize len) {

  struct mwOpaque o = { len, (char *) buf };
  struct mwGetBuffer *b;
  struct mwMessage *msg;

  g_assert(s != NULL);
  g_return_if_fail(buf != NULL);

  /* ignore zero-length messages */
  if(len == 0) return;

  /* wrap up buf */
  b = mwGetBuffer_wrap(&o);

  /* attempt to parse the message. */
  msg = mwMessage_get(b);
  g_return_if_fail(msg != NULL);

  /* handle each of the appropriate incoming types of mwMessage */
  switch(msg->type) {
    CASE(HANDSHAKE_ACK, mwMsgHandshakeAck);
    CASE(LOGIN_REDIRECT, mwMsgLoginRedirect);
    CASE(LOGIN_ACK, mwMsgLoginAck);
    CASE(CHANNEL_CREATE, mwMsgChannelCreate);
    CASE(CHANNEL_DESTROY, mwMsgChannelDestroy);
    CASE(CHANNEL_SEND, mwMsgChannelSend);
    CASE(CHANNEL_ACCEPT, mwMsgChannelAccept);
    CASE(SET_USER_STATUS, mwMsgSetUserStatus);
    CASE(SENSE_SERVICE, mwMsgSenseService);
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

  /* g_message(" session_recv_cont: session = %p, b = %p, n = %u",
	    s, b, n); */
  
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

      struct mwOpaque o = { 4, s->buf };
      struct mwGetBuffer *gb = mwGetBuffer_wrap(&o);
      x = guint32_peek(gb);
      mwGetBuffer_free(gb);

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
	/* there's enough (maybe more) for a full message. don't need
	   the old session buffer (which recall, was only the length
	   bytes) any more */
	
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
  struct mwOpaque o = { n, (char *) b };
  struct mwGetBuffer *gb;
  gsize x;

  if(n < 4) {
    /* uh oh. less than four bytes means we've got an incomplete
       length indicator. Have to buffer to get the rest of it. */
    s->buf = (char *) g_malloc0(4);
    memcpy(s->buf, b, n);
    s->buf_len = 4;
    s->buf_used = n;
    return 0;
  }
  
  /* peek at the length indicator. if it's a zero length message,
     don't process, just skip it */
  gb = mwGetBuffer_wrap(&o);
  x = guint32_peek(gb);
  mwGetBuffer_free(gb);
  if(! x) return n - 4;

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
  
  /* g_message(" session_recv: session = %p, b = %p, n = %u",
	    s, b, n); */
  
  if(n && (s->buf_len == 0) && (*b & 0x80)) {
    /* keep-alive and series bytes are ignored */
    ADVANCE(b, n, 1);
  }

  if(n == 0) {
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

  /* g_message(" mwSession_recv: session = %p, b = %p, n = %u",
	    s, b, n); */

  while(n > 0) {
    remain = session_recv(s, b, n);
    b += (n - remain);
    n = remain;
  }
}


int mwSession_send(struct mwSession *s, struct mwMessage *msg) {
  struct mwPutBuffer *b;
  struct mwOpaque o;
  gsize len;
  int ret = 0;

  g_return_val_if_fail(s != NULL, -1);

  /* writing nothing is easy */
  if(! msg) return 0;

  /* first we render the message into an opaque */
  b = mwPutBuffer_new();
  mwMessage_put(b, msg);
  mwPutBuffer_finalize(&o, b);

  /* then we render the opaque into... another opaque! */
  b = mwPutBuffer_new();
  mwOpaque_put(b, &o);
  mwOpaque_clear(&o);
  mwPutBuffer_finalize(&o, b);

  /* then we use that opaque's data and length to write to the socket */
  len = o.len;
  ret = io_write(s, o.data, o.len);
  mwOpaque_clear(&o);

  /* ensure we could actually write the message */
  if(! ret) {
    g_message("session wrote %u bytes", len);

    /* special case, as the server doesn't always respond to user
       status messages. Thus, we trigger the event when we send the
       messages as well as when we receive them */
    if(msg->type == mwMessage_SET_USER_STATUS) {
      SET_USER_STATUS_recv(s, (struct mwMsgSetUserStatus *) msg);
    }
  }

  return ret;
}


struct mwSessionHandler *mwSession_getHandler(struct mwSession *s) {
  g_return_val_if_fail(s != NULL, NULL);
  return s->handler;
}


struct mwLoginInfo *mwSession_getLoginInfo(struct mwSession *s) {
  g_return_val_if_fail(s != NULL, NULL);
  return &s->login;
}


/** @todo implement */
int mwSession_setPrivacyInfo(struct mwSession *s,
			     struct mwPrivacyInfo *privacy) {
  return 0;
}


struct mwPrivacyInfo *mwSession_getPrivacyInfo(struct mwSession *s) {
  g_return_val_if_fail(s != NULL, NULL);
  return &s->privacy;
}


int mwSession_setUserStatus(struct mwSession *s,
			    struct mwUserStatus *stat) {

  struct mwMsgSetUserStatus *msg;
  int ret;

  g_return_val_if_fail(s != NULL, -1);
  g_return_val_if_fail(stat != NULL, -1);

  msg = (struct mwMsgSetUserStatus *)
    mwMessage_new(mwMessage_SET_USER_STATUS);

  mwUserStatus_clone(&msg->status, stat);

  ret = mwSession_send(s, MW_MESSAGE(msg));
  mwMessage_free(MW_MESSAGE(msg));

  return ret;
}


struct mwUserStatus *mwSession_getUserStatus(struct mwSession *s) {
  g_return_val_if_fail(s != NULL, NULL);
  return &s->status;
}


enum mwSessionState mwSession_getState(struct mwSession *s) {
  g_return_val_if_fail(s != NULL, mwSession_UNKNOWN);
  return s->state;
}


guint32 mwSession_getStateInfo(struct mwSession *s) {
  g_return_val_if_fail(s != NULL, 0);
  return s->state_info;
}


struct mwChannelSet *mwSession_getChannels(struct mwSession *session) {
  g_return_val_if_fail(session != NULL, NULL);
  return session->channels;
}


gboolean mwSession_addService(struct mwSession *s, struct mwService *srv) {
  g_return_val_if_fail(s != NULL, FALSE);
  g_return_val_if_fail(srv != NULL, FALSE);
  g_return_val_if_fail(s->services != NULL, FALSE);

  if(g_hash_table_lookup(s->services, SERVICE_KEY(srv))) {
    return FALSE;

  } else {
    g_hash_table_insert(s->services, SERVICE_KEY(srv), srv);
    if(SESSION_IS_STATE(s, mwSession_STARTED))
      mwSession_senseService(s, mwService_getType(srv));
    return TRUE;
  }
}


struct mwService *mwSession_getService(struct mwSession *s, guint32 srv) {
  g_return_val_if_fail(s != NULL, NULL);
  g_return_val_if_fail(s->services != NULL, NULL);

  return g_hash_table_lookup(s->services, GUINT_TO_POINTER(srv));
}


struct mwService *mwSession_removeService(struct mwSession *s, guint32 srv) {
  struct mwService *svc;

  g_return_val_if_fail(s != NULL, NULL);
  g_return_val_if_fail(s->services != NULL, NULL);

  svc = g_hash_table_lookup(s->services, GUINT_TO_POINTER(srv));
  if(svc) g_hash_table_remove(s->services, GUINT_TO_POINTER(srv));
  return svc;
}


static void collecteach(gpointer key, gpointer val, gpointer l) {
  GList **list = l;
  *list = g_list_append(*list, val);
}


GList *mwSession_getServices(struct mwSession *s) {
  GList *l = NULL;

  g_return_val_if_fail(s != NULL, NULL);
  g_return_val_if_fail(s->services != NULL, NULL);

  g_hash_table_foreach(s->services, collecteach, &l);

  return l;
}


void mwSession_senseService(struct mwSession *s, guint32 srvc) {
  struct mwMsgSenseService *msg;

  g_return_if_fail(s != NULL);
  g_return_if_fail(srvc != 0x00);
  g_return_if_fail(SESSION_IS_STARTED(s));

  msg = (struct mwMsgSenseService *)
    mwMessage_new(mwMessage_SENSE_SERVICE);
  msg->service = srvc;

  mwSession_send(s, MW_MESSAGE(msg));
  mwMessage_free(MW_MESSAGE(msg));
}


static struct mwCipher *get_cipher(struct mwSession *s, guint16 id) {
  guint cid = (guint) id;
  return g_hash_table_lookup(s->ciphers, GUINT_TO_POINTER(cid));
}


static void add_cipher(struct mwSession *s, struct mwCipher *c) {
  guint cid = (guint) mwCipher_getType(c);
  g_hash_table_insert(s->ciphers, GUINT_TO_POINTER(cid), c);
}


static void remove_cipher(struct mwSession *s, guint16 id) {
  guint cid = (guint) id;
  g_hash_table_remove(s->ciphers, GUINT_TO_POINTER(cid));
}


gboolean mwSession_addCipher(struct mwSession *s, struct mwCipher *c) {
  g_return_val_if_fail(s != NULL, FALSE);
  g_return_val_if_fail(c != NULL, FALSE);
  g_return_val_if_fail(s->ciphers != NULL, FALSE);

  if(get_cipher(s, mwCipher_getType(c))) {
    g_message("cipher %s is already added, apparently",
	      NSTR(mwCipher_getName(c)));
    return FALSE;

  } else {
    g_message("adding cipher %s",
	      NSTR(mwCipher_getName(c)));
    add_cipher(s, c);
    return TRUE;
  }
}


struct mwCipher *mwSession_getCipher(struct mwSession *s, guint16 c) {
  g_return_val_if_fail(s != NULL, NULL);
  g_return_val_if_fail(s->ciphers != NULL, NULL);

  return get_cipher(s, c);
}


struct mwCipher *mwSession_removeCipher(struct mwSession *s, guint16 c) {
  struct mwCipher *ciph;

  g_return_val_if_fail(s != NULL, NULL);
  g_return_val_if_fail(s->ciphers != NULL, NULL);

  ciph = get_cipher(s, c);
  if(ciph) remove_cipher(s, c);
  return ciph;
}


GList *mwSession_getCiphers(struct mwSession *s) {
  GList *l = NULL;

  g_return_val_if_fail(s != NULL, NULL);
  g_return_val_if_fail(s->ciphers != NULL, NULL);

  g_hash_table_foreach(s->ciphers, collecteach, &l);

  return l;
}

