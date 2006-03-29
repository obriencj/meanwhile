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
#include "mw_marshal.h"
#include "mw_object.h"


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
  
  return NULL;
}


guint mw_gobject_refcount(gpointer gdata) {
  GObject *gobj;

  g_return_val_if_fail(gdata != NULL, 0);
  gobj = G_OBJECT(gdata);

  return (gobj? gobj->ref_count: 0);
}


void mw_gobject_clear_weak_pointer(gpointer *weak) {
  g_return_if_fail(weak != NULL);

  if(*weak) {
    g_debug("clearing weak pointer at %p to object %p", weak, *weak);
    g_object_remove_weak_pointer(G_OBJECT(*weak), weak);
    *weak = NULL;
  }
}


void mw_gobject_set_weak_pointer(gpointer obj, gpointer *weak) {
  g_return_if_fail(weak != NULL);

  mw_gobject_clear_weak_pointer(weak);

  g_debug("setting weak pointer at %p to object %p", weak, obj);

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
		   GType boxed_type,
		   GParamFlags flags) {
  GParamSpec *gps;
  gps = g_param_spec_boxed(name, NULL, blurb, boxed_type, flags);
  g_object_class_install_property(gclass, property_id, gps);
}


void mw_prop_obj(GObjectClass *gclass,
		 guint property_id,
		 const gchar *name, const gchar *blurb,
		 GType obj_type,
		 GParamFlags flags) {
  GParamSpec *gps;
  gps = g_param_spec_object(name, NULL, blurb, obj_type, flags);
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


void mw_prop_int(GObjectClass *gclass,
		  guint property_id,
		  const gchar *name, const gchar *blurb,
		  GParamFlags flags) {
  GParamSpec *gps;
  gps = g_param_spec_int(name, NULL, blurb, G_MININT, G_MAXINT, 0, flags);
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


void mw_prop_enum(GObjectClass *gclass,
		  guint property_id,
		  const gchar *name, const gchar *blurb,
		  GType enum_type,
		  GParamFlags flags) {
  GParamSpec *gps;
  gps = g_param_spec_enum(name, NULL, blurb, enum_type, 0, flags);
  g_object_class_install_property(gclass, property_id, gps);
}


/* ----- */


enum properties {
  property_state = 1,
};


void MwObjectClass_setStateEnum(MwObjectClass *klass,
				GType enum_type) {
  GEnumClass *new_enum_class;
  GEnumClass *old_enum_class;

  mw_debug_enter();

  g_return_if_fail(klass != NULL);
  new_enum_class = g_type_class_ref(enum_type);

  g_return_if_fail(new_enum_class != NULL);
  g_return_if_fail(G_IS_ENUM_CLASS(new_enum_class));

  /* set the enum class, clear the old enum class */
  old_enum_class = klass->state_spec->enum_class;
  klass->state_spec->enum_class = new_enum_class;
  g_type_class_unref(old_enum_class);

  /* set the spec value type */
  klass->state_spec->parent_instance.value_type = enum_type;

  mw_debug_exit();
}


GEnumClass *MwObjectClass_peekStateEnum(MwObjectClass *klass) {
  g_return_val_if_fail(klass != NULL, NULL);
  g_return_val_if_fail(klass->state_spec != NULL, NULL);
  return klass->state_spec->enum_class;
}


const GEnumValue *
MwObjectClass_getStateValue(MwObjectClass *klass, gint state) {
  GEnumClass *enum_class = MwObjectClass_peekStateEnum(klass);
  g_return_val_if_fail(enum_class != NULL, NULL);
  return g_enum_get_value(enum_class, state);
}


const GEnumValue *
MwObject_getStateValue(MwObject *obj) {
  MwObjectClass *klass;
  gint state;

  g_return_val_if_fail(obj != NULL, NULL);

  klass = MW_OBJECT_GET_CLASS(obj);
  g_object_get(G_OBJECT(obj), "state", &state, NULL);

  return MwObjectClass_getStateValue(klass, state);
}


static void mw_set_property(GObject *object,
			    guint property_id, const GValue *value,
			    GParamSpec *pspec) {

  MwObject *self = MW_OBJECT(object);

  mw_debug_enter();

  switch(property_id) {
  case property_state:
    {
      guint sig;
      self->state = g_value_get_enum(value);
      sig = MW_OBJECT_GET_CLASS(self)->signal_state_changed;
      g_signal_emit(object, sig, 0, self->state, NULL);
    }
    break;

  default:
    ;
  }

  mw_debug_exit();
}


static void mw_get_property(GObject *object,
			    guint property_id, GValue *value,
			    GParamSpec *pspec) {

  MwObject *self = MW_OBJECT(object);

  switch(property_id) {
  case property_state:
    g_value_set_enum(value, self->state);
    break;

  default:
    ;
  }
}


static guint mw_signal_state_changed() {
  return g_signal_new("state-changed",
		      MW_TYPE_OBJECT,
		      0,
		      0,
		      NULL, NULL,
		      mw_marshal_VOID__UINT,
		      G_TYPE_NONE,
		      1,
		      G_TYPE_UINT);
}


static void mw_object_class_init(gpointer g_class,
				 gpointer g_class_data) {

  MwObjectClass *klass = g_class;
  GObjectClass *gobject_class;
  GParamSpec *gps;

  gobject_class = G_OBJECT_CLASS(klass);

  gobject_class->set_property = mw_set_property;
  gobject_class->get_property = mw_get_property;

  /* we store a copy of gps on the class, so we can't just use
     mw_prop_enum, though that's effectively what this is */
  gps = g_param_spec_enum("state", NULL,
			  "set state and emit the state-changed signal",
			  MW_TYPE_OBJECT_STATE_ENUM,
			  0, G_PARAM_READWRITE|G_PARAM_PRIVATE);

  g_object_class_install_property(gobject_class, property_state, gps);

  klass->state_spec = G_PARAM_SPEC_ENUM(gps);

  klass->signal_state_changed = mw_signal_state_changed();
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


static const GEnumValue values[] = {
  { mw_object_stopped,  "mw_object_stopped",  "stopped" },
  { mw_object_starting, "mw_object_starting", "starting" },
  { mw_object_started,  "mw_object_started",  "started" },
  { mw_object_stopping, "mw_object_stopping", "stopping" },
  { mw_object_error,    "mw_object_error",    "error" },
  { 0, NULL, NULL },
};


GType MwObjectStateEnum_getType() {
  static GType type = 0;
   
  if(type == 0) {
    type = g_enum_register_static("MwObjectStateEnumType", values);
  }

  return type;
}


/* The end. */
