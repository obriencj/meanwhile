
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


/** static instance of the type for mwPyChannel objects. */
PyTypeObject *mwPyChannel_type();


/** instantiate a mwPyChannel wrapping a given mwChannel. */
mwPyChannel *mwPyChannel_wrap(mwPySession *sess, struct mwChannel *chan);


struct pyObj_mwService {
  PyObject_HEAD;
  mwPySession *session;             /**< owning python session wrapper */
  struct mwServicePyWrap *wrapper;  /**< python wrapper service */
  struct mwService *wrapped;        /**< optional underlying service */
  gpointer data;                    /**< optional additional data */
};


/** static instance of the type for mwPyService objects. Generic
    wrapper for a native mwService */
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


/** static instance of the type for mwPyServiceAware objects. sub-type
    of mwPyService for wrapping a mwServiceAware instance */
PyTypeObject *mwPyServiceAware_type();


/** static instance of the type for mwPyServiceIm objects. sub-type of
    mwPyService for wrapping a mwServiceIm instance */
PyTypeObject *mwPyServiceIm_type();


/** static instance of the type for mwPyServiceStorage objects.
    sub-type of mwPyService for wrapping a mwServiceStore instance */
PyTypeObject *mwPyServiceStorage_type();


struct pyObj_mwSession {
  PyObject_HEAD;
  struct mwSession *session;
  GHashTable *services; /* directly maps mwService:pyObj_mwService */
};


/** static instance of the type for mwPySession objects. */
PyTypeObject *mwPySession_type();


/** @section utility functions */
/*@{*/


/** @returns new reference to a PyString copying str, or incremented
    reference to Py_None if str is NULL */
PyObject *PyString_SafeFromString(const char *str);


/** @returns contents of PyString, or NULL for Py_None */
const char *PyString_SafeAsString(PyObject *str);


/** @returns incremented reference to Py_None */
PyObject *mw_noargs_none(PyObject *obj);


/** @returns incremented reference to Py_None */
PyObject *mw_varargs_none(PyObject *obj, PyObject *args);


/** useful for creating types with empty callback functions intended to
    be overridden. Calls to mw_noargs_none, which always returns None */
#define MW_METH_NOARGS_NONE   (PyCFunction) mw_noargs_none


/** useful for creating types with empty callback functions intended to
    be overridden. Calls to mw_varargs_none, which always returns None */
#define MW_METH_VARARGS_NONE  (PyCFunction) mw_varargs_none


/** macro to return from current function with an incremented Py_None */
#define mw_return_none() \
  { Py_INCREF(Py_None); return Py_None; }


/** macro to set a python exception of type ex with text e, and return
    NULL */
#define mw_throw_ex(ex, e) \
  { PyErr_SetString((ex), (e)); return NULL; }


/** macro to set a python exception of type Exception with text e, and
    return NULL */
#define mw_throw(e) \
  mw_throw_ex(PyExc_Exception, (e))


/*@}*/


#endif
