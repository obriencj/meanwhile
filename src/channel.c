

#include <glib.h>
#include <glib/glist.h>
#include <string.h>

#include "channel.h"
#include "cipher.h"
#include "error.h"
#include "message.h"
#include "service.h"
#include "session.h"


static void flush_channel(struct mwChannel *);


static void channel_init(struct mwChannel *chan, struct mwSession *s) {
  chan->session = s;
  mwIV_init(chan->outgoing_iv);
  mwIV_init(chan->incoming_iv);
  chan->status = mwChannel_NEW;
}


struct mwChannel *mwChannel_newIncoming(struct mwChannelSet *cs, guint32 id) {
  struct mwChannel *chan;

  g_return_val_if_fail(cs && cs->session, NULL);

  chan = g_new0(struct mwChannel, 1);
  channel_init(chan, cs->session);

  chan->id = id;
  cs->incoming = g_list_prepend(cs->incoming, chan);

  return chan;
}


struct mwChannel *mwChannel_newOutgoing(struct mwChannelSet *cs) {
  struct mwChannel *chan;
  GList *l;

  g_return_val_if_fail(cs && cs->session, NULL);

  /* attempt to find a cached NEW channel */
  for(l = cs->outgoing; l; l = l->next) {
    struct mwChannel *c = (struct mwChannel *) l->data;
    if(c->status == mwChannel_NEW) {
      return c;
    }
  }

  /* if we couldn't find a cached NEW channel, create one */
  chan = g_new0(struct mwChannel, 1);
  channel_init(chan, cs->session);

  if(cs->outgoing) {
    /* if other channels exist, then add one to the highest existing
       id */
    struct mwChannel *c = (struct mwChannel *) cs->outgoing->data;
    chan->id = c->id + 1;

  } else {
    /* the default id for a new outgoing channel */
    chan->id = 0x00000001;
  }
  
  /* prepend the new channel to our chain */
  cs->outgoing = g_list_prepend(cs->outgoing, chan);

  return chan;
}


/* send a channel create message */
static int create_outgoing(struct mwChannel *chan,
			   struct mwMsgChannelCreate *msg) {

  /* - if channel and msg do not match channel ids, return -1
     - if channel is not in status INIT, return -1
     - send the message
     - set the status to WAIT on success
     - return 0 on success
  */

  int ret;

  g_return_val_if_fail(chan->id == msg->channel, -1);
  g_return_val_if_fail(chan->status == mwChannel_INIT, -1);

  ret = mwSession_send(chan->session, (struct mwMessage *) msg);
  if(! ret) {
    /* effectively provide a time-out for outgoing WAIT */
    mwChannel_markActive(chan, FALSE);
    chan->status = mwChannel_WAIT;
  }

  return ret;
}


/* receive a channel create message */
static int create_incoming(struct mwChannel *chan,
			   struct mwMsgChannelCreate *msg) {
  
  chan->service = msg->service;
  chan->proto_type = msg->proto_type;
  chan->proto_ver = msg->proto_ver;

  chan->user.user = g_strdup(msg->creator.user_id);
  chan->user.community = g_strdup(msg->creator.community);

  mwEncryptBlock_clone(&chan->encrypt, &msg->encrypt);

  /* expand the five-byte creator (this session's) id into a key */
  mwKeyExpand((int *)chan->incoming_key, msg->creator.login_id, 5);

  /* WAIT for the service to send the accept through the session */
  chan->status = mwChannel_WAIT;
  return 0;
}


int mwChannel_create(struct mwChannel *chan, struct mwMsgChannelCreate *msg) {

  g_return_val_if_fail(chan, -1);
  g_message("sending channel 0x%08x create", chan->id);

  return CHAN_IS_INCOMING(chan)?
    create_incoming(chan, msg): create_outgoing(chan, msg);
}


static void channel_open(struct mwChannel *chan) {
  struct mwSession *s = chan->session;

  chan->status = mwChannel_OPEN;
  mwChannel_markActive(chan, TRUE);
  flush_channel(chan);
    
  if(s->on_channelOpen)
    s->on_channelOpen(chan);
}


/* receive an acceptance message on a channel we created */
static int accept_outgoing(struct mwChannel *chan,
			   struct mwMsgChannelAccept *msg) {

  g_return_val_if_fail(chan->id == msg->head.channel, -1);
  g_return_val_if_fail(chan->status == mwChannel_WAIT, -1);

  /* expand the five-byte acceptor's id into a key */
  mwKeyExpand((int *)chan->incoming_key, msg->acceptor.login_id, 5);

  channel_open(chan);
  return 0;
}


/* send an acceptance message on a channel we received */
static int accept_incoming(struct mwChannel *chan,
			   struct mwMsgChannelAccept *msg) {
  int ret;

  g_return_val_if_fail(chan->id == msg->head.channel, -1);
  g_return_val_if_fail(chan->status == mwChannel_WAIT, -1);

  ret = mwSession_send(chan->session, (struct mwMessage *) msg);
  if(! ret) channel_open(chan);

  return ret;
}


int mwChannel_accept(struct mwChannel *chan,
		     struct mwMsgChannelAccept *msg) {

  return CHAN_IS_INCOMING(chan)?
    accept_incoming(chan, msg): accept_outgoing(chan, msg);
}


static void channel_clear(struct mwChannel *chan) {
  struct mwSession *s;
  struct mwMessage *msg;
  GSList *l;

  s = chan->session;

  /* the optional cleanup gets to go first */
  if(chan->clear) chan->clear(chan);

  mwIdBlock_clear(&chan->user);
  mwEncryptBlock_clear(&chan->encrypt);

  /* clean up the outgoing queue */
  for(l = chan->outgoing_queue; l; l = l->next) {
    msg = (struct mwMessage *) l->data;
    l->data = NULL;
    mwMessage_free(msg);
  }
  g_slist_free(chan->outgoing_queue);

  /* clean up the incoming queue */
  for(l = chan->incoming_queue; l; l = l->next) {
    msg = (struct mwMessage *) l->data;
    l->data = NULL;
    mwMessage_free(msg);
  }
  g_slist_free(chan->incoming_queue);

  /* quick-set every field to zero */
  memset(chan, 0x00, sizeof(struct mwChannel));

  /* yeehaw */
  channel_init(chan, s);
}


int mwChannel_destroy(struct mwChannel *chan,
		      struct mwMsgChannelDestroy *msg) {

  struct mwSession *s;

  g_return_val_if_fail(chan, -1);
  g_return_val_if_fail(chan->session, -1);

  s = chan->session;

  if(msg) {
    g_message("destroy: channel 0x%08x, reason 0x%08x", chan->id, msg->reason);
  }

  if(s->on_channelClose)
    s->on_channelClose(chan);

  if(CHAN_IS_OUTGOING(chan)) {
    /* outgoing channels are recycled */
    guint32 id = chan->id;
    channel_clear(chan);
    chan->id = id;

  } else {
    /* incoming channels are thrown away */
    struct mwChannelSet *cs = s->channels;
    cs->incoming = g_list_remove_all(cs->incoming, chan);
    channel_clear(chan);
    g_free(chan);
  }

  /* send the message finally */
  return msg? mwSession_send(s, (struct mwMessage *) msg): 0;
}


int mwChannel_destroyQuick(struct mwChannel *chan, guint32 reason) {
  struct mwMsgChannelDestroy *msg;
  int ret;

  if(! chan) return 0;

  msg = (struct mwMsgChannelDestroy *)
    mwMessage_new(mwMessage_CHANNEL_DESTROY);

  msg->head.channel = chan->id;
  msg->reason = reason;
  
  ret = mwChannel_destroy(chan, msg);
  mwMessage_free(MW_MESSAGE(msg));

  return ret;
}


static void queue_outgoing(struct mwChannel *chan,
			   struct mwMsgChannelSend *msg) {

  g_message("queue_outgoing, channel 0x%08x", chan->id);
  chan->outgoing_queue = g_slist_append(chan->outgoing_queue, msg);
}


static int channel_send(struct mwChannel *chan,
			struct mwMsgChannelSend *msg) {

  int ret = 0;

  /* if the channel is open, send and free the message. Otherwise,
     queue the message to be sent once the channel is finally
     opened */

  if(chan->status == mwChannel_OPEN) {

    /*
      g_info("sending %u bytes on channel 0x%08x", msg->data.len, chan->id);
    */
    ret = mwSession_send(chan->session, (struct mwMessage *) msg);
    mwMessage_free(MW_MESSAGE(msg));

  } else {
    queue_outgoing(chan, msg);
  }

  return ret;
}


int mwChannel_send(struct mwChannel *chan, guint32 type,
		   const char *b, gsize n) {

  struct mwMsgChannelSend *msg;

  g_return_val_if_fail(chan != NULL, -1);

  msg = (struct mwMsgChannelSend *) mwMessage_new(mwMessage_CHANNEL_SEND);
  msg->head.channel = chan->id;
  msg->type = type;

  /* if the channel has an encryption block specifying encryption,
     then we use the rc2_40 cipher and encrypt. Otherwise, leave it
     alone. */

  if(! n) {
    msg->data.data = NULL;
    msg->data.len = 0x00;

  } else if(chan->encrypt.type) {
    msg->head.options = mwMessageOption_ENCRYPT;
    mwEncryptExpanded(chan->session->session_key, chan->outgoing_iv,
		      b, n, &msg->data.data, &msg->data.len);
  } else {
    msg->data.data = g_memdup(b, n);
    msg->data.len = n;
  }

  return channel_send(chan, msg);
}


static void queue_incoming(struct mwChannel *chan,
			   struct mwMsgChannelSend *msg) {

  /* we clone the message, because session_process will clear it once
     we return */

  struct mwMsgChannelSend *m = g_new0(struct mwMsgChannelSend, 1);
  m->head.type = msg->head.type;
  m->head.options = msg->head.options;
  m->head.channel = msg->head.channel;
  mwOpaque_clone(&m->head.attribs, &msg->head.attribs);

  m->type = msg->type;
  mwOpaque_clone(&m->data, &msg->data);

  chan->incoming_queue = g_slist_append(chan->incoming_queue, m);
}


static void channel_recv(struct mwChannel *chan,
			 struct mwMsgChannelSend *msg) {

  char *data = msg->data.data;
  gsize len = msg->data.len;
  struct mwService *srvc;

  srvc = mwSession_getService(chan->session, chan->service);

  if(msg->head.options & mwMessageOption_ENCRYPT) {
    char *dec = NULL;
    gsize dec_len = 0;
    
    mwDecryptExpanded(chan->incoming_key, chan->incoming_iv,
		      data, len, &dec, &dec_len);

    mwService_recv(srvc, chan, msg->type, dec, dec_len);
    g_free(dec);
    
  } else {
    mwService_recv(srvc, chan, msg->type, data, len);
  }
}


static void flush_channel(struct mwChannel *chan) {
  GSList *l;

  for(l = chan->outgoing_queue; l; l = l->next) {
    struct mwMessage *msg = (struct mwMessage *) l->data;
    l->data = NULL;

    mwSession_send(chan->session, msg);
    mwMessage_free(msg);
  }
  g_slist_free(chan->outgoing_queue);
  chan->outgoing_queue = NULL;

  for(l = chan->incoming_queue; l; l = l->next) {
    struct mwMsgChannelSend *msg = (struct mwMsgChannelSend *) l->data;
    l->data = NULL;

    channel_recv(chan, msg);
    mwMessage_free(MW_MESSAGE(msg));
  }
  g_slist_free(chan->incoming_queue);
  chan->incoming_queue = NULL;
}


void mwChannel_recv(struct mwChannel *chan, struct mwMsgChannelSend *msg) {
  if(chan->status == mwChannel_OPEN) {
    channel_recv(chan, msg);

  } else {
    queue_incoming(chan, msg);
  }
}


struct mwChannel *mwChannel_find(struct mwChannelSet *cs, guint32 chan) {

  GList *l = (CHAN_ID_IS_OUTGOING(chan))?
    cs->outgoing: cs->incoming;

  for(; l; l = l->next) {
    struct mwChannel *c = (struct mwChannel *) l->data;
    if(c->id == chan) return c;
  }

  return NULL;
}


static int find_inactive(GList *list, time_t thrsh,
			 struct mwChannel **k, int count) {

  for(; list && (count < MAX_INACTIVE_KILLS); list = list->next) {
    struct mwChannel *c = (struct mwChannel *) list->data;
    if((c->inactive > 0) && (c->inactive <= thrsh))
      k[count++] = c;
  }
  return count;
}


void mwChannel_markActive(struct mwChannel *chan, gboolean active) {
  g_return_if_fail(chan != NULL);
  g_message("marking channel 0x%08x as %s",
	    chan->id, active? "active": "inactive");
  chan->inactive = active? 0: time(NULL);
}


void mwChannelSet_destroyInactive(struct mwChannelSet *cs, time_t thrsh) {
  struct mwChannel *kills[MAX_INACTIVE_KILLS];
  int count = 0;

  count = find_inactive(cs->outgoing, thrsh, kills, count);
  count = find_inactive(cs->incoming, thrsh, kills, count);
  
  while(count--)
    mwChannel_destroyQuick(kills[count], ERR_SUCCESS);
}


void mwChannelSet_free(struct mwChannelSet *cs) {
  GList *l;

  for(l = cs->incoming; l; l = l->next) {
    channel_clear((struct mwChannel *) l->data);
    g_free(l->data);
    l->data = NULL;
  }
  g_list_free(cs->incoming);
  cs->incoming = NULL;

  for(l = cs->outgoing; l; l = l->next) {
    channel_clear((struct mwChannel *) l->data);
    g_free(l->data);
    l->data = NULL;
  }
  g_list_free(cs->outgoing);
  cs->outgoing = NULL;
}


struct mwChannelSet *mwChannelSet_new(struct mwSession *s) {
  struct mwChannelSet *cs = g_new0(struct mwChannelSet, 1);
  cs->session = s;
  return cs;
}
