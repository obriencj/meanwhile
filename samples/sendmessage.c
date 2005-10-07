
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>

#include <glib.h>

#include <mw_common.h>
#include <mw_error.h>
#include <mw_service.h>
#include <mw_session.h>
#include <mw_srvc_im.h>


#define BUF_LEN 2048


/* client data should be put into a structure and associated with the
   session. Then it will be available from the many call-backs
   handling events from the session */
struct sample_client {
  struct mwSession *session;  /* the actual meanwhile session */
  int sock;                   /* the socket connecting to the server */
  int sock_event;             /* glib event id polling the socket */
  char *target;
  char *message;
};


/* handler function for when the session wants to close IO */
static void mw_session_io_close(struct mwSession *session) {
  struct sample_client *client;

  client = mwSession_getClientData(session);
  if(client->sock) {    
    g_source_remove(client->sock_event);
    close(client->sock);
    client->sock = 0;
    client->sock_event = 0;
  }
}


/* handler function for when the session wants to write data */
static int mw_session_io_write(struct mwSession *session,
			       const char *buf, gsize len) {

  struct sample_client *client;
  int ret = 0;

  client = mwSession_getClientData(session);

  /* socket was already closed. */
  if(client->sock == 0)
    return 1;

  while(len) {
    ret = write(client->sock, buf, len);
    if(ret <= 0) break;
    len -= ret;
  }

  if(len > 0) {
    /* stop watching the socket */
    g_source_remove(client->sock_event);

    /* close the socket */
    close(client->sock);

    /* remove traces of socket from client */
    client->sock = 0;
    client->sock_event = 0;

    /* return error code */
    return -1;
  }

  return 0;
}


/* handles when a conversation has been fully established between
   this client and another. */
static void mw_im_conversation_opened(struct mwConversation *conv) {
  struct mwServiceIm *srvc;
  struct mwSession *session;
  struct sample_client *client;
  struct mwIdBlock *idb;

  /* get a reference to the client data */
  srvc = mwConversation_getService(conv);
  session = mwService_getSession(MW_SERVICE(srvc));
  client = mwSession_getClientData(session);

  /* make sure that it's not someone just randomly IM-ing us... */
  idb = mwConversation_getTarget(conv);
  g_return_if_fail(! strcmp(client->target, idb->user));

  /* send the message and close the conversation */
  mwConversation_send(conv, mwImSend_PLAIN, client->message);
  mwConversation_close(conv, ERR_SUCCESS);

  /* the polite way to close everything up. Will call
     mw_session_stateChange after doing what needs to be done */
  mwSession_stop(session, ERR_SUCCESS);
}


static struct mwImHandler mw_im_handler = {
  .conversation_opened = mw_im_conversation_opened,
  .conversation_closed = NULL,
  .conversation_recv = NULL,
  .clear = NULL,
};


static void mw_session_stateChange(struct mwSession *session,
				   enum mwSessionState state,
				   gpointer info) {

  struct sample_client *client;
  struct mwServiceIm *service;
  struct mwConversation *conv;
  struct mwIdBlock idb;

  if(state == mwSession_STARTED) {
    /* session is now fully started */

    client = mwSession_getClientData(session);
    g_return_if_fail(client != NULL);

    /* create the im service, add it to the session, and start it up */
    service = mwServiceIm_new(session,&mw_im_handler);
    mwSession_addService(session, MW_SERVICE(service));
    mwService_start(MW_SERVICE(service));

    /* obtain a conversation with the specified user */
    idb.user = client->target;
    idb.community = NULL;
    conv = mwServiceIm_getConversation(service, &idb);

    /* and open it up. When it's finally opened, the
       conversation_opened handler for the IM service will be
       triggered */
    mwConversation_open(conv);

  } else if(state == mwSession_STOPPED) {
    /* session has stopped */
    
    if(info) {
      /* stopped due to an error */
      guint32 errcode;
      char *err;

      errcode = GPOINTER_TO_UINT(info);
      err = mwError(errcode);
      fprintf(stderr, "meanwhile error %s\n", err);
      g_free(err);

      exit(1);

    } else {
      exit(0);
    }
  }
}


/* the session handler structure is where you should indicate what
   functions will perform many of the functions necessary for the
   session to operate. Among these, only io_write and io_close are
   absolutely required. */
static struct mwSessionHandler mw_session_handler = {
  .io_write = mw_session_io_write,
  .io_close = mw_session_io_close,
  .clear = NULL,
  .on_stateChange = mw_session_stateChange,
  .on_setPrivacyInfo = NULL,
  .on_setUserStatus = NULL,
  .on_admin = NULL,
};


/** called from read_cb, attempts to read available data from sock and
    pass it to the session, passing back the return code from the read
    call for handling in read_cb */
static int read_recv(struct mwSession *session, int sock) {
  char buf[BUF_LEN];
  int len;

  len = read(sock, buf, BUF_LEN);
  if(len > 0) mwSession_recv(session, buf, len);

  return len;
}


/** callback triggered from gaim_input_add, watches the socked for
    available data to be processed by the session */
static gboolean read_cb(GIOChannel *chan,
			GIOCondition cond,
			gpointer data) {

  struct sample_client *client = data;
  int ret = 0;
  int source = g_io_channel_unix_get_fd(chan);

  if(cond & G_IO_IN) {
    ret = read_recv(client->session, client->sock);
  }

  /* normal operation ends here */
  if(ret > 0) return TRUE;

  /* read problem occured if we're here, so we'll need to take care of
     it and clean up internal state */

  if(client->sock) {
    g_source_remove(client->sock_event);
    close(client->sock);
    client->sock = 0;
    client->sock_event = 0;
  }

  return FALSE;
}


/* address lookup */
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


/* open a network socket to host:port */
static int init_sock(const char *host, int port) {
  struct sockaddr_in srvrname;
  int sock;

  sock = socket(PF_INET, SOCK_STREAM, 0);
  if(sock < 0) {
    perror("socket failure");
    exit(1);
  }
  
  init_sockaddr(&srvrname, host, port);
  connect(sock, (struct sockaddr *)&srvrname, sizeof(srvrname));

  return sock;
}


/* logging is redirected to here */
static void mw_log_handler(const gchar *domain, GLogLevelFlags flags,
			   const gchar *msg, gpointer data) {
#if DEBUG
  /* ok, debugging is enabled, so let's print it like normal */
  g_log_default_handler(domain, flags, msg, data);
#else
  ; /* nothing! very quiet */
#endif
}


int main(int argc, char *argv[]) {

  char *server;
  int portno;
  char *sender;
  char *password;
  char *recipient;
  char *message;

  /* the meanwhile session itself */
  struct mwSession *session;

  /* client program data */
  struct sample_client *client;

  /* something glib uses to watch the socket for available data */
  GIOChannel *io_chan;
  
  if (argc < 7) {
    fprintf(stderr,"usage %s:  server port sender password recipient message\n", argv[0]);
    exit(0);
  }
  
  server = argv[1];
  portno = atoi(argv[2]);
  sender = argv[3];
  password = argv[4];
  recipient = argv[5];
  message = argv[6];

  /* let's redirect the output of the glib logging facilities */
  g_log_set_handler("meanwhile",
		    G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION,
		    mw_log_handler, NULL);

  /* set up the session stuff */
  session = mwSession_new(&mw_session_handler);
  mwSession_setProperty(session, mwSession_AUTH_USER_ID, sender, NULL);
  mwSession_setProperty(session, mwSession_AUTH_PASSWORD, password, NULL);

  mwSession_setProperty(session, mwSession_CLIENT_TYPE_ID,
			GUINT_TO_POINTER(mwLogin_MEANWHILE), NULL);

  /* set up the client data structure with the things we need it to remember */
  client = g_new0(struct sample_client, 1);
  client->session = session;
  client->sock = init_sock(server, portno);
  client->target = recipient;
  client->message = message;

  /* associate the client data with the session */
  mwSession_setClientData(session, client, g_free);

  /* start the session up */
  mwSession_start(session);

  /* add a watch on the socket */
  io_chan = g_io_channel_unix_new(client->sock);
  client->sock_event = g_io_add_watch(io_chan, G_IO_IN | G_IO_ERR | G_IO_HUP,
				      read_cb, client);

  /* ... and start the glib loop */
  g_main_loop_run(g_main_loop_new(NULL, FALSE));
}
