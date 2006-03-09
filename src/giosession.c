/*
  Meanwhile - Unofficial Lotus Sametime Community Client Library
  Copyright (C) 2004  Christopher (siege) O'Brien
  
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.
  
  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.
  
  You should have received a copy of the GNU Library General Public
  License along with this library; if not, write to the Free
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/


#include <glib.h>
#include <glib/giochannel.h>
#include <string.h>
#include "mw_common.h"
#include "mw_config.h"
#include "mw_debug.h"
#include "mw_giosession.h"
#include "mw_object.h"


static GIOChannel *MwGIOChannel_copy(GIOChannel *chan) {
  g_io_channel_ref(chan);
  return chan;
}


static void MwGIOChannel_free(GIOChannel *chan) {
  g_io_channel_unref(chan);
}


GType MwGIOChannel_getType() {
  static GType type = 0;

  if(type == 0) {
    type = g_boxed_type_register_static("MwGIOChannelType",
					(GBoxedCopyFunc) MwGIOChannel_copy,
					(GBoxedFreeFunc) MwGIOChannel_free);
  }

  return type;
}


/** set in mw_giosession_class_init */
static GObjectClass *parent_class;


struct mw_gio_session_private {
  GIOChannel *giochannel;

  /**
     Source IDs watching giochannel

     [0] for incoming
     [1] for outgoing
  */
  gint giosource[2];

  guchar *buf;    /**< buffer for single outgoing message */
  gsize buf_len;  /**< length of allocated buffer */

  guchar *head;   /**< offset in buf for next data to write */
  gsize head_len; /**< length of head left to be written */
};


enum properties {
  property_giochannel = 1,
};


static gboolean cb_read_ready(GIOChannel *chan, GIOCondition cond,
			      MwGIOSession *self) {

  guchar buf[MW_DEFAULT_BUFLEN];
  gsize len = MW_DEFAULT_BUFLEN, len_read = 0;
  GIOStatus stat;

  stat = g_io_channel_read_chars(chan, (gchar *) buf, len, &len_read, NULL);

  switch(stat) {
  case G_IO_STATUS_ERROR:
  case G_IO_STATUS_EOF:
    MwSession_stop(MW_SESSION(self), 0x80000000);
    return FALSE;

  case G_IO_STATUS_NORMAL:
  default:
    MwSession_feed(MW_SESSION(self), buf, len_read);
    return TRUE;
  }
}


/** loops over buffer pointed to by b, of length pointed to by l,
    writing MW_DEFAULT_BUFLEN chunks at a time. If for some reason a
    write call results in no data being written (EAGAIN, EOF, etc),
    stops. b and l will be advanced and decremented as data is
    successfully written. */
static GIOStatus write_loop(GIOChannel *chan, const guchar **b, gsize *l) {
  GIOStatus stat;

  gchar *buf = (gchar *) *b;
  gsize len = *l;

  while(len) {

    /* write up to MW_DEFAULT_BUFLEN in a single iteration */
    gsize step = MW_DEFAULT_BUFLEN, written = 0;
    if(step > len) step = len;

    stat = g_io_channel_write_chars(chan, buf, step, &written, NULL);

    if(! written)
      break;

    g_debug("wrote %u", written);
    len -= written;
    buf += written;
  }

  *b = (guchar *) buf;
  *l = len;

  return stat;
}


/** called when the io channel has non-blocking write available */
static gboolean cb_write_ready(GIOChannel *chan, GIOCondition cond,
			       MwGIOSession *self) {

  MwGIOSessionPrivate *priv;

  g_return_val_if_fail(self->private != NULL, FALSE);
  priv = self->private;

  if(priv->head) {
    /* if there is something in the session's buffer, try and finish
       sending that, return TRUE */

    const guchar *buf = priv->head;
    gsize len = priv->head_len;
    GIOStatus stat;
    
    stat = write_loop(chan, &buf, &len);

    /* reset the head and head_len. If there's nothing left to write, then
       set head to NULL */
    priv->head = len? (guchar *) buf: NULL;
    priv->head_len = len;

    return TRUE;

  } else if(MwSession_pending(MW_SESSION(self))) {
    /* if the session has a pending message, call flush, which in turn
       triggers mw_write_cb, then return TRUE */

    MwSession_flush(MW_SESSION(self));
    return TRUE;

  } else {
    /* nothing needs sending, return FALSE */

    g_io_channel_flush(chan, NULL);
    priv->giosource[1] = 0;

    return FALSE;
  }
}


/** called when the session writes a message */
static void mw_write_cb(MwSession *ses, const guchar *buf, gsize len,
			gpointer junk) {

  MwGIOSession *self;
  MwGIOSessionPrivate *priv;
  GIOStatus stat;

  self = MW_GIO_SESSION(ses);
  
  g_return_if_fail(self->private != NULL);
  priv = self->private;

  stat = write_loop(priv->giochannel, &buf, &len);

  if(stat == G_IO_STATUS_AGAIN) {

    /* ensure there's enough buffer. Don't bother with a realloc,
       since we don't care about what's currently in the buffer. */
    if(priv->buf_len <= len) {
      g_free(priv->buf);
      while(priv->buf_len <= len) {
	priv->buf_len += MW_DEFAULT_BUFLEN;
      }
      priv->buf = g_malloc(priv->buf_len);
    }

    priv->head = priv->buf;
    priv->head_len = len;
    memcpy(priv->head, buf, len);
  }
}


/** called when the session has an outgoing message ready */
static gboolean mw_pending_cb(MwSession *ses, gpointer junk) {
  MwGIOSession *self;
  MwGIOSessionPrivate *priv;

  self = MW_GIO_SESSION(ses);

  g_return_val_if_fail(self->private != NULL, FALSE);
  priv = self->private;
  
  if(priv->giosource[1] == 0) {
    /* start the write loop */
    priv->giosource[1] = g_io_add_watch(priv->giochannel,
					G_IO_OUT,
					(GIOFunc) cb_write_ready,
					self);
  }

  /* stop any other pending handlers from being triggered */
  return TRUE;
}


static GObject *
mw_gio_session_constructor(GType type, guint props_count,
			   GObjectConstructParam *props) {

  MwGIOSessionClass *klass;

  GObject *obj;
  MwGIOSession *self;
  MwGIOSessionPrivate *priv;

  klass = MW_GIO_SESSION_CLASS(g_type_class_peek(MW_TYPE_GIOSESSION));

  obj = parent_class->constructor(type, props_count, props);
  self = MW_GIO_SESSION(obj);

  /* should be done in mw_gio_session_init */
  g_return_val_if_fail(self->private != NULL, NULL);
  priv = self->private;

  priv->buf = g_malloc(MW_DEFAULT_BUFLEN);
  priv->buf_len = MW_DEFAULT_BUFLEN;
  self->private = priv;

  /* watch for the pending and write signals. */
  /* @todo Is it possible to override the defalt handlers instead of
     connecting to the signals? */
  g_signal_connect(obj, "pending", G_CALLBACK(mw_pending_cb), NULL);
  g_signal_connect(obj, "write", G_CALLBACK(mw_write_cb), NULL);

  return obj;
}


static void mw_gio_session_dispose(GObject *obj) {
  MwGIOSession *self;
  MwGIOSessionPrivate *priv;

  mw_debug_enter();
  
  self = MW_GIO_SESSION(obj);
  
  g_return_if_fail(self->private != NULL);
  priv = self->private;

  g_free(priv->buf);

  parent_class->dispose(obj);
  
  mw_debug_exit();
}


static void mw_set_property(GObject *object,
			    guint property_id, const GValue *value,
			    GParamSpec *pspec) {

  MwGIOSession *self = MW_GIO_SESSION(object);
  MwGIOSessionPrivate *priv;

  g_return_if_fail(self->private != NULL);
  priv = self->private;

  switch(property_id) {
  case property_giochannel:
    priv->giochannel = g_value_dup_boxed(value);
    break;
  }
}


static void mw_get_property(GObject *object,
			    guint property_id, GValue *value,
			    GParamSpec *pspec) {

  MwGIOSession *self = MW_GIO_SESSION(object);
  MwGIOSessionPrivate *priv;

  g_return_if_fail(self->private != NULL);
  priv = self->private;

  switch(property_id) {
  case property_giochannel:
    g_value_set_boxed(value, &priv->giochannel);
    break;
  }
}


static void mw_start(MwSession *session) {
  MwGIOSession *self;
  MwGIOSessionPrivate *priv;
  void (*fn)(MwSession *);

  self = MW_GIO_SESSION(session);

  g_return_if_fail(self->private != NULL);
  priv = self->private;

  /* connect G_IO_IN event */
  priv->giosource[0] = g_io_add_watch(priv->giochannel,
				      G_IO_IN,
				      (GIOFunc) cb_read_ready,
				      self);

  /* call overridden start method */
  fn = MW_SESSION_CLASS(parent_class)->start;
  fn(session);
}


static void mw_stop(MwSession *session, guint32 reason) {
  MwGIOSession *self;
  MwGIOSessionPrivate *priv;
  void (*fn)(MwSession *, guint32);

  mw_debug_enter();
  
  self = MW_GIO_SESSION(session);

  g_return_if_fail(self->private != NULL);
  priv = self->private;

  /* disconnect G_IO_IN and G_IO_OUT events */
  if(priv->giosource[0]) {
    g_source_remove(priv->giosource[0]);
    priv->giosource[0] = 0;
  }
  if(priv->giosource[1]) {
    g_source_remove(priv->giosource[1]);
    priv->giosource[1] = 0;   
  }

  /* call overridden stop method */
  fn = MW_SESSION_CLASS(parent_class)->stop;
  fn(session, reason);

  mw_debug_exit();
}


static void mw_set_giochannel(MwGIOSession *self, GIOChannel *chan) {
  MwGIOSessionPrivate *priv;
  GIOChannel *old;

  g_return_if_fail(self->private != NULL);
  priv = self->private;

  old = priv->giochannel;
  priv->giochannel = chan;

  if(chan) g_io_channel_ref(chan);
  if(old) g_io_channel_unref(old);
}


static GIOChannel *mw_get_giochannel(MwGIOSession *self) {
  MwGIOSessionPrivate *priv;

  g_return_val_if_fail(self->private != NULL, NULL);
  priv = self->private;

  return priv->giochannel;
}


static void mw_gio_session_class_init(gpointer g_class,
				      gpointer g_class_data) {
  GObjectClass *gobject_class;
  MwSessionClass *session_class;
  MwGIOSessionClass *klass;

  gobject_class = G_OBJECT_CLASS(g_class);
  session_class = MW_SESSION_CLASS(g_class);
  klass = MW_GIO_SESSION_CLASS(g_class);

  parent_class = g_type_class_peek_parent(gobject_class);
  
  gobject_class->constructor = mw_gio_session_constructor;
  gobject_class->dispose = mw_gio_session_dispose;
  gobject_class->get_property = mw_get_property;
  gobject_class->set_property = mw_set_property;

  mw_prop_boxed(gobject_class, property_giochannel,
		"giochannel", "get/set backing GIOChannel",
		MW_TYPE_GIOCHANNEL,
		G_PARAM_READWRITE|G_PARAM_CONSTRUCT);

  session_class->start = mw_start;
  session_class->stop = mw_stop;
  
  klass->set_giochannel = mw_set_giochannel;
  klass->get_giochannel = mw_get_giochannel;
}


static void mw_gio_session_init(GTypeInstance *instance, gpointer g_class) {
  MwGIOSession *self;
  
  self = (MwGIOSession *) instance;
  self->private = g_new0(MwGIOSessionPrivate, 1);;
}


static const GTypeInfo info = {
  .class_size = sizeof(MwGIOSessionClass),
  .base_init = NULL,
  .base_finalize = NULL,
  .class_init = mw_gio_session_class_init,
  .class_data = NULL,
  .instance_size = sizeof(MwGIOSession),
  .n_preallocs = 0,
  .instance_init = mw_gio_session_init,
  .value_table = NULL,
};


GType MwGIOSession_getType() {
  static GType type = 0;

  if(type == 0) {
    type = g_type_register_static(MW_TYPE_SESSION, "MwGIOSessionType",
				  &info, 0);
  }

  return type;
}


MwGIOSession *MwGIOSession_newFromChannel(GIOChannel *chan) {
  return g_object_new(MW_TYPE_GIOSESSION, "giochannel", chan, NULL);
}



MwGIOSession *MwGIOSession_newFromSocket(gint fd) {
  MwGIOSession *ret;
  GIOChannel *chan = NULL;
  
  if(fd) {
     chan = g_io_channel_unix_new(fd);
     g_io_channel_set_encoding(chan, NULL, NULL);
     g_io_channel_set_flags(chan, G_IO_FLAG_NONBLOCK, NULL);
  }

  ret = g_object_new(MW_TYPE_GIOSESSION, "giochannel", chan, NULL);
  
  if(chan) {
    g_io_channel_unref(chan);
  }

  return ret;
}


static void mw_main_state_changed(MwSession *self, guint state, gpointer info,
				  GMainLoop *loop) {

  if(state == mw_session_STOPPED) {
    g_main_loop_quit(loop);
  }
}


void MwGIOSession_main(MwGIOSession *session) {
  GMainLoop *loop;

  g_return_if_fail(session != NULL);

  loop = g_main_loop_new(NULL, FALSE);

  g_signal_connect(G_OBJECT(session), "state-changed",
		   G_CALLBACK(mw_main_state_changed), loop);

  MwSession_start(MW_SESSION(session));

  g_main_loop_run(loop);
  g_main_loop_unref(loop);
}


static gboolean mw_polite_stop_cb(gpointer data) {
  MwSession_stop(data, 0x00);
  return FALSE;
}


void MwGIOSession_politeStop(MwGIOSession *session) {
  MwGIOSessionPrivate *priv;

  g_return_if_fail(session != NULL);
  priv = session->private;

  if(priv->giosource[0]) {
    g_source_remove(priv->giosource[0]);
    priv->giosource[0] = 0;
  }

  g_idle_add(mw_polite_stop_cb, session);
}


/* The end. */
