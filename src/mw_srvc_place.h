
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

#ifndef _MW_SRVC_PLACE_H
#define _MW_SRVC_PLACE_H


#include <glib/glist.h>
#include "mw_common.h"


/** Type identifier for the place service */
#define SERVICE_PLACE  0x80000022


/** @struct mwServicePlace */
struct mwServicePlace;


/** @struct mwPlace */
struct mwPlace;


struct mwPlaceHandler {
  void (*place_opened)(struct mwPlace *place);
  void (*place_joined)(struct mwPlace *place);
  void (*place_closed)(struct mwPlace *place, guint32 code);

  void (*place_peerJoined)(struct mwPlace *place,
			   struct mwIdBlock *peer);

  void (*place_peerParted)(struct mwPlace *place,
			   struct mwIdBlock *peer);

  void (*place_peerTyping)(struct mwPlace *place,
			   struct mwIdBlock *peer, gboolean typing);

  void (*place_message)(struct mwPlace *place,
			struct mwIdBlock *peer, const char *msg);

  void (*clear)(struct mwServicePlace *srvc);
};


struct mwServicePlace *mwServicePlace_new(struct mwSession *session,
					  struct mwPlaceHandler *handler);


struct mwPlaceHandler *
mwServicePlace_getHandler(struct mwServicePlace *srvc);


const GList *mwServicePlace_getPlaces(struct mwServicePlace *srvc);


struct mwPlace *mwPlace_new(struct mwServicePlace *srvc,
			    const char *name, const char *title);


struct mwServicePlace *mwPlace_getService(struct mwPlace *place);


const char *mwPlace_getName(struct mwPlace *place);


const char *mwPlace_getTitle(struct mwPlace *place);


int mwPlace_open(struct mwPlace *place);


int mwPlace_close(struct mwPlace *place, guint32 code);


void mwPlace_free(struct mwPlace *place);


const struct mwLoginInfo *mwPlace_getPeerInfo(const struct mwIdBlock *who);


const GList *mwPlace_getPeers(struct mwPlace *place);


const struct mwLoginInfo *mwPlace_getSelf(struct mwPlace *place);


const GList *mwPlace_getSections(struct mwPlace *place);

