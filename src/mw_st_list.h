

#ifndef _MW_ST_LIST_H
#define _MW_ST_LIST_H


#include <glib.h>
#include <glib/glist.h>
#include "mw_common.h"


#define LIST_VERSION_MAJOR     3
#define LIST_VERSION_MINOR     1
#define LIST_VERSION_REVISION  3


enum mwSametimeGroupType {
  mwGroupType_NORMAL =   0x0002,
  mwGroupType_DYNAMIC =  0x0003,
};


enum mwSametimeUserType {
  mwUserType_NORMAL =  0x0001,
};


/** @struct mwSametimeList
    Represents a group-based buddy list.
*/
struct mwSametimeList;


/** @struct mwSametimeGroup
    Represents a group in a buddy list
*/
struct mwSametimeGroup;


/** @struct mwSametimeUser
    Represents a user in a group in a buddy list
*/
struct mwSametimeUser;


/** Create a new list */
struct mwSametimeList *mwSametimeList_new();


/** Free the list, all of its groups, and all of the groups' members */
void mwSametimeList_free(struct mwSametimeList *l);


/** Determines the length of the buffer required to write the list in its
    entirety */
gsize mwSametimeList_buflen(struct mwSametimeList *l);


/** Load a sametime list from a buffer */
int mwSametimeList_get(char **b, gsize *n, struct mwSametimeList *l);


/** Write a sametime list onto a buffer */
int mwSametimeList_put(char **b, gsize *n, struct mwSametimeList *l);


void mwSametimeList_setMajor(struct mwSametimeList *l, guint v);


guint mwSametimeList_getMajor(struct mwSametimeList *l);


void mwSametimeList_setMinor(struct mwSametimeList *l, guint v);


guint mwSametimeList_getMinor(struct mwSametimeList *l);


void mwSametimeList_setRevision(struct mwSametimeList *l, guint v);


guint mwSametimeList_getRevision(struct mwSametimeList *l);


/** Get a GList snapshot of the groups in a list */
GList *mwSametimeList_getGroups(struct mwSametimeList *l);


/** Create a new group in a list */
struct mwSametimeGroup *mwSametimeGroup_new(struct mwSametimeList *l,
					    const char *name);


/** Remove a group from its list, and free it. Also frees all users
    contained in the group */
void mwSametimeGroup_free(struct mwSametimeGroup *g);


const char *mwSametimeGroup_getGroup(struct mwSametimeGroup *g);


const char *mwSametimeGroup_getName(struct mwSametimeGroup *g);


enum mwSametimeGroupType mwSametimeGroup_getType(struct mwSametimeGroup *g);


gboolean mwSametimeGroup_isOpen(struct mwSametimeGroup *g);


void mwSametimeGroup_setOpen(struct mwSametimeGroup *g, gboolean open);


struct mwSametimeList *mwSametimeGroup_getList(struct mwSametimeGroup *g);


/** Get a GList snapshot of the users in a list */
GList *mwSametimeGroup_getUsers(struct mwSametimeGroup *g);


/** Create a user in a group */
struct mwSametimeUser *mwSametimeUser_new(struct mwSametimeGroup *g,
					  struct mwIdBlock *user,
					  const char *name,
					  const char *alias);


/** Remove user from its group, and free it */
void mwSametimeUser_free(struct mwSametimeUser *u);


struct mwSametimeGroup *mwSametimeUser_getGroup(struct mwSametimeUser *u);


const char *mwSametimeUser_getUser(struct mwSametimeUser *u);


const char *mwSametimeUser_getCommunity(struct mwSametimeUser *u);


const char *mwSametimeUser_getName(struct mwSametimeUser *u);


const char *mwSametimeUser_getAlias(struct mwSametimeUser *u);


enum mwSametimeUserType mwSametimeUser_getType(struct mwSametimeUser *u);


#endif
