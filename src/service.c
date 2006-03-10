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
#include <glib-object.h>
#include "mw_channel.h"
#include "mw_debug.h"
#include "mw_error.h"
#include "mw_marshal.h"
#include "mw_message.h"
#include "mw_object.h"
#include "mw_service.h"


static GObjectClass *parent_class;


enum properties {
  property_state = 1,
  property_session,
  property_auto_start,
};


struct mw_service_private {
  guint state;          /**< property: state */
  MwSession *session;   /**< property: session (weak reference) */
  gboolean auto_start;  /**< property: auto start/stop with session */
  glong auto_ev;        /**< handler for session's state-changed */
};


static const gchar *mw_get_name(MwService *self) {
  return "Meanwhile Service Base Class";
}


static const gchar *mw_get_desc(MwService *self) {
  return "Generic utility base class for a service implementation";
}


static gboolean mw_starting(MwService *self) {
  guint state;
  g_object_get(G_OBJECT(self), "state", &state, NULL);

  return (state == mw_service_STOPPED);
}


static gboolean mw_start(MwService *self) {
  return TRUE;
}


static void mw_started(MwService *self) {
  g_object_set(G_OBJECT(self), "state", mw_service_STARTED, NULL);
}


static gboolean mw_stopping(MwService *self) {
  guint state;
  g_object_get(G_OBJECT(self), "state", &state, NULL);

  return (state == mw_service_ERROR ||
	  state == mw_service_STARTED);
}


static gboolean mw_stop(MwService *self) {
  return TRUE;
}


static void mw_stopped(MwService *self) {
  g_object_set(G_OBJECT(self), "state", mw_service_STOPPED, NULL);
}


static void mw_auto_cb(MwSession *session, guint state, gpointer info,
		       MwService *self) {

  gboolean auto_start = FALSE;

  g_object_get(G_OBJECT(self), "auto-start", &auto_start, NULL);

  if(! auto_start)
    return;

  switch(state) {
  case mw_session_STOPPING:
    MwService_stop(self);
    break;

  case mw_session_STARTED:
    MwService_start(self);
    break;

  default:
    ;
  }
}


static void mw_setup_session(MwService *self) {
  MwSession *session;  
  MwServicePrivate *priv;

  mw_debug_enter();

  priv = self->private;

  g_object_get(G_OBJECT(self), "session", &session, NULL);

  g_debug("mw_setup_session found session %p", session);

  priv->auto_ev = g_signal_connect(G_OBJECT(session), "state-changed",
				   G_CALLBACK(mw_auto_cb), self);

  mw_gobject_unref(session);

  mw_debug_exit();
}


static void mw_set_property(GObject *object,
			    guint property_id, const GValue *value,
			    GParamSpec *pspec) {

  MwService *self = MW_SERVICE(object);
  MwServicePrivate *priv = self->private;

  switch(property_id) {
  case property_state:
    {
      MwServiceClass *klass = MW_SERVICE_GET_CLASS(self);
      priv->state = g_value_get_uint(value);
      g_signal_emit(self, klass->signal_state_changed, 0,
		    priv->state, NULL);
    }
    break;

  case property_session:
    mw_gobject_unref(priv->session);
    priv->session = MW_SESSION(g_value_dup_object(value));
    break;

  case property_auto_start:
    priv->auto_start = g_value_get_boolean(value);
    break;

  default:
    ;
  }
}


static void mw_get_property(GObject *object,
			    guint property_id, GValue *value,
			    GParamSpec *pspec) {

  MwService *self = MW_SERVICE(object);
  MwServicePrivate *priv = self->private;

  switch(property_id) {
  case property_state:
    g_value_set_uint(value, priv->state);
    break;

  case property_session:
    g_value_set_object(value, G_OBJECT(priv->session));
    break;

  case property_auto_start:
    g_value_set_boolean(value, priv->auto_start);
    break;

  default:
    ;
  }
}


static GObject *
mw_constructor(GType type, guint props_count,
	       GObjectConstructParam *props) {

  MwServiceClass *klass;

  GObject *obj;
  MwService *self;
  MwServicePrivate *priv;

  mw_debug_enter();
  
  klass = MW_SERVICE_CLASS(g_type_class_peek(MW_TYPE_SERVICE));

  obj = parent_class->constructor(type, props_count, props);
  self = (MwService *) obj;
  
  /* set in mw_service_init */
  priv = self->private;
  
  priv->state = mw_service_STOPPED;

  mw_setup_session(self);

  mw_debug_exit();

  return obj;
}


static void mw_dispose(GObject *object) {
  MwService *self;
  MwServicePrivate *priv;

  mw_debug_enter();

  self = MW_SERVICE(object);
  priv = self->private;

  if(priv) {
    self->private = NULL;

    if(priv->session) {
      g_signal_handler_disconnect(priv->session, priv->auto_ev);
      mw_gobject_unref(priv->session);
      priv->session = NULL;
    }

    g_free(priv);
  }

  parent_class->dispose(object);

  mw_debug_exit();
}


static guint mw_signal_state_changed() {
  return g_signal_new("state-changed",
		      MW_TYPE_SERVICE,
		      0,
		      0,
		      NULL, NULL,
		      mw_marshal_VOID__UINT,
		      G_TYPE_NONE,
		      1,
		      G_TYPE_UINT);
}


static void mw_class_init(gpointer gclass, gpointer gclass_data) {

  GObjectClass *gobject_class = gclass;
  MwServiceClass *klass = gclass;

  parent_class = g_type_class_peek_parent(gobject_class);

  gobject_class->set_property = mw_set_property;
  gobject_class->get_property = mw_get_property;
  gobject_class->constructor = mw_constructor;
  gobject_class->dispose = mw_dispose;

  mw_prop_uint(gclass, property_state,
	       "state", "get/set state of this service",
	       G_PARAM_READWRITE);

  mw_prop_obj(gclass, property_session,
	      "session", "get MwSession reference",
	      MW_TYPE_SESSION, G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY);

  mw_prop_boolean(gclass, property_auto_start,
		  "auto-start", ("get/set whether this service should"
				 " automatically start or stop with the"
				 " session"),
		  G_PARAM_READWRITE);

  klass->signal_state_changed = mw_signal_state_changed();
  
  klass->get_name = mw_get_name;
  klass->get_desc = mw_get_desc;

  klass->starting = mw_starting;
  klass->start = mw_start;
  klass->started = mw_started;

  klass->stopping = mw_stopping;
  klass->stop = mw_stop;
  klass->stopped = mw_stopped;
}


static void mw_service_init(GTypeInstance *instance, gpointer g_class) {
  MwService *self;

  self = (MwService *) instance;
  self->private = g_new0(MwServicePrivate, 1);
}


static const GTypeInfo mw_service_info = {
  .class_size = sizeof(MwServiceClass),
  .base_init = NULL,
  .base_finalize = NULL,
  .class_init = mw_class_init,
  .class_finalize = NULL,
  .class_data = NULL,
  .instance_size = sizeof(MwService),
  .n_preallocs = 0,
  .instance_init = mw_service_init,
  .value_table = NULL,
};


GType MwService_getType() {
  static GType type = 0;

  if(type == 0) {
    type = g_type_register_static(MW_TYPE_OBJECT, "MwServiceType",
				  &mw_service_info, 0);
  }

  return type;
}


const gchar *MwService_getName(MwService *srvc) {
  const gchar *(*fn)(MwService *);

  g_return_val_if_fail(srvc != NULL, NULL);

  fn = MW_SERVICE_GET_CLASS(srvc)->get_name;
  return fn(srvc);
}


const gchar *MwService_getDescription(MwService *srvc) {
  const gchar *(*fn)(MwService *);

  g_return_val_if_fail(srvc != NULL, NULL);

  fn = MW_SERVICE_GET_CLASS(srvc)->get_desc;
  return fn(srvc);
}


void MwService_start(MwService *srvc) {
  gboolean (*fn)(MwService *);

  g_return_if_fail(srvc != NULL);

  fn = MW_SERVICE_GET_CLASS(srvc)->starting;
  if(fn(srvc)) {

    g_debug("starting service %s", MwService_getName(srvc));
    fn = MW_SERVICE_GET_CLASS(srvc)->start;
    if(fn(srvc)) {
      MwService_started(srvc);
    }
  }
}


void MwService_started(MwService *srvc) {
  void (*fn)(MwService *);

  g_return_if_fail(srvc != NULL);

  fn = MW_SERVICE_GET_CLASS(srvc)->started;
  fn(srvc);

  g_debug("started service %s", MwService_getName(srvc));
}


void MwService_stop(MwService *srvc) {
  gboolean (*fn)(MwService *);

  g_return_if_fail(srvc != NULL);

  fn = MW_SERVICE_GET_CLASS(srvc)->stopping;
  if(fn(srvc)) {

    g_debug("stopping service %s", MwService_getName(srvc));
    fn = MW_SERVICE_GET_CLASS(srvc)->stop;
    if(fn(srvc)) {
      MwService_stopped(srvc);
    }
  }
}


void MwService_stopped(MwService *srvc) {
  void (*fn)(MwService *);

  g_return_if_fail(srvc != NULL);

  fn = MW_SERVICE_GET_CLASS(srvc)->stopped;
  fn(srvc);

  g_debug("stopped service %s", MwService_getName(srvc));
}


void MwService_error(MwService *srvc) {
  g_return_if_fail(srvc != NULL);

  g_debug("error in service %s", MwService_getName(srvc));

  g_object_set(G_OBJECT(srvc), "state", mw_service_ERROR, NULL);
  MwService_stop(srvc);
}


/* The end. */
