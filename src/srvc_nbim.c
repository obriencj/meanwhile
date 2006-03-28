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


#include "mw_srvc_nbim.h"


#define MW_SERVICE_ID  0x00001000
#define MW_PROTO_TYPE  0x00001000
#define MW_PROTO_VER   0x00000003  


static GObjectClass *srvc_parent_class;
static GObjectClass *conv_parent_class;


enum srvc_properties {
  srvc_property_compat_mode = 1,
};


struct mw_im_service_private {
  gboolean compat;
};


static const char *mw_get_name(MwService *self) {
  return "NotesBuddy IM";
}


static const char *mw_get_desc(MwService *self) {
  return "Extension to the IM service to provide compatability with"
    " features of the NotesBuddy client";
}


static void mw_incoming_accept(MwSession *session, MwNBIMService *self,
			       MwChannel *chan) {
  
}


static gboolean mw_incoming_channel(MwSession *session, MwChannel *chan,
				    MwIMService *self) {
  
}


static gboolean mw_start(MwService *srvc) {
  MwNBIMService *self;
  MwSession *session;

  self = MW_NB_IM_SERVICE(srvc);

  g_object_get(G_OBJECT(self), "session", &session, NULL);

  g_signal_connect(G_OBJECT(session), "channel",
		   G_CALLBACK(mw_incoming_channel), self);

  mw_gobject_unref(session);

  return TRUE;
}


static MwConversation *mw_new_conv(MwIMService *self,
				   const gchar *user,
				   const gchar *community) {
  MwNBConversation *conv;

  conv = g_object_new(MW_TYPE_NB_CONVERSATION,
		      "service", self,
		      "user", user,
		      "community", community,
		      NULL);

  return MW_CONVERSATION(conv);
}


static void mw_srvc_set_property(GObject *object,
				 guint property_id, const GValue *value,
				 GParamSpec *pspec) {
  MwNBIMService *self;
  MwNBIMServicePrivate *priv;

  self = MW_NB_IM_SERVICE(object);
  priv = self->private;

  switch(property_id) {
  case srvc_property_compat:
    priv->compat = g_value_get_boolean(value);
    break;
  default:
    ;      
  }
}


static void mw_srvc_get_property(GObject *object,
				 guint property_id, GValue *value,
				 GParamSpec *pspec) {
  MwNBIMService *self;
  MwNBIMServicePrivate *priv;

  self = MW_NB_IM_SERVICE(object);
  priv = self->private;

  switch(property_id) {
  case srvc_property_compat:
    g_value_set_boolean(value, priv->compat);
    break;
  default:
    ;      
  }
}


static void mw_srvc_class_init(gpointer class, gpointer gclass_data) {
  GObjectClass *gobject_class = gclass;
  MwServiceClass *service_class = gclass;
  MwIMServiceClass *imservice_class = gclass;
  
  srvc_parent_class = g_type_class_peek_parent(gobject_class);

  gobject_class->set_property = mw_srvc_set_property;
  gobject_class->get_property = mw_srvc_get_property;

  mw_prop_boolean(gobject_class, srvc_property_compat,
		  "im-compat", "get/set basic IM compatability mode",
		  G_PARAM_READWRITE|G_PARAM_CONSTRUCTOR);
  
  service_class->get_name = mw_get_name;
  service_class->get_desc = mw_get_desc;
  service_class->start = mw_start;
  
  imservice_class->new_conv = mw_new_conv;
}


static const GTypeInfo mw_srvc_info = {
  .class_size = sizeof(MwNBIMServiceClass),
  .base_init = NULL,
  .base_finalize = NULL,
  .class_init = mw_srvc_class_init,
  .class_finalize = NULL,
  .class_data = NULL,
  .instance_size = sizeof(MwNBIMService),
  .n_preallocs = 0,
  .instance_init = NULL,
  .value_table = NULL,
};


GType MwNBIMService_getType() {
  static GType type = 0;

  if(type == 0) {
    type = g_type_register_static(MW_TYPE_IM_SERVICE, "MwNBIMServiceType",
				  &mw_srvc_info, 0);
  }

  return type;
}


MwNBIMService *MwNBIMService_new(MwSession *session, gboolean compat) {
  MwNBIMService *srvc;

  srvc = g_object_new(MW_NB_IM_SERVICE_TYPE,
		      "session", session,
		      "im-compat", compat,
		      NULL);

  return srvc;
}


enum conv_properties {
  conv_property_allow_compat = 1,
  conv_property_compat_mode,
};


static void mw_open(MwConversation *self) {

}


static void mw_close(MwConversation *self) {

}





/* The end. */
