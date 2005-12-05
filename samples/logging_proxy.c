
/*
  Logging Sametime Proxy Utility
  The Meanwhile Project

  This is a tool which can act as a proxy between a client and a
  sametime server, which will log all messages to stdout. It will also
  munge channel creation messages in order to be able to decrypt any
  encrypted data sent over a channel, and will log decrypted chunks to
  stdout as well. This makes reverse-engineering of services much,
  much easier.

  The idea is simple, but the implementation made my head hurt.

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


/** one side of the proxy (either the client side or the server
    side). The forward method for one should push data into the socket
    of the other. */
struct proxy_side {
  int sock;
  GIOChannel *chan;
  gint chan_io;

  guchar *buf;
  gsize buf_size;
  gsize buf_recv;

  void (*forward)(const guchar *buf, gsize len);
};


static struct proxy_side client;  /**< side facing the client */
static struct proxy_side server;  /**< side facing the server */


static char *host = NULL;
static int client_port = 0;
static int server_port = 0;


static int counter = 0;
static int listen_sock = 0;
static GIOChannel *listen_chan = NULL;
static gint listen_io = 0;





/** given one side, get the other */
#define OTHER_SIDE(side) \
  ((side == &client)? &server: &client)


/** encryption state information used in the RC2/40 cipher */
struct rc2_40enc {
  guchar outgoing_iv[8];
  int outgoing_key[64];
  guchar incoming_iv[8];
  int incoming_key[64];
};


/* re-usable rc2 40 stuff */
static int session_key[64] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};


/** encryption state information used in the RC2/128 cipher */
struct rc2_128enc {
  guchar outgoing_iv[8];
  guchar incoming_iv[8];
  int shared_key[64];
};


/* re-usable rc2 128 stuff */
static struct mwMpi *private_key;
static struct mwOpaque public_key;


/** represents a channel. The channel has a left side and a right
    side. The left side is the creator of the channel. The right side
    is the target of the channel. Each side has its own encryption
    state information, so an incoming message from either side can
    be decrypted, then re-encrypted for the other side. */
struct channel {
  guint32 id;

  /* login id of creator or NULL if created by client side */
  char *creator;

  /* the offer from the left side */
  struct mwEncryptOffer offer;

  /** the mode of encryption */
  enum {
    enc_none = 0,  /**< nothing encrypted */
    enc_easy,      /**< easy (rc2/40) encryption */
    enc_hard,      /**< hard (rc2/128) encryption */
  } enc_mode;

  /** encryption data for the left side */
  union {
    struct rc2_40enc easy;
    struct rc2_128enc hard;
  } left_enc;

  /** encryption data for the right side */
  union {
    struct rc2_40enc easy;
    struct rc2_128enc hard;
  } right_enc;

  struct proxy_side *left;   /**< proxy side acting as the left side */
  struct proxy_side *right;  /**< proxy side acting as the right side */
};


/* collection of channels */
static GHashTable *channels;


#define PUT_CHANNEL(chan) \
  g_hash_table_insert(channels, GUINT_TO_POINTER((chan)->id), (chan))

#define GET_CHANNEL(id) \
  g_hash_table_lookup(channels, GUINT_TO_POINTER(id))

#define REMOVE_CHANNEL(id) \
  g_hash_table_remove(channels, GUINT_TO_POINTER(id))


/** print a message to stdout and use hexdump to print a data chunk */
static void hexdump_vprintf(const guchar *buf, gsize len,
			    const char *txt, va_list args) {
  FILE *fp;

  if(txt) {
    fputc('\n', stdout);
    vfprintf(stdout, txt, args);
    fputc('\n', stdout);
  }
  fflush(stdout);

  fp = popen("hexdump -C", "w");
  fwrite(buf, len, 1, fp);
  fflush(fp);
  pclose(fp);
}


/** print a message to stdout and use hexdump to print a data chunk */
static void hexdump_printf(const guchar *buf, gsize len,
			   const char *txt, ...) {
  va_list args;
  va_start(args, txt);
  hexdump_vprintf(buf, len, txt, args);
  va_end(args);
}


/** serialize a message for sending */
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


static void key_copy(int to[64], int from[64]) {
  int i = 64;
  while(i--) to[i] = from[i];
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
    c->creator = g_strdup(msg->creator.login_id);
  }

  /* record the mode and encryption items */
  c->offer.mode = msg->encrypt.mode;
  c->offer.items = msg->encrypt.items;
  c->offer.extra = msg->encrypt.extra;
  c->offer.flag = msg->encrypt.flag;

  PUT_CHANNEL(c);

  /* replace the encryption items with our own as applicable */
  if(msg->encrypt.items) {
    l = msg->encrypt.items;
    msg->encrypt.items = NULL; /* steal them */

    for(; l; l = l->next) {
      struct mwEncryptItem *i1, *i2;

      /* the original we've stolen */
      i1 = l->data;

      /* the munged replacement */
      i2 = g_new0(struct mwEncryptItem, 1);
      i2->id = i1->id;

      switch(i1->id) {
      case mwCipher_RC2_128:
	printf("munging an offered RC2/128\n");
	mwOpaque_clone(&i2->info, &public_key);
	break;
      case mwCipher_RC2_40:
	printf("munging an offered RC2/40\n");
      default:
	;
      }

      msg->encrypt.items = g_list_append(msg->encrypt.items, i2);
    }
  }

  put_msg(MW_MESSAGE(msg), &o);
  side->forward(o.data, o.len);
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
  item = msg->encrypt.item;

  if(! item) {
    /* cut to the chase */
    put_msg(MW_MESSAGE(msg), &o);
    side->forward(o.data, o.len);
    mwOpaque_clear(&o);
    return;
  }

  /* init right-side encryption with our enc and accepted enc */
  switch(item->id) {
  case mwCipher_RC2_128: {
    struct mwMpi *remote, *shared;
    struct mwOpaque k;

    remote = mwMpi_new();
    shared = mwMpi_new();

    printf("right side accepted RC2/128\n");

    mwMpi_import(remote, &item->info);
    mwMpi_calculateDHShared(shared, remote, private_key);
    mwMpi_export(shared, &k);

    chan->enc_mode = enc_hard;

    mwIV_init(chan->right_enc.hard.outgoing_iv);
    mwIV_init(chan->right_enc.hard.incoming_iv);
    mwKeyExpand(chan->right_enc.hard.shared_key, k.data+(k.len-16), 16);

    mwMpi_free(remote);
    mwMpi_free(shared);
    mwOpaque_clear(&k);
    break;
  }
  case mwCipher_RC2_40: {
    char *who;
    
    printf("right side accepted RC2/40\n");

    chan->enc_mode = enc_easy;

    mwIV_init(chan->right_enc.easy.outgoing_iv);
    mwIV_init(chan->right_enc.easy.incoming_iv);

    if(msg->acceptor_flag) {
      who = msg->acceptor.login_id;
      printf("right side is the server\n");
      printf("server is %s\n", who);
      mwKeyExpand(chan->right_enc.easy.incoming_key, (guchar *) who, 5);
      key_copy(chan->right_enc.easy.outgoing_key, session_key);

    } else {
      who = chan->creator;
      printf("right side is the client\n");
      printf("server is %s\n", who);
      key_copy(chan->right_enc.easy.incoming_key, session_key);
      mwKeyExpand(chan->right_enc.easy.outgoing_key, (guchar *) who, 5);
    }

    break;
  }
  default:
    chan->enc_mode = enc_none;
    break;
  }

  /* init left-side encryption with offered enc and our enc, munge accept */
  switch(item->id) {
  case mwCipher_RC2_128: {
    struct mwMpi *remote, *shared;
    struct mwOpaque k;
    struct mwEncryptItem *offered;
    
    remote = mwMpi_new();
    shared = mwMpi_new();

    printf("accepting left side with RC2/128\n");
    
    offered = find_item(chan->offer.items, mwCipher_RC2_128);
    mwMpi_import(remote, &offered->info);
    mwMpi_calculateDHShared(shared, remote, private_key);
    mwMpi_export(shared, &k);
    
    mwIV_init(chan->left_enc.hard.outgoing_iv);
    mwIV_init(chan->left_enc.hard.incoming_iv);
    mwKeyExpand(chan->left_enc.hard.shared_key, k.data+(k.len-16), 16);

    mwMpi_free(remote);
    mwMpi_free(shared);
    mwOpaque_clear(&k);
    
    /* munge accept with out public key */
    mwOpaque_clear(&item->info);
    mwOpaque_clone(&item->info, &public_key);
    break;
  }
  case mwCipher_RC2_40:
    printf("accepting left side with RC2/40\n");

    mwIV_init(chan->left_enc.easy.outgoing_iv);
    mwIV_init(chan->left_enc.easy.incoming_iv);

    key_copy(chan->left_enc.easy.outgoing_key,
	     chan->right_enc.easy.incoming_key);

    key_copy(chan->left_enc.easy.incoming_key,
	     chan->right_enc.easy.outgoing_key);
    break;
    
  default:
    ;
  }

  put_msg(MW_MESSAGE(msg), &o);
  side->forward(o.data, o.len);
  mwOpaque_clear(&o);
}


static void dec(struct channel *chan, struct proxy_side *side,
		struct mwOpaque *to, struct mwOpaque *from) {
  
  switch(chan->enc_mode) {
  case enc_easy: {
    if(chan->left == side) {
      /* left side decrypt */
      mwDecryptExpanded(chan->left_enc.easy.incoming_key,
			chan->left_enc.easy.incoming_iv,
			from, to);
    } else {
      /* right side decrypt */
      mwDecryptExpanded(chan->right_enc.easy.incoming_key,
			chan->right_enc.easy.incoming_iv,
			from, to);
    }
    break;
  }
  case enc_hard: {
    if(chan->left == side) {
      /* left side decrypt */
      mwDecryptExpanded(chan->left_enc.hard.shared_key,
			chan->left_enc.hard.incoming_iv,
			from, to);
    } else {
      /* right side decrypt */
      mwDecryptExpanded(chan->right_enc.hard.shared_key,
			chan->right_enc.hard.incoming_iv,
			from, to);
    }
    break;
  }
  }
}


static void enc(struct channel *chan, struct proxy_side *side,
		struct mwOpaque *to, struct mwOpaque *from) {

  switch(chan->enc_mode) {
  case enc_easy: {
    if(chan->left == side) {
      /* left side encrypt */
      mwEncryptExpanded(chan->left_enc.easy.outgoing_key,
			chan->left_enc.easy.outgoing_iv,
			from, to);
    } else {
      /* right side encrypt */
      mwEncryptExpanded(chan->right_enc.easy.outgoing_key,
			chan->right_enc.easy.outgoing_iv,
			from, to);
    }
    break;
  }
  case enc_hard: {
    if(chan->left == side) {
      /* left side encrypt */
      mwEncryptExpanded(chan->left_enc.hard.shared_key,
			chan->left_enc.hard.outgoing_iv,
			from, to);
    } else {
      /* right side encrypt */
      mwEncryptExpanded(chan->right_enc.hard.shared_key,
			chan->right_enc.hard.outgoing_iv,
			from, to);
    }
    break;
  }
  }
}


static void munge_channel(struct proxy_side *side,
			  struct mwMsgChannelSend *msg) {

  struct mwOpaque o = {0,0};

  if(msg->head.options & mwMessageOption_ENCRYPT) {
    struct mwOpaque d = {0,0};
    struct channel *chan;

    chan = GET_CHANNEL(msg->head.channel);

    /* decrypt from side */
    dec(chan, side, &d, &msg->data);

    /* display */
    hexdump_printf(d.data, d.len, "decrypted channel message data:",
		   msg->type);

    /* encrypt to other side */
    mwOpaque_clear(&msg->data);
    enc(chan, OTHER_SIDE(side), &msg->data, &d);
    mwOpaque_clear(&d);
  }

  /* send to other side */
  put_msg(MW_MESSAGE(msg), &o);
  side->forward(o.data, o.len);
  mwOpaque_clear(&o);
}


/* handle destruction of a channel */
static void handle_destroy(struct proxy_side *side,
			   struct mwMsgChannelDestroy *msg) {

  struct channel *chan;
  GList *l;

  chan = GET_CHANNEL(msg->head.channel);
  REMOVE_CHANNEL(msg->head.channel);

  if(! chan) return;

  for(l = chan->offer.items; l; l = l->next) {
    mwEncryptItem_free(l->data);
  }
  g_list_free(chan->offer.items);
  chan->offer.items = NULL;

  g_free(chan->creator);
  chan->creator = NULL;

  g_free(chan);
}


static void forward(struct proxy_side *to,
		    struct mwOpaque *data) {

  struct mwPutBuffer *pb = mwPutBuffer_new();
  struct mwOpaque po = { 0, 0 };

  mwOpaque_put(pb, data);
  mwPutBuffer_finalize(&po, pb);
  to->forward(po.data, po.len);
  mwOpaque_clear(&po);
}


/* handle messages from either side */
static void side_process(struct proxy_side *s, const guchar *buf, gsize len) {
  struct mwOpaque o = { .len = len, .data = (guchar *) buf };
  struct mwGetBuffer *b;
  guint16 type;

  if(! len) return;

  if(s == &server) {
    hexdump_printf(buf, len, "server -> client");
  } else {
    hexdump_printf(buf, len, "client -> server");
  }

  b = mwGetBuffer_wrap(&o);
  type = guint16_peek(b);

  switch(type) {
  case mwMessage_LOGIN_ACK: {
    struct mwMsgLoginAck *msg = (struct mwMsgLoginAck *) mwMessage_get(b);
    printf("client is %s\n", msg->login.login_id);
    mwKeyExpand(session_key, (guchar *) msg->login.login_id, 5);
    mwMessage_free(MW_MESSAGE(msg));
    forward(s, &o);
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
    forward(s, &o);
    break;
  }

  case mwMessage_CHANNEL_SEND: {
    struct mwMessage *msg = mwMessage_get(b);
    munge_channel(s, (struct mwMsgChannelSend *) msg);
    mwMessage_free(msg);
    break;
  }
    
  default:
    forward(s, &o);
  }

  mwGetBuffer_free(b);
}


/** clean up a proxy side's inner buffer */
static void side_buf_free(struct proxy_side *s) {
  g_free(s->buf);
  s->buf = NULL;
  s->buf_size = 0;
  s->buf_recv = 0;
}


#define ADVANCE(b, n, count) { b += count; n -= count; }


/** handle input to complete an existing buffer */
static gsize side_recv_cont(struct proxy_side *s, const guchar *b, gsize n) {

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
	guchar *t;
	x += 4;
	t = (guchar *) g_malloc(x);
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


/** handle input when there's nothing previously buffered */
static gsize side_recv_empty(struct proxy_side *s, const guchar *b, gsize n) {
  struct mwOpaque o = { .len = n, .data = (guchar *) b };
  struct mwGetBuffer *gb;
  gsize x;

  if(n < 4) {
    s->buf = (guchar *) g_malloc0(4);
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
    s->buf = (guchar *) g_malloc(x);
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


/** handle input in chunks */
static gsize side_recv(struct proxy_side *s, const guchar *b, gsize n) {

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


/** handle input */
static void feed_buf(struct proxy_side *side, const guchar *buf, gsize n) {
  guchar *b = (guchar *) buf;
  gsize remain = 0;
  
  g_return_if_fail(side != NULL);

  while(n > 0) {
    remain = side_recv(side, b, n);
    b += (n - remain);
    n = remain;
  }
}


static int read_recv(struct proxy_side *side) {
  guchar buf[2048];
  int len;

  len = read(side->sock, buf, 2048);
  if(len > 0) feed_buf(side, buf, (gsize) len);

  return len;
}


static void init_listen();


static void side_done(struct proxy_side *side) {
  if(side->sock) {
    g_source_remove(side->chan_io);
    close(side->sock);
    side->sock = 0;
    side->chan = NULL;
    side->chan_io = 0;
  }
}


static void done() {
  printf("closing connection\n");

  side_done(&client);
  side_done(&server);

  if(counter--) {
    init_listen();
  } else {
    exit(0);
  }
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

  done();
  
  return FALSE;
}


static void client_cb(const guchar *buf, gsize len) {
  if(server.sock) write(server.sock, buf, len);
}


/** setup the client */
static void init_client(int sock) {

  client.forward = client_cb;
  client.sock = sock;
  client.chan = g_io_channel_unix_new(sock);
  client.chan_io = g_io_add_watch(client.chan, G_IO_IN | G_IO_ERR | G_IO_HUP,
				  read_cb, &client);
}


static void server_cb(const guchar *buf, gsize len) {
  if(client.sock) write(client.sock, buf, len);
}


/** generate a private/public DH keypair for internal (re)use */
static void init_rc2_128() {
  struct mwMpi *public;

  private_key = mwMpi_new();
  public = mwMpi_new();

  mwMpi_randDHKeypair(private_key, public);
  mwMpi_export(public, &public_key);

  mwMpi_free(public);
}


/** address lookup used by init_sock */
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


/** connect to server on host:port */
static void init_server() {
  struct sockaddr_in srvrname;
  int sock;

  printf("connecting to %s:%i\n", host, server_port);

  sock = socket(PF_INET, SOCK_STREAM, 0);
  if(sock < 0) {
    fprintf(stderr, "socket failure");
    exit(1);
  }
  
  init_sockaddr(&srvrname, host, server_port);
  connect(sock, (struct sockaddr *)&srvrname, sizeof(srvrname));

  server.forward = server_cb;
  server.sock = sock;
  server.chan = g_io_channel_unix_new(sock);
  server.chan_io = g_io_add_watch(server.chan, G_IO_IN | G_IO_ERR | G_IO_HUP,
				  read_cb, &server);

  printf("connected to %s:%i\n", host, server_port);
}



static gboolean listen_cb(GIOChannel *chan,
			  GIOCondition cond,
			  gpointer data) {

  struct sockaddr_in rem;
  guint len = sizeof(rem);
  int sock;
  
  printf("got connection\n");

  sock = accept(listen_sock, (struct sockaddr *) &rem, &len);
  /* g_assert(sock > 0); */

  init_server();
  init_client(sock);

  listen_io = 0;

  return FALSE;
}


static void init_listen() {
  if(! listen_sock) {
    struct sockaddr_in sin;
    int sock;

    sock = socket(PF_INET, SOCK_STREAM, 0);
    g_assert(sock >= 0);

    memset(&sin, 0, sizeof(struct sockaddr_in));
    sin.sin_family = PF_INET;
    sin.sin_port = htons(client_port);
    sin.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0)
      g_assert_not_reached();

    if(listen(sock, 1) < 0)
      g_assert_not_reached();

    listen_sock = sock;
    listen_chan = g_io_channel_unix_new(sock);
  }

  if(! listen_io) {
    listen_io = g_io_add_watch(listen_chan, G_IO_IN | G_IO_ERR | G_IO_HUP,
			       listen_cb, NULL);
    printf("listening on port %i\n", client_port);
  }
}


int main(int argc, char *argv[]) {

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

  if(argc > 2) {
    counter = atoi(argv[2]);
  }

  if(!host || !*host || !client_port || !server_port) {
    fprintf(stderr,
	    ( " Usage: %s local_port:remote_host:remote_port [n]\n"
	      " Creates a locally-running sametime proxy which enforces"
	      " unencrypted channels. Will drop the first n connections\n" ),
	    argv[0]);
    exit(1);
  }

  /* @todo create signal handlers to cleanup sockets */

  channels = g_hash_table_new(g_direct_hash, g_direct_equal);

  init_rc2_128();
  init_listen();

  g_main_loop_run(g_main_loop_new(NULL, FALSE)); 
  return 0;
}

