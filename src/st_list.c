
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

#include <glib/ghash.h>
#include <stdio.h>
#include <string.h>

#include "mw_debug.h"
#include "mw_util.h"
#include "mw_st_list.h"


struct mwSametimeList {
  guint ver_major;
  guint ver_minor;
  guint ver_revision;

  GHashTable *groups;
};


struct mwSametimeGroup {
  struct mwSametimeList *list;
  char *group;
  char *name;
  enum mwSametimeGroupType type;
  gboolean open;
  GHashTable *users;
};


struct mwSametimeUser {
  struct mwSametimeGroup *group;
  struct mwIdBlock id;
  char *name;
  char *alias;
  char type;
};


#define USER_KEY(stuser) (&stuser->id)


static void str_replace(char *str, char from, char to) {
  for(; *str; str++) if(*str == from) *str = to;
}


static void user_free(struct mwSametimeUser *user) {
  if(! user) return;

  g_free(user->name);
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


GList *mwSametimeList_getGroups(struct mwSametimeList *l) {
  g_return_val_if_fail(l != NULL, NULL);
  return map_collect_values(l->groups);
}


struct mwSametimeGroup *mwSametimeList_getGroup(struct mwSametimeList *l,
						const char *name) {
  g_return_val_if_fail(l != NULL, NULL);
  g_return_val_if_fail(name != NULL, NULL);
  g_return_val_if_fail(strlen(name) > 0, NULL);

  return g_hash_table_lookup(l->groups, name);
}


void mwSametimeList_setMajor(struct mwSametimeList *l, guint v) {
  g_return_if_fail(l != NULL);
  l->ver_major = v;
}


guint mwSametimeList_getMajor(struct mwSametimeList *l) {
  g_return_val_if_fail(l != NULL, 0x00);
  return l->ver_major;
}


void mwSametimeList_setMinor(struct mwSametimeList *l, guint v) {
  g_return_if_fail(l != NULL);
  l->ver_minor = v;
}


guint mwSametimeList_getMinor(struct mwSametimeList *l) {
  g_return_val_if_fail(l != NULL, 0x00);
  return l->ver_minor;
}


void mwSametimeList_setRevision(struct mwSametimeList *l, guint v) {
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
  grp->users = g_hash_table_new_full((GHashFunc) mwIdBlock_hash,
				     (GEqualFunc) mwIdBlock_equal,
				     NULL,
				     (GDestroyNotify) user_free);
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


void mwSametimeGroup_setName(struct mwSametimeGroup *g, const char *name) {
  struct mwSametimeList *l;

  g_return_if_fail(g != NULL);
  g_return_if_fail(name != NULL);
  g_return_if_fail(strlen(name) > 0);

  l = g->list;
  g_hash_table_remove(l->groups, g->name);

  g_free(g->name);
  g->name = g_strdup(name);

  g_hash_table_insert(l->groups, g->name, g);
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
  g_return_val_if_fail(g != NULL, NULL);
  return map_collect_values(g->users);
}


struct mwSametimeUser *mwSametimeUser_new(struct mwSametimeGroup *g,
					  struct mwIdBlock *user,
					  enum mwSametimeUserType type,
					  const char *name,
					  const char *alias) {
  struct mwSametimeUser *usr;

  g_return_val_if_fail(g != NULL, NULL);
  g_return_val_if_fail(user != NULL, NULL);

  usr = g_new0(struct mwSametimeUser, 1);

  mwIdBlock_clone(USER_KEY(usr), user);
  usr->type = type;
  usr->name = g_strdup(name);
  usr->alias = g_strdup(alias);
  usr->group = g;

  g_hash_table_insert(g->users, USER_KEY(usr), usr);

  return usr;
}


void mwSametimeUser_free(struct mwSametimeUser *u) {
  if(! u) return;
  g_hash_table_remove(u->group->users, USER_KEY(u));
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


const char *mwSametimeUser_getName(struct mwSametimeUser *u) {
  g_return_val_if_fail(u != NULL, NULL);
  return u->name;
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

  len += strlen(u->id.user);

  if(u->name) {
    len += strlen(u->name);
  } else {
    len += strlen(u->id.user);
  }

  if(u->alias)
    len += strlen(u->alias);
    
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
  guint major = 0, minor = 0, rev = 0;
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

  char *id, *name, *alias = NULL;
  char *tmp;

  struct mwIdBlock idb = { NULL, NULL };
  struct mwSametimeUser *user;

  g_return_val_if_fail(strlen(b) > 2, -1);
  g_return_val_if_fail(g != NULL, -1);

  /* just get everything now */
  str_replace(b, ';', ' ');

  id = b + 2; /* advance past "U " */
  tmp = strstr(b, "1:: "); /* backwards thinking saves overruns */
  if(! tmp) return -1;
  *tmp = '\0';
  b = tmp;

  name = b + 4; /* advance past the "1:: " */

  tmp = strrchr(name, ',');
  if(tmp) {
    *tmp = '\0';
   
    tmp++;
    if(*tmp) {
      alias = tmp;
    }
  }
  
  idb.user = id;
  user = mwSametimeUser_new(g, &idb, mwUser_NORMAL, name, alias);

  return 0;
}


int mwSametimeList_get(char **b, gsize *n, struct mwSametimeList *l) {
  /* - read a line
     - process line
     - free line  */

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

      if(ret) g_warning("unused sametime data line: %s", NSTR(line));
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

  writ = sprintf(*b, "G %s2 %s %c\n",
		 name, name, group->open? 'O': 'C');

  g_free(name);

  *b += writ;
  *n -= writ;

  return 0;
}


static int put_user(char **b, gsize *n, struct mwSametimeUser *user) {
  char *id, *name, *alias;
  int writ;

  id = g_strdup(user->id.user);
  name = g_strdup(user->name);
  alias = g_strdup(user->alias);

  if(id) str_replace(id, ' ', ';');
  if(name) str_replace(name, ' ', ';');
  if(alias) str_replace(alias, ' ', ';');

  if(!name && alias) name = g_strdup(alias);

  writ = sprintf(*b, "U %s1:: %s,%s\n",
		 id, name? name: id, alias? alias: "");

  g_free(id);
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

  writ = sprintf(*b, "Version=%u.%u.%u\n",
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

