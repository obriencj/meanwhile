/*
  Meanwhile - Unofficial Lotus Sametime Community Client Library
  Copyright (C) 2004 - 2006  Christopher (siege) O'Brien
  
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
   @file sendmessage.c
*/


#include <netinet/in.h>
#include <netdb.h>
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


#define HELP \
  ("usage %s:  server port sender password recipient message\n")


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
static gint init_sock(const gchar *host, gint port) {
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


static void conv_state_changed(MwConversation *conv, gint state,
			       gpointer data) {

  if(state == mw_conversation_open) {
    MwIMService *srvc;
    const gchar *message;

    g_object_get(G_OBJECT(conv), "service", &srvc, NULL);

    message = g_object_get_data(G_OBJECT(srvc), "send-message");

    mw_gobject_unref(srvc);

    MwConversation_sendText(conv, message);
    MwConversation_close(conv, 0x00);
    MwGIOSession_politeStop(data);
  }
}


static void srvc_state_changed(MwIMService *srvc, gint state,
			       gpointer data) {

  if(state == mw_object_started) {
    MwConversation *conv;
    const gchar *user;

    user = g_object_get_data(G_OBJECT(srvc), "send-to-user");
    conv = MwIMService_getConversation(srvc, user, NULL);

    MwConversation_open(conv);
    g_signal_connect(G_OBJECT(conv), "state-changed",
		     G_CALLBACK(conv_state_changed), data);

    /* we're not actually keeping a reference to the conv we got from
       the IM service, so knock the refcount back down */
    mw_gobject_unref(conv);
  }
}


gint main(gint argc, gchar *argv[]) {

  gint sock;
  MwGIOSession *session;
  MwCipherClass *cipher_dhrc2;
  MwIMService *imsrvc;
  
  if (argc < 7) {
    fprintf(stderr, HELP, *argv);
    return 1;
  }
  
  /* set up a connection to the host */
  sock = init_sock(argv[1], atoi(argv[2]));

  /* init the GLib type system */
  g_type_init();

  /* create the session */
  session = MwGIOSession_newFromSocket(sock);
  
  /* set the user name and password */
  g_object_set(G_OBJECT(session),
	       "auth-user", argv[3],
	       "auth-password", argv[4],
	       NULL);

  /* provide a cipher */
  cipher_dhrc2 = MW_CIPHER_CLASS(MwDHRC2Cipher_getCipherClass());
  MwSession_addCipher(MW_SESSION(session), cipher_dhrc2);

  /* provide an IM service */
  imsrvc = MwIMService_new(MW_SESSION(session));

  /* store the target user and message on the IM service */
  g_object_set_data(G_OBJECT(imsrvc), "send-to-user", argv[5]);
  g_object_set_data(G_OBJECT(imsrvc), "send-message", argv[6]);

  /* watch the state of the IM service */
  g_signal_connect(G_OBJECT(imsrvc), "state-changed",
		   G_CALLBACK(srvc_state_changed), session);

  /* start the session in its own event oop. This won't return until
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
