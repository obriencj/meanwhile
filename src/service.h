

#ifndef _MW_SERVICE_H
#define _MW_SERVICE_H


#include <glib.h>


struct mwChannel;
struct mwSession;
struct mwMsgChannelCreate;
struct mwMsgChannelAccept;
struct mwMsgChannelDestroy;


/** Identification numbers for the basic services
    @relates mwService::type */
enum mwBaseServiceTypes {
  mwService_AWARE    = 0x00000011, /**< buddy list */
  mwService_RESOLVE  = 0x00000015, /**< name resolution */
  mwService_STORAGE  = 0x00000018, /**< storage */
  mwService_IM       = 0x00001000, /**< instant messaging */
  mwService_CONF     = 0x80000010, /**< conferencing */
};


/** Identification numbers for the basic service protocols */
enum mwBaseProtocolTypes {
  mwProtocol_AWARE    = 0x00000011,
  mwProtocol_RESOLVE  = 0x00000015,
  mwProtocol_STORAGE  = 0x00000025,
  mwProtocol_IM       = 0x00001000,
  mwProtocol_CONF     = 0x00000010
};


/** State-tracking for a service */
enum mwServiceState {
  mwServiceState_STOPPED,   /**< the service is not active */
  mwServiceState_STOPPING,  /**< the service is shutting down */
  mwServiceState_STARTED,   /**< the service is active */
  mwServiceState_STARTING   /**< the service is starting up */
};


/** Casts a concrete service (such as mwServiceAware) into a mwService */
#define MW_SERVICE(srv) ((struct mwService *) srv)


#define MW_SERVICE_STOPPED(srvc)  ((srvc)->state == mwServiceState_STOPPED)
#define MW_SERVICE_STOPPING(srvc) ((srvc)->state == mwServiceState_STOPPING)
#define MW_SERVICE_STARTED(srvc)  ((srvc)->state == mwServiceState_STARTED)
#define MW_SERVICE_STARTING(srvc) ((srvc)->state == mwServiceState_STARTING)


/** If a service is STARTING or STARTED, it's LIVE */
#define MW_SERVICE_LIVE(srvc) \
  (MW_SERVICE_STARTING(srvc) || MW_SERVICE_STARTED(srvc))

/** If a service is STOPPING or STOPPED, it's DEAD */
#define MW_SERVICE_DEAD(srvc) \
  (MW_SERVICE_STOPPING(srvc) || MW_SERVICE_STOPPED(srvc))


/** A service is the recipient of sendOnCnl messages sent over
    channels marked with the corresponding service id. Services
    provide functionality such as IM relaying, Awareness tracking and
    notification, and Conference handling.  It is a service's
    responsibility to accept or destroy channels, and to process data
    sent over those channels */
struct mwService {

  /** the unique identifier by which this service is registered.
      @relates BaseServiceTypes  */
  guint32 type;

  /** the state of this service. Determines whether or not the session
      should call the start function upon receipt of a service
      available message */
  enum mwServiceState state;

  /** session this service is attached to. */
  struct mwSession *session;

  /** @return string short name of the service
      @relates mwService_getName */
  const char *(*get_name)();

  /** @return string short description of the service
      @relates mwService_getDesc */
  const char *(*get_desc)();

  /** The service's channel create handler. Called when the session
      receives a channel create message with a service matching this
      service's type.
      @relates mwService_recvChannelCreate */
  void (*recv_channelCreate)(struct mwService *, struct mwChannel *,
			     struct mwMsgChannelCreate *);

  /** The service's channel accept handler. Called when the session
      receives a channel accept message for a channel with a service
      matching this service's type.
      @relates mwService_recvChannelAccept */
  void (*recv_channelAccept)(struct mwService *, struct mwChannel *,
			     struct mwMsgChannelAccept *);

  /** The service's channel destroy handler. Called when the session
      receives a channel destroy message for a channel with a service
      matching this service's type.
      @relates mwService_recvChannelDestroy */
  void (*recv_channelDestroy)(struct mwService *, struct mwChannel *,
			      struct mwMsgChannelDestroy *);

  /** The service's input handler. Called when the session receives
      data on a channel with a service matching this service's
      type.
      @relates mwService_recv */
  void (*recv)(struct mwService *, struct mwChannel *, guint16 msg_type,
	       const char *, gsize);

  /** The service's start handler. Called upon the receipt of a
      service available message.
      @relates mwService_start */
  void (*start)(struct mwService *);

  /** The service's stop handler. Called when the session is shutting
      down, or when the service is free'd.
      @relates mwService_stop */
  void (*stop)(struct mwService *);

  
  /** The service's cleanup handler.
      @relates mwService_free */
  void (*clear)(struct mwService *);
};


/** Prepares a newly allocated service for use. Connects the base
    service signals, etc.
    
    Intended for use by service implementations, rather than by code
    utilizing a service.
    
    The service state will be initialized to STOPPED.
    
    @param service The service to initialize
    @param session The service's owning session
    @param service_type The service number
*/
void mwService_init(struct mwService *service, struct mwSession *session,
		    guint32 service_type);


/** @name General Services API

    These functions provide unified access to the general functions of
    a client service, with some simple sanity-checking. */
/*@{*/


/** Triggers the recv_channelCreate handler on the service.
    @param service the service to handle the message
    @param channel the channel being created
    @param msg the channel create message
*/
void mwService_recvChannelCreate(struct mwService *service,
				 struct mwChannel *channel,
				 struct mwMsgChannelCreate *msg);


/** Triggers the recv_channelAccept handler on the service.
    @param service the service to handle the message
    @param channel the channel being accepted
    @param msg the channel accept message
*/
void mwService_recvChannelAccept(struct mwService *service,
				 struct mwChannel *channel,
				 struct mwMsgChannelAccept *msg);


/** Triggers the recv_channelDestroy handler on the service.
    @param service the service to handle the message
    @param channel the channel being destroyed
    @param msg the channel destroy message
*/
void mwService_recvChannelDestroy(struct mwService *service,
				  struct mwChannel *channel,
				  struct mwMsgChannelDestroy *msg);


/** Triggers the input handler on the service
    @param service the service to receive the input
    @param channel the channel the input was received from
    @param msg_type the service-dependant message type
    @param buf the input buffer
    @param len the length of the input buffer
*/
void mwService_recv(struct mwService *service, struct mwChannel *channel,
		    guint16 msg_type, const char *buf, gsize len);


/** @return string short name of the service */
const char *mwService_getName(struct mwService *);


/** @return string short description of the service */
const char *mwService_getDesc(struct mwService *);


/** @return the service's session */
struct mwSession *mwService_getSession(struct mwService *service);


/** Triggers the start handler for the service. Normally called from
    the session upon receipt of a service available message. Service
    implementations should use this handler to open any necessary
    channels, etc. Checks that the service is STOPPED, or returns.
    
    @param service The service to start
*/
void mwService_start(struct mwService *service);


/** Triggers the stop handler for the service. Normally called from
    the session before closing down the connection. Checks that the
    service is STARTED or STARTING, or returns

    @param service The service to stop
*/
void mwService_stop(struct mwService *service);


/** Frees memory used by a service. Will trigger the stop handler if
    the service is STARTED or STARTING. Triggers clear handler to allow
    cleanup.

    @param service The service to clear and free
*/
void mwService_free(struct mwService *service);


/*@}*/


#endif

