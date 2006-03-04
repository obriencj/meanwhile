/*
  Meanwhile - Unofficial Lotus Sametime Community Client Library
  Copyright (C) 2004-2006  Christopher O'Brien  <siege@preoccupied.net>
  
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
#include "mw_config.h"
#include "mw_debug.h"
#include "mw_object.h"


static GObjectClass *parent_class;


gpointer mw_gobject_ref(gpointer data) {
  if(data) {
    g_object_ref(G_OBJECT(data));
  
#if DEBUG
    g_debug("mw_gobject_ref %p to %u", data, mw_gobject_refcount(data));
#endif
  }
  
  return data;
}


gpointer mw_gobject_unref(gpointer data) {
  if(data) {

#if DEBUG
    g_debug("mw_gobject_unref %p from %u", data, mw_gobject_refcount(data));
#endif

    g_object_unref(G_OBJECT(data));
  }
  
  return data;
}


guint mw_gobject_refcount(gpointer gdata) {
  GObject *gobj;

  g_return_val_if_fail(gdata != NULL, 0);
  gobj = G_OBJECT(gdata);

  return (gobj? gobj->ref_count: 0);
}


void mw_gobject_set_weak_pointer(gpointer obj, gpointer *weak) {
  g_return_if_fail(weak != NULL);

  if(*weak) {
    g_object_remove_weak_pointer(*weak, weak);
    *weak = NULL;
  }

  if(obj) {
    GObject *gobj = G_OBJECT(obj);
    g_object_add_weak_pointer(gobj, weak);
    *weak = obj;
  }
}


void mw_prop_boolean(GObjectClass *gclass,
		     guint property_id,
		     const gchar *name, const gchar *blurb,
		     GParamFlags flags) {
  GParamSpec *gps;
  gps = g_param_spec_boolean(name, NULL, blurb, FALSE, flags);
  g_object_class_install_property(gclass, property_id, gps);
}


void mw_prop_str(GObjectClass *gclass,
		 guint property_id,
		 const gchar *name, const gchar *blurb,
		 GParamFlags flags) {
  GParamSpec *gps;
  gps = g_param_spec_string(name, NULL, blurb, NULL, flags);
  g_object_class_install_property(gclass, property_id, gps);
}


void mw_prop_boxed(GObjectClass *gclass,
		   guint property_id,
		   const gchar *name, const gchar *blurb,
		   GType type,
		   GParamFlags flags) {
  GParamSpec *gps;
  gps = g_param_spec_boxed(name, NULL, blurb, type, flags);
  g_object_class_install_property(gclass, property_id, gps);
}


void mw_prop_obj(GObjectClass *gclass,
		 guint property_id,
		 const gchar *name, const gchar *blurb,
		 GType type,
		 GParamFlags flags) {
  GParamSpec *gps;
  gps = g_param_spec_object(name, NULL, blurb, type, flags);
  g_object_class_install_property(gclass, property_id, gps);
}


void mw_prop_ptr(GObjectClass *gclass,
		 guint property_id,
		 const gchar *name, const gchar *blurb,
		 GParamFlags flags) {
  GParamSpec *gps;
  gps = g_param_spec_pointer(name, NULL, blurb, flags);
  g_object_class_install_property(gclass, property_id, gps);
}


void mw_prop_uint(GObjectClass *gclass,
		  guint property_id,
		  const gchar *name, const gchar *blurb,
		  GParamFlags flags) {
  GParamSpec *gps;
  gps = g_param_spec_uint(name, NULL, blurb, 0, G_MAXUINT, 0, flags);
  g_object_class_install_property(gclass, property_id, gps);
}


static void mw_object_class_init(gpointer g_class,
				 gpointer g_class_data) {

  MwObjectClass *klass = g_class;
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS(klass);
  parent_class = g_type_class_peek_parent(gobject_class);
}


/* MwOject type info */
static const GTypeInfo info = {
  .class_size = sizeof(struct mw_object_class),
  .base_init = NULL,
  .base_finalize = NULL,
  .class_init = mw_object_class_init,
  .class_finalize = NULL,
  .class_data = NULL,
  .instance_size = sizeof(struct mw_object),
  .n_preallocs = 0,
  .instance_init = NULL,
  .value_table = NULL,
};


GType MwObject_getType() {
  static GType type = 0;

  if(type == 0) {
    type = g_type_register_static(G_TYPE_OBJECT, "MwObjectType", &info, 0);
  }

  return type;
}


/* The end. */
