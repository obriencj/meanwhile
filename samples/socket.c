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
   @file test.c
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
#include <glib-object.h>
#include <glib/giochannel.h>

#include "mw_channel.h"
#include "mw_common.h"
#include "mw_debug.h"
#include "mw_encrypt.h"
#include "mw_error.h"
#include "mw_giosession.h"
#include "mw_session.h"
#include "mw_srvc_im.h"


/** help text if you don't give the right number of arguments */
#define HELP								\
  "Meanwhile sample socket client\n"					\
  "Usage: %s server userid password\n"					\
  "\n"									\
  "Connects to a sametime server and logs in with the supplied user ID\n" \
  "and password. Doesn't actually do anything useful after that.\n\n"


/* address lookup used by init_sock */
static void init_sockaddr(struct sockaddr_in *addr,
			  const gchar *host, gint port) {

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
static int init_sock(const gchar *host, gint port) {
  struct sockaddr_in srvrname;
  gint sock;

  sock = socket(PF_INET, SOCK_STREAM, 0);
  if(sock < 0) {
    fprintf(stderr, "socket failure");
    exit(1);
  }
  
  init_sockaddr(&srvrname, host, port);
  connect(sock, (struct sockaddr *)&srvrname, sizeof(srvrname));

  return sock;
}


static void got_status(MwSession *self, gpointer data) {
  guint stat, time;
  gchar *desc;

  g_object_get(G_OBJECT(self),
	       "status", &stat,
	       "status-idle-since", &time,
	       "status-message", &desc,
	       NULL);

  g_debug("status set to 0x%x, %u, \"%s\"", stat, time, desc);
  g_free(desc);
}


static void state_changed(MwSession *self, gpointer data) {
  guint state;

  g_object_get(G_OBJECT(self), "state", &state, NULL);

  switch(state) {
  case mw_session_starting:
    {
      guint major, minor;

      g_object_get(G_OBJECT(self),
		   "client-ver-major", &major,
		   "client-ver-minor", &minor,
		   NULL);

      g_debug("client version: %u.%u", major, minor);
    }
    break;
  case mw_session_handshake_ack:
    {
      guint major, minor;

      g_object_get(G_OBJECT(self),
		   "server-ver-major", &major,
		   "server-ver-minor", &minor,
		   NULL);

      g_debug("server version: %u.%u", major, minor);
    }
    break;
  case mw_session_login_ack:
    {
      gchar *srvr;
      g_object_get(G_OBJECT(self), "server-id", &srvr, NULL);
      g_debug("server id: %s", srvr);
      g_free(srvr);
    }
    break;
  case mw_session_started:
    {
      MwStatus stat;
      stat.status = mw_status_ACTIVE;
      stat.time = 0;
      stat.desc = "I am a test script. Isn't that awesome? (Manwhile 2.0)";
      MwSession_setStatus(self, &stat);
    }
    break;
  case mw_session_stopping:
    break;
  case mw_session_stopped:
    break;
  default:
    ;
  }
}


static void got_text(MwConversation *conv, const gchar *msg,
		     gpointer data) {
  
  gchar *user;
  g_object_get(G_OBJECT(conv), "user", &user, NULL);

  g_debug("%s sent: %s", user, msg);

  if(! strcmp(msg, "shutdown")) {
    MwConversation_close(conv, 0x00);
    MwGIOSession_politeStop(data);

  } else {
    gchar *reply = g_strdup_printf("You said, \"%s\"", msg);
    MwConversation_sendText(conv, reply);
    g_free(reply);
  }

  g_free(user);
}


static void got_typing(MwConversation *conv, const gboolean typing,
		       gpointer data) {

  gchar *user;
  g_object_get(G_OBJECT(conv), "user", &user, NULL);

  g_debug("%s %s", user, (typing? "is typing": "stopped typing"));
  g_free(user);
}


static void incoming_conversation(MwIMService *self,
				  MwConversation *conv,
				  gpointer data) {

  g_signal_connect(G_OBJECT(conv), "got-text",
		   G_CALLBACK(got_text), data);

  g_signal_connect(G_OBJECT(conv), "got-typing",
		   G_CALLBACK(got_typing), data);
}


gint main(gint argc, gchar *argv[]) {

  gint sock;
  MwGIOSession *session;
  MwCipherClass *cipher_dhrc2;
  MwIMService *imsrvc;

  /* specify host, user, pass on the command line */
  if(argc != 4) {
    fprintf(stderr, HELP, *argv);
    return 1;
  }

  /* set up a connection to the host */
  sock = init_sock(argv[1], 1533);

  /* init the GLib type system */
  g_type_init();

  /* create the session */
  session = MwGIOSession_newFromSocket(sock);

  /* set the username and password */
  g_object_set(G_OBJECT(session),
	       "auth-user", argv[2],
	       "auth-password", argv[3],
	       NULL);

  /* connect to the state-changed signal */
  g_signal_connect(G_OBJECT(session), "state-changed",
		   G_CALLBACK(state_changed), NULL);

  /* connect to the got-status signal */
  g_signal_connect(G_OBJECT(session), "got-status",
		   G_CALLBACK(got_status), NULL);

  /* provide a cipher */
  cipher_dhrc2 = MW_CIPHER_CLASS(MwDHRC2Cipher_getCipherClass());
  MwSession_addCipher(MW_SESSION(session), cipher_dhrc2);

  /* provide an IM service */
  imsrvc = MwIMService_new(MW_SESSION(session));

  /* watch for incoming conversations on the IM service */
  g_signal_connect(G_OBJECT(imsrvc), "incoming-conversation",
		   G_CALLBACK(incoming_conversation), session);

  /* start the session in its own event loop. This won't return until
     the session is closed */
  MwGIOSession_main(session);

  /* get rid of the IM service */
  mw_gobject_unref(imsrvc);

  /* get rid of the session */
  mw_gobject_unref(session);

  /* get rid of the cipher class */
  g_type_class_unref(cipher_dhrc2);

  return 0;
}


/* The end. */
