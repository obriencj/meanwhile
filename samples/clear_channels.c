
/*
  Clear Channel Sametime Proxy Utility
  The Meanwhile Project

  This is a tool which can act as a proxy between a client and a
  sametime server, which will force all channels to be created without
  any encryption method. This makes reverse-engineering much, much
  easier.

  It also outputs the messages sent to and from the client to stdout
  as hex pairs.
  
  All it really does is nab all Channel Create messages, strip the
  offered ciphers portion from the message and replace it with an
  empty ciphers list.

  TODO: also catch Login Redirect messages and auto-reply with a Force
  Login message

  Christopher O'Brien <siege@preoccupied.net>
*/


#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <glib.h>
#include <glib/glist.h>

#include <mw_common.h>
#include <mw_message.h>


struct proxy_side {
  int sock;
  GIOChannel *chan;
  gint chan_io;

  char *buf;
  gsize buf_size;
  gsize buf_recv;

  void (*forward)(const char *buf, gsize len);
};


static struct proxy_side client;
static struct proxy_side server;


#define HX   "%02x"
#define HX1  HX "   "
#define HX2  HX HX " "
#define HX4  HX2 HX2
#define HX8  HX4 HX4
#define HXF  HX8 HX8


static void hexout(const char *txt, const unsigned char *buf, gsize len) {
#if USE_HEXDUMP

  FILE *fp;

  if(txt) fprintf(stdout, "\n%s\n", txt);
  fflush(stdout);

  fp = popen("hexdump -C", "w");
  fwrite(buf, len, 1, fp);
  fflush(fp);
  pclose(fp);

#else

  if(txt) printf("\n%s\n", txt);

  while(len >= 16) {
    /* printf("%s\n", HXF); */
    printf((HXF),
	   buf[0],  buf[1],  buf[2],  buf[3],
	   buf[4],  buf[5],  buf[6],  buf[7],
	   buf[8],  buf[9],  buf[10], buf[11],
	   buf[12], buf[13], buf[14], buf[15]);
    buf += 16;
    len -= 16;

    if(len) putchar('\n');
  }

  if(len >= 8) {
    /* printf("%s\n", HX8); */
    printf((HX8),
	    buf[0],  buf[1],  buf[2],  buf[3],
	    buf[4],  buf[5],  buf[6],  buf[7]);
    buf += 8;
    len -= 8;
  }
  
  if(len >= 4) {
    /* printf("%s\n", HX4); */
    printf((HX4), buf[0],  buf[1],  buf[2],  buf[3]);
    buf += 4;
    len -= 4;
  }
  
  if(len >= 2) {
    /* printf("%s\n", HX2); */
    printf((HX2), buf[0],  buf[1]);
    buf += 2;
    len -= 2;
  }
  
  if(len) {
    /* printf("%s\n", HX1); */
    printf((HX1), buf[0]);
    buf++;
    len--;
  }

  putchar('\n');
#endif
}


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


static void munge_redir() {
  struct mwMessage *msg;
  struct mwOpaque o = { 0, 0 };

  msg = mwMessage_new(mwMessage_LOGIN_CONTINUE);
  put_msg(msg, &o);
  mwMessage_free(msg);

  server.forward(o.data, o.len);

  mwOpaque_clear(&o);
}


static void munge_create(struct proxy_side *side,
			 struct mwMsgChannelCreate *msg) {

  struct mwOpaque o = { 0, 0 };
  GList *l;

  for(l = msg->encrypt.items; l; l = l->next) {
    mwEncryptItem_clear(l->data);
    g_free(l->data);
  }
  g_list_free(msg->encrypt.items);
  msg->encrypt.items = NULL;

  msg->encrypt.mode = 0x00;
  msg->encrypt.extra = 0x00;
  msg->encrypt.flag = FALSE;

  put_msg(MW_MESSAGE(msg), &o);
  side->forward(o.data, o.len);
  mwOpaque_clear(&o);
}


static void side_process(struct proxy_side *s, const char *buf, gsize len) {
  struct mwOpaque o = { .len = len, .data = (char *) buf };
  struct mwGetBuffer *b;
  guint16 type;

  if(! len) return;

  b = mwGetBuffer_wrap(&o);
  type = guint16_peek(b);

  switch(type) {
  case mwMessage_LOGIN_REDIRECT:
    munge_redir();
    break;

  case mwMessage_CHANNEL_CREATE:
    {
      struct mwMessage *msg = mwMessage_get(b);
      munge_create(s, (struct mwMsgChannelCreate *) msg);
      mwMessage_free(msg);
      break;
    }
    
  default:
    {
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


static void init_server(const char *host, int port) {
  /* connect to server on host/port */
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

  init_client(client_port);
  init_server(host, server_port);

  g_main_loop_run(g_main_loop_new(NULL, FALSE)); 
  return 0;
}

