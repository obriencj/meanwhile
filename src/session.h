

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


/* how a session manages to feed data back to the server. */
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

  /* provides I/O capabilities for the session */
  struct mwSessionHandler *handler;

  /* buffer for incoming message data */
  char *buf;
  gsize buf_len;
  gsize buf_used;

  /* authentication data (usually password) */
  enum mwAuthType auth_type;
  union {
    char *password;
    char *token;
  } auth;
  
  /* bits of user information. Obtained from server responses */
  struct mwLoginInfo login;
  struct mwUserStatus status;
  struct mwPrivacyInfo privacy;

  /* session key */
  int session_key[64];

  /* the collection of channels */
  struct mwChannelSet *channels;

  /* collection of services */
  GList *services;

  /* session call-backs */
  void (*on_initConnect)(struct mwSession *);
  void (*on_closeConnect)(struct mwSession *, guint32);

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


/* clear, free and NULL a session */
void mwSession_free(struct mwSession **);


/* instruct the session to begin. This will trigger the on_initConnect
   call-back. If there's no such call-back, this function does very
   little.  Use or wrap initConnect_sendHandshake as a call-back to
   have a handshake message sent, and to begin an actual session with
   a sametime server */
void mwSession_initConnect(struct mwSession *);


/* instruct the session to shut down with the following reason
   code. Triggers the on_closeConnect call-back */
void mwSession_closeConnect(struct mwSession *, guint32 reason);


/* data is buffered, unpacked, and parsed into a message, then
   processed accordingly. */
void mwSession_recv(struct mwSession *, const char *, gsize);


/* primarily used by services to have messages serialized and sent */
int mwSession_send(struct mwSession *, struct mwMessage *);


/* set the internal privacy information, and inform the server as
   necessary. Triggers the on_setPrivacyInfo call-back */
int mwSession_setPrivacyInfo(struct mwSession *, struct mwPrivacyInfo *);


/* set the internal user status state, and inform the server as
   necessary. Triggers the on_setUserStatus call-back */
int mwSession_setUserStatus(struct mwSession *, struct mwUserStatus *);


/* a default call-back for the on_initConnect slot which composes and
   sends a handshake message */
void initConnect_sendHandshake(struct mwSession *);


/* a default call-back for the on_handshake slot which composes and
   sends a login message */
void handshakeAck_sendLogin(struct mwSession *, struct mwMsgHandshakeAck *);


int mwSession_putService(struct mwSession *, struct mwService *);


struct mwService *mwSession_getService(struct mwSession *, guint32 type);


int mwSession_removeService(struct mwSession *, guint32 type);


#endif

