

#ifndef _MW_MESSAGE_
#define _MW_MESSAGE_


#include "common.h"


/** Cast a pointer to a message subtype (eg, mwMsgHandshake,
    mwMsgAdmin) into a pointer to a mwMessage */
#define MW_MESSAGE(msg) (&msg->head)


/** Indicates the type of a message. */
enum mwMessageType {
  mwMessage_HANDSHAKE         = 0x0000, /**< mwMsgHandshake */
  mwMessage_HANDSHAKE_ACK     = 0x8000, /**< mwMsgHandshakeAck */
  mwMessage_LOGIN             = 0x0001, /**< mwMsgLogin */
  /* mwMessage_LOGIN_REDIRECT    = ...,
     mwMessage_LOGIN_CONTINUE    = ..., */
  mwMessage_LOGIN_ACK         = 0x8001, /**< mwMsgLoginAck */
  mwMessage_CHANNEL_CREATE    = 0x0002, /**< mwMsgChannelCreate */
  mwMessage_CHANNEL_DESTROY   = 0x0003, /**< mwMsgChannelDestroy */
  mwMessage_CHANNEL_SEND      = 0x0004, /**< mwMsgChannelSend */
  mwMessage_CHANNEL_ACCEPT    = 0x0006, /**< mwMsgChannelAccept */
  mwMessage_SET_USER_STATUS   = 0x0009, /**< mwMsgSetUserStatus */
  mwMessage_SET_PRIVACY_LIST  = 0x0010, /**< mwMsgSetPrivacyList (maybe?) */
  mwMessage_SENSE_SERVICE     = 0x0011, /**< mwMsgSenseService */
  mwMessage_ADMIN             = 0x0019  /**< mwMsgAdmin */
};


enum mwMessageOption {
  mwMessageOption_NONE         = 0x0000,
  mwMessageOption_HAS_ATTRIBS  = 0x8000,
  mwMessageOption_ENCRYPT      = 0x4000
};


struct mwMessage {
  enum mwMessageType type;
  enum mwMessageOption options;
  guint channel;
  struct mwOpaque attribs;
};


/** Allocate and initialize a new message of the specified type */
struct mwMessage *mwMessage_new(enum mwMessageType type);


/** build a message from its representation. buf should already have
    been advanced past the four-byte length indicator */
struct mwMessage *mwMessage_get(const char *buf, gsize len);


gsize mwMessage_buflen(struct mwMessage *msg);


int mwMessage_put(char **b, gsize *n, struct mwMessage *msg);


void mwMessage_free(struct mwMessage *msg);


/* 8.4 Messages */
/* 8.4.1 Basic Community Messages */
/* 8.4.1.1 Handshake */

struct mwMsgHandshake {
  struct mwMessage head;
  unsigned int major;
  unsigned int minor;
  unsigned int srvrcalc_addr;
  enum mwLoginType login_type;
  unsigned int loclcalc_addr;
};


/* 8.4.1.2 HandshakeAck */

struct mwMsgHandshakeAck {
  struct mwMessage head;
  unsigned int major;
  unsigned int minor;
  unsigned int srvrcalc_addr;   /* server-calculated address */
  unsigned int unknown;         /* four bytes of something */
  struct mwOpaque data;         /* there's an opaque here. maybe a key? */
};


/* 8.3.7 Authentication Types */

enum mwAuthType {
  mwAuthType_PLAIN    = 0x0000,
  mwAuthType_TOKEN    = 0x0001,
  mwAuthType_ENCRYPT  = 0x0002
};


/* 8.4.1.3 Login */

struct mwMsgLogin {
  struct mwMessage head;
  enum mwLoginType type;
  char *name;
  enum mwAuthType auth_type;
  struct mwOpaque auth_data;
};


/* 8.4.1.4 LoginAck */

struct mwMsgLoginAck {
  struct mwMessage head;
  struct mwLoginInfo login;
  struct mwPrivacyInfo privacy;
  struct mwUserStatus status;
};


/* 8.4.1.6 AuthPassed */

struct mwMsgLoginRedirect {
  struct mwMessage head;
  char *server;
  unsigned int server_ver;
};


/* 8.4.1.7 CreateCnl */

struct mwMsgChannelCreate {
  struct mwMessage head;
  unsigned int reserved;
  unsigned int channel;
  struct mwIdBlock target;
  unsigned int service;
  unsigned int proto_type;
  unsigned int proto_ver;
  unsigned int options;
  struct mwOpaque addtl;
  gboolean creator_flag;
  struct mwLoginInfo creator;
  struct mwEncryptBlock encrypt;
};


/* 8.4.1.8 AcceptCnl */

struct mwMsgChannelAccept {
  struct mwMessage head;
  unsigned int service;
  unsigned int proto_type;
  unsigned int proto_ver;
  struct mwOpaque addtl;
  gboolean acceptor_flag;
  struct mwLoginInfo acceptor;
  struct mwEncryptBlock encrypt;
};


/* 8.4.1.9 SendOnCnl */

struct mwMsgChannelSend {
  struct mwMessage head;

  /** each service defines its own send types. The uniqueness of a
      type is only necessary within a given service */
  guint type;

  /** protocol data to be interpreted by the handling service */
  struct mwOpaque data;
};


/* 8.4.1.10 DestroyCnl */

struct mwMsgChannelDestroy {
  struct mwMessage head;
  guint reason;
  struct mwOpaque data;
};


/* 8.4.1.11 SetUserStatus */

struct mwMsgSetUserStatus {
  struct mwMessage head;
  struct mwUserStatus status;
};


/* 8.4.1.12 SetPrivacyList */

struct mwMsgSetPrivacyList {
  struct mwMessage head;
  struct mwPrivacyInfo privacy;
};


/* Admin */

struct mwMsgAdmin {
  struct mwMessage head;
  char *text;
};


#endif

