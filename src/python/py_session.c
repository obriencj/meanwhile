

#include <Python.h>
#include <structmember.h>

#include "py_meanwhile.h"
#include "../service.h"
#include "../session.h"
#include "../channel.h"


#define ON_IO_WRITE      "onIoWrite"
#define ON_IO_CLOSE      "onIoClose"
#define ON_STATE_CHANGE  "onStateChange"
#define ON_SET_PRIVACY   "onSetPrivacy"
#define ON_SET_USER      "onSetUserStatus"
#define ON_ADMIN         "onAdmin"
#define ON_REDIR         "onLoginRedirect"


static int mw_io_write(struct mwSession *s, const char *buf, gsize len) {
  PyObject *sobj, *robj;
  PyObject *pbuf;
  int ret = -1;
  
  sobj = mwSession_getHandler(s)->data;
  g_return_val_if_fail(sobj != NULL, -1);

  pbuf = PyBuffer_FromMemory((char *) buf, len);
  g_return_val_if_fail(pbuf != NULL, -1);

  robj = PyObject_CallMethod(sobj, ON_IO_WRITE, "O", pbuf);

  if(robj && PyInt_Check(robj))
    ret = (int) PyInt_AsLong(robj);

  Py_DECREF(pbuf);
  Py_XDECREF(robj);

  return ret;
}


static void mw_io_close(struct mwSession *s) {
  PyObject *sobj, *robj;

  sobj = mwSession_getHandler(s)->data;
  g_return_if_fail(sobj != NULL);

  robj = PyObject_CallMethod(sobj, ON_IO_CLOSE, NULL);
  Py_XDECREF(robj);
}


static void mw_clear(struct mwSession *s) {
  g_free(mwSession_getHandler(s));
}


static void mw_on_state(struct mwSession *s,
		      enum mwSessionState state, guint32 info) {

  PyObject *sobj, *robj;

  sobj = mwSession_getHandler(s)->data;
  g_return_if_fail(sobj != NULL);

  robj = PyObject_CallMethod(sobj, ON_STATE_CHANGE, NULL);
  Py_XDECREF(robj);
}


static void mw_on_privacy(struct mwSession *s) {
  PyObject *sobj, *robj;

  sobj = mwSession_getHandler(s)->data;
  g_return_if_fail(sobj != NULL);

  robj = PyObject_CallMethod(sobj, ON_SET_PRIVACY, NULL);
  Py_XDECREF(robj);
}


static void mw_on_user(struct mwSession *s) {
  PyObject *sobj, *robj;

  sobj = mwSession_getHandler(s)->data;
  g_return_if_fail(sobj != NULL);

  robj = PyObject_CallMethod(sobj, ON_SET_USER, NULL);
  Py_XDECREF(robj);
}


static void mw_on_admin(struct mwSession *s, const char *text) {
  PyObject *sobj, *robj;
  PyObject *a;

  sobj = mwSession_getHandler(s)->data;
  g_return_if_fail(sobj != NULL);

  a = PyString_SafeFromString(text);
  robj = PyObject_CallMethod(sobj, ON_ADMIN, "O", a);

  Py_DECREF(a);
  Py_XDECREF(robj);
}


static void mw_on_redirect(struct mwSession *s, const char *host) {
  PyObject *sobj, *robj;
  PyObject *a;

  sobj = mwSession_getHandler(s)->data;
  g_return_if_fail(sobj != NULL);

  a = PyString_SafeFromString(host);
  robj = PyObject_CallMethod(sobj, ON_REDIR, "O", a);

  Py_DECREF(a);
  Py_XDECREF(robj);
}


static PyObject *py_io_write(mwPySession *self, PyObject *args) {
  return PyInt_FromLong(-1);
}


static PyObject *py_io_close(mwPySession *self) {
  Py_INCREF(Py_None);
  return Py_None;
}


static PyObject *py_on_state(mwPySession *self) {
  Py_INCREF(Py_None);
  return Py_None;
}


static PyObject *py_on_privacy(mwPySession *self) {
  Py_INCREF(Py_None);
  return Py_None;
}


static PyObject *py_on_user(mwPySession *self) {
  Py_INCREF(Py_None);
  return Py_None;
}


static PyObject *py_on_admin(mwPySession *self, PyObject *args) {
  Py_INCREF(Py_None);
  return Py_None;
}


static PyObject *py_on_redirect(mwPySession *self, PyObject *args) {
  Py_INCREF(Py_None);
  return Py_None;
}


static PyObject *py_start(mwPySession *self, PyObject *args) {
  PyObject *uo, *po;
  const char *user, *pass;

  if(! PyArg_ParseTuple(args, "(OO)", &uo, &po))
    return NULL;

  user = PyString_SafeAsString(uo);
  pass = PyString_SafeAsString(po);

  mwSession_setUserId(self->session, user);
  mwSession_setPassword(self->session, pass);

  mwSession_start(self->session);

  Py_INCREF(Py_None);
  return Py_None;
}


static PyObject *py_stop(mwPySession *self, PyObject *args) {
  guint32 reason = 0x00;
  
  if(! PyArg_ParseTuple(args, "|l", &reason))
    return NULL;

  mwSession_stop(self->session, reason);

  Py_INCREF(Py_None);
  return Py_None;
}


static PyObject *py_recv(mwPySession *self, PyObject *args) {
  char *buf;
  gsize len;

  if(! PyArg_ParseTuple(args, "t#", &buf, &len))
    return NULL;

  mwSession_recv(self->session, buf, len);

  Py_INCREF(Py_None);
  return Py_None;
}


static PyObject *py_find_channel(mwPySession *self, PyObject *args) {
  guint32 id = 0;
  struct mwChannel *c;

  if(! PyArg_ParseTuple(args, "i", &id))
    return NULL;

  c = mwChannel_find(mwSession_getChannels(self->session), id);
  if(c) {
    return (PyObject *) mwPyChannel_wrap(self, c);

  } else {
    Py_INCREF(Py_None);
    return Py_None;
  }
}


static PyObject *py_add_service(mwPySession *self, PyObject *args) {
  struct mwSession *sess;
  mwPyService *srvcobj;
  struct mwService *srvc;

  if(! PyArg_ParseTuple(args, "O", &srvcobj))
    return NULL;

  if(! PyObject_IsInstance((PyObject *) srvcobj,
			   (PyObject *) mwPyService_type()))
    return NULL;

  sess = self->session;
  srvc = MW_SERVICE(srvcobj->wrapper);

  if(g_hash_table_lookup(self->services, srvc) ||
     ! mwSession_addService(sess, srvc)) {
    return PyInt_FromLong(0);
  }
      
  Py_INCREF(srvcobj);
  g_hash_table_insert(self->services, srvc, srvcobj);
  return PyInt_FromLong(1);
}


static PyObject *py_get_service(mwPySession *self, PyObject *args) {
  guint32 id = 0;
  struct mwService *srvc;
  mwPyService *srvcobj;

  if(! PyArg_ParseTuple(args, "i", &id))
    return NULL;

  srvc = mwSession_getService(self->session, id);
  if(! srvc) {
    Py_INCREF(Py_None);
    return Py_None;
  }

  srvcobj = g_hash_table_lookup(self->services, srvc);
  g_return_val_if_fail(srvcobj != NULL, NULL);

  Py_INCREF(srvcobj);
  return (PyObject *) srvcobj;
}


static PyObject *py_rem_service(mwPySession *self, PyObject *args) {
  guint32 id = 0;
  struct mwService *srvc;
  mwPyService *srvcobj;

  if(! PyArg_ParseTuple(args, "i", &id))
    return NULL;

  srvc = mwSession_removeService(self->session, id);
  if(! srvc) {
    Py_INCREF(Py_None);
    return Py_None;
  }

  srvcobj = g_hash_table_lookup(self->services, srvc);
  g_return_val_if_fail(srvcobj != NULL, NULL);

  g_hash_table_steal(self->services, srvc);
  return (PyObject *) srvcobj;
}


static struct PyMethodDef tp_methods[] = {
  /* intended to be overridden by handler methods */
  { ON_IO_WRITE, (PyCFunction) py_io_write,
    METH_VARARGS, "override to handle session's outgoing data writes" },

  { ON_IO_CLOSE, (PyCFunction) py_io_close,
    METH_NOARGS, "override to handle session's outgoing close request" },

  { ON_STATE_CHANGE, (PyCFunction) py_on_state,
    METH_NOARGS, "override to be notified on session state change" },

  { ON_SET_PRIVACY, (PyCFunction) py_on_privacy,
    METH_NOARGS, "override to be notified on session privacy change" },

  { ON_SET_USER, (PyCFunction) py_on_user,
    METH_NOARGS, "override to be notified on session user status change" },

  { ON_ADMIN, (PyCFunction) py_on_admin,
    METH_VARARGS, "override to handle session's admin broadcast messages" },

  { ON_REDIR, (PyCFunction) py_on_redirect,
    METH_VARARGS, "override to handle login redirect messages" },

  /* real methods */
  { "start", (PyCFunction) py_start,
    METH_VARARGS, "start the session" },

  { "stop", (PyCFunction) py_stop,
    METH_VARARGS, "stop the session" },

  { "recv", (PyCFunction) py_recv,
    METH_VARARGS, "pass incoming socket data to the session for processing" },

  { "findChannel", (PyCFunction) py_find_channel,
    METH_NOARGS, "reference a channel by its channel id" },

  { "addService", (PyCFunction) py_add_service,
    METH_VARARGS, "add a service to the sesssion" },

  { "getService", (PyCFunction) py_get_service,
    METH_VARARGS, "lookup a service by its ID" },

  { "removeService", (PyCFunction) py_rem_service,
    METH_VARARGS, "remove a service from the session by its ID" },

  {NULL},
};


static PyObject *_new(PyTypeObject *t, PyObject *args, PyObject *kwds) {
  struct pyObj_mwSession *self;

  self = (struct pyObj_mwSession *) t->tp_alloc(t, 0);
  if(self) {
    struct mwSessionHandler *h;
    h = g_new0(struct mwSessionHandler, 1);
    h->data = self;

    h->io_write = mw_io_write;
    h->io_close = mw_io_close;
    h->clear = mw_clear;
    h->on_stateChange = mw_on_state;
    h->on_setPrivacyInfo = mw_on_privacy;
    h->on_setUserStatus = mw_on_user;
    h->on_admin = mw_on_admin;
    h->on_loginRedirect = mw_on_redirect;

    self->session = mwSession_new(h);
    self->services = g_hash_table_new(g_direct_hash, g_direct_equal);
  }

  return (PyObject *) self;
}


static void _dealloc(struct pyObj_mwSession *self) {
  mwSession_free(self->session);

  g_hash_table_destroy(self->services);
  /** @todo: need to Py_DECREF contained services */

  self->ob_type->tp_free((PyObject *) self);
}


static PyTypeObject pyType_mwSession = {
  PyObject_HEAD_INIT(NULL)
  0, /* ob_size */
  "_meanwhile.Session",
  sizeof(mwPySession),
  0, /* tp_itemsize */
  (destructor) _dealloc,
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
  "a Meanwhile client session",
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
  (newfunc) _new,
};


static PyTypeObject *py_type = NULL;


PyTypeObject *mwPySession_type() {
  if(! py_type) {
    g_message("readying type mwPySession");
    py_type = &pyType_mwSession;
    Py_INCREF(py_type);
    PyType_Ready(py_type);
  }

  return py_type;
}


