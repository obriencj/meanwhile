
/*
  Clear Channel Sametime Proxy Utility
  The Meanwhile Project

  This is a tool which can act as a proxy between a client and a
  sametime server, which will force all channels to be created without
  any encryption method. This makes reverse-engineering much, much
  easier.

  It also outputs the messages sent to and from the client to stdout
  as hex pairs. If compiled with USE_HEXDUMP, output will be printed
  via `hexdump -C`
  
  All it really does is nab all Channel Create messages, strip the
  offered ciphers portion from the message and replace it with an
  empty ciphers list.

  Christopher O'Brien <siege@preoccupied.net>
*/


#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <glib.h>
#include <glib/glist.h>

#include <mw_cipher.h>
#include <mw_common.h>
#include <mw_message.h>



struct rc2_40enc {
  char *outgoing_iv[8];
  char *incoming_iv[8];
  int incoming_key[64];
  int outgoing_key[64];
};


struct rc2_128enc {
  char *outgoing_iv[8];
  char *incoming_iv[8];
  int shared_key[64];
};


/* the left side of a channel is the side that originated the channel
   create message. The right side of the channel is the target, and is
   where the accept message will come from. Because either the client
   or the server may initiate a channel, neither side is specifically
   either */
struct channel {
  guint32 id;

  /* login id of creator or NULL if created by client side */
  char *creator;

  /* the offer from the left side */
  struct mwEncryptOffer offer;

  enum {
    enc_none = 0,
    enc_easy,
    enc_hard,
  } enc_mode;

  /* encryption data for the left side */
  union {
    struct rc2_40enc easy;
    struct rc2_128enc hard;
  } left_enc;

  /* encryption data for the right side */
  union {
    struct rc2_40enc easy;
    struct rc2_128enc hard;
  } right_enc;

  struct proxy_side *left;
  struct proxy_side *right;
};


#define PUT_CHANNEL(chan) \
  g_hash_table_insert(channels, GUINT_TO_POINTER((chan)->id), (chan))

#define GET_CHANNEL(id) \
  g_hash_table_lookup(channels, GUINT_TO_POINTER(id))

#define STEAL_CHANNEL(id) \
  g_hash_table_steal(channels, GUINT_TO_POINTER(id))


struct proxy_side {
  int sock;
  GIOChannel *chan;
  gint chan_io;

  char *buf;
  gsize buf_size;
  gsize buf_recv;

  void (*forward)(const char *buf, gsize len);
};


/* collection of channels */
static GHashTable *channels;


/* re-usable rc2 128 stuff */
static mpz_t private_key;
static struct mwOpaque public_key;


/* re-usable rc2 40 stuff */
static int session_key[64];


static struct proxy_side client;
static struct proxy_side server;

#define OTHER_SIDE(side) \
  ((side) == client? server: client)


/* dump data to stdout via the hexdump utility */
static void hexdump(const char *txt, const unsigned char *buf, gsize len) {
  FILE *fp;

  if(txt) fprintf(stdout, "\n%s\n", txt);
  fflush(stdout);

  fp = popen("hexdump -C", "w");
  fwrite(buf, len, 1, fp);
  fflush(fp);
  pclose(fp);
}


/* serialize a message for sending */
static void put_msg(struct mwMessage *msg, struct mwOpaque *o) {
  struct mwPutBuffer *b;

  b = mwPutBuffer_new();
  mwMessage_put(b, msg);
  mwPutBuffer_finalize(o, b);
  
  b = mwPutBuffer_new();
  mwOpaque_put(b, o);
  mwOpaque_clear(o);
  mwPutBuffer_finalize(o, b);
}


static void side_buf_free(struct proxy_side *s) {
  g_free(s->buf);
  s->buf = NULL;
  s->buf_size = 0;
  s->buf_recv = 0;
}


/* we don't want to be redirected away from the proxy, so eat any
   redirect messages from the server and respond with a login cont */
static void munge_redir() {
  struct mwMessage *msg;
  struct mwOpaque o = { 0, 0 };

  msg = mwMessage_new(mwMessage_LOGIN_CONTINUE);
  put_msg(msg, &o);
  mwMessage_free(msg);

  server.forward(o.data, o.len);

  mwOpaque_clear(&o);
}


/* handle receipt of channel create messages from either side,
   recording the offered ciphers, and munging it to instead include
   our own key as applicable, then sending it on */
static void munge_create(struct proxy_side *side,
			 struct mwMsgChannelCreate *msg) {

  struct mwOpaque o = { 0, 0 };
  GList *l;
  struct channel *c;

  /* create a new channel on the side */
  c = g_new0(struct channel, 1);
  c->id = msg->channel;
  c->left = side;
  c->right = OTHER_SIDE(side);

  if(msg->creator_flag) {
    msg->creator = g_strdup(msg->creator.login_id);
  }

  /* record the mode and encryption items */
  c->offer.mode = msg->encrypt.mode;
  c->offer.items = msg->encrypt.items;
  c->offer.extra = msg->encrypt.extras;
  c->offer.flag = msg->encrypt.flag;

  PUT_CHANNEL(c);

  /* replace the encryption items with our own as applicable */
  if(msg->encrypt.items) {
    l = msg->encrypt.items;
    msg->encrypt.items = NULL;

    for(; l; l = l->next) {
      struct mwEncryptItem *i1, *i2;

      i1 = l->data;
      i2 = g_new0(struct mwEncryptItem, 1);
      i2->id = i1->id;

      switch(i1->id) {
      case mwCipher_RC2_128:
	mwOpaque_clone(&i2->data, &public_key);
	break;	
      case mwCipher_RC2_40:
      default:
	break;
      }

      g_list_append(msg->encrypt.items, i2);
    }
  }

  put_msg(MW_MESSAGE(msg), &o);
  OTHER_SIDE(side)->forward(o.data, o.len);
  mwOpaque_clear(&o);
}


/* find an enc item by id in a list of items */
struct mwEncryptItem *find_item(GList *items, guint16 id) {
  GList *ltmp;
  for(ltmp = items; ltmp; ltmp = ltmp->next) {
    struct mwEncryptItem *i = ltmp->data;
    if(i->id == id) return i;
  }
  return NULL;
}


/* handle acceptance of a channel */
static void munge_accept(struct proxy_side *side,
			 struct mwMsgChannelAccept *msg) {

  struct mwOpaque o = {0,0};
  struct channel *chan;
  struct mwEncryptItem *item;

  chan = GET_CHANNEL(msg->head.channel);
  item = msg->encrypt->item;

  /* init right-side encryption with our enc and accepted enc */
  switch(item->id) {
  case mwCipher_RC2_128: {
    mpz_t remote, shared;
    struct mwOpaque k;

    mpz_init(remote);
    mpz_init(shared);

    mwDHImportKey(remote, &item->info);
    mwDHCalculateShared(shared, remote, private_key);
    mwDHExportKey(shared, &k);

    chan->enc_mode = enc_hard;
    mwIVInit(&chan->right_enc.hard->outgoing_iv);
    mwIVInit(&chan->right_enc.hard->incoming_iv);
    mwKeyExpand(chan->right_enc.hard->shared_key, k.data+(k.len-16), 16);

    mpz_clear(remote);
    mpz_clear(shared);
    mwOpaque_clear(&k);
    break;
  }
  case mwCipher_RC2_40: {
    char *who;
    
    chan->enc_mode = enc_easy;
    mwIVInit(&chan->right_enc.easy->outgoing_iv);
    mwIVInit(&chan->right_enc.easy->incoming_iv);

    who = (msg->acceptor_flag? msg->acceptor->login_id: chan->creator);
    if(who) mwKeyExpand(chan->right_enc.easy->incoming_key, who, 5);
    memcpy(chan->right_enc.easy->outgoing_key, session_key, 64*sizeof(int));

    break;
  }
  default:
    chan->enc_mode = enc_none;
    break;
  }

  /* init left-side encryption with offered enc and our enc, munge accept */
  switch(item->id) {
  case mwCipher_RC2_128: {
    mpz_t remote, shared;
    struct mwOpaque k;
    struct mwEncryptItem *offered;

    mpz_init(remote);
    mpz_init(shared);

    offered = find_item(chan->offer.items, mwCipher_RC2_128);
    mwDHImportKey(remote, &offered->info);
    mwDHCalculateShared(shared, remote, private_key);
    mwDHExportKey(shared, &k);

    mwIVInit(&chan->left_enc.hard->outgoing_iv);
    mwIVInit(&chan->left_enc.hard->incoming_iv);
    mwKeyExpand(chan->left_enc.hard->shared_key, k.data+(k.len-16), 16);

    mpz_clear(remote);
    mpz_clear(shared);

    mwOpaque_clear(&item->info);
    mwOpaque_clone(&item->info, &k);
    mwOpaque_clear(&k);
    break;
  }
  case mwCipher_RC2_40:
  default:
    break;
  }

  put_msg(MW_MESSAGE(msg), &o);
  OTHER_SIDE(side)->forward(o.data, o.len);
  mwOpaque_clear(&o);
}


static void munge_channel(struct proxy_side *side,
			  struct mwMsgChannelSend *msg) {

  struct mwOpaque o = {0,0};

  if(! msg->head.options & mwMessageOption_ENCRYPT) {
    
  } else {
    /* decrypt from side */
    /* display */
    /* encrypt to other side */
  }

  put_msg(MW_MESSAGE(msg), &o);
  OTHER_SIDE(side)->forward(o.data, o.len);
  mwOpaque_clear(&o);
}


/* handle destruction of a channel */
static void handle_destroy(struct proxy_side *side,
			   struct mwMsgChannelDestroy *msg) {

  struct channel *chan;
  GList *l;

  chan = STEAL_CHANNEL(msg->channel);

  for(l = chan->offer.items; l; l = l->next) {
    mwEncryptItem_free(l->data);
  }
  g_list_free(chan->offer.items);

  g_free(chan->creator);
  g_free(chan);
}


/* handle messages from either side */
static void side_process(struct proxy_side *s, const char *buf, gsize len) {
  struct mwOpaque o = { .len = len, .data = (char *) buf };
  struct mwGetBuffer *b;
  guint16 type;

  if(! len) return;

  b = mwGetBuffer_wrap(&o);
  type = guint16_peek(b);

  switch(type) {
  case mwMessage_LOGIN: {
    struct mwMsgLogin *msg = (struct mwMsgLogin *) mwMessage_get(b);
    mwKeyExpand(session_key, msg->name, 5);
    mwMessage_free(msg);
    goto default;
    break;
  }

  case mwMessage_LOGIN_REDIRECT: {
    munge_redir();
    break;
  }

  case mwMessage_CHANNEL_CREATE: {
    struct mwMessage *msg = mwMessage_get(b);
    munge_create(s, (struct mwMsgChannelCreate *) msg);
    mwMessage_free(msg);
    break;
  }

  case mwMessage_CHANNEL_ACCEPT: {
    struct mwMessage *msg = mwMessage_get(b);
    munge_accept(s, (struct mwMsgChannelAccept *) msg);
    mwMessage_free(msg);
    break;
  }

  case mwMessage_CHANNEL_DESTROY: {
    struct mwMessage *msg = mwMessage_get(b);
    handle_destroy(s, (struct mwMsgChannelDestroy *) msg);
    mwMessage_free(msg);
    goto default;
    break;
  }

  case mwMessage_CHANNEL_SEND: {
    struct mwMessage *msg = mwMessage_get(b);
    munge_channel(s, (struct mwMsgChannelSend *) msg);
    mwMessage_free(msg);
    break;
  }
    
  default: {
    struct mwPutBuffer *pb = mwPutBuffer_new();
    struct mwOpaque po = { 0, 0 };
    mwOpaque_put(pb, &o);
    mwPutBuffer_finalize(&po, pb);
    s->forward(po.data, po.len);
    mwOpaque_clear(&po);
  }
  }

  mwGetBuffer_free(b);
}


#define ADVANCE(b, n, count) { b += count; n -= count; }


/* handle input to complete an existing buffer */
static gsize side_recv_cont(struct proxy_side *s, const char *b, gsize n) {

  gsize x = s->buf_size - s->buf_recv;

  if(n < x) {
    memcpy(s->buf+s->buf_recv, b, n);
    s->buf_recv += n;
    return 0;
    
  } else {
    memcpy(s->buf+s->buf_recv, b, x);
    ADVANCE(b, n, x);
    
    if(s->buf_size == 4) {
      struct mwOpaque o = { .len = 4, .data = s->buf };
      struct mwGetBuffer *gb = mwGetBuffer_wrap(&o);
      x = guint32_peek(gb);
      mwGetBuffer_free(gb);

      if(n < x) {
	char *t;
	x += 4;
	t = (char *) g_malloc(x);
	memcpy(t, s->buf, 4);
	memcpy(t+4, b, n);
	
	side_buf_free(s);
	
	s->buf = t;
	s->buf_size = x;
	s->buf_recv = n + 4;
	return 0;
	
      } else {
	side_buf_free(s);
	side_process(s, b, x);
	ADVANCE(b, n, x);
      }
      
    } else {
      side_process(s, s->buf+4, s->buf_size-4);
      side_buf_free(s);
    }
  }

  return n;
}


/* handle input when there's nothing previously buffered */
static gsize side_recv_empty(struct proxy_side *s, const char *b, gsize n) {
  struct mwOpaque o = { .len = n, .data = (char *) b };
  struct mwGetBuffer *gb;
  gsize x;

  if(n < 4) {
    s->buf = (char *) g_malloc0(4);
    memcpy(s->buf, b, n);
    s->buf_size = 4;
    s->buf_recv = n;
    return 0;
  }
  
  gb = mwGetBuffer_wrap(&o);
  x = guint32_peek(gb);
  mwGetBuffer_free(gb);
  if(! x) return n - 4;

  if(n < (x + 4)) {

    x += 4;
    s->buf = (char *) g_malloc(x);
    memcpy(s->buf, b, n);
    s->buf_size = x;
    s->buf_recv = n;
    return 0;
    
  } else {
    ADVANCE(b, n, 4);
    side_process(s, b, x);
    ADVANCE(b, n, x);

    return n;
  }
}


static gsize side_recv(struct proxy_side *s, const char *b, gsize n) {

  if(n && (s->buf_size == 0) && (*b & 0x80)) {
    ADVANCE(b, n, 1);
  }

  if(n == 0) {
    return 0;

  } else if(s->buf_size > 0) {
    return side_recv_cont(s, b, n);

  } else {
    return side_recv_empty(s, b, n);
  }
}


static void feed_buf(struct proxy_side *side, const char *buf, gsize n) {
  char *b = (char *) buf;
  gsize remain = 0;
  
  g_return_if_fail(side != NULL);

  while(n > 0) {
    remain = side_recv(side, b, n);
    b += (n - remain);
    n = remain;
  }
}


static int read_recv(struct proxy_side *side) {
  char buf[2048];
  int len;

  len = read(side->sock, buf, 2048);
  if(len > 0) feed_buf(side, buf, (gsize) len);

  return len;
}


static void done() {
  close(client.sock);
  close(server.sock);
  exit(0);
}


static gboolean read_cb(GIOChannel *chan,
			GIOCondition cond,
			gpointer data) {

  struct proxy_side *side = data;
  int ret = 0;

  if(cond & G_IO_IN) {
    ret = read_recv(side);
    if(ret > 0) return TRUE;
  }

  if(side->sock) {
    g_source_remove(side->chan_io);
    close(side->sock);
    side->sock = 0;
    side->chan = NULL;
    side->chan_io = 0;
  }

  done();
  
  return FALSE;
}


static void client_cb(const char *buf, gsize len) {
  if(server.sock) {
    hexout("client -> server", buf, len);
    write(server.sock, buf, len);
  }
}


static gboolean listen_cb(GIOChannel *chan,
			  GIOCondition cond,
			  gpointer data) {

  struct sockaddr_in rem;
  int len = sizeof(rem);
  struct proxy_side *side = data;
  int sock;
  
  sock = accept(side->sock, (struct sockaddr *) &rem, &len);
  g_assert(sock > 0);

  g_source_remove(side->chan_io);
  side->sock = sock;
  side->chan = g_io_channel_unix_new(sock);
  side->chan_io = g_io_add_watch(side->chan, G_IO_IN | G_IO_ERR | G_IO_HUP,
				 read_cb, side);

  return FALSE;
}


static void init_client(int port) {
  /* start listening on the local port specifier */

  struct sockaddr_in sin;
  int sock;

  sock = socket(PF_INET, SOCK_STREAM, 0);
  g_assert(sock >= 0);

  memset(&sin, 0, sizeof(struct sockaddr_in));
  sin.sin_family = PF_INET;
  sin.sin_port = htons(port);
  sin.sin_addr.s_addr = htonl(INADDR_ANY);

  if(bind(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0)
    g_assert_not_reached();

  if(listen(sock, 1) < 0)
    g_assert_not_reached();

  client.forward = client_cb;
  client.sock = sock;
  client.chan = g_io_channel_unix_new(sock);
  client.chan_io = g_io_add_watch(client.chan, G_IO_IN | G_IO_ERR | G_IO_HUP,
				  listen_cb, &client);
}


static void server_cb(const char *buf, gsize len) {
  if(client.sock) {
    hexout("server -> client", buf, len);
    write(client.sock, buf, len);
  }
}


/* address lookup used by init_sock */
static void init_sockaddr(struct sockaddr_in *addr,
			  const char *host, int port) {

  struct hostent *hostinfo;

  addr->sin_family = AF_INET;
  addr->sin_port = htons (port);
  hostinfo = gethostbyname(host);
  if(hostinfo == NULL) {
    fprintf(stderr, "Unknown host %s.\n", host);
    exit(1);
  }
  addr->sin_addr = *(struct in_addr *) hostinfo->h_addr;
}


/* connect to server on host:port */
static void init_server(const char *host, int port) {
  struct sockaddr_in srvrname;
  int sock;

  sock = socket(PF_INET, SOCK_STREAM, 0);
  if(sock < 0) {
    fprintf(stderr, "socket failure");
    exit(1);
  }
  
  init_sockaddr(&srvrname, host, port);
  connect(sock, (struct sockaddr *)&srvrname, sizeof(srvrname));

  server.forward = server_cb;
  server.sock = sock;
  server.chan = g_io_channel_unix_new(sock);
  server.chan_io = g_io_add_watch(server.chan, G_IO_IN | G_IO_ERR | G_IO_HUP,
				  read_cb, &server);
}


int main(int argc, char *argv[]) {
  char *host = NULL;
  int client_port = 0, server_port = 0;

  memset(&client, 0, sizeof(struct proxy_side));
  memset(&server, 0, sizeof(struct proxy_side));

  if(argc > 1) {
    char *z;

    host = argv[1];
    z = host;

    host = strchr(z, ':');
    if(host) *host++ = '\0';
    client_port = atoi(z);

    z = strchr(host, ':');
    if(z) *z++ = '\0';
    server_port = atoi(z);
  }

  if(!host || !*host || !client_port || !server_port) {
    fprintf(stderr,
	    ( " Usage: %s local_port:remote_host:remote_port\n"
	      " Creates a locally-running sametime proxy which enforces"
	      " unencrypted channels\n" ),
	    argv[0]);
    exit(1);
  }

  /* @todo create signal handlers to cleanup sockets */

  channels = g_hash_table_new(g_direct_hash, g_direct_equals);

  init_rc2_128();
  init_client(client_port);
  init_server(host, server_port);

  g_main_loop_run(g_main_loop_new(NULL, FALSE)); 
  return 0;
}

