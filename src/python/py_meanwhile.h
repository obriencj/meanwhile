
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


#define mwPyChannel_check(obj) \
  PyObject_IsInstance((obj), (PyObject *) mwPyChannel_type())


/** instantiate a mwPyChannel wrapping a given mwChannel. */
mwPyChannel *mwPyChannel_wrap(mwPySession *sess, struct mwChannel *chan);


struct pyObj_mwService {
  PyObject_HEAD;
  mwPySession *session;             /**< owning python session wrapper */
  struct mwServicePyWrap *wrapper;  /**< python wrapper service */
  struct mwService *wrapped;        /**< optional underlying service */
  gpointer data;                    /**< optional additional data */
  GDestroyNotify cleanup;           /**< optional cleanup for data */
};


/** static instance of the type for mwPyService objects. Generic
    wrapper for a native mwService */
PyTypeObject *mwPyService_type();


#define mwPyService_check(obj) \
  PyObject_IsInstance((obj),(PyObject *)  mwPyService_type())


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


#define mwPyServiceAware_check(obj) \
  PyObject_IsInstance((obj), (PyObject *) mwPyServiceAware_type())


/** static instance of the type for mwPyServiceConference
    objects. sub-type of mwPyService for wrapping a
    mwServiceConference instance */
PyTypeObject *mwPyServiceConference_type();


#define mwPyServiceConference_check(obj) \
  PyObject_IsInstance((obj),(PyObject *)  mwPyServiceConference_type())


/** static instance of the type for mwPyServiceIm objects. sub-type of
    mwPyService for wrapping a mwServiceIm instance */
PyTypeObject *mwPyServiceIm_type();


#define mwPyServiceIm_check(obj) \
  PyObject_IsInstance((obj), (PyObject *) mwPyServiceIm_type())


/** static instance of the type for mwPyServiceResolve objects.
    sub-type of mwPyService for wrapping a mwServiceResolve instance */
PyTypeObject *mwPyServiceResolve_type();


#define mwPyServiceResolve_check(obj) \
  PyObject_IsInstance((obj), (PyObject *) mwPyServiceResolve_type())


/** static instance of the type for mwPyServiceStorage objects.
    sub-type of mwPyService for wrapping a mwServiceStore instance */
PyTypeObject *mwPyServiceStorage_type();


#define mwPyServiceStorage_check(obj) \
  PyObject_IsInstance((obj), (PyObject *) mwPyServiceStorage_type())


struct pyObj_mwSession {
  PyObject_HEAD;
  struct mwSession *session;
  GHashTable *services; /* directly maps mwService:pyObj_mwService */
};


/** static instance of the type for mwPySession objects. */
PyTypeObject *mwPySession_type();


#define mwPySession_check(obj) \
  PyObject_IsInstance((obj), (PyObject *) mwPySession_type())


/** @section utility functions */
/*@{*/


/** @returns new reference to a PyString copying str, or incremented
    reference to Py_None if str is NULL */
PyObject *PyString_SafeFromString(const char *str);


/** @returns contents of PyString, or NULL for Py_None */
const char *PyString_SafeAsString(PyObject *str);


#define PyGuint32_FromGuint32(ui32) \
  PyInt_FromLong(ui32)


#define PyGuint32_AsGuint32(pyobj) \
  (guint32) PyInt_AsLong(pyobj)


#define PyGuint16_FromGuint16(ui16) \
  PyInt_FromLong(ui16)


#define PyGuint16_AsGuint16(pyobj) \
  (guint16) PyInt_AsLong(pyobj)


/** @returns incremented reference to Py_None */
PyObject *mw_noargs_none(PyObject *obj);


/** @returns incremented reference to Py_None */
PyObject *mw_varargs_none(PyObject *obj, PyObject *args);


/** useful for creating types with empty callback functions intended
    to be overridden. Calls to mw_noargs_none, which always returns
    None */
#define MW_METH_NOARGS_NONE   (PyCFunction) mw_noargs_none


/** useful for creating types with empty callback functions intended
    to be overridden. Calls to mw_varargs_none, which always returns
    None */
#define MW_METH_VARARGS_NONE  (PyCFunction) mw_varargs_none


/** macro to return from current function with an incremented
    Py_None */
#define mw_return_none() \
  { Py_INCREF(Py_None); return Py_None; }


/** macro to set a python exception of type exc with text e, and
    return ret (which should be NULL or -1) */
#define mw_raise_exc(exc, e, ret) \
  { PyErr_SetString((exc), (e)); return (ret); }


/** macro to set a python exception of type Exception with text e, and
    return ret (which should be NULL or -1) */
#define mw_raise(e, ret) \
  mw_raise_exc(PyExc_Exception, (e), (ret))


/** macro to set python TypeError when a setter is called on a
    read-only member and to return -1. name must be a string
    literal, and should be the name of the member */
#define mw_raise_readonly(name) \
  mw_raise_exc(PyExc_TypeError, "member '" name "' is read-only", -1)



/*@}*/


#endif
