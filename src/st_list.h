

#ifndef _MW_ST_LIST_H_
#define _MW_ST_LIST_H_


#include <glib.h>
#include <glib/ghash.h>
#include <glib/glist.h>
#include "common.h"


#define LIST_VERSION_MAJOR    3
#define LIST_VERSION_MINOR    1
#define LIST_VERSION_RELEASE  3


struct mwSametimeList {
  guint ver_major;
  guint ver_minor;
  guint ver_release;

  GHashTable *groups;
};


struct mwSametimeGroup {
  struct mwSametimeList *list;
  char *name;
  gboolean open;
  GHashTable *users;
};


struct mwSametimeUser {
  struct mwSametimeGroup *group;
  char *key;
  char *alias;
  struct mwIdBlock id;
};


/** Create a new list */
struct mwSametimeList *mwSametimeList_new(guint major, guint minor,
					  guint release);


/** Free the list, all of its groups, and all of the groups' members */
void mwSametimeList_free(struct mwSametimeList *l);


/** Determines the length of the buffer required to write the list in its
    entirety */
gsize mwSametimeList_buflen(struct mwSametimeList *l);


/** Load a sametime list from a buffer */
int mwSametimeList_get(char **b, gsize *n, struct mwSametimeList *l);


/** Write a sametime list onto a buffer */
int mwSametimeList_put(char **b, gsize *n, struct mwSametimeList *l);


/** Create a new group in a list */
struct mwSametimeGroup *mwSametimeGroup_new(struct mwSametimeList *l,
					    const char *name);


/** Remove a group from its list, and free it. Also frees all users
    contained in the group */
void mwSametimeGroup_free(struct mwSametimeGroup *g);


/** Create a user in a group */
struct mwSametimeUser *mwSametimeUser_new(struct mwSametimeGroup *g,
					  struct mwIdBlock *user,
					  const char *alias);


/** Remove user from its group, and free it */
void mwSametimeUser_free(struct mwSametimeUser *u);


#endif
