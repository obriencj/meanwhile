

#ifndef _MW_CHANNEL_H
#define _MW_CHANNEL_H


#include <glib.h>
#include <glib/gslist.h>
#include <time.h>
#include "common.h"


struct mwSession;
struct mwMsgChannelCreate;
struct mwMsgChannelAccept;
struct mwMsgChannelDestroy;
struct mwMsgChannelSend;


/** this should never ever need to change, but just in case... */
#define MASTER_CHANNEL_ID  0x00000000


/** 1 if a channel id appears to be that of an incoming channel, or 0 if not */
#define CHAN_ID_IS_INCOMING(id) \
  (0x80000000 & (id))


/** 1 if a channel id appears to be that of an outgoing channel, or 0 if not */
#define CHAN_ID_IS_OUTGOING(id) \
  (! CHAN_ID_IS_INCOMING(id))


/** 1 if a channel appears to be an incoming channel, or 0 if not */
#define CHAN_IS_INCOMING(chan) \
  CHAN_ID_IS_INCOMING((chan)->id)


/** 1 if a channel appears to be an outgoing channel, or 0 if not */
#define CHAN_IS_OUTGOING(chan) \
  CHAN_ID_IS_OUTGOING((chan)->id)


/** Life-cycle of an outgoing channel:

   1: mwChannel_new is called. If there is a channel in the outgoing
   collection in state NEW, then it is returned. Otherwise, a channel
   is allocated, assigned a unique outgoing id, marked as NEW, and
   returned.

   2: channel is set to INIT status (effectively earmarking it as in
   use).  fields on the channel can then be set as necessary to
   prepare it for creation.

   3: mwChannel_create is called. The channel is marked to WAIT status
   and a message is sent to the server. The channel is also marked as
   inactive as of that moment.

   4: the channel is accepted (step 5) or rejected (step 7)

   5: an accept message is received from the server, and the channel
   is marked as OPEN, and the inactive mark is removed. And messages
   in the in or out queues for that channel are processed. The channel
   is now ready to be used.

   6: data is sent and received over the channel

   7: the channel is closed either by receipt of a close message or by
   local action. If by local action, then a close message is sent to
   the server.  The channel is cleaned up, its queues dumped, and it
   is set to NEW status to await re-use.

   Life-cycle of an incoming channel:

   1: a channel create message is received. A channel is allocated and
   given an id matching the message. It is placed in status WAIT, and
   marked as inactive as of that moment. The service matching that
   channel is alerted of the incoming creation request.

   2: the service can either accept (step 3) or reject (step 5) the
   channel

   3: mwChannel_accept is called. The channel is marked as OPEN, and
   an accept message is sent to the server. And messages in the in or
   out queues for that channel are processed. The channel is now ready
   to be used.

   4: data is sent and received over the channel

   5: The channel is closed either by receipt of a close message or by
   local action. If by local action, then a close message is sent to
   the server.  The channel is cleaned up, its queues dumped, and it
   is deallocated. */
enum mwChannelStatus {
  mwChannel_NEW   = 0x00,
  mwChannel_INIT  = 0x01,
  mwChannel_WAIT  = 0x10,
  mwChannel_OPEN  = 0x80
};


struct mwChannel {

  /** session this channel belongs to */
  struct mwSession *session;

  enum mwChannelStatus status;

  /** timestamp when channel was marked as inactive. */
  unsigned int inactive;

  /** creator for incoming channel, target for outgoing channel */
  struct mwIdBlock user;

  /* similar to data from the CreateCnl message in 8.4.1.7 */
  guint32 reserved;
  guint32 id;
  guint32 service;
  guint32 proto_type;
  guint32 proto_ver;

  /** encryption information from the channel create message */
  struct mwEncryptBlock encrypt;

  /** the expanded rc2/40 key for receiving encrypted messages */
  int incoming_key[64];
  char outgoing_iv[8];  /**< iv for outgoing messages */
  char incoming_iv[8];  /**< iv for incoming messages */

  GSList *outgoing_queue; /**< queued outgoing messages */
  GSList *incoming_queue; /**< queued incoming messages */

  /** optional slot for attaching an extra bit of state, usually by the
      owning service */
  void *addtl;

  /** optional cleanup function. Useful for ensuring proper cleanup of
      an attached value in the addtl slot. */
  void (*clear)(struct mwChannel *);
};


struct mwChannelSet {
  struct mwSession *session;
  GList *outgoing;
  GList *incoming;
};


void mwChannelSet_clear(struct mwChannelSet *);

struct mwChannel *mwChannel_newIncoming(struct mwChannelSet *, guint32 id);

struct mwChannel *mwChannel_newOutgoing(struct mwChannelSet *);


/** for outgoing channels: instruct the session to send a channel
    create message to the server, and to mark the channel (which must
    be in INIT status) as being in WAIT status.
   
    for incoming channels: configures the channel according to options
    in the channel create message. Marks the channel as being in WAIT
    status */
int mwChannel_create(struct mwChannel *, struct mwMsgChannelCreate *);


/** for outgoing channels: receives the acceptance message and marks
    the channel as being OPEN.

    for incoming channels: instruct the session to send a channel
    accept message to the server, and to mark the channel (which must
    be an incoming channel in WAIT status) as being OPEN. */
int mwChannel_accept(struct mwChannel *, struct mwMsgChannelAccept *);


/** instruct the session to destroy a channel. The channel may be
    either incoming or outgoing, but must be in WAIT or OPEN status. */
int mwChannel_destroy(struct mwChannel *, struct mwMsgChannelDestroy *);


/** Destroy a channel. Composes and sends channel destroy message with
    the passed reason, and send it via mwChannel_destroy. Has no effect
    if chan is NULL.
    @returns value of mwChannel_destroy call, or zero if chan is NULL
*/
int mwChannel_destroyQuick(struct mwChannel *chan, guint32 reason);


/* compose a sendOnCnl message, encrypt it as per the channel's
   specification, and send it */
int mwChannel_send(struct mwChannel *, guint32 msg_type,
		   const char *, gsize);


void mwChannel_recv(struct mwChannel *, struct mwMsgChannelSend *);


/* locate a channel by its id. */
struct mwChannel *mwChannel_find(struct mwChannelSet *, guint32 chan);


/** used in destroyInactiveChannels to determine how many inactive
    channels can be destroyed in a single call. 32 is a big number for
    a single client */
#define MAX_INACTIVE_KILLS  32


/** intended to be called periodically to close channels which have
    been marked as inactive since before the threshold timestamp. */
void mwChannelSet_destroyInactive(struct mwChannelSet *, time_t thrsh);



#endif

