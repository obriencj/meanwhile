#ifndef _MW_SRVC_STORE_H
#define _MW_SRVC_STORE_H


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
   @file mw_srvc_store.h
*/


#include <glib.h>
#include "mw_common.h"


G_BEGIN_DECLS


#define MW_TYPE_STORAGE_KEY_ENUM  (MwStorageKeyEnum_getType())


/**
   A collection of commonly used storage keys. This is obviously not
   an exhaustive list, and just about any 32bit value can be used.
*/
enum mw_storage_key {
  mw_storage_list             = 0x00000000,
  mw_storage_invite_chat      = 0x00000006,
  mw_storage_invite_meeting   = 0x0000000e,
  mw_storage_away_messages    = 0x00000050,
  mw_storage_busy_messages    = 0x0000005a,
  mw_storage_active_messages  = 0x00000064,
};


GType MwStorageKeyEnum_getType();


/**
   The overly complex callback function prototype for storage service
   events.
   
   @param srvc   the owning storage service
   @param event  the event ID as returned by the save/load/watch method
   @param code   the result code, 0x00 for success
   @param key    the storage key
   @param unit   the storage data, empty for save callbacks
   @param data   optional user data passed to the save/load/watch method
*/
typedef void (*MwStorageCallback)
  (MwStorageService *srvc, guint event,
   guint32 code, guint32 key, const MwOpaque *unit,
   gpointer data);


#define MW_STORAGE_SERVICE(obj)						\
  (G_TYPE_CHECK_INSTANCE_CAST((obj), MW_TYPE_STORAGE_SERVICE,		\
			      MwStorageService))


#define MW_STORAGE_SERVICE_CLASS(klass)					\
  (G_TYPE_CHECK_CLASS_CAST((klass), MW_TYPE_STORAGE_SERVICE,		\
			   MwStorageServiceClass))


#define MW_IS_STORAGE_SERVICE(obj)				\
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), MW_TYPE_STORAGE_SERVICE))


#define MW_IS_STORAGE_SERVICE_CLASS(klass)			\
  (G_TYPE_CHECK_CLASS_TYPE((klass), MW_TYPE_STORAGE_SERVICE))


#define MW_STORAGE_SERVICE_GET_CLASS(obj)				\
  (G_TYPE_INSTANCE_GET_CLASS((obj), MW_TYPE_STORAGE_SERVICE,		\
			     MwStorageServiceClass))


typedef struct mw_storage_service_private MwStorageServicePrivate;


struct mw_storage_service_private;


struct mw_storage_service {
  MwService mwservice;

  MwStorageServicePrivate *private;
};


typedef struct mw_storage_service_class MwStorageServiceClass;


struct mw_storage_service_class {
  MwServiceClass mwservice_class;

  guint (*load)(MwStorageService *self, guint32 key,
		MwStorageCallback cb, gpointer user_data);

  guint (*save)(MwStorageService *self,
		guint32 key, const MwOpaque *unit,
		MwStorageCallback cb, gpointer user_data);

  void (*cancel)(MwStorageService *self, guint event);

  guint signal_key_updated;
};


GType MwStorageService_getType();


MwStorageService *MwStorageService_new(MwSession *session);


guint MwStorageService_load(MwStorageService *srvc, guint32 key,
			    MwStorageCallback cb, gpointer user_data);


guint MwStorageService_loadClosure(MwStorageService *srvc, guint32 key,
				   GClosure *closure);


guint MwStorageService_save(MwStorageService *srvc,
			    guint32 key, const MwOpaque *unit,
			    MwStorageCallback cb, gpointer user_data);


guint MwStorageService_saveClosure(MwStorageService *srvc,
				   guint32 key, const MwOpaque *unit,
				   GClosure *closure);


/**
   prevents the triggering of the callback associated with the given
   event id, but does not cancel the actual load or save operation, as
   that will likely have already been sent to the server
*/
void MwStorageService_cancel(MwStorageService *srvc, guint event);


G_END_DECLS


#endif /* _MW_SRVC_STORE_H */
