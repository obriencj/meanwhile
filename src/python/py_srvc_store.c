
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

#include <Python.h>
#include <structmember.h>

#include <glib.h>

#include "py_meanwhile.h"
#include "../mw_common.h"
#include "../mw_debug.h"
#include "../mw_service.h"
#include "../mw_srvc_store.h"


enum cb_type {
  type_INT,
  type_STR,
  type_BUF,
};


struct cb_data {
  PyObject *callback;
  enum cb_type type;
};


static struct cb_data *cb_data_new(PyObject *cb, enum cb_type t) {
  struct cb_data *cbd = g_new0(struct cb_data, 1);
  cbd->callback = cb;
  cbd->type = t;
  return cbd;
}


static void cb_data_free(struct cb_data *data) {
  Py_XDECREF(data->callback);
  g_free(data);
}


static PyObject *unmarshal(struct mwStorageUnit *item, enum cb_type t) {
  struct mwOpaque *p;
  char *c;
  PyObject *o;

  switch(t) {
  case type_INT:
    o = PyInt_FromLong(mwStorageUnit_asInteger(item, 0));
    break;

  case type_STR:
    c = mwStorageUnit_asString(item);
    o = PyString_SafeFromString(c);
    g_free(c);
    break;

  case type_BUF:
    p = mwStorageUnit_asOpaque(item);
    o = PyBuffer_FromMemory(p->data, p->len);
    break;

  default:
    Py_INCREF(Py_None);
    g_return_val_if_reached(Py_None);
  }

  return o;
}


static void mw_stored(struct mwServiceStorage *srvc,
		      guint32 result, struct mwStorageUnit *item,
		      struct cb_data *cb) {

  PyObject *args, *robj;

  g_return_if_fail(cb != NULL);
  g_return_if_fail(cb->callback != NULL);

  args = PyTuple_New(3);
  PyTuple_SetItem(args, 0, PyInt_FromLong(mwStorageUnit_getKey(item)));
  PyTuple_SetItem(args, 1, PyInt_FromLong(result));
  PyTuple_SetItem(args, 2, unmarshal(item, cb->type));

  robj = PyObject_CallObject(cb->callback, args);

  Py_DECREF(args);
  Py_XDECREF(robj);
}


static PyObject *py_load_t(mwPyService *self, PyObject *args, enum cb_type t) {
  struct mwServiceStorage *srvc;
  guint32 key;
  PyObject *cb;
  struct cb_data *cbd;
  struct mwStorageUnit *su;

  srvc = (struct mwServiceStorage *) self->wrapped;

  if(! PyArg_ParseTuple(args, "lO", &key, &cb))
    return NULL;

  if(! PyCallable_Check(cb))
    return NULL;

  Py_INCREF(cb);

  cbd = cb_data_new(cb, t);
  su = mwStorageUnit_new(key);
  
  mwServiceStorage_load(srvc, su, (mwStorageCallback) mw_stored,
			cbd, (GDestroyNotify) cb_data_free);

  mw_return_none();
}


static PyObject *py_load_int(mwPyService *self, PyObject *args) {
  return py_load_t(self, args, type_INT);
}


static PyObject *py_load_str(mwPyService *self, PyObject *args) {
  return py_load_t(self, args, type_STR);
}


static PyObject *py_load_buf(mwPyService *self, PyObject *args) {
  return py_load_t(self, args, type_BUF);
}


static struct PyMethodDef tp_methods[] = {
  { "load", (PyCFunction) py_load_buf, METH_VARARGS,
    "initiate a load request" },

  { "loadString", (PyCFunction) py_load_str, METH_VARARGS,
    "initiate a load request for a string value" },

  { "loadInt", (PyCFunction) py_load_int, METH_VARARGS,
    "initiate a load request for an integer value" },

  { NULL }
};


static PyObject *tp_new(PyTypeObject *t, PyObject *args, PyObject *kwds) {
  mwPyService *self;
  mwPySession *sessobj;
  struct mwSession *session;
  struct mwServiceStorage *srvc_store;
  
  if(! PyArg_ParseTuple(args, "O", &sessobj))
    return NULL;

  if(! PyObject_IsInstance((PyObject *) sessobj,
			   (PyObject *) mwPySession_type()))
    return NULL;

  self = (mwPyService *) t->tp_alloc(t, 0);

  /* wrapped version of the underlying service's session */
  Py_INCREF(sessobj);
  self->session = sessobj;
  session = sessobj->session;

  /* create the storage service */
  srvc_store = mwServiceStorage_new(session);

  /* create a python wrapper service built around this instance */
  /* sets self->wrapper and self->service */
  mwServicePyWrap_wrap(self, MW_SERVICE(srvc_store));
  
  return (PyObject *) self;
}


static PyTypeObject pyType_mwServiceStorage = {
  PyObject_HEAD_INIT(NULL)
  0, /* ob_size */
  "_meanwhile.ServiceStorage",
  sizeof(mwPyService),
  0, /* tp_itemsize */
  0, /* tp_dealloc */
  0, /* tp_print */
  0, /* tp_getattr */
  0, /* tp_setattr */
  0, /* tp_compare */
  0, /* tp_repr */
  0, /* tp_as_number */
  0, /* tp_as_sequence */
  0, /* tp_as_mapping */
  0, /* tp_hash */
  0, /* tp_call */
  0, /* tp_str */
  0, /* tp_getattro */
  0, /* tp_setattro */
  0, /* tp_as_buffer */
  Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
  "Meanwhile client storage service",
  0, /* tp_traverse */
  0, /* tp_clear */
  0, /* tp_richcompare */
  0, /* tp_weaklistoffset */
  0, /* tp_iter */
  0, /* tp_iternext */
  tp_methods,
  0, /* tp_members */
  0, /* tp_getset */
  0, /* tp_base */
  0, /* tp_dict */
  0, /* tp_descr_get */
  0, /* tp_descr_set */
  0, /* tp_dictoffset */
  0, /* tp_init */
  0, /* tp_alloc */
  (newfunc) tp_new,
};


static PyTypeObject *py_type = NULL;


PyTypeObject *mwPyServiceStorage_type() {
  if(! py_type) {
    g_message("readying type mwPyServiceStorage");

    py_type = &pyType_mwServiceStorage;
    py_type->tp_base = mwPyService_type();

    Py_INCREF(py_type);
    PyType_Ready(py_type);
  }

  return py_type;
}

