

#include <glib.h>

#include "channel.h"
#include "service.h"


void mwService_recvChannelCreate(struct mwService *s, struct mwChannel *chan,
				 struct mwMsgChannelCreate *msg) {

  g_return_if_fail(s && chan && msg);
  g_return_if_fail(s->session == chan->session);

  if(s->recv_channelCreate)
    s->recv_channelCreate(s, chan, msg);
}


void mwService_recvChannelAccept(struct mwService *s, struct mwChannel *chan,
				 struct mwMsgChannelAccept *msg) {

  g_return_if_fail(s && chan && msg);
  g_return_if_fail(s->session == chan->session);

  if(s->recv_channelAccept)
    s->recv_channelAccept(s, chan, msg);
}


void mwService_recvChannelDestroy(struct mwService *s, struct mwChannel *chan,
				  struct mwMsgChannelDestroy *msg) {

  g_return_if_fail(s && chan && msg);
  g_return_if_fail(s->session == chan->session);

  if(s->recv_channelDestroy)
    s->recv_channelDestroy(s, chan, msg);
}


void mwService_recv(struct mwService *s, struct mwChannel *chan,
		    guint32 msg_type, const char *buf, gsize n) {

  g_return_if_fail(s && chan && buf);
  g_return_if_fail(s->session == chan->session);

  if(s->recv)
    s->recv(s, chan, msg_type, buf, n);
}


const char *mwService_getName(struct mwService *s) {
  g_return_val_if_fail((s && s->get_name), NULL);
  return s->get_name();
}


const char *mwService_getDesc(struct mwService *s) {
  g_return_val_if_fail((s && s->get_desc), NULL);
  return s->get_desc();
}


struct mwSession *mwService_getSession(struct mwService *s) {
  g_return_val_if_fail(s, NULL);
  return s->session;
}


void mwService_setSession(struct mwService *srvc, struct mwSession *session) {
  g_return_if_fail(srvc);
  srvc->session = session;
}


/* call the clean function, clean the service, free it, set it to NULL */
void mwService_free(struct mwService **srvc) {
  struct mwService *s;

  g_return_if_fail(srvc && *srvc);

  s = *srvc;
  *srvc = NULL;

  if(s->clear) s->clear(s);
  g_free(s);
}

