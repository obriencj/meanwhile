
#include "channel.h"
#include "message.h"
#include "service.h"


/* I tried to be explicit with the g_return_* calls, to make the debug
   logging a bit more sensible. Hence all the explicit "foo != NULL"
   checks. */


void mwService_recvChannelCreate(struct mwService *s, struct mwChannel *chan,
				 struct mwMsgChannelCreate *msg) {

  /* ensure none are null, ensure that the service and channel belong
     to the same session, and ensure that the message belongs on the
     channel */
  g_return_if_fail(s != NULL);
  g_return_if_fail(chan != NULL);
  g_return_if_fail(msg != NULL);
  g_return_if_fail(s->session == chan->session);
  g_return_if_fail(chan->id == msg->channel);

  if(s->recv_channelCreate)
    s->recv_channelCreate(s, chan, msg);
}


void mwService_recvChannelAccept(struct mwService *s, struct mwChannel *chan,
				 struct mwMsgChannelAccept *msg) {

  /* ensure none are null, ensure that the service and channel belong
     to the same session, and ensure that the message belongs on the
     channel */
  g_return_if_fail(s != NULL);
  g_return_if_fail(chan != NULL);
  g_return_if_fail(msg != NULL);
  g_return_if_fail(s->session == chan->session);
  g_return_if_fail(chan->id == msg->head.channel);

  if(s->recv_channelAccept)
    s->recv_channelAccept(s, chan, msg);
}


void mwService_recvChannelDestroy(struct mwService *s, struct mwChannel *chan,
				  struct mwMsgChannelDestroy *msg) {

  /* ensure none are null, ensure that the service and channel belong
     to the same session, and ensure that the message belongs on the
     channel */
  g_return_if_fail(s != NULL);
  g_return_if_fail(chan != NULL);
  g_return_if_fail(msg != NULL);
  g_return_if_fail(s->session == chan->session);
  g_return_if_fail(chan->id == msg->head.channel);

  if(s->recv_channelDestroy)
    s->recv_channelDestroy(s, chan, msg);
}


void mwService_recv(struct mwService *s, struct mwChannel *chan,
		    guint32 msg_type, const char *buf, gsize n) {

  /* ensure that none are null. buf and n should both be set to
     something greater-than zero, as empty messages are supposed to be
     filtered at the session level. ensure that the service and
     channel belong to the same session */
  g_return_if_fail(s != NULL);
  g_return_if_fail(chan != NULL);
  g_return_if_fail(buf != NULL);
  g_return_if_fail(n > 0);
  g_return_if_fail(s->session == chan->session);

  if(s->recv)
    s->recv(s, chan, msg_type, buf, n);
}


const char *mwService_getName(struct mwService *s) {
  g_return_val_if_fail(s != NULL, "");
  g_return_val_if_fail(s->get_name != NULL, "");

  return s->get_name();
}


const char *mwService_getDesc(struct mwService *s) {
  g_return_val_if_fail(s != NULL, "");
  g_return_val_if_fail(s->get_desc != NULL, "");

  return s->get_desc();
}


struct mwSession *mwService_getSession(struct mwService *s) {
  g_return_val_if_fail(s != NULL, NULL);
  return s->session;
}


void mwService_init(struct mwService *srvc, struct mwSession *sess,
		    guint type) {

  /* ensure nothing is null, and there's no such thing as a zero
     service type */
  g_return_if_fail(srvc != NULL);
  g_return_if_fail(sess != NULL);
  g_return_if_fail(type != 0x00);

  srvc->session = sess;
  srvc->type = type;
  srvc->state = mwServiceState_STOPPED;
}


void mwService_start(struct mwService *srvc) {
  g_return_if_fail(srvc != NULL);

  if(! MW_SERVICE_STOPPED(srvc))
    return;

  if(srvc->start)
    srvc->start(srvc);
}


void mwService_stop(struct mwService *srvc) {
  g_return_if_fail(srvc != NULL);

  if(MW_SERVICE_STOPPED(srvc) || MW_SERVICE_STOPPING(srvc))
    return;

  if(srvc->stop)
    srvc->stop(srvc);
}


void mwService_free(struct mwService *srvc) {
  g_return_if_fail(srvc != NULL);

  mwService_stop(srvc);

  if(srvc->clear)
    srvc->clear(srvc);

  g_free(srvc);
}

