
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

#include "py_meanwhile.h"
#include "../mw_channel.h"
#include "../mw_session.h"


static PyObject *py_get_id(mwPyChannel *self, gpointer data) {
  struct mwChannel *chan = self->channel;
  guint32 id = mwChannel_getId(chan);

  return PyGuint32_FromGuint32(id);
}


static int py_set_id(mwPyChannel *self, PyObject *val, gpointer data) {
  PyErr_SetString(PyExc_TypeError, "member 'id' is read-only");
  return -1;
}


static PyObject *py_get_session(mwPyChannel *self) {
  mwPySession *session = self->session;
  Py_INCREF(session);
  return (PyObject *) session;
}


static int py_set_session(mwPyChannel *self, PyObject *val, gpointer data) {
  PyErr_SetString(PyExc_TypeError, "member 'session' is read-only");
  return -1;
}


static PyObject *py_send(mwPyChannel *self, PyObject *args) {
  struct mwChannel *chan;
  guint32 type;
  struct mwOpaque o = { 0, 0 };
  int ret;

  if(! PyArg_ParseTuple(args, "it#", &type, &o.data, &o.len))
    return NULL;

  chan = self->channel;
  g_return_val_if_fail(chan != NULL, NULL);

  ret = mwChannel_send(chan, type, &o);
  return PyInt_FromLong(ret);
}


static struct PyMethodDef tp_methods[] = {
  { "send", (PyCFunction) py_send,
    METH_VARARGS, "send a message over a channel" },

  {NULL}
};


static struct PyGetSetDef tp_getset[] = {
  { "id", (getter) py_get_id, (setter) py_set_id,
    "channel's read-only unique ID number", NULL },

  { "session", (getter) py_get_session, (setter) py_set_session,
    "channel's owning session", NULL },
  
  {NULL}
};


static PyObject *tp_new(PyTypeObject *t, PyObject *args, PyObject *kwds) {
  mwPyChannel *self;

  PyObject *sobj;
  mwPySession *sess;
  struct mwSession *s;

  struct mwChannelSet *cs;
  struct mwChannel *chan;
  
  if(! PyArg_ParseTuple(args, "O", &sobj))
    return NULL;

  if(! PyObject_IsInstance(sobj, (PyObject *) mwPySession_type()))
    return NULL;

  sess = (mwPySession *) sobj;
  s = sess->session;
  g_return_val_if_fail(s != NULL, NULL);

  cs = mwSession_getChannels(s);
  g_return_val_if_fail(cs != NULL, NULL);

  chan = mwChannel_newOutgoing(cs);
  g_return_val_if_fail(chan != NULL, NULL);

  self = (mwPyChannel *) t->tp_alloc(t, 0);

  self->session = sess;
  Py_INCREF(sess);

  self->channel = chan;

  return (PyObject *) self;
}


static void tp_dealloc(mwPyChannel *self) {
  Py_XDECREF(self->session);
  self->session = NULL;
  self->channel = NULL;
  self->ob_type->tp_free((PyObject *) self);
}


static PyTypeObject pyType_mwChannel = {
  PyObject_HEAD_INIT(NULL)
  0, /* ob_size */
  "_meanwhile.Channel",
  sizeof(mwPyChannel),
  0, /* tp_itemsize */
  (destructor) tp_dealloc,
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
  Py_TPFLAGS_DEFAULT,
  "a Meanwhile channel",
  0, /* tp_traverse */
  0, /* tp_clear */
  0, /* tp_richcompare */
  0, /* tp_weaklistoffset */
  0, /* tp_iter */
  0, /* tp_iternext */
  tp_methods, /* tp_methods */
  0, /* tp_members */
  tp_getset, /* tp_getset */
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


PyTypeObject *mwPyChannel_type() {
  if(! py_type) {
    g_message("readying type mwPyChannel");
    py_type = &pyType_mwChannel;
    Py_INCREF(py_type);
    PyType_Ready(py_type);
  }

  return py_type;
}


mwPyChannel *mwPyChannel_wrap(mwPySession *session,
			      struct mwChannel *chan) {

  PyTypeObject *t = mwPyChannel_type();
  mwPyChannel *self;

  g_return_val_if_fail(session != NULL, NULL);
  g_return_val_if_fail(chan != NULL, NULL);

  self = (mwPyChannel *) t->tp_alloc(t, 0);

  self->session = session;
  Py_INCREF(session);

  self->channel = chan;

  return self;
}

