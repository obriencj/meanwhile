
#ifndef _HAVE_PYMW_H
#define _HAVE_PYMW_H

#include <glib/ghash.h>

/* place-holders */
struct mwChannel;
struct mwService;
struct mwServiceIm;
struct mwSession;


typedef struct pyObj_mwChannel mwPyChannel;
typedef struct pyObj_mwService mwPyService;
typedef struct pyObj_mwSession mwPySession;


/** @struct mwServicePyWrap
    specialty service which wraps calls to common mwService methods to an
    underlying Python object or another mwService
 */
struct mwServicePyWrap;


struct pyObj_mwChannel {
  PyObject_HEAD;
  struct mwChannel *channel;
  mwPySession *session;
};


PyTypeObject *mwPyChannel_type();


mwPyChannel *mwPyChannel_wrap(mwPySession *sess, struct mwChannel *chan);


struct pyObj_mwService {
  PyObject_HEAD;
  mwPySession *session;             /**< owning python session wrapper */
  struct mwServicePyWrap *wrapper;  /**< python wrapper service */
  struct mwService *wrapped;        /**< optional underlying service */
};


PyTypeObject *mwPyService_type();


struct mwServicePyWrap *mwServicePyWrap_new(mwPyService *self,
					    guint32 type);


struct mwServicePyWrap *mwServicePyWrap_wrap(mwPyService *self,
					     struct mwService *service);


mwPyService *mwServicePyWrap_getSelf(struct mwServicePyWrap *srvc);


/** sub-type of mwPyService for wrapping a mwServiceIm instance */
PyTypeObject *mwPyServiceIm_type();


struct pyObj_mwSession {
  PyObject_HEAD;
  struct mwSession *session;
  GHashTable *services; /* maps mwService:pyObj_mwService */
};


PyTypeObject *mwPySession_type();


/** @section utility functions */


/** @returns new PyString, or Py_None for NULL str */
PyObject *PyString_SafeFromString(const char *str);


/** @returns contents of PyString, or NULL for Py_None */
const char *PyString_SafeAsString(PyObject *str);


#endif
