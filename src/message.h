

#ifndef _MW_MESSAGE_
#define _MW_MESSAGE_


#include "common.h"


#define MESSAGE(msg) (&msg->head)


enum mwMessageType {
  mwMessage_HANDSHAKE         = 0x0000,
  mwMessage_HANDSHAKE_ACK     = 0x8000,
  mwMessage_LOGIN             = 0x0001,
  /* mwMessage_LOGIN_REDIRECT    = ...,
     mwMessage_LOGIN_CONTINUE    = ..., */
  mwMessage_LOGIN_ACK         = 0x8001,
  mwMessage_CHANNEL_CREATE    = 0x0002,
  mwMessage_CHANNEL_DESTROY   = 0x0003,
  mwMessage_CHANNEL_SEND      = 0x0004,
  mwMessage_CHANNEL_ACCEPT    = 0x0006,
  mwMessage_SET_USER_STATUS   = 0x0009,
  mwMessage_SET_PRIVACY_LIST  = 0x0010, /* maybe? */
  mwMessage_SENSE_SERVICE     = 0x0011,
  mwMessage_ADMIN             = 0x0019
};


enum mwMessageOption {
  mwMessageOption_NONE         = 0x0000,
  mwMessageOption_HAS_ATTRIBS  = 0x8000,
  mwMessageOption_ENCRYPT      = 0x4000
};


struct mwMessage {
  enum mwMessageType type;
  enum mwMessageOption options;
  unsigned int channel;
  struct mwOpaque attribs;
};


struct mwMessage *mwMessage_new(enum mwMessageType type);


/* build a message from its representation. buf should already have been
   advanced past the four-byte length indicator */
struct mwMessage *mwMessage_get(const char *buf, unsigned int len);


unsigned int mwMessage_buflen(struct mwMessage *msg);


int mwMessage_put(char **buf, unsigned int *len, struct mwMessage *msg);


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

enum mwChannelSendType {
  mwChannelSend_CONF_WELCOME    = 0x0000, /* grr. shouldn't use zero */
  mwChannelSend_CONF_INVITE     = 0x0001,
  mwChannelSend_CONF_JOIN       = 0x0002,
  mwChannelSend_CONF_PART       = 0x0003,
  mwChannelSend_CONF_MESSAGE    = 0x0004, /* conference */
  mwChannelSend_CHAT_MESSAGE    = 0x0064, /* im */
  mwChannelSend_AWARE_ADD       = 0x0068,
  mwChannelSend_AWARE_REMOVE    = 0x0069,
  mwChannelSend_AWARE_SNAPSHOT  = 0x01f4,
  mwChannelSend_AWARE_UPDATE    = 0x01f5,
  mwChannelSend_RESOLVE_SEARCH  = 0x8000, /* placeholder */
  mwChannelSend_RESOLVE_RESULT  = 0x8001  /* placeholder */
};


struct mwMsgChannelSend {
  struct mwMessage head;
  enum mwChannelSendType type;
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

