

#include <glib/ghash.h>
#include <glib/gprintf.h>
#include <stdio.h>
#include <string.h>

#include "st_list.h"


struct mwSametimeList {
  guint ver_major;
  guint ver_minor;
  guint ver_revision;

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
  struct mwIdBlock id;
  char *alias;
};


#define MW_ST_USER_KEY(stuser) (&stuser->id)


static guint id_hash(gconstpointer v) {
  return g_str_hash( ((struct mwIdBlock *)v)->user );
}


static gboolean id_equal(gconstpointer a, gconstpointer b) {
  return mwIdBlock_equal((struct mwIdBlock *) a,
			 (struct mwIdBlock *) b);
}


static void str_replace(char *str, char from, char to) {
  for(; *str; str++) if(*str == from) *str = to;
}


static void user_free(gpointer u) {
  struct mwSametimeUser *user;

  if(! u) return;
  user = (struct mwSametimeUser *) u;

  g_free(user->alias);
  mwIdBlock_clear(&user->id);

  g_free(user);
}


struct mwSametimeList *mwSametimeList_new() {

  struct mwSametimeList *stl = g_new0(struct mwSametimeList, 1);
  stl->ver_major = LIST_VERSION_MAJOR;
  stl->ver_minor = LIST_VERSION_MINOR;
  stl->ver_revision = LIST_VERSION_REVISION;

  stl->groups = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);

  return stl;
}


void mwSametimeList_free(struct mwSametimeList *l) {
  GList *groups;

  if(! l) return;

  for(groups = mwSametimeList_getGroups(l); groups; groups = groups->next)
    mwSametimeGroup_free((struct mwSametimeGroup *) groups->data);

  g_hash_table_destroy(l->groups);
  g_free(l);
}


static void collect(gpointer key, gpointer val, gpointer data) {
  GList **list = (GList **) data;
  *list = g_list_append(*list, val);
}


GList *mwSametimeList_getGroups(struct mwSametimeList *l) {
  GList *list = NULL;
  g_hash_table_foreach(l->groups, collect, &list);
  return list;
}


void mwSametimeList_putMajor(struct mwSametimeList *l, guint v) {
  g_return_if_fail(l != NULL);
  l->ver_major = v;
}


guint mwSametimeList_getMajor(struct mwSametimeList *l) {
  g_return_val_if_fail(l != NULL, 0x00);
  return l->ver_major;
}


void mwSametimeList_putMinor(struct mwSametimeList *l, guint v) {
  g_return_if_fail(l != NULL);
  l->ver_minor = v;
}


guint mwSametimeList_getMinor(struct mwSametimeList *l) {
  g_return_val_if_fail(l != NULL, 0x00);
  return l->ver_minor;
}


void mwSametimeList_putRevision(struct mwSametimeList *l, guint v) {
  g_return_if_fail(l != NULL);
  l->ver_revision = v;
}


guint mwSametimeList_getRevision(struct mwSametimeList *l) {
  g_return_val_if_fail(l != NULL, 0x00);
  return l->ver_revision;
}


struct mwSametimeGroup *mwSametimeGroup_new(struct mwSametimeList *l,
					    const char *name) {
  struct mwSametimeGroup *grp;

  g_return_val_if_fail(l != NULL, NULL);

  grp = g_new0(struct mwSametimeGroup, 1);

  grp->name = g_strdup(name);
  grp->open = TRUE;
  grp->users = g_hash_table_new_full(id_hash, id_equal, NULL, user_free);

  grp->list = l;
  g_hash_table_insert(l->groups, grp->name, grp);

  return grp;
}


void mwSametimeGroup_free(struct mwSametimeGroup *g) {
  if(! g) return;
  g_hash_table_remove(g->list->groups, g->name);
  g_hash_table_destroy(g->users);
  g_free(g->name);
  g_free(g);
}


const char *mwSametimeGroup_getName(struct mwSametimeGroup *g) {
  g_return_val_if_fail(g != NULL, NULL);
  return g->name;
}


gboolean mwSametimeGroup_isOpen(struct mwSametimeGroup *g) {
  g_return_val_if_fail(g != NULL, FALSE);
  return g->open;
}


void mwSametimeGroup_setOpen(struct mwSametimeGroup *g, gboolean open) {
  g_return_if_fail(g != NULL);
  g->open = open;
}


struct mwSametimeList *mwSametimeGroup_getList(struct mwSametimeGroup *g) {
  g_return_val_if_fail(g != NULL, NULL);
  return g->list;
}


GList *mwSametimeGroup_getUsers(struct mwSametimeGroup *g) {
  GList *list = NULL;
  g_hash_table_foreach(g->users, collect, &list);
  return list;
}


struct mwSametimeUser *mwSametimeUser_new(struct mwSametimeGroup *g,
					  struct mwIdBlock *user,
					  const char *alias) {
  struct mwSametimeUser *usr;

  g_return_val_if_fail(g != NULL, NULL);
  g_return_val_if_fail(user != NULL, NULL);

  usr = g_new0(struct mwSametimeUser, 1);

  mwIdBlock_clone(MW_ST_USER_KEY(usr), user);
  usr->alias = g_strdup(alias);
  usr->group = g;

  g_hash_table_insert(g->users, MW_ST_USER_KEY(usr), usr);

  return usr;
}


void mwSametimeUser_free(struct mwSametimeUser *u) {
  if(! u) return;
  g_hash_table_remove(u->group->users, MW_ST_USER_KEY(u));
  user_free(u);
}


struct mwSametimeGroup *mwSametimeUser_getGroup(struct mwSametimeUser *u) {
  g_return_val_if_fail(u != NULL, NULL);
  return u->group;
}


const char *mwSametimeUser_getUser(struct mwSametimeUser *u) {
  g_return_val_if_fail(u != NULL, NULL);
  return u->id.user;
}


const char *mwSametimeUser_getCommunity(struct mwSametimeUser *u) {
  g_return_val_if_fail(u != NULL, NULL);
  return u->id.community;
}


const char *mwSametimeUser_getAlias(struct mwSametimeUser *u) {
  g_return_val_if_fail(u != NULL, NULL);
  return u->alias;
}


static void user_buflen(gpointer k, gpointer v, gpointer data) {
  struct mwSametimeUser *u = (struct mwSametimeUser *) v;
  gsize *l = (gsize *) data;

  /** @todo this doesn't know how to do user/community, only user */

  gsize len = 8; /* "U %s1:: %s,%s\n" */

  len += (strlen(u->id.user) * 2);
  if(u->alias) len += strlen(u->alias);
    
  *l += len;
}


static void group_buflen(gpointer k, gpointer v, gpointer data) {
  struct mwSametimeGroup *g = (struct mwSametimeGroup *) v;
  gsize *l = (gsize *) data;

  gsize len = 7; /* "G %s2 %s %c\n" */
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
  gsize len = 11; /* "Version=%u.%u.%u\n" */
  len += digit_buflen(l->ver_major);
  len += digit_buflen(l->ver_minor);
  len += digit_buflen(l->ver_revision);

  g_hash_table_foreach(l->groups, group_buflen, &len);
  return len + 1; /* add null termination */
}


static char *fetch_line(char **buf, gsize *len) {
  char *b = *buf, *ret = NULL;
  gsize tmp;

  while((*len)--) {
    char peek = **buf;
    (*buf)++;
    if(peek == '\n' || peek == '\r') break;
  }

  tmp = *buf - b;
  if(tmp--) ret = g_strndup(b, tmp);

  return ret;
}


static int get_version(char *b, struct mwSametimeList *l) {
  unsigned int major = 0, minor = 0, rev = 0;
  int ret;

  ret = sscanf(b, "Version=%u.%u.%u", &major, &minor, &rev);
  
  l->ver_major = major;
  l->ver_minor = minor;
  l->ver_revision = rev;

  return ret != 3;
}


static int get_group(char *b, struct mwSametimeList *l,
		     struct mwSametimeGroup **g) {

  char **split = NULL;
  char *title;
  struct mwSametimeGroup *grp;

  g_return_val_if_fail(l != NULL, -1);

  split = g_strsplit(b, " ", 4);

  title = split[2];
  str_replace(title, ';', ' ');

  grp = mwSametimeGroup_new(l, title);
  grp->open = (split[3][0] == 'O');

  g_strfreev(split);
  *g = grp;

  return 0;
}


static int get_user(char *b, struct mwSametimeList *l,
		    struct mwSametimeGroup *g) {

  char **split = NULL;
  char *id, *name, *alias;

  struct mwIdBlock idb = { NULL, NULL };
  struct mwSametimeUser *user;

  g_return_val_if_fail(g != NULL, -1);

  split = g_strsplit_set(b, " :,", 6);

  id = g_strdup(split[1]);
  name = split[4];
  alias = split[5];

  id[strlen(id)-1] = '\0';
  str_replace(id, ';', ' ');
  idb.user = id;

  /* if(! *alias) alias = name; */
  if(alias) str_replace(alias, ';', ' ');

  user = mwSametimeUser_new(g, &idb, alias);

  g_free(id);
  g_strfreev(split);

  return 0;
}


int mwSametimeList_get(char **b, gsize *n, struct mwSametimeList *l) {
  /* - read a line
     - process line */

  struct mwSametimeGroup *g = NULL;
  char *line = NULL;

  g_return_val_if_fail(l != NULL, -1);

  while(*n) {
    line = fetch_line(b, n);
    if(line && *line) {
      int ret = 0;
      
      switch(*line) {
      case 'V':
	ret = get_version(line, l);
	break;

      case 'G':
	g = NULL;
	ret = get_group(line, l, &g);
	break;

      case 'U':
	ret = get_user(line, l, g);
	break;

      default:
	ret = -1;
      }

      if(ret) g_warning("unused sametime data line: %s", line);
    }
    g_free(line);
  }

  return 0;
}


static int put_group(char **b, gsize *n, struct mwSametimeGroup *group) {
  char *name;
  int writ;
  
  name = g_strdup(group->name);
  str_replace(name, ' ', ';');

  writ = g_sprintf(*b, "G %s2 %s %c\n",
		   name, name, group->open? 'O': 'C');

  g_free(name);

  *b += writ;
  *n -= writ;

  return 0;
}


static int put_user(char **b, gsize *n, struct mwSametimeUser *user) {
  char *name, *alias;
  int writ;

  name = g_strdup(user->id.user);
  alias = g_strdup(user->alias);
  str_replace(name, ' ', ';');
  if(alias) str_replace(alias, ' ', ';');

  writ = g_sprintf(*b, "U %s1:: %s,%s\n", name, name, alias? alias: "");

  g_free(name);
  g_free(alias);

  *b += writ;
  *n -= writ;

  return 0;
}


int mwSametimeList_put(char **b, gsize *n, struct mwSametimeList *list) {
  struct mwSametimeGroup *group;
  struct mwSametimeUser *user;

  int writ = 0;
  GList *g, *gst, *u, *ust;

  g_return_val_if_fail(list != NULL, -1);

  writ = g_sprintf(*b, "Version=%u.%u.%u\n",
		   list->ver_major, list->ver_minor, list->ver_revision);

  *b += writ;
  *n -= writ;

  g = gst = mwSametimeList_getGroups(list);
  for(; g; g = g->next) {
    group = (struct mwSametimeGroup *) g->data;
    put_group(b, n, group);

    u = ust = mwSametimeGroup_getUsers(group);
    for(; u; u = u->next) {
      user = (struct mwSametimeUser *) u->data;
      put_user(b, n, user);
    }
    g_list_free(ust);
  }
  g_list_free(gst);

  **b = '\0';

  return 0;
}

