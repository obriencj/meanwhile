

#include <glib.h>
#include <glib/glist.h>
#include <string.h>

#include "channel.h"
#include "error.h"
#include "message.h"
#include "mw_debug.h"
#include "service.h"
#include "session.h"
#include "srvc_im.h"


/** @todo:
    - service structure should not be public
    - map mwIdBlock to chat structure
    - start/stop 
*/


#define PROTOCOL_TYPE  0x00001000
#define PROTOCOL_VER   0x00000003


#define IM_MAGIC_A  0x00000001
#define IM_MAGIC_B  0x00000001

#define NB_MAGIC_A  0x00000001
#define NB_MAGIC_B  0x00033453


enum send_actions {
  msg_MESSAGE = 0x0064, /* im */
};


enum mwImType {
  mwIm_TEXT  = 0x00000001,  /* text message */
  mwIm_DATA  = 0x00000002,  /* status message (usually) */
};


enum mwImDataType {
  mwImData_TYPING   = 0x00000001,  /** common use */
  mwImData_SUBJECT  = 0x00000003,  /** notesbuddy IM topic */
  mwImData_HTML     = 0x00000004,  /** notesbuddy html message */
};


struct mwServiceIm {
  struct mwService service;
  struct mwServiceImHandler *handler;
  GList *convs;
};


struct im_convo {
  struct mwChannel *chan;
  struct mwIdBlock target;
};


/** momentarily places a mwLoginInfo into a mwIdBlock */
static struct mwIdBlock *login_as_id(struct mwLoginInfo *info) {
  static struct mwIdBlock idb;
  if(! info) return NULL;
  idb.user = info->user_id;
  idb.community = info->community;
  return &idb;
}


#define CHAN_ID_BLOCK(channel) \
  (login_as_id(mwChannel_getUser(channel)))


static struct im_convo *convo_find_by_user(struct mwServiceIm *srvc,
					   struct mwIdBlock *to) {
  GList *l;
  for(l = srvc->convs; l; l = l->next) {
    struct im_convo *c = l->data;
    if(mwIdBlock_equal(&c->target, to))
       return c;
  }

  return NULL;
}


static struct im_convo *convo_find_by_chan(struct mwServiceIm *srvc,
					   struct mwChannel *chan) {
  GList *l;
  for(l = srvc->convs; l; l = l->next) {
    struct im_convo *c = l->data;
    if(c->chan == chan)
      return c;
  }

  return NULL;
}


static struct im_convo *convo_out_new(struct mwServiceIm *srvc,
				      struct mwIdBlock *to) {
  struct im_convo *c = NULL;

  c = convo_find_by_user(srvc, to);
  if(c) return c;

  c = g_new0(struct im_convo, 1);
  c->chan = NULL;
  mwIdBlock_clone(&c->target, to);

  srvc->convs = g_list_prepend(srvc->convs, c);
  return c;
}


static struct im_convo *convo_in_new(struct mwServiceIm *srvc,
				     struct mwChannel *chan) {
  struct im_convo *c;

  c = convo_find_by_chan(srvc, chan);
  if(c) return c;

  c = convo_out_new(srvc, CHAN_ID_BLOCK(chan));
  c->chan = chan;

  return c;
}


static void convo_create_chan(struct mwServiceIm *srvc,
			      struct im_convo *c) {

  struct mwSession *s;
  struct mwChannelSet *cs;
  struct mwChannel *chan;
  struct mwLoginInfo *login;
  struct mwPutBuffer *b;

  if(c->chan) return;

  s = mwService_getSession(MW_SERVICE(srvc));
  cs = mwSession_getChannels(s);

  chan = mwChannel_newOutgoing(cs);

  mwChannel_setService(chan, MW_SERVICE(srvc));
  mwChannel_setProtoType(chan, 0x00001000);
  mwChannel_setProtoVer(chan, 0x00000003);

  /* offer all known ciphers */
  mwChannel_populateSupportedCipherInstances(chan);

  /* set the target */
  login = mwChannel_getUser(chan);
  login->user_id = g_strdup(c->target.user);
  login->community = g_strdup(c->target.community);

  /* compose the addtl create */
  b = mwPutBuffer_new();
  guint32_put(b, NB_MAGIC_A);
  guint32_put(b, NB_MAGIC_B);
  mwPutBuffer_finalize(mwChannel_getAddtlCreate(chan), b);

  c->chan = mwChannel_create(chan)? NULL: chan;
}


static void convo_destroy(struct mwServiceIm *srvc,
			  struct im_convo *c, guint32 reason) {

  srvc->convs = g_list_remove_all(srvc->convs, c);

  if(c->chan) {
    mwChannel_destroy(c->chan, reason, NULL);
    c->chan = NULL;
  }

  mwIdBlock_clear(&c->target);
  g_free(c);
}


static int send_accept(struct mwChannel *chan) {
  struct mwSession *s = mwChannel_getSession(chan);
  struct mwUserStatus *stat = mwSession_getUserStatus(s);
  struct mwPutBuffer *b = mwPutBuffer_new();

  guint32_put(b, NB_MAGIC_A);
  guint32_put(b, NB_MAGIC_B);
  guint32_put(b, 0x02);
  mwUserStatus_put(b, stat);

  mwPutBuffer_finalize(mwChannel_getAddtlAccept(chan), b);

  return mwChannel_accept(chan);
}


static void recv_channelCreate(struct mwService *srvc,
			       struct mwChannel *chan,
			       struct mwMsgChannelCreate *msg) {

  /* - ensure it's the right service,proto,and proto ver
     - check the opaque for the right opaque junk
     - if not, close channel
     - compose & send a channel accept
  */

  struct mwServiceIm *srvc_im = (struct mwServiceIm *) srvc;
  struct mwSession *s;
  struct mwUserStatus *stat;
  guint32 x, y, z;
  struct mwGetBuffer *b;
  struct im_convo *c = NULL;
  
  s = mwChannel_getSession(chan);
  stat = mwSession_getUserStatus(s);

  x = mwChannel_getServiceId(chan);
  y = mwChannel_getProtoType(chan);
  z = mwChannel_getProtoVer(chan);

  if( (x != SERVICE_IM) || (y != PROTOCOL_TYPE) || (z != PROTOCOL_VER) ) {
    g_warning("unacceptable service, proto, ver:"
	      " 0x%08x, 0x%08x, 0x%08x", x, y, z);
    mwChannel_destroy(chan, ERR_SERVICE_NO_SUPPORT, NULL);
    return;
  }

  b = mwGetBuffer_wrap(&msg->addtl);
  guint32_get(b, &x);
  guint32_get(b, &y);
  z = (guint) mwGetBuffer_error(b);
  mwGetBuffer_free(b);

  if(z /* buffer error */ ) {
    g_warning("bad/malformed addtl in IM service");
    mwChannel_destroy(chan, ERR_SERVICE_NO_SUPPORT, NULL);

  } else if(x != 0x01) {
    g_message("unknown params: 0x%08x, 0x%08x", x, y);
    mwChannel_destroy(chan, ERR_IM_NOT_REGISTERED, NULL);
    
  } else if(y == 0x19) {
    g_info("rejecting pre-conference as per normal");
    mwChannel_destroy(chan, ERR_IM_NOT_REGISTERED, NULL);

  } else if(stat->status == mwStatus_BUSY) {

    c = convo_find_by_user(srvc_im, CHAN_ID_BLOCK(chan));
    if(! c) {
      g_info("rejecting IM channel due to DND status");
      mwChannel_destroy(chan, ERR_CLIENT_USER_DND, NULL);
    }
  }

  if(! c) c = convo_in_new(srvc_im, chan);
  if(send_accept(chan)) g_debug("sending IM channel accept failed");
}


static void recv_channelAccept(struct mwService *srvc, struct mwChannel *chan,
			       struct mwMsgChannelAccept *msg) {
  ; /* nuttin' needs doin' */
}


static void recv_channelDestroy(struct mwService *srvc, struct mwChannel *chan,
				struct mwMsgChannelDestroy *msg) {

  struct mwServiceIm *srvc_im = (struct mwServiceIm *) srvc;
  struct mwServiceImHandler *h = srvc_im->handler;
  struct im_convo *c;

  if(CHANNEL_IS_STATE(chan, mwChannel_ERROR)) {
    /* we were trying to create a channel to this person, but it failed. */
    if(h->got_error)
      h->got_error(srvc_im, CHAN_ID_BLOCK(chan), msg->reason);
    
  } else {
    if(h->got_typing)
      h->got_typing(srvc_im, CHAN_ID_BLOCK(chan), FALSE);
  }

  c = convo_find_by_chan(srvc_im, chan);
  c->chan = NULL;
}


static void recv_text(struct mwServiceIm *srvc, struct mwChannel *chan,
		      struct mwGetBuffer *b) {

  struct mwServiceImHandler *h;
  char *text = NULL;

  mwString_get(b, &text);

  /* ignore empty strings */
  if(! text) return;

  h = srvc->handler;
  if(h->got_text)
    h->got_text(srvc, CHAN_ID_BLOCK(chan), text);

  g_free(text);
}


static void recv_data(struct mwServiceIm *srvc, struct mwChannel *chan,
		      struct mwGetBuffer *b) {

  struct mwServiceImHandler *h;
  guint32 type, subtype;
  struct mwOpaque o = { 0, 0 };

  guint32_get(b, &type);
  guint32_get(b, &subtype);
  mwOpaque_get(b, &o);

  if(mwGetBuffer_error(b)) {
    mwOpaque_clear(&o);
    return;
  }

  h = srvc->handler;

  switch(type) {
  case mwImData_TYPING:
    if(h->got_typing)
      h->got_typing(srvc, CHAN_ID_BLOCK(chan), !subtype);
    break;

  case mwImData_HTML:
    if(o.len && h->got_html) {
      char *x;

      x = (char *) g_malloc(o.len + 1);
      x[o.len] = '\0';
      memcpy(x, o.data, o.len);

      h->got_html(srvc, CHAN_ID_BLOCK(chan), x);
      g_free(x);
    }
    break;

  case mwImData_SUBJECT:
    if(h->got_subject) {
      char *x;

      x = (char *) g_malloc(o.len + 1);
      x[o.len] = '\0';
      if(o.len) memcpy(x, o.data, o.len);

      h->got_subject(srvc, CHAN_ID_BLOCK(chan), x);
      g_free(x);
    }
    break;

  default:
    g_warning("unknown data message type in IM service:"
	      " 0x%08x, 0x%08x", type, subtype);
    pretty_opaque(&o);
  }

  mwOpaque_clear(&o);
}


static void recv(struct mwService *srvc, struct mwChannel *chan,
		 guint16 type, struct mwOpaque *data) {

  /* - ensure message type is something we want
     - parse message type into either mwIMText or mwIMData
     - handle
  */

  struct mwGetBuffer *b;
  guint32 mt;

  g_return_if_fail(type == msg_MESSAGE);

  b = mwGetBuffer_wrap(data);
  guint32_get(b, &mt);

  if(mwGetBuffer_error(b)) {
    g_warning("failed to parse message for IM service");
    mwGetBuffer_free(b);
    return;
  }

  switch(mt) {
  case mwIm_TEXT:
    recv_text((struct mwServiceIm *) srvc, chan, b);
    break;

  case mwIm_DATA:
    recv_data((struct mwServiceIm *) srvc, chan, b);
    break;

  default:
    g_warning("unknown message type 0x%08x for IM service", mt);
  }

  if(mwGetBuffer_error(b))
    g_warning("failed to parse message type 0x%08x for IM service", mt);

  mwGetBuffer_free(b);
}


static void clear(struct mwService *srvc) {
  ; /* no special cleanup required */
}


static const char *name(struct mwService *srvc) {
  return "Basic Instant Messaging";
}


static const char *desc(struct mwService *srvc) {
  return "A simple IM service, with typing notification";
}


static void start(struct mwService *srvc) {
  mwService_started(srvc);
}


static void stop(struct mwService *srvc) {
  mwService_stopped(srvc);
}


struct mwServiceIm *mwServiceIm_new(struct mwSession *session,
				    struct mwServiceImHandler *hndl) {

  struct mwServiceIm *srvc_im;
  struct mwService *srvc;

  g_return_val_if_fail(session != NULL, NULL);
  g_return_val_if_fail(hndl != NULL, NULL);

  srvc_im = g_new0(struct mwServiceIm, 1);
  srvc = &srvc_im->service;

  mwService_init(srvc, session, SERVICE_IM);
  srvc->recv_channelCreate = recv_channelCreate;
  srvc->recv_channelAccept = recv_channelAccept;
  srvc->recv_channelDestroy = recv_channelDestroy;
  srvc->recv = recv;
  srvc->clear = clear;
  srvc->get_name = name;
  srvc->get_desc = desc;
  srvc->start = start;
  srvc->stop = stop;

  srvc_im->handler = hndl;
  srvc_im->convs = NULL;

  return srvc_im;
}


struct mwServiceImHandler *mwServiceIm_getHandler(struct mwServiceIm *srvc) {
  g_return_val_if_fail(srvc != NULL, NULL);
  return srvc->handler;
}


int mwServiceIm_sendText(struct mwServiceIm *srvc,
			 struct mwIdBlock *target,
			 const char *text) {

  struct im_convo *c;
  struct mwPutBuffer *b;
  struct mwOpaque o;
  int ret;

  g_return_val_if_fail(srvc != NULL, -1);
  g_return_val_if_fail(target != NULL, -1);
  
  c = convo_out_new(srvc, target);
  if(! c->chan) convo_create_chan(srvc, c);

  b = mwPutBuffer_new();

  guint32_put(b, mwIm_TEXT);
  mwString_put(b, text);

  mwPutBuffer_finalize(&o, b);
  ret = mwChannel_send(c->chan, msg_MESSAGE, &o);
  mwOpaque_clear(&o);

  return ret;
}


int mwServiceIm_sendHtml(struct mwServiceIm *srvc,
			 struct mwIdBlock *target,
			 const char *html) {

  struct im_convo *c;
  struct mwPutBuffer *b;
  struct mwOpaque o;
  int ret;

  g_return_val_if_fail(srvc != NULL, -1);
  g_return_val_if_fail(target != NULL, -1);
  
  c = convo_out_new(srvc, target);
  if(! c->chan) convo_create_chan(srvc, c);

  b = mwPutBuffer_new();

  guint32_put(b, mwIm_DATA);
  guint32_put(b, mwImData_HTML);
  guint32_put(b, 0x00);

  /* use o first as a shell of an opaque for the text */
  o.len = strlen(html);
  o.data = (char *) html;
  mwOpaque_put(b, &o);

  /* use o again as the holder of the buffer's finalized data */
  mwPutBuffer_finalize(&o, b);
  ret = mwChannel_send(c->chan, msg_MESSAGE, &o);
  mwOpaque_clear(&o);

  return ret;
}


int mwServiceIm_sendSubject(struct mwServiceIm *srvc,
			    struct mwIdBlock *target,
			    const char *subject) {
  struct im_convo *c;
  struct mwPutBuffer *b;
  struct mwOpaque o;
  int ret;

  g_return_val_if_fail(srvc != NULL, -1);
  g_return_val_if_fail(target != NULL, -1);
  
  c = convo_out_new(srvc, target);
  if(! c->chan) convo_create_chan(srvc, c);

  b = mwPutBuffer_new();

  guint32_put(b, mwIm_DATA);
  guint32_put(b, mwImData_SUBJECT);
  guint32_put(b, 0x00);

  /* use o first as a shell of an opaque for the text */
  o.len = strlen(subject);
  o.data = (char *) subject;
  mwOpaque_put(b, &o);

  /* use o again as the holder of the buffer's finalized data */
  mwPutBuffer_finalize(&o, b);
  ret = mwChannel_send(c->chan, msg_MESSAGE, &o);
  mwOpaque_clear(&o);

  return ret;
}


int mwServiceIm_sendTyping(struct mwServiceIm *srvc,
			   struct mwIdBlock *target,
			   gboolean typing) {

  struct im_convo *c;
  struct mwPutBuffer *b;
  struct mwOpaque o = { 0, NULL };
  int ret;

  g_return_val_if_fail(srvc != NULL, -1);
  g_return_val_if_fail(target != NULL, -1);

  c = convo_out_new(srvc, target);
  
  /* don't bother creating a channel just to send a typing
     notificiation message, sheesh */
  if(! c->chan) return 0;

  b = mwPutBuffer_new();

  guint32_put(b, mwIm_DATA);
  guint32_put(b, mwImData_TYPING);
  guint32_put(b, !typing);

  /* not to be confusing, but we're re-using o first as an empty
     opaque, and later as the contents of the finalized buffer */
  mwOpaque_put(b, &o);

  mwPutBuffer_finalize(&o, b);
  ret = mwChannel_send(c->chan, msg_MESSAGE, &o);
  mwOpaque_clear(&o);

  return ret;
}


void mwServiceIm_closeChat(struct mwServiceIm *srvc,
			   struct mwIdBlock *target) {

  struct im_convo *c;

  g_return_if_fail(srvc != NULL);
  g_return_if_fail(target != NULL);

  c = convo_find_by_user(srvc, target);
  if(c) convo_destroy(srvc, c, ERR_SUCCESS);
}
