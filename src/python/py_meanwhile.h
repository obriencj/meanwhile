
#ifndef _HAVE_PYMW_H
#define _HAVE_PYMW_H


#include <glib/ghash.h>


/* place-holders */
struct mwChannel;
struct mwService;
struct mwSession;


typedef struct pyObj_mwChannel mwPyChannel;
typedef struct pyObj_mwService mwPyService;
typedef struct pyObj_mwSession mwPySession;


/** @struct mwServicePyWrap
    specialty service which wraps calls to common mwService methods to an
    underlying Python object or another mwService */
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
  gpointer data;                    /**< optional additional data */
};


PyTypeObject *mwPyService_type();


/** create an instance of the wrapper service with the given service
    type id */
struct mwServicePyWrap *mwServicePyWrap_new(mwPyService *self,
					    guint32 type);


/** create an instance of the wrapper service, backed by srvc */
struct mwServicePyWrap *mwServicePyWrap_wrap(mwPyService *self,
					     struct mwService *srvc);


/** the PyObject the wrapper service is connected to */
mwPyService *mwServicePyWrap_getSelf(struct mwServicePyWrap *srvc);


/** sub-type of mwPyService for wrapping a mwServiceAware instance */
PyTypeObject *mwPyServiceAware_type();


/** sub-type of mwPyService for wrapping a mwServiceIm instance */
PyTypeObject *mwPyServiceIm_type();


/** sub-type of mwPyService for wrapping a mwServiceStore instance */
PyTypeObject *mwPyServiceStorage_type();


struct pyObj_mwSession {
  PyObject_HEAD;
  struct mwSession *session;
  GHashTable *services; /* maps mwService:pyObj_mwService */
};


PyTypeObject *mwPySession_type();


/** @section utility functions */
/*@{*/


/** @returns new PyString, or Py_None for NULL str */
PyObject *PyString_SafeFromString(const char *str);


/** @returns contents of PyString, or NULL for Py_None */
const char *PyString_SafeAsString(PyObject *str);


/** @returns incremented Py_None */
PyObject *mw_noargs_none(PyObject *obj);


/** @returns incremented Py_None */
PyObject *mw_varargs_none(PyObject *obj, PyObject *args);


#define MW_METH_NOARGS_NONE   (PyCFunction) mw_noargs_none
#define MW_METH_VARARGS_NONE  (PyCFunction) mw_varargs_none


#define mw_return_none() \
  { Py_INCREF(Py_None); return Py_None; }


#define mw_throw(e) \
  { PyErr_SetString(PyExc_Exception, (e)); return NULL; }


/*@}*/


#endif
