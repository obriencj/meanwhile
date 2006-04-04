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
  
  const gchar *(*get_name)(MwService *self);
  const gchar *(*get_desc)(MwService *self);

  gboolean (*starting)(MwService *self);
  gboolean (*start)(MwService *self);
  void (*started)(MwService *self);

  gboolean (*stopping)(MwService *self);
  gboolean (*stop)(MwService *self);
  void (*stopped)(MwService *self);
};


GType MwService_getType();


/** @return string short name of the service */
const gchar *MwService_getName(MwService *service);


/** @return string short description of the service */
const gchar *MwService_getDescription(MwService *service);


/**
   Calls service's starting method. If starting returns TRUE, the
   state of the service is set to STARTING, and the start method is
   called. If start returns TRUE, MwService_started is called, setting
   the state to STARTED, otherwise startup is considered deferred,
   and MwService_started must be called later from the service
   implementation when it is ready for use.

   If an error occurs during startup, the implementation should call
   MwService_error.
*/
void MwService_start(MwService *service);


void MwService_stop(MwService *service);


#define MW_TYPE_SERVICE_STATE_ENUM  (MwServiceStateEnum_getType())


enum mw_service_state {
  mw_service_stopped = mw_object_stopped,
  mw_service_starting = mw_object_starting,
  mw_service_started = mw_object_started,
  mw_service_stopping = mw_object_stopping,
  mw_service_error = mw_object_error,
};


GType MwServiceStateEnum_getType();


/*
  @section Session extension functions
  
  These should only be called from service implementations, not from
  client code.

*/
/* @{ */


/**
   utility function for service implementations, to be called when
   they have completed a deferred startup. Called automatically from
   MwService_start if startup is not deferred
*/
void MwService_started(MwService *service);


/**
   utility function for service implementations, to be called when
   they have completed a deferred shutdown. Called automatically from
   MwService_stop if shutdown is not deferred.
*/
void MwService_stopped(MwService *service);


/**
   triggers the mw_service_ERROR state, then calle MwService_stop
*/
void MwService_error(MwService *service);


/* @} */


G_END_DECLS


#endif /* _MW_SERVICE_H */
