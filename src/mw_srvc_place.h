
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
  void (*place_closed)(struct mwPlace *place, guint32 code);

  void (*place_peerJoined)(struct mwPlace *place,
			   const struct mwIdBlock *peer);

  void (*place_peerParted)(struct mwPlace *place,
			   const struct mwIdBlock *peer);

  void (*place_peerSetAttribute)(struct mwPlace *place,
				 const struct mwIdBlock *peer,
				 guint32 attr, struct mwOpaque *o);

  void (*place_peerUnsetAttribute)(struct mwPlace *place,
				   const struct mwIdBlock *peer,
				   guint32 attr);

  void (*place_message)(struct mwPlace *place,
			const struct mwIdBlock *peer,
			const char *msg);

  void (*clear)(struct mwPlace *place);
};


struct mwServicePlace *
mwServicePlace_new(struct mwSession *session);


struct mwPlaceHandler *
mwServicePlace_getHandler(struct mwServicePlace *srvc);


const GList *mwServicePlace_getPlaces(struct mwServicePlace *srvc);


struct mwPlace *mwPlace_new(struct mwServicePlace *srvc,
			    struct mwPlaceHandler *handler,
			    const char *name, const char *title);


struct mwServicePlace *mwPlace_getService(struct mwPlace *place);


const char *mwPlace_getName(struct mwPlace *place);


const char *mwPlace_getTitle(struct mwPlace *place);


int mwPlace_open(struct mwPlace *place);


int mwPlace_destroy(struct mwPlace *place, guint32 code);


void mwPlace_free(struct mwPlace *place);


const struct mwLoginInfo *
mwPlace_getPeerInfo(const struct mwIdBlock *who);


GList *mwPlace_getPeers(struct mwPlace *place);


int mwPlace_sendText(struct mwPlace *place, const char *msg);


int mwPlace_sendTyping(struct mwPlace *place, gboolean typing);


#endif
