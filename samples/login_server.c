
/*
  Login-parsing Faux Server
  The Meanwhile Project

  This is a tool to aide in reverse engineering different types of
  authentication schemes.

  Christopher O'Brien <siege@preoccupied.net>
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>

#include <glib.h>
#include <glib/glist.h>

#include <gmp.h>

#include <mw_common.h>
#include <mw_message.h>


/** the server socket or the connected socket */
static int sock;

/** the io channel */
static GIOChannel *chan;

/** the listening event on the io channel */
static int chan_io;


static char *sbuf;
static gsize sbuf_size;
static gsize sbuf_recv;


mpz_t private, public;


static void hexout(const char *txt, const unsigned char *buf, gsize len) {
  FILE *fp;

  if(txt) fprintf(stdout, "\n%s\n", txt);
  fflush(stdout);

  fp = popen("hexdump -C", "w");
  fwrite(buf, len, 1, fp);
  fflush(fp);
  pclose(fp);
}


static void send_msg(struct mwMessage *msg) {
  struct mwPutBuffer *b;
  struct mwOpaque o = { 0, 0 };

  b = mwPutBuffer_new();
  mwMessage_put(b, msg);
  mwPutBuffer_finalize(&o, b);

  b = mwPutBuffer_new();
  mwOpaque_put(b, &o);
  mwOpaque_clear(&o);
  mwPutBuffer_finalize(&o, b);

  if(sock) write(sock, o.data, o.len);

  hexout("sent:", o.data, o.len);

  mwOpaque_clear(&o);
}


static void handshake_ack() {
  struct mwMsgHandshakeAck *msg;

  msg = (struct mwMsgHandshakeAck *)
    mwMessage_new(mwMessage_HANDSHAKE_ACK);

  mwDHRandKeypair(private, public);

  mwDHExportKey(public, &msg->data);

  send_msg(MW_MESSAGE(msg));
  mwMessage_free(MW_MESSAGE(msg));
}


static void handle_login(struct mwMsgLogin *msg) {
  struct mwGetBuffer *gb;
  struct mwOpaque a, b, c;
  guint16 z;
  mpz_t remote, shared;
  char iv[8];

  mpz_init(remote);
  mpz_init(shared);

  mwIV_init(iv);

  gb = mwGetBuffer_wrap(&msg->auth_data);
  guint16_get(gb, &z);
  mwOpaque_get(gb, &a);
  mwOpaque_get(gb, &b);
  mwGetBuffer_free(gb);

  mwDHImportKey(remote, &a);
  mwOpaque_clear(&a);

  mwDHCalculateShared(shared, remote, private);
  mwDHExportKey(shared, &a);

  mwDecrypt(a.data+(a.len-16), a.len, iv, &b, &c);
  hexout("decrypted to:", c.data, c.len);

  mwOpaque_clear(&a);
  mwOpaque_clear(&b);
  mwOpaque_clear(&c);

  mpz_clear(remote);
  mpz_clear(shared);
}


static void done() {
  close(sock);
  exit(0);
}


static void side_process(const char *buf, gsize len) {
  struct mwOpaque o = { .len = len, .data = (char *) buf };
  struct mwGetBuffer *b;
  guint16 type;

  if(! len) return;

  b = mwGetBuffer_wrap(&o);
  type = guint16_peek(b);

  hexout("received:", buf, len);

  switch(type) {
  case mwMessage_HANDSHAKE:
    printf("got handshake, sending handshake_ack\n");
    handshake_ack();
    break;

  case mwMessage_LOGIN:
    printf("got login, attempting to decipher\n");
    {
      struct mwMsgLogin *msg = (struct mwMsgLogin *) mwMessage_get(b);
      handle_login(msg);
      mwMessage_free(MW_MESSAGE(msg));
      done();
    }
    break;

  case mwMessage_CHANNEL_DESTROY:
    printf("channel destroy\n");
    done();
    break;

  default:
    ;
  }

  mwGetBuffer_free(b);
}


static void sbuf_free() {
  g_free(sbuf);
  sbuf = NULL;
  sbuf_size = 0;
  sbuf_recv = 0;
}


#define ADVANCE(b, n, count) { b += count; n -= count; }


/* handle input to complete an existing buffer */
static gsize side_recv_cont(const char *b, gsize n) {

  gsize x = sbuf_size - sbuf_recv;

  if(n < x) {
    memcpy(sbuf + sbuf_recv, b, n);
    sbuf_recv += n;
    return 0;
    
  } else {
    memcpy(sbuf + sbuf_recv, b, x);
    ADVANCE(b, n, x);
    
    if(sbuf_size == 4) {
      struct mwOpaque o = { .len = 4, .data = sbuf };
      struct mwGetBuffer *gb = mwGetBuffer_wrap(&o);
      x = guint32_peek(gb);
      mwGetBuffer_free(gb);

      if(n < x) {
	char *t;
	x += 4;
	t = (char *) g_malloc(x);
	memcpy(t, sbuf, 4);
	memcpy(t+4, b, n);
	
	sbuf_free();
	
	sbuf = t;
	sbuf_size = x;
	sbuf_recv = n + 4;
	return 0;
	
      } else {
	sbuf_free();
	side_process(b, x);
	ADVANCE(b, n, x);
      }
      
    } else {
      side_process(sbuf+4, sbuf_size-4);
      sbuf_free();
    }
  }

  return n;
}


/* handle input when there's nothing previously buffered */
static gsize side_recv_empty(const char *b, gsize n) {
  struct mwOpaque o = { .len = n, .data = (char *) b };
  struct mwGetBuffer *gb;
  gsize x;

  if(n < 4) {
    sbuf = (char *) g_malloc0(4);
    memcpy(sbuf, b, n);
    sbuf_size = 4;
    sbuf_recv = n;
    return 0;
  }
  
  gb = mwGetBuffer_wrap(&o);
  x = guint32_peek(gb);
  mwGetBuffer_free(gb);
  if(! x) return n - 4;

  if(n < (x + 4)) {

    x += 4;
    sbuf = (char *) g_malloc(x);
    memcpy(sbuf, b, n);
    sbuf_size = x;
    sbuf_recv = n;
    return 0;
    
  } else {
    ADVANCE(b, n, 4);
    side_process(b, x);
    ADVANCE(b, n, x);

    return n;
  }
}


static gsize side_recv(const char *b, gsize n) {

  if(n && (sbuf_size == 0) && (*b & 0x80)) {
    ADVANCE(b, n, 1);
  }

  if(n == 0) {
    return 0;

  } else if(sbuf_size > 0) {
    return side_recv_cont(b, n);

  } else {
    return side_recv_empty(b, n);
  }
}


static void feed_buf(const char *buf, gsize n) {
  char *b = (char *) buf;
  gsize remain = 0;

  while(n > 0) {
    remain = side_recv(b, n);
    b += (n - remain);
    n = remain;
  }
}


static int read_recv() {
  char buf[2048];
  int len;

  len = read(sock, buf, 2048);
  if(len > 0) feed_buf(buf, (gsize) len);

  return len;
}


static gboolean read_cb(GIOChannel *chan,
			GIOCondition cond,
			gpointer data) {
  int ret = 0;

  if(cond & G_IO_IN) {
    ret = read_recv();
    if(ret > 0) return TRUE;
  }

  if(sock) {
    g_source_remove(chan_io);
    close(sock);
    sock = 0;
    chan = NULL;
    chan_io = 0;
  }

  done();
  
  return FALSE;
}


static gboolean listen_cb(GIOChannel *chan,
			  GIOCondition cond,
			  gpointer data) {

  struct sockaddr_in rem;
  int len = sizeof(rem);

  printf("accepting connection\n");
  
  sock = accept(sock, (struct sockaddr *) &rem, &len);
  g_assert(sock > 0);

  g_source_remove(chan_io);
  chan = g_io_channel_unix_new(sock);
  chan_io = g_io_add_watch(chan, G_IO_IN | G_IO_ERR | G_IO_HUP,
			   read_cb, NULL);
  
  return FALSE;
}


static void init_socket(int port) {
  /* start listening on the local port specifier */

  struct sockaddr_in sin;

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

  chan = g_io_channel_unix_new(sock);
  chan_io = g_io_add_watch(chan, G_IO_IN | G_IO_ERR | G_IO_HUP,
			   listen_cb, NULL);
}


int main(int argc, char *argv[]) {
  int port = 0;

  mpz_init(private);
  mpz_init(public);

  if(argc > 1) {
    port = atoi(argv[1]);
  }

  if(!port) {
    fprintf(stderr,
	    ( " Usage: %s local_port\n"
	      " Creates a locally-running sametime server which prints"
	      " login information to stdout\n" ),
	    argv[0]);
    exit(1);
  }

  /* @todo create signal handlers to cleanup socket */

  init_socket(port);

  g_main_loop_run(g_main_loop_new(NULL, FALSE)); 
  return 0;
}

