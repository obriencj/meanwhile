

#include "st_list.h"


#define USER_KEY(user)  &user->id


static guint id_hash(gconstpointer v) {
  return g_str_hash( ((struct mwIdBlock *)v)->user );
}


static gboolean id_equal(gconstpointer, a, gconstpointer b) {
  return mwIdBlock_equal((struct mwIdBlock *) a,
			 (struct mwIdBlock *) b);
}


static void user_free(gpointer u) {
  struct mwSametimeUser *user;

  if(! u) return;
  user = (struct mwSametimeUser *) u;

  g_free(user->key);
  g_free(user->alias);
  mwIdBlock_clear(&user->id);

  g_free(user);
}


struct mwSametimeList *mwSametimeList_new() {

  struct mwSametimeList *stl = g_new0(struct mwSametimeList, 1);
  stl->ver_major = LIST_VERSION_MAJOR;
  stl->ver_minor = LIST_VERSION_MINOR;
  stl->ver_release = LIST_VERSION_RELEASE;

  stl->groups = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);

  return stl;
}


void mwSametimeList_free(struct mwSametimeList *l) {
  
}


struct mwSametimeGroup *mwSametimeGroup_new(struct mwSametimeList *l,
					    const char *name) {

  struct mwSametimeGroup *grp = g_new0(struct mwSametimeGroup, 1);

  grp->name = g_strdup(name);
  grp->open = TRUE;
  grp->users = g_hash_table_new_full(id_hash, id_equal, NULL, NULL);

  grp->list = l;
  g_hash_table_insert(l->groups, grp->name, grp);

  return grp;
}


void mwSametimeGroup_free(struct mwSametimeGroup *g) {

}


struct mwSametimeUser *mwSametimeUser_new(struct mwSametimeGroup *g,
					  struct mwIdBlock *user,
					  const char *alias) {

  struct mwSametimeUser *usr = g_new0(struct mwSametimeUser, 1);

  usr->alias = g_strdup(alias);
  mwIdBlock_clone(&usr->id, user);

  usr->group = g;
  g_hash_table_insert(g->users, usr->id.user, usr);

  return usr;
}


void mwSametimeUser_free(struct mwSametimeUser *u) {

}


static void user_buflen(gpointer k, gpointer v, gpointer data) {
  struct mwSametimeUser *u = (struct mwSametimeUser *) v;
  gsize *l = (gsize *) data;

  /* @todo this doesn't know how to do user/community, only user */

  gsize len = 7; /* "U 1:: \n" */
  len += (strlen(g->id.user) * 2);

  if(u->alias) {
    len++; /* "," */
    len += strlen(u->alias);
  }
    
  *l += len;
}


static void group_buflen(gpointer k, gpointer v, gpointer data) {
  struct mwSametimeGroup *g = (struct mwSametimeGroup *) v;
  gsize *l = (gsize *) data;

  gsize len = 7; /* "G 2  O\n" */
  len += (strlen(g->name) * 2);

  *l += len;

  g_hash_table_foreach(g->users, user_buflen, l);
}


static gsize digit_buflen(guint d) {
  gsize len = 1;
  for(; d >= 10; d = d / 10) len++;
  return len;
}


gsize mwSametimeList_buflen(struct mwSametimeList *l) {
  gsize len = 11; /* "Version=..\n" */
  len += digit_buflen(l->ver_major);
  len += digit_buflen(l->ver_minor);
  len += digit_buflen(l->ver_release);

  g_hash_table_foreach(l->groups, group_buflen, &len);
  return len;
}


int mwSametimeList_get(char **b, gsize *n, struct mwSametimeList *l) {
  /* - read a line
     - process line */

  struct mwSametimeGroup *g = NULL;
  struct mwSametimeUser *u = NULL;

}

