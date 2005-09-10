
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



/**
   @file socket.c
   
   This file is a simple demonstration of using unix socket code to
   connect a mwSession to a sametime server and get it fully logged
   in.
   
   Here you'll find examples of:
    - opening a socket to the host
    - using the socket to feed data to the session
    - using a session handler to allow the session to write data to the
      socket
    - using a session handler to allow the session to close the socket
    - watching for error conditions on read/write
*/



#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <glib.h>

#include <mw_common.h>
#include <mw_session.h>



/** help text if you don't give the right number of arguments */
#define HELP \
"Meanwhile sample socket client\n" \
"Usage: %s server userid password\n" \
"\n" \
"Connects to a sametime server and logs in with the supplied user ID\n" \
"and password. Doesn't actually do anything useful after that.\n\n"



/** how much to read from the socket in a single call */
#define BUF_LEN 2048



/* client data should be put into a structure and associated with the
   session. Then it will be available from the many call-backs
   handling events from the session */
struct sample_client {
  struct mwSession *session;  /* the actual meanwhile session */
  int sock;                   /* the socket connecting to the server */
  int sock_event;             /* glib event id polling the socket */
};



/* the io_close function from the session handler */
static void mw_session_io_close(struct mwSession *session) {
  struct sample_client *client;

  /* get the client data from the session */
  client = mwSession_getClientData(session);

  /* safety check */
  g_return_if_fail(client != NULL);

  /* if the client still has a socket to be closed, close it */
  if(client->sock) {    
    g_source_remove(client->sock_event);
    close(client->sock);
    client->sock = 0;
    client->sock_event = 0;
  }
}



/* the io_write function from the session handler */
static int mw_session_io_write(struct mwSession *session,
			       const char *buf, gsize len) {

  struct sample_client *client;
  int ret = 0;

  /* get the client data from the session */
  client = mwSession_getClientData(session);

  /* safety check */
  g_return_val_if_fail(client != NULL, -1);

  /* socket was already closed, so we can't possibly write to it */
  if(client->sock == 0) return -1;

  /* write out the data to the socket until it's all gone */
  while(len) {
    ret = write(client->sock, buf, len);
    if(ret <= 0) break;
    len -= ret;
  }

  /* if for some reason we couldn't finish writing all the data, there
     must have been a problem */
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

  /* return success code */
  return 0;
}



/* the on_stateChange function from the session handler */
static void mw_session_stateChange(struct mwSession *session,
				   enum mwSessionState state, guint32 info) {

  if(state == mwSession_STARTED) {
    /* at this point the session is all ready to go. */
    printf("session fully started\n");
  }
}



/* the session handler structure is where you should indicate what
   functions will perform many of the functions necessary for the
   session to operate. Among these, only io_write and io_close are
   absolutely required. */
static struct mwSessionHandler mw_session_handler = {
  .io_write = mw_session_io_write,  /**< handle session to socket */
  .io_close = mw_session_io_close,  /**< handle session closing socket */
  .clear = NULL,                    /**< cleanup function */
  .on_stateChange = mw_session_stateChange,  /**< session status changed */
  .on_setPrivacyInfo = NULL,        /**< received privacy information */
  .on_setUserStatus = NULL,         /**< received status information */
  .on_admin = NULL,                 /**< received an admin message */
  .on_loginRedirect = NULL,         /**< connection redirected */
};



/** called from read_cb, attempts to read available data from sock and
    pass it to the session. Returns zero when the socket has been
    closed, less-than zero in the event of an error, and greater than
    zero for success */
static int read_recv(struct mwSession *session, int sock) {
  char buf[BUF_LEN];
  int len;

  len = read(sock, buf, BUF_LEN);
  if(len > 0) mwSession_recv(session, buf, len);

  return len;
}



/** callback registerd via g_io_add_watch in main, watches the socket
    for available data to be processed by the session */
static gboolean read_cb(GIOChannel *chan,
			GIOCondition cond,
			gpointer data) {

  struct sample_client *client = data;
  int ret = 0;

  if(cond & G_IO_IN) {
    ret = read_recv(client->session, client->sock);

    /* successful operation ends here, as the read_recv function
       should only return sero or lower in the event of a disconnect
       or error */
    if(ret > 0) return TRUE;
  }

  /* read problem occured if we're here, so we'll need to take care of
     it and clean up internal state */
  if(client->sock) {

    /* stop watching the socket */
    g_source_remove(client->sock_event);

    /* close it */
    close(client->sock);

    /* don't reference the socket or its event now that they're gone */
    client->sock = 0;
    client->sock_event = 0;
  }

  return FALSE;
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



/* open and return a network socket fd connected to host:port */
static int init_sock(const char *host, int port) {
  struct sockaddr_in srvrname;
  int sock;

  sock = socket(PF_INET, SOCK_STREAM, 0);
  if(sock < 0) {
    fprintf(stderr, "socket failure");
    exit(1);
  }
  
  init_sockaddr(&srvrname, host, port);
  connect(sock, (struct sockaddr *)&srvrname, sizeof(srvrname));

  return sock;
}



int main(int argc, char *argv[]) {

  /* the meanwhile session itself */
  struct mwSession *session;

  /* client program data */
  struct sample_client *client;

  /* something glib uses to watch the socket for available data */
  GIOChannel *io_chan;

  /* specify host, user, pass on the command line */
  if(argc != 4) {
    fprintf(stderr, HELP, *argv);
    return 1;
  }

  /* create the session and set the user and password */
  session = mwSession_new(&mw_session_handler);
  mwSession_setProperty(session, mwSession_AUTH_USER_ID, argv[2], NULL);
  mwSession_setProperty(session, mwSession_AUTH_PASSWORD, argv[3], NULL);

  /* create the client data. This is arbitrary data that a client will
     want to store along with the session for its own use */
  client = g_new0(struct sample_client, 1);
  client->session = session;

  /* associate the client data with the session, specifying an
     optional cleanup function which will be called upon the data when
     the session is free'd via mwSession_free */
  mwSession_setClientData(session, client, g_free);

  /* set up a connection to the host */
  client->sock = init_sock(argv[1], 1533);

  /* start the session. This will cause the session to send the
     handshake message (using the io_write function specified in the
     session handler). */
  mwSession_start(session);

  /* add a watch on the socket. Any data returning from the server
     will trigger read_cb, which will in turn read the data and pass
     it to the session for interpretation */
  io_chan = g_io_channel_unix_new(client->sock);
  client->sock_event = g_io_add_watch(io_chan, G_IO_IN | G_IO_ERR | G_IO_HUP,
				      read_cb, client);

  /* Create a new main loop and start it. This will cause the above
     watch to begin actually polling the socket. Use g_idle_add,
     g_timeout_add to insert events into the event loop */
  g_main_loop_run(g_main_loop_new(NULL, FALSE));

  /* this won't happen until after the main loop finally terminates */
  return 0;
}


