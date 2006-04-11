#ifndef _MW_SRVC_AWARE_H
#define _MW_SRVC_AWARE_H


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


/** @file mw_srvc_aware.h

    The aware service...
*/


#include <glib.h>
#include "mw_common.h"
#include "mw_service.h"
#include "mw_session.h"


G_BEGIN_DECLS


#define MW_TYPE_AWARE_TYPE_ENUM  (MwAwareTypeEnum_getType())


/**
   Types of awareness. Either a user, a group, or a server.
*/
enum mw_aware_type {
  mw_aware_user   = 0x0002,
  mw_aware_group  = 0x0003,
  mw_aware_server = 0x0008,
};


GType MwAwareTypeEnum_getType();


#define MW_TYPE_AWARE_ATTRIBUTE_ENUM  (MwAwareAttributeEnum_getType())


/**
   A collection of the most commonly used aware attributes. This is by
   no means an exhaustive list, as any guint32 can be used as an
   attribute key, and clients can define their own attributes at
   run-time.
*/
enum mw_aware_attribute {

  /** A/V prefs have been specified, gboolean */
  mw_aware_attrib_av_prefs_set   = 0x01,
  mw_aware_attrib_microphone     = 0x02, /**< has a microphone, gboolean */
  mw_aware_attrib_speakers       = 0x03, /**< has speakers, gboolean */
  mw_aware_attrib_video_camera   = 0x04, /**< has a video camera, gboolean */

  /** supports file transfers, gboolean */
  mw_aware_attrib_file_transfer  = 0x06,
};


GType MwAwareAttributeEnum_getType();


#define MW_TYPE_AWARE_SERVICE  (MwAwareService_getType())


#define MW_AWARE_SERVICE(obj)						\
  (G_TYPE_CHECK_INSTANCE_CAST((obj), MW_TYPE_AWARE_SERVICE,		\
			      MwAwareService))


#define MW_AWARE_SERVICE_CLASS(klass)					\
  (G_TYPE_CHECK_CLASS_CAST((klass), MW_TYPE_AWARE_SERVICE,		\
			   MwAwareServiceClass))


#define MW_IS_AWARE_SERVICE(obj)				\
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), MW_TYPE_AWARE_SERVICE))


#define MW_IS_AWARE_SERVICE_CLASS(klass)			\
  (G_TYPE_CHECK_CLASS_TYPE((klass), MW_TYPE_AWARE_SERVICE))


#define MW_AWARE_SERVICE_GET_CLASS(obj)					\
  (G_TYPE_INSTANCE_GET_CLASS((obj), MW_TYPE_AWARE_SERVICE,		\
			     MwAwareServiceClass))


struct mw_aware_service {
  MwService mwservice;
};


typedef struct mw_aware_service_class MwAwareServiceClass;


struct mw_aware_service_class {
  MwServiceClass mwservice_class;
  
  MwAware *(*new_aware)(MwAwareService *self, enum mw_aware_type type,
			const gchar *user, const gchar *community);
  MwAware *(*get_aware)(MwAwareService *self, enum mw_aware_type type,
			const gchar *user, const gchar *community);
  MwAware *(*find_aware)(MwAwareService *self, enum mw_aware_type type,
			 const gchar *user, const gchar *community);
  void (*foreach_aware)(MwAwareService *self, GFunc func, gpointer data);

  void (*watch_attrib)(MwAwareService *self, guint32 key);
  void (*unwatch_attrib)(MwAwareService *self, guint32 key);

  void (*set_attrib)(MwAwareService *self, guint32 key, const MwOpaque *val);
  const MwOpaque *(get_attrib)(MwAwareService *self, guint32 key);

  guint signal_attrib_changed;
};


GType MwAwareService_getType();


MwAwareService *MwAwareService_new(MwSession *session);


MwAware *MwAwareService_getAware(MwAwareService *self,
				 enum mw_aware_type type,
				 const gchar *user, const gchar *community);


MwAware *MwAwareService_findAware(MwAwareService *self,
				  enum mw_aware_type type,
				  const gchar *user, const gchar *community);


void MwAwareService_foreachAware(MwAwareService *self,
				 GFunc func, gpointer data);


void MwAwareService_foreachAwareClosure(MwAwareService *self,
					GClosure *closure);


#define MW_TYPE_AWARE  (MwAware_getType())


#define MW_AWARE(obj)						\
  (G_TYPE_CHECK_INSTANCE_CAST((obj), MW_TYPE_AWARE, MwAware))


#define MW_AWARE_CLASS(klass)						\
  (G_TYPE_CHECK_CLASS_CAST((klass), MW_TYPE_AWARE, MwAwareClass))


#define MW_IS_AWARE(obj)				\
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), MW_TYPE_AWARE))


#define MW_IS_AWARE_CLASS(klass)			\
  (G_TYPE_CHECK_CLASS_TYPE((klass), MW_TYPE_AWARE))


#define MW_AWARE_GET_CLASS(obj)						\
  (G_TYPE_INSTANCE_GET_CLASS((obj), MW_TYPE_AWARE, MwAwareClass))


typedef struct mw_aware MwAware;


struct mw_aware {
  MwObject mwobject;
};


typedef struct mw_aware_class MwAwareClass;


struct mw_aware_class {
  MwObjectClass mwobject_class;

  void (*set_attrib)(MwAware *self, guint32 key, const MwOpaque *val);
  const MwOpaque *(*get_attrib)(MwAware *self, guint32 key);

  guint signal_attrib_changed;
};


GType MwAware_getType();


const MwOpaque *MwAware_getAttribute(MwAware *self, guint32 key);


gboolean MwAware_getBoolean(MwAware *self, guint32 key);


#define MW_TYPE_AWARE_LIST  (MwAwareList_getType())


#define MW_AWARE_LIST(obj)						\
  (G_TYPE_CHECK_INSTANCE_CAST((obj), MW_TYPE_AWARE_LIST, MwAwareList))


#define MW_AWARE_LIST_CLASS(klass)					\
  (G_TYPE_CHECK_CLASS_CAST((klass), MW_TYPE_AWARE_LIST, MwAwareListClass))


#define MW_IS_AWARE_LIST(obj)					\
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), MW_TYPE_AWARE_LIST))


#define MW_IS_AWARE_LIST_CLASS(klass)				\
  (G_TYPE_CHECK_CLASS_TYPE((klass), MW_TYPE_AWARE_LIST))


#define MW_AWARE_LIST_GET_CLASS(obj)					\
  (G_TYPE_INSTANCE_GET_CLASS((obj), MW_TYPE_AWARE_LIST, MwAwareListClass))


typedef struct mw_aware_list MwAwareList;


struct mw_aware_list {
  GObject gobject;
};


typedef struct mw_aware_list_class MwAwareListClass;


struct mw_aware_list_class {
  GObjectClass gobject_class;

  gboolean (*add_aware)(MwAwareList *self, MwAware *aware);
  gboolean (*rem_aware)(MwAwareList *self, MwAware *aware);
  void (*foreach_aware)(MwAwareList *self, GFunc func, gpointer data);

  guint signal_aware_changed;
};


MwAwareList *MwAwareList_new();


gboolean MwAwareList_addAware(MwAwareList *self, MwAware *aware);


gboolean MwAwareList_removeAware(MwAwareList *self, MwAware *aware);


void MwAwareList_foreachAware(MwAwareList *self, GFunc func, gpointer data);


void MwAwareList_foreachAwareClosure(MwAwareList *self, GClosure *closure);


G_END_DECLS


#endif /* _MW_SRVC_AWARE_H */


#if 0


/* -- old below -- */


/** Type identifier for the aware service */
#define mwService_AWARE  0x00000011


/** @struct mwServiceAware

    Instance of an Aware Service. The members of this structure are
    not made available. Accessing the parts of an aware service should
    be performed through the appropriate functions. Note that
    instances of this structure can be safely cast to a mwService.
*/
struct mwServiceAware;


/** @struct mwAwareList

    Instance of an Aware List. The members of this structure are not
    made available. Access to the parts of an aware list should be
    handled through the appropriate functions.

    Any references to an aware list are rendered invalid when the
    parent service is free'd
*/
struct mwAwareList;


/** @struct mwAwareAttribute

    Key/Opaque pair indicating an identity's attribute.
 */
struct mwAwareAttribute;


/** Predefined keys appropriate for a mwAwareAttribute
 */
enum mwAwareAttributeKeys {
};


typedef void (*mwAwareAttributeHandler)
     (struct mwServiceAware *srvc,
      struct mwAwareAttribute *attrib);


struct mwAwareHandler {
  mwAwareAttributeHandler on_attrib;
  void (*clear)(struct mwServiceAware *srvc);
};


/** Appropriate function type for the on-aware signal

    @param list  mwAwareList emiting the signal
    @param id    awareness status information
    @param data  user-specified data
*/
typedef void (*mwAwareSnapshotHandler)
     (struct mwAwareList *list,
      struct mwAwareSnapshot *id);


/** Appropriate function type for the on-option signal. The option's
    value may need to be explicitly loaded in some instances,
    resulting in this handler being triggered again.

    @param list    mwAwareList emiting the signal
    @param id      awareness the attribute belongs to
    @param attrib  attribute
*/
typedef void (*mwAwareIdAttributeHandler)
     (struct mwAwareList *list,
      struct mwAwareIdBlock *id,
      struct mwAwareAttribute *attrib);


struct mwAwareListHandler {
  /** handle aware updates */
  mwAwareSnapshotHandler on_aware;

  /** handle attribute updates */
  mwAwareIdAttributeHandler on_attrib;

  /** optional. Called from mwAwareList_free */
  void (*clear)(struct mwAwareList *list);
};


struct mwServiceAware *
mwServiceAware_new(struct mwSession *session,
		   struct mwAwareHandler *handler);


/** Set an attribute value for this session */
int mwServiceAware_setAttribute(struct mwServiceAware *srvc,
				guint32 key, struct mwOpaque *opaque);


int mwServiceAware_setAttributeBoolean(struct mwServiceAware *srvc,
				       guint32 key, gboolean val);


int mwServiceAware_setAttributeInteger(struct mwServiceAware *srvc,
				       guint32 key, guint32 val);


int mwServiceAware_setAttributeString(struct mwServiceAware *srvc,
				      guint32 key, const char *str);


/** Unset an attribute for this session */
int mwServiceAware_unsetAttribute(struct mwServiceAware *srvc,
				  guint32 key);


guint32 mwAwareAttribute_getKey(const struct mwAwareAttribute *attrib);


gboolean mwAwareAttribute_asBoolean(const struct mwAwareAttribute *attrib);


guint32 mwAwareAttribute_asInteger(const struct mwAwareAttribute *attrib);


/** Copy of attribute string, must be g_free'd. If the attribute's
    content cannot be loaded as a string, returns NULL */
char *mwAwareAttribute_asString(const struct mwAwareAttribute *attrib);


/** Direct access to an attribute's underlying opaque */
const struct mwOpaque *
mwAwareAttribute_asOpaque(const struct mwAwareAttribute *attrib);


/** Allocate and initialize an aware list */
struct mwAwareList *
mwAwareList_new(struct mwServiceAware *srvc,
		struct mwAwareListHandler *handler);


/** Clean and free an aware list */
void mwAwareList_free(struct mwAwareList *list);


struct mwAwareListHandler *mwAwareList_getHandler(struct mwAwareList *list);


/** Add a collection of user IDs to an aware list.
    @param list     mwAwareList to add user ID to
    @param id_list  mwAwareIdBlock list of user IDs to add
    @return         0 for success, non-zero to indicate an error.
*/
int mwAwareList_addAware(struct mwAwareList *list, GList *id_list);


/** Remove a collection of user IDs from an aware list.
    @param list     mwAwareList to remove user ID from
    @param id_list  mwAwareIdBlock list of user IDs to remove
    @return  0      for success, non-zero to indicate an error.
*/
int mwAwareList_removeAware(struct mwAwareList *list, GList *id_list);


int mwAwareList_removeAllAware(struct mwAwareList *list);


/** watch an NULL terminated array of keys */
int mwAwareList_watchAttributeArray(struct mwAwareList *list,
				    guint32 *keys);


/** watch a NULL terminated list of keys */
int mwAwareList_watchAttributes(struct mwAwareList *list,
				guint32 key, ...);


/** stop watching a NULL terminated array of keys */
int mwAwareList_unwatchAttributeArray(struct mwAwareList *list,
				      guint32 *keys);


/** stop watching a NULL terminated list of keys */
int mwAwareList_unwatchAttributes(struct mwAwareList *list,
				  guint32 key, ...);


/** remove all watched attributes */
int mwAwareList_unwatchAllAttributes(struct mwAwareList *list);


guint32 *mwAwareList_getWatchedAttributes(struct mwAwareList *list);


void mwAwareList_setClientData(struct mwAwareList *list,
			       gpointer data, GDestroyNotify cleanup);


void mwAwareList_removeClientData(struct mwAwareList *list);


gpointer mwAwareList_getClientData(struct mwAwareList *list);


/** trigger a got_aware event constructed from the passed user and
    status information. Useful for adding false users and having the
    getText function work for them */
void mwServiceAware_setStatus(struct mwServiceAware *srvc,
			      struct mwAwareIdBlock *user,
			      struct mwUserStatus *stat);


/** look up the status description for a user */
const char *mwServiceAware_getText(struct mwServiceAware *srvc,
				   struct mwAwareIdBlock *user);


/** look up the last known copy of an attribute for a user by the
    attribute's key */
const struct mwAwareAttribute *
mwServiceAware_getAttribute(struct mwServiceAware *srvc,
			    struct mwAwareIdBlock *user,
			    guint32 key);

#endif /* 0 */
