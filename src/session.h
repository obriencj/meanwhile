

#ifndef _MW_SESSION_H_
#define _MW_SESSION_H_


#include <glib.h>
#include <glib/glist.h>
#include "common.h"


/* protocol versioning */
#ifndef PROTOCOL_VERSION_MAJOR
#define PROTOCOL_VERSION_MAJOR  0x001e
#endif
#ifndef PROTOCOL_VERSION_MINOR
#define PROTOCOL_VERSION_MINOR  0x0017
#endif


/** how a session manages to perform output */
struct mwSessionHandler {
  int (*write)(struct mwSessionHandler *, const char *, gsize);
  void (*close)(struct mwSessionHandler *);
};


struct mwChannelSet;
struct mwService;
struct mwMessage;
struct mwMsgHandshake;
struct mwMsgHandshakeAck;
struct mwMsgLogin;
struct mwMsgLoginAck;
struct mwMsgLoginRedirect;
struct mwMsgLoginContinue;
struct mwMsgSetPrivacyInfo;
struct mwMsgSetUserStatus;
struct mwMsgAdmin;


struct mwSession {

  /** provides I/O capabilities for the session */
  struct mwSessionHandler *handler;

  char *buf;       /**< buffer for incoming message data */
  gsize buf_len;   /**< length of buf */
  gsize buf_used;  /**< offset to last-used byte of buf */

  /** authentication data (usually password)

      @todo the token auth_type is probably more ornate than just a
      string */
  enum mwAuthType auth_type;
  union {
    char *password;
    char *token;
  } auth;
  
  struct mwLoginInfo login;     /**< login information for this session */
  struct mwUserStatus status;   /**< session's user status */
  struct mwPrivacyInfo privacy; /**< session's privacy list */

  /** the session key

      @todo with new ciphers, this may need to become an EncryptBlock
      or something crazy */
  int session_key[64];

  /** the collection of channels */
  struct mwChannelSet *channels;

  /** the collection of services */
  GList *services;

  /* session call-backs */
  void (*on_start)(struct mwSession *);
  void (*on_stop)(struct mwSession *, guint32);

  /* channel call-backs */
  void (*on_channelOpen)(struct mwChannel *);
  void (*on_channelClose)(struct mwChannel *);

  /* authentication call-backs */
  void (*on_handshake)(struct mwSession *, struct mwMsgHandshake *);
  void (*on_handshakeAck)(struct mwSession *, struct mwMsgHandshakeAck *);
  void (*on_login)(struct mwSession *, struct mwMsgLogin *);
  void (*on_loginRedirect)(struct mwSession *, struct mwMsgLoginRedirect *);
  void (*on_loginContinue)(struct mwSession *, struct mwMsgLoginContinue *);
  void (*on_loginAck)(struct mwSession *, struct mwMsgLoginAck *);

  /* other call-backs */
  void (*on_setPrivacyInfo)(struct mwSession *, struct mwMsgSetPrivacyInfo *);
  void (*on_setUserStatus)(struct mwSession *, struct mwMsgSetUserStatus *);
  void (*on_admin)(struct mwSession *, struct mwMsgAdmin *);
};


/* allocate and prepare a new session */
struct mwSession *mwSession_new();


/* stop, clear, free a session */
void mwSession_free(struct mwSession *);


/** instruct the session to begin. This will trigger the on_start
    call-back. If there's no such call-back, this function does very
    little. Use or wrap onStart_sendHandshake as a call-back to have a
    handshake message sent, and to begin an actual session with a
    sametime server */
void mwSession_start(struct mwSession *);


/** instruct the session to shut down with the following reason
    code. Triggers the on_stop call-back */
void mwSession_stop(struct mwSession *, guint32 reason);


/** Data is buffered, unpacked, and parsed into a message, then
    processed accordingly. */
void mwSession_recv(struct mwSession *, const char *, gsize);


/** primarily used by services to have messages serialized and sent */
int mwSession_send(struct mwSession *, struct mwMessage *);


/** set the internal privacy information, and inform the server as
    necessary. Triggers the on_setPrivacyInfo call-back */
int mwSession_setPrivacyInfo(struct mwSession *, struct mwPrivacyInfo *);


/** set the internal user status state, and inform the server as
    necessary. Triggers the on_setUserStatus call-back */
int mwSession_setUserStatus(struct mwSession *, struct mwUserStatus *);


/** a call-back for use in the on_start slot which composes and sends
    a handshake message */
void onStart_sendHandshake(struct mwSession *);


/** a call-back for use in the on_handshake slot which composes and
    sends a login message */
void onHandshakeAck_sendLogin(struct mwSession *, struct mwMsgHandshakeAck *);


/** adds a service to the session. If the session is started and the
    service has a start function, the session will request service
    availability form the server. On receipt of the service
    availability notification, the session will call the start
    function. */
int mwSession_putService(struct mwSession *, struct mwService *);


/** obtain a reference to a mwService by its type identifier */
struct mwService *mwSession_getService(struct mwSession *, guint32 type);


/** removes a service from the session. If the session is started and
    the service has a stop function, it will be called. */
int mwSession_removeService(struct mwSession *, guint32 type);


#endif

