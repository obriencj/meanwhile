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
#include <glib/ghash.h>
#include <glib/glist.h>
#include <string.h>

#include "mw_channel.h"
#include "mw_debug.h"
#include "mw_error.h"
#include "mw_message.h"
#include "mw_service.h"
#include "mw_session.h"
#include "mw_srvc_aware.h"
#include "mw_util.h"


/* ----- MwAwareService ----- */


#define MW_SERVICE_ID  0x00000011
#define MW_PROTO_TYPE  0x00000011
#define MW_PROTO_VER   0x00030005


#define MW_AWARE_SERVICE_GET_PRIVATE(o)				\
  (G_TYPE_INSTANCE_GET_PRIVATE((o), MW_TYPE_AWARE_SERVICE,	\
			       MwAwareServicePrivate))


typedef struct mw_aware_service_private MwAwareServicePrivate;


struct mw_aware_service_private {
  GHashTable *awares;
  MwChannel *channel;
};


/* cached parent class */
static GObjectClass *srvc_parent_class;


static const gchar *mw_get_name(MwService *self) {
  return "Presence Awareness";
}


static const gchar *mw_get_desc(MwService *self) {
  return "Presence Awareness Service";
}


static void mw_aware_weak_cb(gpointer data, GObject *gone) {
  gpointer *mem = data;
  MwAwareService *self;
  MwAwareServicePrivate *priv;

  self = mem[0];
  priv = MW_AWARE_SERVICE_GET_PRIVATE(self);

  g_hash_table_remove(priv->awares, mem[1]);
  g_free(mem);
}


static void mw_aware_setup(MwAwareService *self, MwAware *aware) {
  MwAwareServicePrivate *priv;
  MwIdentity *id;
  gpointer *mem;

  priv = MW_AWARE_SERVICE_GET_PRIVATE(self);

  /* id will be free'd as a key when the hash table is destroyed */
  g_object_get(G_OBJECT(aware), "identity", &id, NULL);

  g_hash_table_replace(priv->awares, id, aware);

  mem = g_new(gpointer, 2);
  mem[0] = self;
  mem[1] = id;
  g_object_weak_ref(G_OBJECT(aware), mw_aware_weak_cb, mem);
}


MwAware *mw_new_aware(MwAwareService *self, enum mw_aware_type type,
		      const gchar *user, const gchar *community) {

  MwAware *aware;

  aware = g_object_new(MW_TYPE_AWARE,
		       "service", self,
		       "type", type,
		       "user", user,
		       "community", community,
		       NULL);

  return aware;
}


MwAware *mw_get_aware(MwAwareService *self, enum mw_aware_type type,
		      const gchar *user, const gchar *community) {

  MwAware *aware;

  aware = MwAwareService_findConversation(self, type, user, community);
  
  if(! aware) {
    aware = MW_AWARE_SERVICE_GET_CLASS(self)->new_aware(self, type,
							user, community);
    mw_setup_aware(self, aware);
  }

  return aware;
}


MwAware *mw_find_aware(MwAwareService *self, enum mw_aware_type type,
		       const gchar *user, const gchar *community) {

  MwAwareServicePrivate *priv;
  MwAware *aware;
  MwIdentity id = { .user = (gchar *) user,
		    .community = (gchar *) community };

  priv = MW_AWARE_SERVICE_GET_PRIVATE(self);
  aware = g_hash_table_lookup(priv->awares, &id);
  
  return mw_gobject_ref(aware);
}


void mw_foreach_aware(MwAwareService *self, GFunc func, gpointer data) {
  MwAwareServicePrivate *priv;
  priv = MW_AWARE_SERVICE_GET_PRIVATE(self);
  mw_map_foreach_val(priv->awares, func, data)
}


void mw_watch_aware(MwAwareService *self, MwAware *aware) {
  /* XXX */
}


gboolean mw_unwatch_aware(MwAwareService *self, MwAware *aware) {
  /* XXX */
}


void mw_watch_awares(MwAwareService *self, const GList *awares) {
  /* XXX */
}


void mw_unwatch_awares(MwAwareService *self, const GList *awares) {
  /* XXX */
}


void mw_watch_attrib(MwAwareService *self, guint32 key) {
  /* XXX */
}


void mw_unwatch_attrib(MwAwareService *self, guint32 key) {
  /* XXX */
}


void mw_set_attrib(MwAwareService *self, guint32 key, const MwOpaque *val) {
  /* XXX */
}


const MwOpaque *mw_get_attrib(MwAwareService *self, guint32 key) {
  /* XXX */
}


static void mw_recv_AWARE_SNAPSHOT(MwAwareService *self,
				   MwGetBuffer *gb) {
  /* XXX */
}


static void mw_recv_AWARE_UPDATE(MwAwareService *self,
				 MwGetBuffer *gb) {
  /* XXX */
}


static void mw_recv_AWARE_GROUP(MwAwareService *self,
				MwGetBuffer *gb) {
  /* XXX */
}


static void mw_recv_OPT_GOT_SET(MwAwareService *self,
				   MwGetBuffer *gb) {
  /* XXX */
}


static void mw_recv_OPT_GOT_UNSET(MwAwareService *self,
				  MwGetBuffer *gb) {
  /* XXX */
}


static void mw_channel_recv(MwChannel *chan,
			    guint type, const MwOpaque *msg,
			    MwAwareService *self) {

  MwGetBuffer *gb = MwGetBuffer_wrap(msg);

  switch(type) {
  case msg_AWARE_SNAPSHOT:
    mw_recv_AWARE_SNAPSHOT(self, gb);
    break;

  case msg_AWARE_UPDATE:
    mw_recv_AWARE_UPDATE(self, gb);
    break;

  case msg_AWARE_GROUP:
    mw_recv_AWARE_GROUP(self, gb);
    break;

  case msg_OPT_GOT_SET:
    mw_recv_OPT_GOT_SET(self, gb);
    break;

  case msg_OPT_GOT_UNSET:
    mw_recv_OPT_GOT_UNSET(self, gb);
    break;

  case msg_OPT_GOT_UNKNOWN:
  case msg_OPT_DID_SET:
  case msg_OPT_DID_UNSET:
  case msg_OPT_DID_ERROR:
    break;

  default:
    mw_mailme_opaque(msg, "unknown message in aware service: 0x%x", type);
  }

  if(MwGetBuffer_error(gb)) {
    mw_mailme_opaque(msg, "aware service message type: 0x%x", type);
  }

  MwGetBuffer_free(gb);
}


static void mw_channel_state(MwChannel *chan, gint state, MwService *self) {
  switch(state) {
  case mw_channel_open:
    MwService_started(self);
    break;

  case mw_channel_error:
    MwService_error(self);
    break;

  case mw_channel_closed:
    {
      gint srvc_state;
      g_object_get(G_OBJECT(self), "state", &srvc_state, NULL);

      if(srvc_state == mw_service_stopping) {
	MwService_stopped(self);
      } else {
	MwService_stop(self);
      }
    }
    break;

  default:
    ;
  }
}


static gboolean mw_start(MwService *self) {
  MwAwareServicePrivate *priv;
  MwChannel *chan = NULL;

  priv = MW_AWARE_SERVICE_GET_PRIVATE(self);
  chan = priv->channel;

  if(! chan) {
    MwSession *session = NULL;

    g_object_get(G_OBJECT(self), "session", &session, NULL);

    chan = MwSession_newChannel(session);
    
    mw_gobject_unref(session);

    g_object_set(G_OBJECT(chan),
		 "service-id", MW_SERVICE_ID,
		 "protocol-type", MW_PROTO_TYPE,
		 "protocol-version", MW_PROTO_VER,
		 "offered-policy", mw_channel_encrypt_WHATEVER,
		 NULL);

    priv->channel = chan;

    g_signal_connect(G_OBJECT(chan), "incoming",
		     G_CALLBACK(mw_channel_recv), self);

    g_signal_connect(G_OBJECT(chan), "state-changed",
		     G_CALLBACK(mw_channel_state), self);
  }

  MwChannel_open(chan, NULL);

  return FALSE;
}


static gboolean mw_stop(MwService *self) {
  MwAwareServicePrivate *priv;
  MwChannel *chan;
  gboolean ret = TRUE;

  priv = MW_AWARE_SERVICE_GET_PRIVATE(self);
  chan = priv->channel;

  if(chan) {
    gint chan_state;
    g_object_get(G_OBJECT(chan), "state", &chan_state, NULL);

    if(chan_state == mw_channel_open ||
       chan_state == mw_channel_pending) {

      MwChannel_close(chan, 0x00, NULL);
      ret = FALSE;
    }
  }

  return ret;
}


static GObject *
mw_srvc_constructor(GType type, guint props_count,
		    GObjectConstructParam *props) {

  MwAwareServiceClass *klass;

  GObject obj;
  MwAwareService *self;
  MwAwareServicePrivate *priv;

  mw_debug_enter();
  
  klass = MW_AWARE_SERVICE_CLASS(g_type_class_peek(MW_TYPE_AWARE_SERVICE));

  obj = srvc_parent_class->constructor(type, props_count, props);
  self = (MwAwareService *) obj;

  priv = MW_AWARE_SERVICE_GET_PRIVATE(self);

  priv->awares = g_hash_tabe_new_full((GHashFunc) MwIdentity_hash,
				      (GEqualFunc) MwIdentity_equal,
				      (GDestroyNotify) MwIdentity_free,
				      NULL);

  mw_debug_exit();
  return obj;
}


static void mw_srvc_dispose(GObject *object) {
  MwAwareService *self;
  MwAwareServicePrivate *priv;

  mw_debug_enter();

  self = MW_AWARE_SERVICE(object);
  priv = MW_AWARE_SERVICE_PRIVATE(self);
  
  mw_gobject_unref(priv->channel);
  priv->channel = NULL;

  g_hash_table_destroy(priv->awares);
  priv->awares = NULL;

  srvc_parent_class->dispose(object);

  mw_debug_exit();
}


static void mw_srvc_class_init(gpointer gclass, gpointer gclass_data) {
  GObjectClass *gobject_class = gclass;
  MwServiceClass *service_class = gclass;
  MwIMServiceClass *klass = gclass;

  srvc_parent_class = g_type_class_peek_parent(gobject_class);

  gobject_class->constructor = mw_srvc_constructor;
  gobject_class->dispose = mw_srvc_dispose;

  g_type_class_add_private(klass, sizeof(MwAwareServicePrivate));
}


static const GTypeInfo mw_srvc_info = {
  .class_size = sizeof(MwAwareServiceClass),
  .base_init = NULL,
  .base_finalize = NULL,
  .class_init = mw_srvc_class_init,
  .class_finalize = NULL,
  .class_data = NULL,
  .instance_size = sizeof(MwAwareService),
  .n_preallocs = 0,
  .instance_init = NULL,
  .value_table = NULL,
};


GType MwAwareService_getType() {
  static GType type = 0;

  if(type == 0) {
    type = g_type_register_static(MW_TYPE_SERVICE, "MwAwareServiceType",
				  &mw_srvc_info, 0);
  }

  return type;
}


MwAwareService *MwAwareService_new(MwSession *session) {
  MwAwareService *srvc;

  srvc = g_object_new(MW_TYPE_AWARE_SERVICE,
		      "session", session,
		      "auto-start", TRUE,
		      NULL);

  return srvc;
}


MwAware *MwAwareService_getAware(MwAwareService *self,
				 enum mw_aware_type type,
				 const gchar *user,
				 const gchar *community) {

  MwAware *(*fn)(MwAwareService *, enum mw_aware_type,
		 const gchar *, const gchar *);

  g_return_val_if_fail(self != NULL, NULL);
  g_return_val_if_fail(user != NULL, NULL);

  fn = MW_AWARE_SERVICE_GET_CLASS(self)->get_aware;
  return fn(self, type, user, community);
}


MwAware *MwAwareService_findAware(MwAwareService *self,
				  enum mw_aware_type type,
				  const gchar *user,
				  const gchar *community) {

  MwAware *(*fn)(MwAwareService *, enum mw_aware_type,
		 const gchar *, const gchar *);

  g_return_val_if_fail(self != NULL, NULL);
  g_return_val_if_fail(user != NULL, NULL);

  fn = MW_AWARE_SERVICE_GET_CLASS(self)->find_aware;
  return fn(self, type, user, community);
}


void MwAwareService_foreachAware(MwAwareService *self,
				 GFunc func, gpointer data) {

  void (*fn)(MwAwareService *self, GFunc, gpointer);

  g_return_if_fail(self != NULL);

  fn = MW_AWARE_SERVICE_GET_CLASS(self)->foreach_aware;
  fn(self, func, data);
}


void MwAwareService_foreachAwareClosure(MwAwareService *self,
					GClosure *closure) {

  MwAwareService_foreachAware(self, mw_closure_gfunc_obj, closure);
}


/* ----- MwAware ----- */


enum aware_properties {
  aware_property_service = 1,
  aware_property_type,
  aware_property_user,
  aware_property_community,
  aware_property_id,
};


#define MW_AWARE_GET_PRIVATE(o)						\
  (G_TYPE_INSTANCE_GET_PRIVATE((o), MW_TYPE_AWARE, MwAwarePrivate))


typedef struct mw_aware_private MwAwarePrivate;


struct mw_aware_private {
  MwAwareService *service;  /**< reference to owning service */
  guint type;               /**< enum mw_aware_type */
  MwIdentity id;            /**< user/server/login and community */
  GHashTable *attribs;      /**< awareness attributes */
};


/* cached parent class */
static GObjectClass *aware_parent_class;


static void mw_aware_set_attrib(MwAware *self, 
				guint32 key, const MwOpaque *val) {

}


static const MwOpaque *mw_aware_get_attrib(MwAware *self, guint32 key) {

}


static void mw_aware_get_property(GObject *object,
				  guint property_id, GValue *value,
				  GParamSpec *pspec) {

}


static void mw_aware_set_property(GObject *object,
				  guint property_id, const GValue *value,
				  GParamSpec *pspec) {

}


static GObject *mw_aware_constructor(GType type, guint props_count,
				     GObjectConstructParam *param) {

}


static void mw_aware_dispose(GObject *object) {

}


static void mw_aware_class_init(gpointer gclass, gpointer gclass_data) {

}


static const GTypeInfo mw_srvc_info = {
  .class_size = sizeof(MwAwareClass),
  .base_init = NULL,
  .base_finalize = NULL,
  .class_init = mw_srvc_class_init,
  .class_finalize = NULL,
  .class_data = NULL,
  .instance_size = sizeof(MwAware),
  .n_preallocs = 0,
  .instance_init = NULL,
  .value_table = NULL,
};


GType MwAware_getType() {
  static GType type = 0;

  if(type == 0) {
    type = g_type_register_static(MW_TYPE_OBJECT, "MwAwareType",
				  &mw_aware_info, 0);
  }

  return type;
}


const MwOpaque *MwAware_getAttribute(MwAware *self, guint32 key) {

}


gboolean MwAware_getBoolean(MwAware *self, guint32 key) {

}


/* ----- MwAwareList ----- */


static GObjectClass *list_parent_class;


GType MwAwareList_getType() {
  static GType type = 0;

  if(type == 0) {
    type = g_type_register_static(MW_TYPE_OBJECT, "MwAwareListType",
				  &mw_list_info, 0);
  }

  return type;
}


MwAwareList *MwAwareList_new(MwAwareService *srvc) {
  MwAwareList *list;

  list = g_object_new(MW_TYPE_AWARE_LIST,
		      "service", srvc,
		      NULL);

  return list;
}


void MwAwareList_add(MwAwareList *self, enum mw_aware_type type,
		     const gchar *user, const gchar *community) {
  
}


gboolean MwAwareList_remove(MwAwareList *self, enum mw_aware_type type,
			    const gchar *user, const gchar *community) {
  
}


MwAware *MwAwareList_get(MwAwareList *self, enum mw_aware_type type,
			 const gchar *user, const gchar *community) {
  
}


void MwAwareList_addAware(MwAwareList *self, MwAware *aware) {

}


gboolean MwAwareList_removeAware(MwAwareList *self, MwAware *aware) {
  
}


void MwAwareList_foreachAware(MwAwareList *self, GFunc func, gpointer data) {

}


void MwAwareList_foreachAwareClosure(MwAwareList *self, GClosure *closure) {

}


/* --- */


struct mwServiceAware {
  struct mwService service;

  struct mwAwareHandler *handler;

  /** map of ENTRY_KEY(aware_entry):aware_entry */
  GHashTable *entries;

  /** set of guint32:attrib_watch_entry attribute keys */
  GHashTable *attribs;

  /** collection of lists of awareness for this service. Each item is
      a mwAwareList */
  GList *lists;

  /** the buddy list channel */
  struct mwChannel *channel;
};


struct mwAwareList {

  /** the owning service */
  struct mwServiceAware *service;

  /** map of ENTRY_KEY(aware_entry):aware_entry */
  GHashTable *entries;

  /** set of guint32:attrib_watch_entry attribute keys */
  GHashTable *attribs;

  struct mwAwareListHandler *handler;
  struct mw_datum client_data;
};


struct mwAwareAttribute {
  guint32 key;
  struct mwOpaque data;
};


struct attrib_entry {
  guint32 key;
  GList *membership;
};


/** an actual awareness entry, belonging to any number of aware lists */
struct aware_entry {
  struct mwAwareSnapshot aware;

  /** list of mwAwareList containing this entry */
  GList *membership;

  /** collection of attribute values for this entry.
      map of ATTRIB_KEY(mwAwareAttribute):mwAwareAttribute */
  GHashTable *attribs;
};


#define ENTRY_KEY(entry) &entry->aware.id


/** the channel send types used by this service */
enum msg_types {
  msg_AWARE_ADD       = 0x0068,  /**< remove an aware */
  msg_AWARE_REMOVE    = 0x0069,  /**< add an aware */

  msg_OPT_DO_SET      = 0x00c9,  /**< set an attribute */
  msg_OPT_DO_UNSET    = 0x00ca,  /**< unset an attribute */
  msg_OPT_WATCH       = 0x00cb,  /**< set the attribute watch list */

  msg_AWARE_SNAPSHOT  = 0x01f4,  /**< recv aware snapshot */
  msg_AWARE_UPDATE    = 0x01f5,  /**< recv aware update */
  msg_AWARE_GROUP     = 0x01f6,  /**< recv group aware */

  msg_OPT_GOT_SET     = 0x0259,  /**< recv attribute set update */
  msg_OPT_GOT_UNSET   = 0x025a,  /**< recv attribute unset update */

  msg_OPT_GOT_UNKNOWN = 0x025b,  /**< UNKNOWN */
  
  msg_OPT_DID_SET     = 0x025d,  /**< attribute set response */
  msg_OPT_DID_UNSET   = 0x025e,  /**< attribute unset response */
  msg_OPT_DID_ERROR   = 0x025f,  /**< attribute set/unset error */
};


static void aware_entry_free(struct aware_entry *ae) {
  mwAwareSnapshot_clear(&ae->aware);
  g_list_free(ae->membership);
  g_hash_table_destroy(ae->attribs);
  g_free(ae);
}


static void attrib_entry_free(struct attrib_entry *ae) {
  g_list_free(ae->membership);
  g_free(ae);
}


static void attrib_free(struct mwAwareAttribute *attrib) {
  mwOpaque_clear(&attrib->data);
  g_free(attrib);
}


static struct aware_entry *aware_find(struct mwServiceAware *srvc,
				      struct mwAwareIdBlock *srch) {
  g_return_val_if_fail(srvc != NULL, NULL);
  g_return_val_if_fail(srvc->entries != NULL, NULL);
  g_return_val_if_fail(srch != NULL, NULL);
  
  return g_hash_table_lookup(srvc->entries, srch);
}


static struct aware_entry *list_aware_find(struct mwAwareList *list,
					   struct mwAwareIdBlock *srch) {
  g_return_val_if_fail(list != NULL, NULL);
  g_return_val_if_fail(list->entries != NULL, NULL);
  g_return_val_if_fail(srch != NULL, NULL);

  return g_hash_table_lookup(list->entries, srch);
}


static void compose_list(struct mwPutBuffer *b, GList *id_list) {
  guint32_put(b, g_list_length(id_list));
  for(; id_list; id_list = id_list->next)
    mwAwareIdBlock_put(b, id_list->data);
}


static int send_add(struct mwChannel *chan, GList *id_list) {
  struct mwPutBuffer *b = mwPutBuffer_new();
  struct mwOpaque o;
  int ret;

  g_return_val_if_fail(chan != NULL, 0);

  compose_list(b, id_list);

  mwPutBuffer_finalize(&o, b);

  ret = mwChannel_send(chan, msg_AWARE_ADD, &o);
  mwOpaque_clear(&o);

  return ret;  
}


static int send_rem(struct mwChannel *chan, GList *id_list) {
  struct mwPutBuffer *b = mwPutBuffer_new();
  struct mwOpaque o;
  int ret;

  g_return_val_if_fail(chan != NULL, 0);

  compose_list(b, id_list);
  mwPutBuffer_finalize(&o, b);

  ret = mwChannel_send(chan, msg_AWARE_REMOVE, &o);
  mwOpaque_clear(&o);

  return ret;
}


static gboolean collect_dead(gpointer key, gpointer val, gpointer data) {
  struct aware_entry *aware = val;
  GList **dead = data;

  if(aware->membership == NULL) {
    g_info(" removing %s, %s",
	   NSTR(aware->aware.id.user), NSTR(aware->aware.id.community));
    *dead = g_list_append(*dead, aware);
    return TRUE;

  } else {
    return FALSE;
  }
}


static int remove_unused(struct mwServiceAware *srvc) {
  /* - create a GList of all the unused aware entries
     - remove each unused aware from the service
     - if the service is alive, send a removal message for the collected
     unused.
  */

  int ret = 0;
  GList *dead = NULL, *l;

  if(srvc->entries) {
    g_info("bring out your dead *clang*");
    g_hash_table_foreach_steal(srvc->entries, collect_dead, &dead);
  }
 
  if(dead) {
    if(MW_SERVICE_IS_LIVE(srvc))
      ret = send_rem(srvc->channel, dead) || ret;
    
    for(l = dead; l; l = l->next)
      aware_entry_free(l->data);

    g_list_free(dead);
  }

  return ret;
}


static int send_attrib_list(struct mwServiceAware *srvc) {
  struct mwPutBuffer *b;
  struct mwOpaque o;

  int tmp;
  GList *l;

  g_return_val_if_fail(srvc != NULL, -1);
  g_return_val_if_fail(srvc->channel != NULL, 0);

  l = map_collect_keys(srvc->attribs);
  tmp = g_list_length(l);

  b = mwPutBuffer_new();
  guint32_put(b, 0x00);
  guint32_put(b, tmp);
  
  for(; l; l = g_list_delete_link(l, l)) {
    guint32_put(b, GPOINTER_TO_UINT(l->data));
  }

  mwPutBuffer_finalize(&o, b);
  tmp = mwChannel_send(srvc->channel, msg_OPT_WATCH, &o);
  mwOpaque_clear(&o);

  return tmp;
}


static gboolean collect_attrib_dead(gpointer key, gpointer val,
				    gpointer data) {

  struct attrib_entry *attrib = val;
  GList **dead = data;

  if(attrib->membership == NULL) {
    g_info(" removing 0x%08x", GPOINTER_TO_UINT(key));
    *dead = g_list_append(*dead, attrib);
    return TRUE;

  } else {
    return FALSE;
  }
}


static int remove_unused_attrib(struct mwServiceAware *srvc) {
  GList *dead = NULL;

  if(srvc->attribs) {
    g_info("collecting dead attributes");
    g_hash_table_foreach_steal(srvc->attribs, collect_attrib_dead, &dead);
  }
 
  /* since we stole them, we'll have to clean 'em up manually */
  for(; dead; dead = g_list_delete_link(dead, dead)) {
    attrib_entry_free(dead->data);
  }

  return MW_SERVICE_IS_LIVE(srvc)? send_attrib_list(srvc): 0;
}


static void recv_accept(struct mwServiceAware *srvc,
			struct mwChannel *chan,
			struct mwMsgChannelAccept *msg) {

  g_return_if_fail(srvc->channel != NULL);
  g_return_if_fail(srvc->channel == chan);

  if(MW_SERVICE_IS_STARTING(MW_SERVICE(srvc))) {
    GList *list = NULL;

    list = map_collect_values(srvc->entries);
    send_add(chan, list);
    g_list_free(list);

    send_attrib_list(srvc);

    mwService_started(MW_SERVICE(srvc));

  } else {
    mwChannel_destroy(chan, ERR_FAILURE, NULL);
  }
}


static void recv_destroy(struct mwServiceAware *srvc,
			 struct mwChannel *chan,
			 struct mwMsgChannelDestroy *msg) {

  srvc->channel = NULL;
  mwService_stop(MW_SERVICE(srvc));

  /** @todo session sense service and mwService_start */
}


/** called from SNAPSHOT_recv, UPDATE_recv, and
    mwServiceAware_setStatus */
static void status_recv(struct mwServiceAware *srvc,
			struct mwAwareSnapshot *idb) {

  struct aware_entry *aware;
  GList *l;

  aware = aware_find(srvc, &idb->id);

  if(! aware) {
    /* we don't deal with receiving status for something we're not
       monitoring, but it will happen sometimes, eg from manually set
       status */
    return;
  }
  
  /* clear the existing status, then clone in the new status */
  mwAwareSnapshot_clear(&aware->aware);
  mwAwareSnapshot_clone(&aware->aware, idb);
  
  /* trigger each of the entry's lists */
  for(l = aware->membership; l; l = l->next) {
    struct mwAwareList *alist = l->data;
    struct mwAwareListHandler *handler = alist->handler;

    if(handler && handler->on_aware)
      handler->on_aware(alist, idb);
  }
}


static void attrib_recv(struct mwServiceAware *srvc,
			struct mwAwareIdBlock *idb,
			struct mwAwareAttribute *attrib) {

  struct aware_entry *aware;
  struct mwAwareAttribute *old_attrib = NULL;
  GList *l;
  guint32 key;
  gpointer k;

  aware = aware_find(srvc, idb);
  g_return_if_fail(aware != NULL);

  key = attrib->key;
  k = GUINT_TO_POINTER(key);

  if(aware->attribs)
    old_attrib = g_hash_table_lookup(aware->attribs, k);

  if(! old_attrib) {
    old_attrib = g_new0(struct mwAwareAttribute, 1);
    old_attrib->key = key;
    g_hash_table_insert(aware->attribs, k, old_attrib);
  }
  
  mwOpaque_clear(&old_attrib->data);
  mwOpaque_clone(&old_attrib->data, &attrib->data);
  
  for(l = aware->membership; l; l = l->next) {
    struct mwAwareList *list = l->data;
    struct mwAwareListHandler *h = list->handler;

    if(h && h->on_attrib &&
       list->attribs && g_hash_table_lookup(list->attribs, k))

      h->on_attrib(list, idb, old_attrib);
  }
}


static gboolean list_add(struct mwAwareList *list,
			 struct mwAwareIdBlock *id) {

  struct mwServiceAware *srvc = list->service;
  struct aware_entry *aware;

  g_return_val_if_fail(id->user != NULL, FALSE);
  g_return_val_if_fail(strlen(id->user) > 0, FALSE);

  if(! list->entries)
    list->entries = g_hash_table_new((GHashFunc) mwAwareIdBlock_hash,
				     (GEqualFunc) mwAwareIdBlock_equal);

  aware = list_aware_find(list, id);
  if(aware) return FALSE;

  aware = aware_find(srvc, id);
  if(! aware) {
    aware = g_new0(struct aware_entry, 1);
    aware->attribs = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL,
					   (GDestroyNotify) attrib_free);
    mwAwareIdBlock_clone(ENTRY_KEY(aware), id);

    g_hash_table_insert(srvc->entries, ENTRY_KEY(aware), aware);
  }

  aware->membership = g_list_append(aware->membership, list);

  g_hash_table_insert(list->entries, ENTRY_KEY(aware), aware);

  return TRUE;
}


static void group_member_recv(struct mwServiceAware *srvc,
			      struct mwAwareSnapshot *idb) {
  /* @todo
     - look up group by id
     - find each list group belongs to
     - add user to lists
  */

  struct mwAwareIdBlock gsrch = { mwAware_GROUP, idb->group, NULL };
  struct aware_entry *grp;
  GList *l, *m;

  grp = aware_find(srvc, &gsrch);
  g_return_if_fail(grp != NULL); /* this could happen, with timing. */

  l = g_list_prepend(NULL, &idb->id);

  for(m = grp->membership; m; m = m->next) {

    /* if we just list_add, we won't receive updates for attributes,
       so annoyingly we have to turn around and send out an add aware
       message for each incoming group member */

    /* list_add(m->data, &idb->id); */
    mwAwareList_addAware(m->data, l);
  }

  g_list_free(l);
}


static void recv_SNAPSHOT(struct mwServiceAware *srvc,
			  struct mwGetBuffer *b) {

  guint32 count;

  struct mwAwareSnapshot *snap;
  snap = g_new0(struct mwAwareSnapshot, 1);

  guint32_get(b, &count);

  while(count--) {
    mwAwareSnapshot_get(b, snap);

    if(mwGetBuffer_error(b)) {
      mwAwareSnapshot_clear(snap);
      break;
    }

    if(snap->group)
      group_member_recv(srvc, snap);

    status_recv(srvc, snap);
    mwAwareSnapshot_clear(snap);
  }

  g_free(snap);
}


static void recv_UPDATE(struct mwServiceAware *srvc,
			struct mwGetBuffer *b) {

  struct mwAwareSnapshot *snap;

  snap = g_new0(struct mwAwareSnapshot, 1);
  mwAwareSnapshot_get(b, snap);

  if(snap->group)
    group_member_recv(srvc, snap);

  if(! mwGetBuffer_error(b))
    status_recv(srvc, snap);

  mwAwareSnapshot_clear(snap);
  g_free(snap);
}


static void recv_GROUP(struct mwServiceAware *srvc,
		       struct mwGetBuffer *b) {

  struct mwAwareIdBlock idb = { 0, 0, 0 };

  /* really nothing to be done with this. The group should have
     already been added to the list and service, and is now simply
     awaiting a snapshot/update with users listed as belonging in said
     group. */

  mwAwareIdBlock_get(b, &idb);
  mwAwareIdBlock_clear(&idb);
}


static void recv_OPT_GOT_SET(struct mwServiceAware *srvc,
			     struct mwGetBuffer *b) {

  struct mwAwareAttribute attrib;
  struct mwAwareIdBlock idb;
  guint32 junk, check;

  guint32_get(b, &junk);
  mwAwareIdBlock_get(b, &idb);
  guint32_get(b, &junk);
  guint32_get(b, &check);
  guint32_get(b, &junk);
  guint32_get(b, &attrib.key);

  if(check) {
    mwOpaque_get(b, &attrib.data);
  } else {
    attrib.data.len = 0;
    attrib.data.data = NULL;
  }

  attrib_recv(srvc, &idb, &attrib);

  mwAwareIdBlock_clear(&idb);
  mwOpaque_clear(&attrib.data);
}


static void recv_OPT_GOT_UNSET(struct mwServiceAware *srvc,
			       struct mwGetBuffer *b) {

  struct mwAwareAttribute attrib;
  struct mwAwareIdBlock idb;
  guint32 junk;

  attrib.key = 0;
  attrib.data.len = 0;
  attrib.data.data = NULL;

  guint32_get(b, &junk);
  mwAwareIdBlock_get(b, &idb);
  guint32_get(b, &attrib.key);

  attrib_recv(srvc, &idb, &attrib);

  mwAwareIdBlock_clear(&idb);
}


static void recv(struct mwService *srvc, struct mwChannel *chan,
		 guint16 type, struct mwOpaque *data) {

  struct mwServiceAware *srvc_aware = (struct mwServiceAware *) srvc;
  struct mwGetBuffer *b;

  g_return_if_fail(srvc_aware->channel == chan);
  g_return_if_fail(srvc->session == mwChannel_getSession(chan));
  g_return_if_fail(data != NULL);

  b = mwGetBuffer_wrap(data);

  switch(type) {
  case msg_AWARE_SNAPSHOT:
    recv_SNAPSHOT(srvc_aware, b);
    break;

  case msg_AWARE_UPDATE:
    recv_UPDATE(srvc_aware, b);
    break;

  case msg_AWARE_GROUP:
    recv_GROUP(srvc_aware, b);
    break;

  case msg_OPT_GOT_SET:
    recv_OPT_GOT_SET(srvc_aware, b);
    break;

  case msg_OPT_GOT_UNSET:
    recv_OPT_GOT_UNSET(srvc_aware, b);
    break;

  case msg_OPT_GOT_UNKNOWN:
  case msg_OPT_DID_SET:
  case msg_OPT_DID_UNSET:
  case msg_OPT_DID_ERROR:
    break;

  default:
    mw_mailme_opaque(data, "unknown message in aware service: 0x%04x", type);
  }

  mwGetBuffer_free(b);  
}


static void clear(struct mwService *srvc) {
  struct mwServiceAware *srvc_aware = (struct mwServiceAware *) srvc;

  g_return_if_fail(srvc != NULL);

  while(srvc_aware->lists)
    mwAwareList_free( (struct mwAwareList *) srvc_aware->lists->data );

  g_hash_table_destroy(srvc_aware->entries);
  srvc_aware->entries = NULL;

  g_hash_table_destroy(srvc_aware->attribs);
  srvc_aware->attribs = NULL;
}


static const char *name(struct mwService *srvc) {
  return "Presence Awareness";
}


static const char *desc(struct mwService *srvc) {
  return "Buddy list service with support for server-side groups";
}


static struct mwChannel *make_blist(struct mwServiceAware *srvc,
				    struct mwChannelSet *cs) {

  struct mwChannel *chan = mwChannel_newOutgoing(cs);

  mwChannel_setService(chan, MW_SERVICE(srvc));
  mwChannel_setProtoType(chan, 0x00000011);
  mwChannel_setProtoVer(chan, 0x00030005);

  return mwChannel_create(chan)? NULL: chan;
}


static void start(struct mwService *srvc) {
  struct mwServiceAware *srvc_aware;
  struct mwChannel *chan = NULL;

  srvc_aware = (struct mwServiceAware *) srvc;
  chan = make_blist(srvc_aware, mwSession_getChannels(srvc->session));

  if(chan != NULL) {
    srvc_aware->channel = chan;
  } else {
    mwService_stopped(srvc);
  }
}


static void stop(struct mwService *srvc) {
  struct mwServiceAware *srvc_aware;

  srvc_aware = (struct mwServiceAware *) srvc;

  if(srvc_aware->channel) {
    mwChannel_destroy(srvc_aware->channel, ERR_SUCCESS, NULL);
    srvc_aware->channel = NULL;
  }

  mwService_stopped(srvc);
}


struct mwServiceAware *
mwServiceAware_new(struct mwSession *session,
		   struct mwAwareHandler *handler) {

  struct mwService *service;
  struct mwServiceAware *srvc;

  g_return_val_if_fail(session != NULL, NULL);
  g_return_val_if_fail(handler != NULL, NULL);

  srvc = g_new0(struct mwServiceAware, 1);
  srvc->handler = handler;
  srvc->entries = g_hash_table_new_full((GHashFunc) mwAwareIdBlock_hash,
					(GEqualFunc) mwAwareIdBlock_equal,
					NULL,
					(GDestroyNotify) aware_entry_free);

  srvc->attribs = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL,
					(GDestroyNotify) attrib_entry_free);

  service = MW_SERVICE(srvc);
  mwService_init(service, session, mwService_AWARE);

  service->recv_accept = (mwService_funcRecvAccept) recv_accept;
  service->recv_destroy = (mwService_funcRecvDestroy) recv_destroy;
  service->recv = recv;
  service->start = start;
  service->stop = stop;
  service->clear = clear;
  service->get_name = name;
  service->get_desc = desc;

  return srvc;
}


int mwServiceAware_setAttribute(struct mwServiceAware *srvc,
				guint32 key, struct mwOpaque *data) {
  struct mwPutBuffer *b;
  struct mwOpaque o;
  int ret;

  b = mwPutBuffer_new();

  guint32_put(b, 0x00);
  guint32_put(b, data->len);
  guint32_put(b, 0x00);
  guint32_put(b, key);
  mwOpaque_put(b, data);

  mwPutBuffer_finalize(&o, b);
  ret = mwChannel_send(srvc->channel, msg_OPT_DO_SET, &o);
  mwOpaque_clear(&o);

  return ret;
}


int mwServiceAware_setAttributeBoolean(struct mwServiceAware *srvc,
				       guint32 key, gboolean val) {
  int ret;
  struct mwPutBuffer *b;
  struct mwOpaque o;
 
  b = mwPutBuffer_new();

  gboolean_put(b, FALSE);
  gboolean_put(b, val);

  mwPutBuffer_finalize(&o, b);

  ret = mwServiceAware_setAttribute(srvc, key, &o);
  mwOpaque_clear(&o);

  return ret;
}


int mwServiceAware_setAttributeInteger(struct mwServiceAware *srvc,
				       guint32 key, guint32 val) {
  int ret;
  struct mwPutBuffer *b;
  struct mwOpaque o;
  
  b = mwPutBuffer_new();
  guint32_put(b, val);

  mwPutBuffer_finalize(&o, b);

  ret = mwServiceAware_setAttribute(srvc, key, &o);
  mwOpaque_clear(&o);

  return ret;
}


int mwServiceAware_setAttributeString(struct mwServiceAware *srvc,
				      guint32 key, const char *str) {
  int ret;
  struct mwPutBuffer *b;
  struct mwOpaque o;

  b = mwPutBuffer_new();
  mwString_put(b, str);

  mwPutBuffer_finalize(&o, b);

  ret = mwServiceAware_setAttribute(srvc, key, &o);
  mwOpaque_clear(&o);

  return ret;
}


int mwServiceAware_unsetAttribute(struct mwServiceAware *srvc,
				  guint32 key) {
  struct mwPutBuffer *b;
  struct mwOpaque o;
  int ret;

  b = mwPutBuffer_new();

  guint32_put(b, 0x00);
  guint32_put(b, key);
  
  mwPutBuffer_finalize(&o, b);
  ret = mwChannel_send(srvc->channel, msg_OPT_DO_UNSET, &o);
  mwOpaque_clear(&o);

  return ret;
}


guint32 mwAwareAttribute_getKey(const struct mwAwareAttribute *attrib) {
  g_return_val_if_fail(attrib != NULL, 0x00);
  return attrib->key;
}


gboolean mwAwareAttribute_asBoolean(const struct mwAwareAttribute *attrib) {
  struct mwGetBuffer *b;
  gboolean ret;
  
  if(! attrib) return FALSE;

  b = mwGetBuffer_wrap(&attrib->data);
  if(attrib->data.len >= 4) {
    guint32 r32 = 0x00;
    guint32_get(b, &r32);
    ret = !! r32;

  } else if(attrib->data.len >= 2) {
    guint16 r16 = 0x00;
    guint16_get(b, &r16);
    ret = !! r16;

  } else if(attrib->data.len) {
    gboolean_get(b, &ret);
  }

  mwGetBuffer_free(b);

  return ret;
}


guint32 mwAwareAttribute_asInteger(const struct mwAwareAttribute *attrib) {
  struct mwGetBuffer *b;
  guint32 r32 = 0x00;
  
  if(! attrib) return 0x00;

  b = mwGetBuffer_wrap(&attrib->data);
  if(attrib->data.len >= 4) {
    guint32_get(b, &r32);

  } else if(attrib->data.len == 3) {
    gboolean rb = FALSE;
    guint16 r16 = 0x00;
    gboolean_get(b, &rb);
    guint16_get(b, &r16);
    r32 = (guint32) r16;

  } else if(attrib->data.len == 2) {
    guint16 r16 = 0x00;
    guint16_get(b, &r16);
    r32 = (guint32) r16;

  } else if(attrib->data.len) {
    gboolean rb = FALSE;
    gboolean_get(b, &rb);
    r32 = (guint32) rb;
  }

  mwGetBuffer_free(b);

  return r32;
}


char *mwAwareAttribute_asString(const struct mwAwareAttribute *attrib) {
  struct mwGetBuffer *b;
  char *ret = NULL;

  if(! attrib) return NULL;

  b = mwGetBuffer_wrap(&attrib->data);
  mwString_get(b, &ret);
  mwGetBuffer_free(b);

  return ret;
}


const struct mwOpaque *
mwAwareAttribute_asOpaque(const struct mwAwareAttribute *attrib) {
  g_return_val_if_fail(attrib != NULL, NULL);
  return &attrib->data;
}
			  

struct mwAwareList *
mwAwareList_new(struct mwServiceAware *srvc,
		struct mwAwareListHandler *handler) {

  struct mwAwareList *al;

  g_return_val_if_fail(srvc != NULL, NULL);
  g_return_val_if_fail(handler != NULL, NULL);

  al = g_new0(struct mwAwareList, 1);
  al->service = srvc;
  al->handler = handler;

  srvc->lists = g_list_prepend(srvc->lists, al);

  return al;
}


void mwAwareList_free(struct mwAwareList *list) {
  struct mwServiceAware *srvc;
  struct mwAwareListHandler *handler;

  g_return_if_fail(list != NULL);
  g_return_if_fail(list->service != NULL);

  srvc = list->service;
  srvc->lists = g_list_remove_all(srvc->lists, list);

  handler = list->handler;
  if(handler && handler->clear) {
    handler->clear(list);
    list->handler = NULL;
  }

  mw_datum_clear(&list->client_data);

  mwAwareList_unwatchAllAttributes(list);
  mwAwareList_removeAllAware(list);

  list->service = NULL;

  g_free(list);
}


struct mwAwareListHandler *mwAwareList_getHandler(struct mwAwareList *list) {
  g_return_val_if_fail(list != NULL, NULL);
  return list->handler;
}


static void watch_add(struct mwAwareList *list, guint32 key) {
  struct mwServiceAware *srvc;
  struct attrib_entry *watch;
  gpointer k = GUINT_TO_POINTER(key);

  if(! list->attribs)
    list->attribs = g_hash_table_new(g_direct_hash, g_direct_equal);

  if(g_hash_table_lookup(list->attribs, k))
    return;

  srvc = list->service;

  watch = g_hash_table_lookup(srvc->attribs, k);
  if(! watch) {
    watch = g_new0(struct attrib_entry, 1);
    watch->key = key;
    g_hash_table_insert(srvc->attribs, k, watch);
  }

  g_hash_table_insert(list->attribs, k, watch);

  watch->membership = g_list_prepend(watch->membership, list);
}


static void watch_remove(struct mwAwareList *list, guint32 key) {
  struct attrib_entry *watch = NULL;
  gpointer k = GUINT_TO_POINTER(key);

  if(list->attribs)
    watch = g_hash_table_lookup(list->attribs, k);

  g_return_if_fail(watch != NULL);

  g_hash_table_remove(list->attribs, k);
  watch->membership = g_list_remove(watch->membership, list);
}


int mwAwareList_watchAttributeArray(struct mwAwareList *list,
				    guint32 *keys) {
  guint32 k;

  g_return_val_if_fail(list != NULL, -1);
  g_return_val_if_fail(list->service != NULL, -1);

  if(! keys) return 0;

  for(k = *keys; k; keys++)
    watch_add(list, k);

  return send_attrib_list(list->service);
}


int mwAwareList_watchAttributes(struct mwAwareList *list,
				guint32 key, ...) {
  guint32 k;
  va_list args;

  g_return_val_if_fail(list != NULL, -1);
  g_return_val_if_fail(list->service != NULL, -1);

  va_start(args, key);
  for(k = key; k; k = va_arg(args, guint32))
    watch_add(list, k);
  va_end(args);

  return send_attrib_list(list->service);
}


int mwAwareList_unwatchAttributeArray(struct mwAwareList *list,
				      guint32 *keys) {
  guint32 k;

  g_return_val_if_fail(list != NULL, -1);
  g_return_val_if_fail(list->service != NULL, -1);

  if(! keys) return 0;

  for(k = *keys; k; keys++)
    watch_add(list, k);

  return remove_unused_attrib(list->service);
}


int mwAwareList_unwatchAttributes(struct mwAwareList *list,
				  guint32 key, ...) {
  guint32 k;
  va_list args;

  g_return_val_if_fail(list != NULL, -1);
  g_return_val_if_fail(list->service != NULL, -1);

  va_start(args, key);
  for(k = key; k; k = va_arg(args, guint32))
    watch_remove(list, k);
  va_end(args);

  return remove_unused_attrib(list->service);
}


static void dismember_attrib(gpointer k, struct attrib_entry *watch,
			    struct mwAwareList *list) {

  watch->membership = g_list_remove(watch->membership, list);
}


int mwAwareList_unwatchAllAttributes(struct mwAwareList *list) {
  
  struct mwServiceAware *srvc;

  g_return_val_if_fail(list != NULL, -1);
  srvc = list->service;

  if(list->attribs) {
    g_hash_table_foreach(list->attribs, (GHFunc) dismember_attrib, list);
    g_hash_table_destroy(list->attribs);
  }

  return remove_unused_attrib(srvc);
}


static void collect_attrib_keys(gpointer key, struct attrib_entry *attrib,
				guint32 **ck) {
  guint32 *keys = (*ck)++;
  *keys = GPOINTER_TO_UINT(key);
}


guint32 *mwAwareList_getWatchedAttributes(struct mwAwareList *list) {
  guint32 *keys, **ck;
  guint count;

  g_return_val_if_fail(list != NULL, NULL);
  g_return_val_if_fail(list->attribs != NULL, NULL);
  
  count = g_hash_table_size(list->attribs);
  keys = g_new0(guint32, count + 1);

  ck = &keys;
  g_hash_table_foreach(list->attribs, (GHFunc) collect_attrib_keys, ck);

  return keys;
}


int mwAwareList_addAware(struct mwAwareList *list, GList *id_list) {

  /* for each awareness id:
     - if it's already in the list, continue
     - if it's not in the service list:
       - create an awareness
       - add it to the service list
     - add this list to the membership
     - add to the list
  */

  struct mwServiceAware *srvc;
  GList *additions = NULL;
  int ret = 0;

  g_return_val_if_fail(list != NULL, -1);

  srvc = list->service;
  g_return_val_if_fail(srvc != NULL, -1);

  for(; id_list; id_list = id_list->next) {
    if(list_add(list, id_list->data))
      additions = g_list_prepend(additions, id_list->data);
  }

  /* if the service is alive-- or getting there-- we'll need to send
     these additions upstream */
  if(MW_SERVICE_IS_LIVE(srvc) && additions)
    ret = send_add(srvc->channel, additions);

  g_list_free(additions);
  return ret;
}


int mwAwareList_removeAware(struct mwAwareList *list, GList *id_list) {

  /* for each awareness id:
     - if it's not in the list, forget it
     - remove from the list
     - remove list from the membership

     - call remove round
  */

  struct mwServiceAware *srvc;
  struct mwAwareIdBlock *id;
  struct aware_entry *aware;

  g_return_val_if_fail(list != NULL, -1);

  srvc = list->service;
  g_return_val_if_fail(srvc != NULL, -1);

  for(; id_list; id_list = id_list->next) {
    id = id_list->data;
    aware = list_aware_find(list, id);

    if(! aware) {
      g_warning("buddy %s, %s not in list",
		NSTR(id->user),
		NSTR(id->community));
      continue;
    }

    aware->membership = g_list_remove(aware->membership, list);
    g_hash_table_remove(list->entries, id);
  }

  return remove_unused(srvc);
}


static void dismember_aware(gpointer k, struct aware_entry *aware,
			    struct mwAwareList *list) {

  aware->membership = g_list_remove(aware->membership, list);
}


int mwAwareList_removeAllAware(struct mwAwareList *list) {
  struct mwServiceAware *srvc;

  g_return_val_if_fail(list != NULL, -1);
  srvc = list->service;

  g_return_val_if_fail(srvc != NULL, -1);

  /* for each entry, remove the aware list from the service entry's
     membership collection */
  if(list->entries) {
    g_hash_table_foreach(list->entries, (GHFunc) dismember_aware, list);
    g_hash_table_destroy(list->entries);
  }

  return remove_unused(srvc);
}


void mwAwareList_setClientData(struct mwAwareList *list,
			       gpointer data, GDestroyNotify clear) {

  g_return_if_fail(list != NULL);
  mw_datum_set(&list->client_data, data, clear);
}


gpointer mwAwareList_getClientData(struct mwAwareList *list) {
  g_return_val_if_fail(list != NULL, NULL);
  return mw_datum_get(&list->client_data);
}


void mwAwareList_removeClientData(struct mwAwareList *list) {
  g_return_if_fail(list != NULL);
  mw_datum_clear(&list->client_data);
}


void mwServiceAware_setStatus(struct mwServiceAware *srvc,
			      struct mwAwareIdBlock *user,
			      struct mwUserStatus *stat) {

  struct mwAwareSnapshot idb;

  g_return_if_fail(srvc != NULL);
  g_return_if_fail(user != NULL);
  g_return_if_fail(stat != NULL);

  /* just reference the strings. then we don't need to free them */
  idb.id.type = user->type;
  idb.id.user = user->user;
  idb.id.community = user->community;

  idb.group = NULL;
  idb.online = TRUE;
  idb.alt_id = NULL;

  idb.status.status = stat->status;
  idb.status.time = stat->time;
  idb.status.desc = stat->desc;

  idb.name = NULL;

  status_recv(srvc, &idb);
}


const struct mwAwareAttribute *
mwServiceAware_getAttribute(struct mwServiceAware *srvc,
			    struct mwAwareIdBlock *user,
			    guint32 key) {

  struct aware_entry *aware;

  g_return_val_if_fail(srvc != NULL, NULL);
  g_return_val_if_fail(user != NULL, NULL);
  g_return_val_if_fail(key != 0x00, NULL);

  aware = aware_find(srvc, user);
  g_return_val_if_fail(aware != NULL, NULL);

  return g_hash_table_lookup(aware->attribs, GUINT_TO_POINTER(key));
}


const char *mwServiceAware_getText(struct mwServiceAware *srvc,
				   struct mwAwareIdBlock *user) {

  struct aware_entry *aware;

  g_return_val_if_fail(srvc != NULL, NULL);
  g_return_val_if_fail(user != NULL, NULL);

  aware = aware_find(srvc, user);
  if(! aware) return NULL;

  return aware->aware.status.desc;
}



void mwAwareIdBlock_put(MwPutBuffer *b,
			const MwAwareIdBlock *idb) {

  g_return_if_fail(b != NULL);
  g_return_if_fail(idb != NULL);

  mw_uint16_put(b, idb->type);
  mw_str_put(b, idb->user);
  mw_str_put(b, idb->community);
}


void mwAwareIdBlock_get(MwGetBuffer *b, MwAwareIdBlock *idb) {
  g_return_if_fail(b != NULL);
  g_return_if_fail(idb != NULL);

  if(b->error) return;

  mw_uint16_get(b, &idb->type);
  mw_str_get(b, &idb->user);
  mw_str_get(b, &idb->community);
}


void mwAwareIdBlock_clone(MwAwareIdBlock *to,
			  const MwAwareIdBlock *from) {

  g_return_if_fail(to != NULL);
  g_return_if_fail(from != NULL);

  to->type = from->type;
  to->user = g_strdup(from->user);
  to->community = g_strdup(from->community);
}


void mwAwareIdBlock_clear(MwAwareIdBlock *idb) {
  if(! idb) return;
  g_free(idb->user);
  g_free(idb->community);
  memset(idb, 0x00, sizeof(MwAwareIdBlock));
}


guint mwAwareIdBlock_hash(const MwAwareIdBlock *a) {
  return (a)? g_str_hash(a->user): 0;
}


gboolean mwAwareIdBlock_equal(const MwAwareIdBlock *a,
			      const MwAwareIdBlock *b) {
  
  return ( (a == b) ||
	   (a & b &
	    (a->type == b->type) &&
	    (mw_str_equal(a->user, b->user)) &&
	    (mw_str_equal(a->community, b->community))) );
}


/* 8.4.2.4 Snapshot */

void mwAwareSnapshot_get(MwGetBuffer *b, MwAwareSnapshot *idb) {
  guint32 junk;
  char *empty = NULL;

  g_return_if_fail(b != NULL);
  g_return_if_fail(idb != NULL);

  mw_uint32_get(b, &junk);
  mwAwareIdBlock_get(b, &idb->id);
  mw_str_get(b, &idb->group);
  mw_boolean_get(b, &idb->online);

  g_free(empty);

  if(idb->online) {
    mw_str_get(b, &idb->alt_id);
    mwStatus_get(b, &idb->status);
    mw_str_get(b, &idb->name);
  }
}


void mwAwareSnapshot_clone(MwAwareSnapshot *to,
			   const MwAwareSnapshot *from) {

  g_return_if_fail(to != NULL);
  g_return_if_fail(from != NULL);

  mwAwareIdBlock_clone(&to->id, &from->id);
  if( (to->online = from->online) ) {
    to->alt_id = g_strdup(from->alt_id);
    mwStatus_clone(&to->status, &from->status);
    to->name = g_strdup(from->name);
    to->group = g_strdup(from->group);
  }
}


void mwAwareSnapshot_clear(MwAwareSnapshot *idb) {
  if(! idb) return;
  mwAwareIdBlock_clear(&idb->id);
  mwStatus_clear(&idb->status);
  g_free(idb->alt_id);
  g_free(idb->name);
  g_free(idb->group);
  memset(idb, 0x00, sizeof(MwAwareSnapshot));
}

