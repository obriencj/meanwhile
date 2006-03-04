#ifndef _MW_GIOSESSION_H
#define _MW_GIOSESSION_H


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


/** @file mw_giosession.h

    MwGIOSession is a subclass of MwSession that attaches IO to a
    GIOChannel and automatically reads and writes based on signals
    emitted from that channel. This can help to keep a large amount of
    buffering and IO code out of clients built on Meanwhile.

    The channel is monitored for the conditions G_IO_IN and G_IO_OUT.
    When G_IO_IN is emitted, any available data in the channel will be
    fed to the session. When G_IO_OUT is emitted, all queued outgoing
    messages will be sent until a blocking write would occur.
*/


#include <glib/giochannel.h>
#include "mw_session.h"
#include "mw_typedef.h"


#define MW_TYPE_GIOSESSION  (MwGIOSession_getType())


#define MW_GIO_SESSION(obj)						\
  (G_TYPE_CHECK_INSTANCE_CAST((obj), MW_TYPE_GIOSESSION, MwGIOSession))


#define MW_GIO_SESSION_CLASS(klass)					\
  (G_TYPE_CHECK_CLASS_CAST((klass), MW_TYPE_GIOSESSION, MwGIOSessionClass))


#define MW_IS_GIO_SESSION(obj)					\
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), MW_TYPE_GIOSESSION))


#define MW_IS_GIO_SESSION_CLASS(klass)				\
  (G_TYPE_CHECK_CLASS_TYPE((klass), MW_TYPE_GIOSESSION))


#define MW_GIO_SESSION_GET_CLASS(obj)					\
  (G_TYPE_INSTANCE_GET_CLASS((obj), MW_TYPE_GIOSESSION, MwGIOSessionClass))


typedef struct mw_gio_session_private MwGIOSessionPrivate;


struct mw_gio_session_private;


struct mw_gio_session {
  MwSession mwsession;

  MwGIOSessionPrivate *private;
};


typedef struct mw_gio_session_class MwGIOSessionClass;


struct mw_gio_session_class {
  MwSessionClass mwsessionclass;
  
  void (*set_giochannel)(MwGIOSession *self, GIOChannel *chan);
  GIOChannel *(*get_giochannel)(MwGIOSession *self);
};


GType MwGIOSession_getType();


/**
   Instantiate a new MwGIOSessio based on the given GIOChannel
*/
MwGIOSession *MwGIOSession_new(GIOChannel *chan);


/**
   Start a session, then start a new GMainLoop in the default
   context. When the session stops, the new GMainLoop will be quit,
   and this function will return.
*/
void MwGIOSession_main(MwGIOSession *session);


/**
   Stop the session after all pending output has had a chance to be
   written. No new input will be processed.
*/
void MwGIOSession_politeStop(MwGIOSession *session);


/**
   @section GIOChannel Boxed Type

   Provides a boxed GType for GIOChannels.
*/
/* @{ */


#define MW_TYPE_GIOCHANNEL  (MwGIOChannel_getType())


/**
   @since 2.0.0

   Boxed GType for a GIOChannel reference. Will call g_io_channel_ref
   from g_boxed_copy, and g_io_channel_unref from g_boxed_free.
*/
GType MwGIOChannel_getType();


/* @} */


#endif /* _MW_GIOSESSION_H */
