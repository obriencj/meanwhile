#ifndef _MW_OBJECT_H
#define _MW_OBJECT_H

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


/** @file mw_object.h
    @since 2.0.0
    
    Base class for object types defined in Meanwhile
*/


#include <glib.h>
#include <glib-object.h>

#include "mw_typedef.h"


G_BEGIN_DECLS


#define MW_TYPE_OBJECT  (MwObject_getType())


#define MW_OBJECT(obj)						\
  (G_TYPE_CHECK_INSTANCE_CAST((obj), MW_TYPE_OBJECT, MwObject))


#define MW_OBJECT_CLASS(klass)						\
  (G_TYPE_CHECK_CLASS_CAST((klass), MW_TYPE_OBJECT, MwObjectClass))


#define MW_IS_OBJECT(obj)				\
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), MW_TYPE_OBJECT))


#define MW_IS_OBJECT_CLASS(klass)			\
  (G_TYPE_CHECK_CLASS_TYPE((klass), MW_TYPE_OBJECT))


#define MW_OBJECT_GET_CLASS(obj)					\
  (G_TYPE_INSTANCE_GET_CLASS((obj), MW_TYPE_OBJECT, MwObjectClass))



struct mw_object {
  GObject gobject;

  gpointer user_data;
  GDestroyNotify user_data_clear;
};


typedef struct mw_object_class MwObjectClass;


struct mw_object_class {
  GObjectClass gobjectclass;
};


/** @since 2.0.0 */
GType MwObject_getType();


/**
   @section GObject utility additions
   
   A few utility functions to debug reference counting, and to work
   with weak references.
*/
/* @{ */


/** Calls g_object_ref on obj. Will print a g_debug of the reference
    count of obj after incrementing if DEBUG was set during library
    compilation

    @return @p obj
*/
gpointer mw_gobject_ref(gpointer obj);


/** Calls g_object_unref on obj. Will print a g_debug of the reference
    count of obj before decrementing if DEBUG was set during library
    compilation

    @return @p obj
*/
gpointer mw_gobject_unref(gpointer obj);


/** The reference count of a GObject */
guint mw_gobject_refcount(gpointer obj);


/** calls g_object_remove_weak_pointer on @p where if it is already
    set, then calls g_object_add_weak_pointer, and sets @p where to @p
    obj */
void mw_gobject_set_weak_pointer(gpointer obj, gpointer *where);


/** calls g_object_remove_weak_pointer on @p where and sets it to
    NULL */
void mw_gobject_clear_weak_pointer(gpointer *where);


/* @} */


/** @section Type Building Utility Functions */
/* @{ */


/** @since 2.0.0

    add a gboolean property to a GObjectClass */
void mw_prop_boolean(GObjectClass *gclass,
		     guint property_id,
		     const char *name, const char *blurb,
		     GParamFlags flags);


/** @since 2.0.0

    add a string property to a GObjectClass */
void mw_prop_str(GObjectClass *gclass,
		 guint property_id,
		 const char *name, const char *blurb,
		 GParamFlags flags);


/** @since 2.0.0

    add a gpointer property to a GObjectClass */
void mw_prop_ptr(GObjectClass *gclass,
		 guint property_id,
		 const char *name, const char *blurb,
		 GParamFlags flags);


/** @since 2.0.0

    add a reference to GObject to a GObjectClass */
void mw_prop_obj(GObjectClass *gclass,
		 guint property_id,
		 const char *name, const char *blurb,
		 GType type,
		 GParamFlags flags);


/** @since 2.0.0

    add a guint property to a GObjectClass */
void mw_prop_uint(GObjectClass *gclass,
		  guint property_id,
		  const char *name, const char *blurb,
		  GParamFlags flags);


/** @since 2.0.0

    add a boxed type property to a GObjectClass */
void mw_prop_boxed(GObjectClass *gclass,
		   guint property_id,
		   const char *name, const char *blurb,
		   GType type,
		   GParamFlags flags);


/* @} */


G_END_DECLS


#endif /* _MW_OBJECT_H */
