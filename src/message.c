

#include <glib.h>

#include "message.h"


/* 7.1 Layering and message encapsulation */
/* 7.1.1 The Sametime Message Header */

static unsigned int mwMessageHead_buflen(struct mwMessage *msg) {
  int s = 2 + 2 + 4;
  
  if(msg->options & mwMessageOption_HAS_ATTRIBS)
    s += mwOpaque_buflen(&msg->attribs);
  
  return s;
}


static int mwMessageHead_put(char **b, unsigned int *n,
			     struct mwMessage *msg) {

  if( guint16_put(b, n, msg->type) ||
      guint16_put(b, n, msg->options) ||
      guint32_put(b, n, msg->channel) )
    return 1;
  
  if(msg->options & mwMessageOption_HAS_ATTRIBS)
    if( mwOpaque_put(b, n, &msg->attribs) )
      return 1;

  return 0;
}


static int mwMessageHead_get(char **b, unsigned int *n,
			     struct mwMessage *msg) {

  if( guint16_get(b, n, &msg->type) ||
      guint16_get(b, n, &msg->options) ||
      guint32_get(b, n, &msg->channel) )
    return 1;

  if(msg->options & mwMessageOption_HAS_ATTRIBS)
    if( mwOpaque_get(b, n, &msg->attribs) )
      return 1;

  return 0;
}


static void mwMessageHead_clone(struct mwMessage *to, struct mwMessage *from) {
  to->type = from->type;
  to->options = from->options;
  to->channel = from->channel;
  mwOpaque_clone(&to->attribs, &from->attribs);
}


static void mwMessageHead_clear(struct mwMessage *msg) {
  mwOpaque_clear(&msg->attribs);
}


/* 8.4 Messages */
/* 8.4.1 Basic Community Messages */
/* 8.4.1.1 Handshake */

static unsigned int HANDSHAKE_buflen(struct mwMsgHandshake *msg) {
  return 18; /* fixed-size message */
}


static int HANDSHAKE_put(char **b, unsigned int *n,
			 struct mwMsgHandshake *msg) {

  if( guint16_put(b, n, msg->major) ||
      guint16_put(b, n, msg->minor) ||
      guint32_put(b, n, msg->head.channel) ||
      guint32_put(b, n, msg->srvrcalc_addr) ||
      guint16_put(b, n, msg->login_type) ||
      guint32_put(b, n, msg->loclcalc_addr) )
    return *n;

  return 0;
}


static void HANDSHAKE_clear(struct mwMsgHandshake *msg) {
  ; /* nothing to clean up */
}


/* 8.4.1.2 HandshakeAck */

static int HANDSHAKE_ACK_get(char **b, unsigned int *n,
			     struct mwMsgHandshakeAck *msg) {

  if( guint16_get(b, n, &msg->major) ||
      guint16_get(b, n, &msg->minor) ||
      guint32_get(b, n, &msg->srvrcalc_addr) ||
      guint32_get(b, n, &msg->unknown) ||
      mwOpaque_get(b, n, &msg->data) ) {
    return 1;
  }

  return 0;
}


static void HANDSHAKE_ACK_clear(struct mwMsgHandshakeAck *msg) {
  mwOpaque_clear(&msg->data);
}


/* 8.4.1.3 Login */

static unsigned int LOGIN_buflen(struct mwMsgLogin *msg) {
  return 2 + mwString_buflen(msg->name) +
    2 + mwOpaque_buflen(&msg->auth_data);
}


static int LOGIN_put(char **b, unsigned int *n, struct mwMsgLogin *msg) {
  if( guint16_put(b, n, msg->type) ||
      mwString_put(b, n, msg->name) )
    return 1;

  /* reversed from draft?? */
  if( mwOpaque_put(b, n, &msg->auth_data) ||
      guint16_put(b, n, msg->auth_type) )
    return 1;

  return 0;
}


static void LOGIN_clear(struct mwMsgLogin *msg) {
  g_free(msg->name);
  mwOpaque_clear(&msg->auth_data);
}


/* 8.4.1.4 LoginAck */

static int LOGIN_ACK_get(char **b, unsigned int *n,
			 struct mwMsgLoginAck *msg) {

  return ( mwLoginInfo_get(b, n, &msg->login) ||
	   mwPrivacyInfo_get(b, n, &msg->privacy) ||
	   mwUserStatus_get(b, n, &msg->status) );
}


static void LOGIN_ACK_clear(struct mwMsgLoginAck *msg) {
  mwLoginInfo_clear(&msg->login);
  mwPrivacyInfo_clear(&msg->privacy);
  mwUserStatus_clear(&msg->status);
}


/* 8.4.1.7 CreateCnl */

static unsigned int CHANNEL_CREATE_buflen(struct mwMsgChannelCreate *msg) {
  int len = 4 + 4 + mwIdBlock_buflen(&msg->target) +
    4 + 4 + 4 + 4 + mwOpaque_buflen(&msg->addtl) + 1;

  if(msg->creator_flag)
    len += mwLoginInfo_buflen(&msg->creator);
  len += mwEncryptBlock_buflen(&msg->encrypt);

  return len;
}


static int CHANNEL_CREATE_put(char **b, unsigned int *n,
			      struct mwMsgChannelCreate *msg) {

  if( guint32_put(b, n, msg->reserved) ||
      guint32_put(b, n, msg->channel) ||
      mwIdBlock_put(b, n, &msg->target) ||
      guint32_put(b, n, msg->service) ||
      guint32_put(b, n, msg->proto_type) ||
      guint32_put(b, n, msg->proto_ver) ||
      guint32_put(b, n, msg->options) ||
      mwOpaque_put(b, n, &msg->addtl) ||
      gboolean_put(b, n, msg->creator_flag) ) {
    return 1;
  }

  if(msg->creator_flag && mwLoginInfo_put(b, n, &msg->creator))
    return 1;
  
  if(mwEncryptBlock_put(b, n, &msg->encrypt))
    return 1;

  return 0;
}


static int CHANNEL_CREATE_get(char **b, unsigned int *n,
			      struct mwMsgChannelCreate *msg) {

  if( guint32_get(b, n, &msg->reserved) ||
      guint32_get(b, n, &msg->channel) ||
      mwIdBlock_get(b, n, &msg->target) ||
      guint32_get(b, n, &msg->service) ||
      guint32_get(b, n, &msg->proto_type) ||
      guint32_get(b, n, &msg->proto_ver) ||
      guint32_get(b, n, &msg->options) ||
      mwOpaque_get(b, n, &msg->addtl) ||
      gboolean_get(b, n, &msg->creator_flag) )
    return 1;

  if(msg->creator_flag && mwLoginInfo_get(b, n, &msg->creator))
      return 1;

  if(mwEncryptBlock_get(b, n, &msg->encrypt))
      return 1;

  return 0;
}


static void CHANNEL_CREATE_clear(struct mwMsgChannelCreate *msg) {
  mwIdBlock_clear(&msg->target);
  mwOpaque_clear(&msg->addtl);
  mwLoginInfo_clear(&msg->creator);
  mwEncryptBlock_clear(&msg->encrypt);
}


/* 8.4.1.8 AcceptCnl */

static unsigned int CHANNEL_ACCEPT_buflen(struct mwMsgChannelAccept *msg) {
  int len = 4 + 4 + 4 + mwOpaque_buflen(&msg->addtl) + 1;

  if(msg->acceptor_flag)
    len += mwLoginInfo_buflen(&msg->acceptor);

  len += mwEncryptBlock_buflen(&msg->encrypt);

  return len;
}


static int CHANNEL_ACCEPT_put(char **b, unsigned int *n,
			      struct mwMsgChannelAccept *msg) {

  if( guint32_put(b, n, msg->service) ||
      guint32_put(b, n, msg->proto_type) ||
      guint32_put(b, n, msg->proto_ver) ||
      mwOpaque_put(b, n, &msg->addtl) ||
      gboolean_put(b, n, msg->acceptor_flag) )
    return 1;

  if(msg->acceptor_flag) {
    if( mwLoginInfo_put(b, n, &msg->acceptor) )
      return 1;
  }

  if( mwEncryptBlock_put(b, n, &msg->encrypt) )
    return 1;

  return 0;
}


static int CHANNEL_ACCEPT_get(char **b, unsigned int *n,
			      struct mwMsgChannelAccept *msg) {

  if( guint32_get(b, n, &msg->service) ||
      guint32_get(b, n, &msg->proto_type) ||
      guint32_get(b, n, &msg->proto_ver) ||
      mwOpaque_get(b, n, &msg->addtl) ||
      gboolean_get(b, n, &msg->acceptor_flag) )
    return 1;

  if(msg->acceptor_flag) {
    if( mwLoginInfo_get(b, n, &msg->acceptor) )
      return 1;
  }

  if( mwEncryptBlock_get(b, n, &msg->encrypt) )
    return 1;

  return 0;
}


static void CHANNEL_ACCEPT_clear(struct mwMsgChannelAccept *msg) {
  mwOpaque_clear(&msg->addtl);
  mwLoginInfo_clear(&msg->acceptor);
  mwEncryptBlock_clear(&msg->encrypt);
}


/* 8.4.1.9 SendOnCnl */

static unsigned int CHANNEL_SEND_buflen(struct mwMsgChannelSend *msg) {
  return 2 + mwOpaque_buflen(&msg->data);
}


static int CHANNEL_SEND_put(char **b, unsigned int *n,
			    struct mwMsgChannelSend *msg) {

  if( guint16_put(b, n, msg->type) ||
      mwOpaque_put(b, n, &msg->data) )
    return 1;
  
  return 0;
}


static int CHANNEL_SEND_get(char **b, unsigned int *n,
			    struct mwMsgChannelSend *msg) {

  if( guint16_get(b, n, &msg->type) ||
      mwOpaque_get(b, n, &msg->data) )
    return 1;

  return 0;
}


static void CHANNEL_SEND_clear(struct mwMsgChannelSend *msg) {
  mwOpaque_clear(&msg->data);
}


/* 8.4.1.10 DestroyCnl */

static unsigned int CHANNEL_DESTROY_buflen(struct mwMsgChannelDestroy *msg) {
  return 4 + mwOpaque_buflen(&msg->data);
}


static int CHANNEL_DESTROY_put(char **b, unsigned int *n,
			       struct mwMsgChannelDestroy *msg) {
  if( guint32_put(b, n, msg->reason) ||
      mwOpaque_put(b, n, &msg->data) )
    return 1;

  return 0;
}


static int CHANNEL_DESTROY_get(char **b, unsigned int *n,
			       struct mwMsgChannelDestroy *msg) {
  if( guint32_get(b, n, &msg->reason) ||
      mwOpaque_get(b, n, &msg->data) )
    return 1;

  return 0;
}


static void CHANNEL_DESTROY_clear(struct mwMsgChannelDestroy *msg) {
  mwOpaque_clear(&msg->data);
}


/* 8.4.1.11 SetUserStatus */

static unsigned int SET_USER_STATUS_buflen(struct mwMsgSetUserStatus *msg) {
  return mwUserStatus_buflen(&msg->status);
}


static int SET_USER_STATUS_put(char **b, unsigned int *n,
			       struct mwMsgSetUserStatus *msg) {
  return mwUserStatus_put(b, n, &msg->status);
}


static int SET_USER_STATUS_get(char **b, unsigned int *n,
			       struct mwMsgSetUserStatus *msg) {
  return mwUserStatus_get(b, n, &msg->status);
}


static void SET_USER_STATUS_clear(struct mwMsgSetUserStatus *msg) {
  mwUserStatus_clear(&msg->status);
}


/* 8.4.1.12 SetPrivacyList */

static unsigned int SET_PRIVACY_LIST_buflen(struct mwMsgSetPrivacyList *msg) {
  return mwPrivacyInfo_buflen(&msg->privacy);
}


static int SET_PRIVACY_LIST_put(char **b, gsize *n,
				struct mwMsgSetPrivacyList *msg) {

  return mwPrivacyInfo_put(b, n, &msg->privacy);
}


static int SET_PRIVACY_LIST_get(char **b, gsize *n,
				struct mwMsgSetPrivacyList *msg) {

  return mwPrivacyInfo_get(b, n, &msg->privacy);
}


static void SET_PRIVACY_LIST_clear(struct mwMsgSetPrivacyList *msg) {
  mwPrivacyInfo_clear(&msg->privacy);
}


/* Admin messages */

static int ADMIN_get(char **b, unsigned int *n, struct mwMsgAdmin *msg) {
  return mwString_get(b, n, &msg->text);
}


static void ADMIN_clear(struct mwMsgAdmin *msg) {
  g_free(msg->text);
  msg->text = NULL;
}


#define CASE(v, t) \
case mwMessage_ ## v: \
  msg = (struct mwMessage *) g_new0(struct t, 1); \
  msg->type = type; \
  break;


struct mwMessage *mwMessage_new(enum mwMessageType type) {
  struct mwMessage *msg = NULL;
  
  switch(type) {
    CASE(HANDSHAKE, mwMsgHandshake);
    CASE(HANDSHAKE_ACK, mwMsgHandshakeAck);
    CASE(LOGIN, mwMsgLogin);
    /* CASE(LOGIN_REDIRECT, mwMsgLoginRedirect);
       CASE(LOGIN_CONTINUE, mwMsgLoginContinue); */
    CASE(LOGIN_ACK, mwMsgLoginAck);
    CASE(CHANNEL_CREATE, mwMsgChannelCreate);
    CASE(CHANNEL_DESTROY, mwMsgChannelDestroy);
    CASE(CHANNEL_SEND, mwMsgChannelSend);
    CASE(CHANNEL_ACCEPT, mwMsgChannelAccept);
    CASE(SET_USER_STATUS, mwMsgSetUserStatus);
    CASE(SET_PRIVACY_LIST, mwMsgSetPrivacyList);
    CASE(ADMIN, mwMsgAdmin);
    
  default:
    ; /* hrm. */
  }
  
  return msg;
}


#undef CASE


#define CASE(v, t) \
case mwMessage_ ## v: \
  msg = (struct mwMessage *) g_new0(struct t, 1); \
  msg->type = mwMessage_ ## v; \
  ret = v ## _get(&b, &n, (struct t *) msg); \
  break;


struct mwMessage *mwMessage_get(const char *buf, gsize len) {
  struct mwMessage *msg = NULL;
  struct mwMessage head;
  
  int ret = 0;
  
  char *b = (char *) buf;
  gsize n = len;

  head.attribs.len = 0;
  head.attribs.data = NULL;
  mwMessageHead_get(&b, &n, &head);

  switch(head.type) {
    /* CASE(HANDSHAKE, mwMsgHandshake); */
    CASE(HANDSHAKE_ACK, mwMsgHandshakeAck);
    /* CASE(LOGIN, mwMsgLogin); */
    /* CASE(LOGIN_REDIRECT, mwMsgLoginRedirect);
       CASE(LOGIN_CONTINUE, mwMsgLoginContinue); */
    CASE(LOGIN_ACK, mwMsgLoginAck);
    CASE(CHANNEL_CREATE, mwMsgChannelCreate);
    CASE(CHANNEL_DESTROY, mwMsgChannelDestroy);
    CASE(CHANNEL_SEND, mwMsgChannelSend);
    CASE(CHANNEL_ACCEPT, mwMsgChannelAccept);
    CASE(SET_USER_STATUS, mwMsgSetUserStatus);
    CASE(SET_PRIVACY_LIST, mwMsgSetPrivacyList);
    CASE(ADMIN, mwMsgAdmin);

  default:
    g_warning("unknown message type %x, no parse handler\n", head.type);
  }

  if(msg)
    mwMessageHead_clone(msg, &head);

  if(ret)
    mwMessage_free(msg);

  mwMessageHead_clear(&head);
  
  return msg;
}


#undef CASE


#define CASE(v, t) \
case mwMessage_ ## v: \
  len += mwMessageHead_buflen(msg); \
  len += v ## _buflen((struct t *) msg); \
  break;


unsigned int mwMessage_buflen(struct mwMessage *msg) {
  unsigned int len = 0;

  switch(msg->type) {
    CASE(HANDSHAKE, mwMsgHandshake);
    /* CASE(HANDSHAKE_ACK, mwMsgHandshakeAck); */
    CASE(LOGIN, mwMsgLogin);
    /* CASE(LOGIN_REDIRECT, mwMsgLoginRedirect);
       CASE(LOGIN_CONTINUE, mwMsgLoginContinue); */
    /* CASE(LOGIN_ACK, mwMsgLoginAck); */
    CASE(CHANNEL_CREATE, mwMsgChannelCreate);
    CASE(CHANNEL_DESTROY, mwMsgChannelDestroy);
    CASE(CHANNEL_SEND, mwMsgChannelSend);
    CASE(CHANNEL_ACCEPT, mwMsgChannelAccept);
    CASE(SET_USER_STATUS, mwMsgSetUserStatus);
    CASE(SET_PRIVACY_LIST, mwMsgSetPrivacyList);
    /* CASE(ADMIN, mwMsgAdmin); */
    
  default:
    ; /* hrm. */
  }

  return len;
}


#undef CASE


#define CASE(v, t) \
case mwMessage_ ## v: \
  ret = v ## _put(buf, len, (struct t *) msg); \
  break;


int mwMessage_put(char **buf, unsigned int *len, struct mwMessage *msg) {
  int ret = mwMessageHead_put(buf, len, msg);
  if(ret) return ret;

  switch(msg->type) {
    CASE(HANDSHAKE, mwMsgHandshake);
    /* CASE(HANDSHAKE_ACK, mwMsgHandshakeAck); */
    CASE(LOGIN, mwMsgLogin);
    /* CASE(LOGIN_REDIRECT, mwMsgLoginRedirect);
       CASE(LOGIN_CONTINUE, mwMsgLoginContinue); */
    /* CASE(LOGIN_ACK, mwMsgLoginAck); */
    CASE(CHANNEL_CREATE, mwMsgChannelCreate);
    CASE(CHANNEL_DESTROY, mwMsgChannelDestroy);
    CASE(CHANNEL_SEND, mwMsgChannelSend);
    CASE(CHANNEL_ACCEPT, mwMsgChannelAccept);
    CASE(SET_USER_STATUS, mwMsgSetUserStatus);
    CASE(SET_PRIVACY_LIST, mwMsgSetPrivacyList);
    /* CASE(ADMIN, mwMsgAdmin); */
    
  default:
    ; /* hrm. */
  }

  return ret;
}


#undef CASE


#define CASE(v, t) \
case mwMessage_ ## v: \
  v ## _clear((struct t *) msg); \
  break;


void mwMessage_free(struct mwMessage *msg) {
  mwMessageHead_clear(msg);

  switch(msg->type) {
    CASE(HANDSHAKE, mwMsgHandshake);
    CASE(HANDSHAKE_ACK, mwMsgHandshakeAck);
    CASE(LOGIN, mwMsgLogin);
    /* CASE(LOGIN_REDIRECT, mwMsgLoginRedirect);
       CASE(LOGIN_CONTINUE, mwMsgLoginContinue); */
    CASE(LOGIN_ACK, mwMsgLoginAck);
    CASE(CHANNEL_CREATE, mwMsgChannelCreate);
    CASE(CHANNEL_DESTROY, mwMsgChannelDestroy);
    CASE(CHANNEL_SEND, mwMsgChannelSend);
    CASE(CHANNEL_ACCEPT, mwMsgChannelAccept);
    CASE(SET_USER_STATUS, mwMsgSetUserStatus);
    CASE(SET_PRIVACY_LIST, mwMsgSetPrivacyList);
    CASE(ADMIN, mwMsgAdmin);
    
  default:
    ; /* hrm. */
  }

  g_free(msg);
}


#undef CASE


