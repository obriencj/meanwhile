

#ifndef _MW_SERVICE_H
#define _MW_SERVICE_H


#include <glib.h>


struct mwChannel;
struct mwSession;
struct mwMsgChannelCreate;
struct mwMsgChannelAccept;
struct mwMsgChannelDestroy;


enum BaseServiceTypes {
  Service_AWARE    = 0x00000011, /* buddy list */
  Service_RESOLVE  = 0x00000015, /* name resolution */
  Service_IM       = 0x00001000, /* instant messaging */
  Service_CONF     = 0x80000010, /* conferencing */
};


enum BaseProtocolTypes {
  Protocol_AWARE    = 0x00000011,
  Protocol_RESOLVE  = 0x00000015,
  Protocol_IM       = 0x00001000,
  Protocol_CONF     = 0x00000010
};


/* A service is the recipient of sendOnCnl messages sent over channels
   marked with the corresponding service id. Services provide
   functionality such as IM relaying, Awareness tracking and
   notification, and Conference handling.  It is a services
   responsibility to accept or destroy channels, and to process data
   sent over those channels */
struct mwService {

  /* the unique key by which this service is registered. */
  guint32 type;

  /* session this service is attached to */
  struct mwSession *session;

  const char *(*get_name)();
  const char *(*get_desc)();

  void (*recv_channelCreate)(struct mwService *, struct mwChannel *,
			     struct mwMsgChannelCreate *);

  void (*recv_channelAccept)(struct mwService *, struct mwChannel *,
			     struct mwMsgChannelAccept *);

  void (*recv_channelDestroy)(struct mwService *, struct mwChannel *,
			      struct mwMsgChannelDestroy *);

  void (*recv)(struct mwService *, struct mwChannel *, guint32 msg_type,
	       const char *, gsize);
  
  void (*clear)(struct mwService *);
};


void mwService_recvChannelCreate(struct mwService *, struct mwChannel *,
				 struct mwMsgChannelCreate *);

void mwService_recvChannelAccept(struct mwService *, struct mwChannel *,
				 struct mwMsgChannelAccept *);

void mwService_recvChannelDestroy(struct mwService *, struct mwChannel *,
				  struct mwMsgChannelDestroy *);

void mwService_recv(struct mwService *, struct mwChannel *, guint32 msg_type,
		    const char *, gsize);


const char *mwService_getName(struct mwService *);
const char *mwService_getDesc(struct mwService *);

struct mwSession *mwService_getSession(struct mwService *);
void mwService_setSession(struct mwService *, struct mwSession *);

/* call the clean function, clean the service, free it, set it to
   NULL */
void mwService_free(struct mwService **);


#endif

