

#ifndef _MW_SESSION_H
#define _MW_SESSION_H


#include "mw_common.h"


/** @file mw_session.h
    ...
*/



#ifndef PROTOCOL_VERSION_MAJOR
/** protocol versioning. */
#define PROTOCOL_VERSION_MAJOR  0x001e
#endif

#ifndef PROTOCOL_VERSION_MINOR
/** protocol versioning. */
#define PROTOCOL_VERSION_MINOR  0x001d
#endif


/** @section Session Properties
    ...
*/
/*@{*/

/** char *, session user ID */
#define PROPERTY_SESSION_USER_ID   "session.auth.user"

/** char *, plaintext password */
#define PROPERTY_SESSION_PASSWORD  "session.auth.password"

/** struct mwOpaque *, authentication token */
#define PROPERTY_SESSION_TOKEN     "session.auth.token"

/** guint16, major version of client protocol */
#define PROPERTY_CLIENT_VER_MAJOR  "client.version.major"

/** guint16, minor version of client protocol */
#define PROPERTY_CLIENT_VER_MINOR  "client.version.minor"

/** guint16, client type identifier */
#define PROPERTY_CLIENT_TYPE_ID    "client.id"

/** guint16, major version of server protocol */
#define PROPERTY_SERVER_VER_MAJOR  "server.version.major"

/** guint16, minor version of server protocol */
#define PROPERTY_SERVER_VER_MINOR  "server.version.minor"

/*@}*/


/* place-holders */
struct mwChannelSet;
struct mwCipher;
struct mwService;
struct mwMessage;


/** @file session.h

    A client session with a Sametime server is encapsulated in the
    mwSession structure. The session controls channels, provides
    encryption ciphers, and manages services using messages over the
    Master channel.

    A session does not directly communicate with a socket or stream,
    instead the session is initialized from client code with an
    instance of a mwSessionHandler structure. This session handler
    provides functions as call-backs for common session events, and
    provides functions for writing-to and closing the connection to
    the server.

    A session does not perform reads on a socket directly. Instead, it
    must be fed from an outside source via the mwSession_recv
    function. The session will buffer and merge data passed to this
    function to build complete protocol messages, and will act upon
    each complete message accordingly.
*/


enum mwSessionState {
  mwSession_STARTING,      /**< session is starting */
  mwSession_HANDSHAKE,     /**< session has sent handshake */
  mwSession_HANDSHAKE_ACK, /**< session has received handshake ack */
  mwSession_LOGIN,         /**< session has sent login */
  mwSession_LOGIN_REDIR,   /**< session has been redirected */
  mwSession_LOGIN_ACK,     /**< session has received login ack */
  mwSession_STARTED,       /**< session is active */
  mwSession_STOPPING,      /**< session is shutting down */
  mwSession_STOPPED,       /**< session is stopped */
  mwSession_UNKNOWN,       /**< indicates an error determining state */
};


#define SESSION_IS_STATE(session, state) \
  (mwSession_getState((session)) == (state))

#define SESSION_IS_STARTING(s) \
  (SESSION_IS_STATE((s), mwSession_STARTING)  || \
   SESSION_IS_STATE((s), mwSession_HANDSHAKE) || \
   SESSION_IS_STATE((s), mwSession_HANDSHAKE_ACK) || \
   SESSION_IS_STATE((s), mwSession_LOGIN) || \
   SESSION_IS_STATE((s), mwSession_LOGIN_ACK))

#define SESSION_IS_STARTED(s) \
  (SESSION_IS_STATE((s), mwSession_STARTED))

#define SESSION_IS_STOPPING(s) \
  (SESSION_IS_STATE((s), mwSession_STOPPING))

#define SESSION_IS_STOPPED(s) \
  (SESSION_IS_STATE((s), mwSession_STOPPED))


/** @struct mwSession
    Represents a Sametime client session */
struct mwSession;


/** session handler. Structure which interfaces a session with client
    code to provide I/O and event handling */
struct mwSessionHandler {
  
  /** write data to the server connection. Required */
  int (*io_write)(struct mwSession *, const char *buf, gsize len);
  
  /** close the server connection. Required */
  void (*io_close)(struct mwSession *);

  /** triggered by mwSession_free */
  void (*clear)(struct mwSession *);

  /** extra data for implementation use */
  gpointer data;

  /** Called when the session has changed status.

      Uses of the info param:
      - <code>STOPPING</code> error code causing the session to shut down

      @param s      the session
      @param state  the session's state
      @param info   additional state info. */
  void (*on_stateChange)(struct mwSession *s,
			 enum mwSessionState state, guint32 info);

  /** called when privacy information has been sent or received */
  void (*on_setPrivacyInfo)(struct mwSession *);

  /** called when user status has changed */
  void (*on_setUserStatus)(struct mwSession *);

  /** called when an admin messages has been received */
  void (*on_admin)(struct mwSession *, const char *text);

  /** called when a login redirect message is received */
  void (*on_loginRedirect)(struct mwSession *, const char *host);
};


/** allocate a new session */
struct mwSession *mwSession_new(struct mwSessionHandler *);


/** stop, clear, free a session. Does not free contained ciphers or
    services, these must be taken care of explicitly. */
void mwSession_free(struct mwSession *);


/** obtain a reference to the session's handler */
struct mwSessionHandler *mwSession_getHandler(struct mwSession *);


/** instruct the session to begin. This will result in the initial
    handshake message being sent. */
void mwSession_start(struct mwSession *);


/** instruct the session to shut down with the following reason
    code. */
void mwSession_stop(struct mwSession *, guint32 reason);


/** Data is buffered, unpacked, and parsed into a message, then
    processed accordingly. */
void mwSession_recv(struct mwSession *, const char *, gsize);


/** primarily used by services to have messages serialized and sent
    @param s    session to send message over
    @param msg  message to serialize and send
    @returns    0 for success */
int mwSession_send(struct mwSession *s, struct mwMessage *msg);


/** set the internal privacy information, and inform the server as
    necessary. Triggers the on_setPrivacyInfo call-back */
int mwSession_setPrivacyInfo(struct mwSession *, struct mwPrivacyInfo *);


struct mwPrivacyInfo *mwSession_getPrivacyInfo(struct mwSession *);


struct mwLoginInfo *mwSession_getLoginInfo(struct mwSession *);


/** set the internal user status state, and inform the server as
    necessary. Triggers the on_setUserStatus call-back */
int mwSession_setUserStatus(struct mwSession *, struct mwUserStatus *);


struct mwUserStatus *mwSession_getUserStatus(struct mwSession *);


/** current status of the session */
enum mwSessionState mwSession_getState(struct mwSession *);


/** additional status-specific information */
guint32 mwSession_getStateInfo(struct mwSession *);


struct mwChannelSet *mwSession_getChannels(struct mwSession *);


/** adds a service to the session. If the session is started (or when
    the session is successfully started) and the service has a start
    function, the session will request service availability from the
    server. On receipt of the service availability notification, the
    session will call the service's start function.

    @return TRUE if the session was added correctly */
gboolean mwSession_addService(struct mwSession *, struct mwService *);


/** find a service by its type identifier */
struct mwService *mwSession_getService(struct mwSession *, guint32 type);


/** removes a service from the session. If the session is started and
    the service has a stop function, it will be called. Returns the
    removed service */
struct mwService *mwSession_removeService(struct mwSession *, guint32 type);


/** a GList of services in this session. The GList needs to be freed
    after use */
GList *mwSession_getServices(struct mwSession *);


/** instruct a STARTED session to check the server for the presense of
    a given service. The service will be automatically started upon
    receipt of an affirmative reply from the server. This function is
    automatically called upon all services in a session when the
    session is fully STARTED.

    Services which terminate due to an error may call this on
    themselves to re-initialize when their server-side counterpart is
    made available again.

    @param s     owning session
    @param type  service type ID */
void mwSession_senseService(struct mwSession *s, guint32 type);


/** adds a cipher to the session. */
gboolean mwSession_addCipher(struct mwSession *, struct mwCipher *);


/** find a cipher by its type identifier */
struct mwCipher *mwSession_getCipher(struct mwSession *, guint16 type);


/** remove a cipher from the session */
struct mwCipher *mwSession_removeCipher(struct mwSession *, guint16 type);


/** a GList of ciphers in this session. The GList needs to be freed
    after use */
GList *mwSession_getCiphers(struct mwSession *);


/** associate a key:value pair with the session. If an existing value is
    associated with the same key, it will have its clear function called
    and will be replaced with the new value */
void mwSession_setProperty(struct mwSession *, const char *key,
			   gpointer val, GDestroyNotify clear);


gpointer mwSession_getProperty(struct mwSession *, const char *key);


void mwSession_removeProperty(struct mwSession *, const char *key);


#endif

