
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
#include "../mw_service.h"
#include "../mw_srvc_aware.h"


#define ON_AWARE  "onAware"


static void onAwareHandler(struct mwAwareList *l,
			   struct mwAwareSnapshot *snap,
			   gpointer data) {

  mwPyService *self = data;
  PyObject *it, *st; /* id and state tuples */
  PyObject *res;

  /*
  (user, community, type, group)
  None or (status, time, desc, alt, name)
  */

  it = PyTuple_New(4);
  PyTuple_SetItem(it, 0, PyString_SafeFromString(snap->id.user));
  PyTuple_SetItem(it, 1, PyString_SafeFromString(snap->id.community));
  PyTuple_SetItem(it, 2, PyInt_FromLong(snap->id.type));
  PyTuple_SetItem(it, 3, PyString_SafeFromString(snap->group));
  
  if(snap->online) {
    st = PyTuple_New(5);
    PyTuple_SetItem(st, 0, PyInt_FromLong(snap->status.status));
    PyTuple_SetItem(st, 1, PyInt_FromLong(snap->status.time));
    PyTuple_SetItem(st, 2, PyString_SafeFromString(snap->status.desc));
    PyTuple_SetItem(st, 3, PyString_SafeFromString(snap->alt_id));
    PyTuple_SetItem(st, 4, PyString_SafeFromString(snap->name));
		    
  } else {
    Py_INCREF(Py_None);
    st = Py_None;
  }
  
  res = PyObject_CallMethod((PyObject *) self, ON_AWARE, "NN", it, st);
  Py_XDECREF(res);
}


static PyObject *py_aware_add(mwPyService *self, PyObject *args) {
  struct mwAwareIdBlock *idb, *id;
  PyObject *pl;
  int c;
  GList *gl = NULL;

  if(! PyArg_ParseTuple(args, "O", &pl))
    return NULL;

  if(! PyList_Check(pl)) {
    g_warning("not a list");
    return NULL;
  }

  c = PyList_Size(pl);
  idb = id = g_new0(struct mwAwareIdBlock, c);

  while(c--) {
    PyObject *t = PyList_GetItem(pl, c);

    if(!PyTuple_Check(t) || PyTuple_Size(t) < 3) continue;

    id->user = (char *) PyString_SafeAsString(PyTuple_GetItem(t, 0));
    id->community = (char *) PyString_SafeAsString(PyTuple_GetItem(t, 1));
    id->type = PyInt_AsLong(PyTuple_GetItem(t, 2));

    gl = g_list_prepend(gl, id++);
  }

  c = mwAwareList_addAware(self->data, gl);

  g_list_free(gl);
  g_free(idb);

  return PyInt_FromLong(! c);
}


static PyObject *py_aware_remove(mwPyService *self, PyObject *args) {
  struct mwAwareIdBlock *idb, *id;
  PyObject *pl;
  int c;
  GList *gl = NULL;

  if(! PyArg_ParseTuple(args, "O", &pl))
    return NULL;

  if(! PyList_Check(pl)) {
    g_warning("not a list");
    return NULL;
  }

  c = PyList_Size(pl);
  idb = id = g_new0(struct mwAwareIdBlock, c);

  while(c--) {
    PyObject *t = PyList_GetItem(pl, c);

    if(!PyTuple_Check(t) || PyTuple_Size(t) < 3) continue;

    id->user = (char *) PyString_SafeAsString(PyTuple_GetItem(t, 0));
    id->community = (char *) PyString_SafeAsString(PyTuple_GetItem(t, 1));
    id->type = PyInt_AsLong(PyTuple_GetItem(t, 2));

    gl = g_list_prepend(gl, id++);
  }

  c = mwAwareList_removeAware(self->data, gl);

  g_list_free(gl);
  g_free(idb);

  return PyInt_FromLong(! c);
}


static PyObject *py_aware_get(mwPyService *self, PyObject *args) {
  /* @todo implement */

  mw_return_none();
}


static PyObject *py_aware_set(mwPyService *self, PyObject *args) {
  struct mwAwareIdBlock id = { 0, 0, 0 };
  struct mwUserStatus stat = { 0, 0, 0 };
  struct mwServiceAware *srvc;

  srvc = (struct mwServiceAware *) self->wrapped;

  /* id = (user, community, type)
     stat = None or (status, time, desc) */

  /* compose a mwAwareIdBlock and mwUserStatus from args */
  mwServiceAware_setStatus(srvc, &id, &stat);

  mwAwareIdBlock_clear(&id);
  mwUserStatus_clear(&stat);
  
  Py_INCREF(Py_None);
  return Py_None;
}


static struct PyMethodDef tp_methods[] = {
  { "add", (PyCFunction) py_aware_add, METH_VARARGS,
    "add an aware ID to the watch list" },

  { "remove", (PyCFunction) py_aware_remove, METH_VARARGS,
    "remove an aware ID from the watch list" },

  { ON_AWARE, MW_METH_VARARGS_NONE, METH_VARARGS,
    "override to handle receipt of an awareness update" },

  { "getAware", (PyCFunction) py_aware_get, METH_VARARGS,
    "obtain the most recent awareness update for an aware ID" },

  { "setAware", (PyCFunction) py_aware_set, METH_VARARGS,
    "force an awareness update with the given data" },

  { NULL }
};


static PyObject *tp_new(PyTypeObject *t, PyObject *args, PyObject *kwds) {
  mwPyService *self;
  mwPySession *sessobj;
  struct mwSession *session;
  struct mwServiceAware *srvc_aware;
  
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

  /* create the im service and a single aware list */
  srvc_aware = mwServiceAware_new(session);
  self->data = mwAwareList_new(srvc_aware);
  self->cleanup = (GDestroyNotify) mwAwareList_free;
  mwAwareList_setOnAware(self->data, onAwareHandler, self, NULL);

  /* create a python wrapper service built around this instance */
  /* sets self->wrapper and self->service */
  mwServicePyWrap_wrap(self, MW_SERVICE(srvc_aware));
  
  return (PyObject *) self;
}


static PyTypeObject pyType_mwServiceAware = {
  PyObject_HEAD_INIT(NULL)
  0, /* ob_size */
  "_meanwhile.ServiceAware",
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
  "Meanwhile client service for presence awareness",
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


PyTypeObject *mwPyServiceAware_type() {
  if(! py_type) {
    g_message("readying type mwPyServiceAware");

    py_type = &pyType_mwServiceAware;
    py_type->tp_base = mwPyService_type();

    Py_INCREF(py_type);
    PyType_Ready(py_type);
  }

  return py_type;
}

