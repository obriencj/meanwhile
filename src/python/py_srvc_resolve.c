
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
#include "../mw_srvc_resolve.h"


/*
  MATCH ::= ((str) id, (str) name, (str) desc, (int) type)
  RESULT ::= ((int) code, (str) name, (MATCH,...))
  ARGS ::= ((int) key, (int) code, (RESULT,...))
*/


static PyObject *wrap_match(struct mwResolveMatch *m) {
  PyObject *t;

  t = PyTuple_New(4);
  PyTuple_SetItem(t, 0, PyString_SafeFromString(m->id));
  PyTuple_SetItem(t, 1, PyString_SafeFromString(m->name));
  PyTuple_SetItem(t, 2, PyString_SafeFromString(m->desc));
  PyTuple_SetItem(t, 3, PyInt_FromLong(m->type));

  return t;
}


static PyObject *wrap_matches(GList *matches) {
  PyObject *t;
  int count = 0;

  t = PyTuple_New(g_list_length(matches));

  for(; matches; matches = matches->next) {
    PyTuple_SetItem(t, count++, wrap_match(matches->data));
  }

  return t;
}


static PyObject *wrap_result(struct mwResolveResult *r) {
  PyObject *t;

  t = PyTuple_New(3);
  PyTuple_SetItem(t, 0, PyInt_FromLong(r->code));
  PyTuple_SetItem(t, 1, PyString_SafeFromString(r->name));
  PyTuple_SetItem(t, 2, wrap_matches(r->matches));

  return t;
}


static PyObject *wrap_results(GList *results) {
  PyObject *t;
  int count = 0;

  g_return_val_if_fail(results != NULL, NULL);

  t = PyTuple_New(g_list_length(results));

  for(; results; results = results->next) {
    PyTuple_SetItem(t, count++, wrap_result(results->data));
  }

  return t;
}


static void mw_search_cb(struct mwServiceResolve *srvc,
			 guint32 id, guint32 code, GList *results,
			 gpointer data) {

  PyObject *cb = data;
  PyObject *args, *robj;

  args = PyTuple_New(3);
  PyTuple_SetItem(args, 0, PyInt_FromLong(id));
  PyTuple_SetItem(args, 1, PyInt_FromLong(code));
  PyTuple_SetItem(args, 2, wrap_results(results));

  robj = PyObject_CallObject(cb, args);
  if(! robj) PyErr_Print();

  Py_DECREF(cb);
  Py_DECREF(args);
  Py_XDECREF(robj);
}


static GList *str_list(PyObject *l) {
  GList *v = NULL;
  int count = PyList_Size(l);
  
  while(count-- > 0) {
    PyObject *o = PyList_GetItem(l, count);
    v = g_list_prepend(v, (gpointer) PyString_SafeAsString(o));
  }

  return v;
}


static PyObject *py_search(mwPyService *self, PyObject *args) {
  struct mwServiceResolve *srvc;
  GList *queries;
  guint32 flags, id;

  PyObject *query;
  PyObject *cb;

  srvc = (struct mwServiceResolve *) self->wrapped;

  if(! PyArg_ParseTuple(args, "OlO", &query, &flags, &cb))
    return NULL;

  if(! PyList_Check(query)) {
    mw_raise("query needs to be a list of strings to resolve", NULL);
  }

  if(! PyCallable_Check(cb)) {
    mw_raise("callback argument needs to be callable", NULL);
  }

  Py_INCREF(cb);

  queries = str_list(query);
  id = mwServiceResolve_resolve(srvc, queries, flags,
				mw_search_cb, cb, NULL);
  g_list_free(queries);

  if(id != SEARCH_ERROR) {
    return PyInt_FromLong(id);
  } else {
    mw_raise("An error occured initiating the resolve query", NULL);
  }
}


static PyObject *py_cancel(mwPyService *self, PyObject *args) {
  struct mwServiceResolve *srvc;
  guint32 id;
  
  srvc = (struct mwServiceResolve *) self->wrapped;

  if(! PyArg_ParseTuple(args, "l", &id))
    return NULL;

  mwServiceResolve_cancelResolve(srvc, id);

  mw_return_none();
}


static struct PyMethodDef tp_methods[] = {
  { "resolve", (PyCFunction) py_search, METH_VARARGS,
    "initiate a user search" },

  { "cancel", (PyCFunction) py_cancel, METH_VARARGS,
    "cancel a user search" },

  { NULL }
};


static PyObject *tp_new(PyTypeObject *t, PyObject *args, PyObject *kwds) {
  mwPyService *self;
  mwPySession *sessobj;
  struct mwSession *session;
  struct mwServiceResolve *srvc_resolve;
  
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

  /* create the resolve service */
  srvc_resolve = mwServiceResolve_new(session);

  /* create a python wrapper service built around this instance */
  /* sets self->wrapper and self->service */
  mwServicePyWrap_wrap(self, MW_SERVICE(srvc_resolve));
  
  return (PyObject *) self;
}


static PyTypeObject pyType_mwServiceResolve = {
  PyObject_HEAD_INIT(NULL)
  0, /* ob_size */
  "_meanwhile.ServiceResolve",
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
  "Meanwhile client resolve service",
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


PyTypeObject *mwPyServiceResolve_type() {
  if(! py_type) {
    g_message("readying type mwPyServiceResolve");

    py_type = &pyType_mwServiceResolve;
    py_type->tp_base = mwPyService_type();

    Py_INCREF(py_type);
    PyType_Ready(py_type);
  }

  return py_type;
}

