
#include <glib/ghash.h>

#include "mw_channel.h"
#include "mw_common.h"
#include "mw_error.h"
#include "mw_service.h"
#include "mw_session.h"
#include "mw_srvc_resolve.h"


#define PROTOCOL_TYPE  0x00000015
#define PROTOCOL_VER   0x00000000


/** oddly, there is only one message type in this service */
#define RESOLVE_ACTION  0x02


struct mwServiceResolve {
  struct mwService service;

  struct mwChannel *channel;  /**< channel for this service */
  GHashTable *searches;       /**< guint32:struct mw_search */
  guint32 counter;            /**< incremented to provide searche IDs */
};


/** structure representing an active search. keeps track of the ID,
    the handler, and the optional user data and cleanup */
struct mw_search {
  struct mwServiceResolve *service;
  guint32 id;
  mwResolveHandler handler;
  gpointer data;
  GDestroyNotify cleanup;
};


static struct mw_search *search_new(struct mwServiceResolve *srvc,
				    gpointer data, GDestroyNotify cleanup) {

  struct mw_search *search = g_new0(struct mw_search, 1);

  search->service = srvc;

  /* we want search IDs that aren't SEARCH_ERROR */
  do {
    search->id = srvc->counter++;
  } while(search->id == SEARCH_ERROR);

  search->data = data;
  search->cleanup = cleanup;

  return search;
}


/** called whenever a mw_search is removed from the searches table of
    the service */
static void search_free(struct mw_search *search) {
  g_return_if_fail(search != NULL);

  if(search->cleanup)
    search->cleanup(search->data);
  
  g_free(search);
}


static const char *get_name(struct mwService *srvc) {
  return "Resolve Service";
}


static const char *get_desc(struct mwService *srvc) {
  return "Simple implementation of the resolve service";
}


static struct mwChannel *make_channel(struct mwServiceResolve *srvc) {
  struct mwSession *session;
  struct mwChannelSet *cs;
  struct mwChannel *chan;

  session = mwService_getSession(MW_SERVICE(srvc));
  cs = mwSession_getChannels(session);
  chan = mwChannel_newOutgoing(cs);
 
  mwChannel_setService(chan, MW_SERVICE(srvc));
  mwChannel_setProtoType(chan, PROTOCOL_TYPE);
  mwChannel_setProtoVer(chan, PROTOCOL_VER);

  return mwChannel_create(chan)? NULL: chan;
}


static void start(struct mwServiceResolve *srvc) {
  struct mwChannel *chan;

  g_return_if_fail(srvc != NULL);

  chan = make_channel(srvc);
  if(chan) {
    srvc->channel = chan;
  } else {
    mwService_stopped(MW_SERVICE(srvc));
    return;
  }

  /* semi-lazily create the searches table */
  srvc->searches = g_hash_table_new_full(g_direct_hash, g_direct_equal,
					 NULL, (GDestroyNotify) search_free);
}


static void stop(struct mwServiceResolve *srvc) {
  g_return_if_fail(srvc != NULL);

  if(srvc->channel) {
    mwChannel_destroy(srvc->channel, ERR_SUCCESS, NULL);
    srvc->channel = NULL;
  }

  /* destroy all the pending requests. */
  g_hash_table_destroy(srvc->searches);
  
  mwService_stopped(MW_SERVICE(srvc));
}


static void clear(struct mwServiceResolve *srvc) {
  ;
}


static void recv_create(struct mwServiceResolve *srvc,
			struct mwChannel *chan,
			struct mwMsgChannelCreate *msg) {

  /* you serve me, not the other way around */
  mwChannel_destroy(chan, ERR_FAILURE, NULL);
}


static void recv_accept(struct mwServiceResolve *srvc,
			struct mwChannel *chan,
			struct mwMsgChannelAccept *msg) {
  
  g_return_if_fail(srvc != NULL);
  g_return_if_fail(chan != NULL);
  g_return_if_fail(chan == srvc->channel);

  mwService_started(MW_SERVICE(srvc));
}


static void recv_destroy(struct mwServiceResolve *srvc,
			 struct mwChannel *chan,
			 struct mwMsgChannelDestroy *msg) {

  struct mwSession *session;

  g_return_if_fail(srvc != NULL);
  g_return_if_fail(chan != NULL);
  g_return_if_fail(chan == srvc->channel);

  srvc->channel = NULL;
  mwService_stop(MW_SERVICE(srvc));

  session = mwService_getSession(MW_SERVICE(srvc));
  g_return_if_fail(session != NULL);

  mwSession_senseService(session, mwService_getType(MW_SERVICE(srvc)));
}


static void recv(struct mwServiceResolve *srvc,
		 struct mwChannel *chan,
		 guint16 type, struct mwOpaque *data) {

  struct mwGetBuffer *b;
  guint32 id;
  struct mw_search *search;

  g_return_if_fail(srvc != NULL);
  g_return_if_fail(chan != NULL);
  g_return_if_fail(chan == srvc->channel);
  g_return_if_fail(type != RESOLVE_ACTION);
  g_return_if_fail(data != NULL);

  b = mwGetBuffer_wrap(data);
  id = guint32_peek(b);
  search = g_hash_table_lookup(srvc->searches, GUINT_TO_POINTER(id));

  if(! search) {
    mwGetBuffer_free(b);
    return;
  }

  /** @todo: get mwResolveResults from b */
  
}


struct mwServiceResolve *mwServiceResolve_new(struct mwSession *session) {
  struct mwServiceResolve *srvc_resolve;
  struct mwService *srvc;

  g_return_val_if_fail(session != NULL, NULL);

  srvc_resolve = g_new0(struct mwServiceResolve, 1);

  srvc = MW_SERVICE(srvc_resolve);

  mwService_init(srvc, session, SERVICE_RESOLVE);
  srvc->get_name = get_name;
  srvc->get_desc = get_desc;
  srvc->recv_create = (mwService_funcRecvCreate) recv_create;
  srvc->recv_accept = (mwService_funcRecvAccept) recv_accept;
  srvc->recv_destroy = (mwService_funcRecvDestroy) recv_destroy;
  srvc->recv = (mwService_funcRecv) recv;
  srvc->start = (mwService_funcStart) start;
  srvc->stop = (mwService_funcStop) stop;
  srvc->clear = (mwService_funcClear) clear;

  return srvc_resolve;
}


guint32 mwServiceResolve_search(struct mwServiceResolve *srvc,
				const char *query[], enum mwResolveFlag flags,
				mwResolveHandler handler,
				gpointer data, GDestroyNotify cleanup) {

  struct mw_search *search;
  struct mwPutBuffer *b;
  struct mwOpaque o = { 0, 0 };
  int ret;
  int count = 0;

  g_return_val_if_fail(srvc != NULL, SEARCH_ERROR);
  g_return_val_if_fail(query != NULL, SEARCH_ERROR);
  g_return_val_if_fail(handler != NULL, SEARCH_ERROR);

  while(query[count]) count++;
  g_return_val_if_fail(count > 0, SEARCH_ERROR);

  search = search_new(srvc, data, cleanup);

  b = mwPutBuffer_new();
  guint32_put(b, 0x00); /* to be overwritten */
  guint32_put(b, search->id);
  guint32_put(b, count);
  for(; query; query++) mwString_put(b, *query);
  guint32_put(b, flags);

  mwPutBuffer_finalize(&o, b);
  
  ret = mwChannel_send(srvc->channel, RESOLVE_ACTION, &o);
  if(ret) {
    search_free(search);
    return SEARCH_ERROR;

  } else {
    g_hash_table_insert(srvc->searches,
			GUINT_TO_POINTER(search->id), search);
    return search->id;
  }
}


void mwServiceResolve_cancelSearch(struct mwServiceResolve *srvc,
				   guint32 id) {

  g_return_if_fail(srvc != NULL);
  g_return_if_fail(srvc->searches != NULL);

  g_hash_table_remove(srvc->searches, GUINT_TO_POINTER(id));
}

