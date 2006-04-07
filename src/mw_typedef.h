#ifndef _MW_TYPEDEF_H
#define _MW_TYPEDEF_H


/*
  class heirarchy:

  GObject
  MwObject
    MwAwareList
    MwChannel
    MwCipher
      MwCipherRc2_40
      MwCipherRc2_128
      MwCipherBlowfish
    MwConference
    MwConversation
    MwFileTransfer
    MwPlace
    MwService
      MwServiceAware
      MwServiceConf
      MwServiceFt
        MwServiceFtDirect
      MwServiceIm
        MwServiceImNb
      MwServicePlace
      MwServiceResolve
      MwServiceStore
    MwSession
      MwGIOSession
*/


/* @see mw_object.h */
typedef struct mw_object MwObject;


/* @see mw_channel.h */
typedef struct mw_channel MwChannel;


/* @see mw_encrypt.h */
typedef struct mw_cipher MwCipher;


/* @see mw_encrypt.h */
typedef struct mw_rc2_cipher MwRC2Cipher;


/* @see mw_encrypt.h */
typedef struct mw_dh_rc2_cipher MwDHRC2Cipher;


/* @see mw_srvc_im.h */
typedef struct mw_conversation MwConversation;


/* @see mw_giosession.h */
typedef struct mw_gio_session MwGIOSession;


/* @see mw_srvc_place.h */
typedef struct mw_place MwPlace;


/* @see mw_service.h */
typedef struct mw_service MwService;


/* @see mw_srvc_im.h */
typedef struct mw_im_service MwIMService;


/* @see mw_srvc_store.h */
typedef struct mw_storage_service MwStorageService;


/* @see mw_session.h */
typedef struct mw_session MwSession;


#endif /* _MW_TYPEDEF_H */
