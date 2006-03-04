#ifndef _MW_SERVICE_H
#define _MW_SERVICE_H


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


/**
   @file mw_service.h
*/


#include <glib.h>
#include "mw_object.h"
#include "mw_typedef.h"


G_BEGIN_DECLS


#define MW_TYPE_SERVICE  (MwService_getType())


#define MW_SERVICE(obj)							\
  (G_TYPE_CHECK_INSTANCE_CAST((obj), MW_TYPE_SERVICE, MwService))


#define MW_SERVICE_CLASS(klass)						\
  (G_TYPE_CHECK_CLASS_CAST((klass), MW_TYPE_SERVICE, MwServiceClass))


#define MW_IS_SERVICE(obj)				\
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), MW_TYPE_SERVICE))


#define MW_IS_SERVICE_CLASS(klass)			\
  (G_TYPE_CHECK_CLASS_TYPE((klass), MW_TYPE_SERVICE))


#define MW_SERVICE_GET_CLASS(obj)					\
  (G_TYPE_INSTANCE_GET_CLASS((obj), MW_TYPE_SERVICE, MwServiceClass))


typedef struct mw_service_private MwServicePrivate;


struct mw_service_private;


struct mw_service {
  MwObject mwobject;
  
  MwServicePrivate *private;
};


typedef struct mw_service_class MwServiceClass;


struct mw_service_class {
  MwObjectClass mwobject_class;

  guint signal_state_changed;
  
  const gchar *(*get_name)(MwService *self);
  const gchar *(*get_desc)(MwService *self);

  void (*start)(MwService *self);
  void (*stop)(MwService *self);
};


GType MwService_getType();


/** State-tracking for a service */
enum mw_service_state {
  mw_service_state_STOPPED,   /**< the service is not active */
  mw_service_State_STARTING,  /**< the service is starting up */
  mw_service_state_STARTED,   /**< the service is active */
  mw_service_state_STOPPING,  /**< the service is shutting down */
  mw_service_state_ERROR,     /**< error in service, shutting down */
  mw_service_state_UNKNOWN,   /**< error determining state */
};


/** @return string short name of the service */
const gchar *MwService_getName(MwService *service);


/** @return string short description of the service */
const gchar *mwService_getDesc(MwService *service);


void MwService_start(MwService *service);


void MwService_stop(MwService *service);


G_END_DECLS


#endif /* _MW_SERVICE_H */
