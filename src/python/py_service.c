
#include <Python.h>
#include <structmember.h>

#include <glib.h>

#include "py_meanwhile.h"
#include "../service.h"


#define GET_NAME  "getName"
#define GET_DESC  "getDesc"


struct mwServicePyWrap {
  struct mwService service;
  struct pyObj_mwService *self;

  PyObject *name;
  PyObject *desc;
};


static const char *mw_get_name(struct mwService *srvc) {
  struct mwServicePyWrap *py_srvc;
  PyObject *robj = NULL;

  py_srvc = (struct mwServicePyWrap *) srvc;
  robj = PyObject_CallMethod((PyObject *) py_srvc->self, GET_NAME, NULL);

  Py_XDECREF(py_srvc->name);
  py_srvc->name = robj;

  return PyString_SafeAsString(robj);
}


static const char *mw_get_desc(struct mwService *srvc) {
  struct mwServicePyWrap *py_srvc;
  PyObject *robj = NULL;

  py_srvc = (struct mwServicePyWrap *) srvc;
  robj = PyObject_CallMethod((PyObject *) py_srvc->self, GET_DESC, NULL);

  Py_XDECREF(py_srvc->desc);
  py_srvc->desc = robj;

  return PyString_SafeAsString(robj);
}


#if 0
static void __attribute__((used)) _recv(struct mwService *srvc,
					struct mwChannel *chan,
					guint16 msg_type,
					struct mwOpaque *data) {
  struct mwServicePyWrap *py_srvc;
  PyObject *sobj, *robj;
  PyObject *pbuf = PyBuffer_FromMemory(data->data, data->len);
  
  py_srvc = (struct mwServicePyWrap *) srvc;
  sobj = (PyObject *) py_srvc->self;

  robj = PyObject_CallMethod(sobj, "recv","iN",
			     PyInt_FromLong(msg_type), pbuf);
  Py_DECREF(pbuf);
  Py_XDECREF(robj);
}
#endif


static void mw_start(struct mwService *srvc) {
  struct mwServicePyWrap *py_srvc;
  PyObject *robj;

  py_srvc = (struct mwServicePyWrap *) srvc;
  robj = PyObject_CallMethod((PyObject *) py_srvc->self, "start", NULL);
  Py_XDECREF(robj);
}


static void mw_stop(struct mwService *srvc) {
  struct mwServicePyWrap *py_srvc;
  PyObject *robj;

  py_srvc = (struct mwServicePyWrap *) srvc;
  robj = PyObject_CallMethod((PyObject *) py_srvc->self, "stop", NULL);
  Py_XDECREF(robj);
}


static void mw_clear(struct mwService *srvc) {
  struct mwServicePyWrap *py_srvc = (struct mwServicePyWrap *) srvc;
  mwPyService *self = mwServicePyWrap_getSelf(py_srvc);

  if(self->wrapped) {
    mwService_free(self->wrapped);
    self->wrapped = NULL;
  }

  Py_XDECREF(py_srvc->name);
  py_srvc->name = NULL;

  Py_XDECREF(py_srvc->desc);
  py_srvc->desc = NULL;
}


static void mw_recv_create(struct mwService *srvc, struct mwChannel *c,
			 struct mwMsgChannelCreate *msg) {

  struct mwServicePyWrap *py_srvc = (struct mwServicePyWrap *) srvc;
  if(py_srvc->self->wrapped)
    mwService_recvChannelCreate(py_srvc->self->wrapped, c, msg);
}


static void mw_recv_accept(struct mwService *srvc, struct mwChannel *c,
			   struct mwMsgChannelAccept *msg) {
  
  struct mwServicePyWrap *py_srvc = (struct mwServicePyWrap *) srvc;
  if(py_srvc->self->wrapped)
    mwService_recvChannelAccept(py_srvc->self->wrapped, c, msg);
}


static void mw_recv_destroy(struct mwService *srvc, struct mwChannel *c,
			    struct mwMsgChannelDestroy *msg) {
  
  struct mwServicePyWrap *py_srvc = (struct mwServicePyWrap *) srvc;
  if(py_srvc->self->wrapped)
    mwService_recvChannelDestroy(py_srvc->self->wrapped, c, msg);
}


static void fwd_recv(struct mwService *srvc, struct mwChannel *c,
		      guint16 msg_type, struct mwOpaque *data) {

  struct mwServicePyWrap *py_srvc = (struct mwServicePyWrap *) srvc;
  if(py_srvc->self->wrapped)
    mwService_recv(py_srvc->self->wrapped, c, msg_type, data);
}


struct mwServicePyWrap *mwServicePyWrap_new(mwPyService *self,
					    guint32 type) {

  struct mwServicePyWrap *psrvc = g_new0(struct mwServicePyWrap, 1);
  struct mwService *srvc = &psrvc->service;
  struct mwSession *s = self->session->session;

  mwService_init(srvc, s, type);

  srvc->get_name = mw_get_name;
  srvc->get_desc = mw_get_desc;
  srvc->start = mw_start;
  srvc->stop = mw_stop;
  srvc->clear = mw_clear;

  /** @todo need the recv_channel functions */
  srvc->recv_channelCreate = mw_recv_create;
  srvc->recv_channelAccept = mw_recv_accept;
  srvc->recv_channelDestroy = mw_recv_destroy;
  srvc->recv = fwd_recv;

  psrvc->self = self;

  self->wrapper = psrvc;
  self->wrapped = NULL;

  return psrvc;
}


struct mwServicePyWrap *mwServicePyWrap_wrap(mwPyService *self,
					     struct mwService *service) {
  struct mwServicePyWrap *psrvc;
  struct mwSession *s = mwService_getSession(service);

  g_return_val_if_fail(self->session->session == s, NULL);

  psrvc = mwServicePyWrap_new(self, mwService_getType(service));
  self->wrapped = service;

  return psrvc;
}


mwPyService *mwServicePyWrap_getSelf(struct mwServicePyWrap *s) {
  g_return_val_if_fail(s != NULL, NULL);
  return s->self;
}


static PyObject *py_start(mwPyService *self) {
  if(self->wrapped) {
    mwService_start(self->wrapped);

    Py_INCREF(Py_None);
    return Py_None;

  } else {  
    return PyObject_CallMethod((PyObject *) self, "started", NULL);
  }
}


static PyObject *py_started(mwPyService *self) {
  mwService_started(MW_SERVICE(self->wrapper));

  Py_INCREF(Py_None);
  return Py_None;
}


static PyObject *py_stop(mwPyService *self) {
  if(self->wrapped) {
    mwService_stop(self->wrapped);

    Py_INCREF(Py_None);
    return Py_None;

  } else {
    return PyObject_CallMethod((PyObject *) self, "stopped", NULL);
  }
}


/* this is only available from Python, so it should only need to
   affect the wrapper's state */
static PyObject *py_stopped(mwPyService *self) {
  mwService_stopped(MW_SERVICE(self->wrapper));

  Py_INCREF(Py_None);
  return Py_None;
}


static PyObject *py_recv(mwPyService *self, PyObject *args) {
  struct mwService *srvc = self->wrapped;

  if(srvc) {
    mwPyChannel *chan;
    struct mwOpaque o = { 0, 0 };
    guint32 type;

    if(! PyArg_ParseTuple(args, "Oit#", &chan, &type, &o.data, &o.len))
      return NULL;

    if(! PyObject_IsInstance((PyObject *) chan,
			     (PyObject *) mwPyChannel_type()))
      return NULL;

    mwService_recv(srvc, chan->channel, type, &o);
  }

  Py_INCREF(Py_None);
  return Py_None;
}


static PyObject *py_get_type(mwPyService *self, gpointer data) {
  guint32 t = mwService_getType(MW_SERVICE(self->wrapper));
  return PyInt_FromLong(t);
}


static int py_set_type(mwPyService *self, PyObject *val, gpointer data) {
  PyErr_SetString(PyExc_TypeError, "member 'type' is read-only");
  return -1;
}


static PyObject *py_get_name(mwPyService *self) {
  const char *c = "";

  if(self->wrapped)
    c = mwService_getName(self->wrapped);

  return PyString_FromString(c);
}


static PyObject *py_get_desc(mwPyService *self) {
  const char *c = "";

  if(self->wrapped)
    c = mwService_getDesc(self->wrapped);

  return PyString_FromString(c);    
}


static PyObject *py_get_state(mwPyService *self, gpointer data) {
  guint32 state = (self->wrapped)? 
    mwService_getState(self->wrapped):
    mwService_getState(MW_SERVICE(self->wrapper));

  return PyInt_FromLong(state);
}


static int py_set_state(mwPyService *self, PyObject *val, gpointer data) {
  PyErr_SetString(PyExc_TypeError, "member 'state' is read-only");
  return -1;
}


static PyObject *py_get_session(mwPyService *self, gpointer data) {
  PyObject *ret = (PyObject *) self->session;

  if(! ret) ret = Py_None;
  Py_INCREF(ret);

  return ret;
}


static int py_set_session(mwPyService *self, PyObject *val, gpointer data) {
  PyErr_SetString(PyExc_TypeError, "member 'session' is read-only");
  return -1;
}


static struct PyMethodDef tp_methods[] = {
  { "start", (PyCFunction) py_start,
    METH_NOARGS, "override to provide startup functions for the service" },

  { "started", (PyCFunction) py_started,
    METH_NOARGS, "indicate that the service is completely started" },

  { "stop", (PyCFunction) py_stop,
    METH_NOARGS, "override to provide additional shutdown functions for"
    " the service" },

  { "stopped", (PyCFunction) py_stopped,
    METH_NOARGS, "indicate that the service is completely stopped" },

  /*
  { "recvChannelCreate", (PyCFunction) _py_recv_create,
    METH_VARARGS, "" },

  { "recvChannelAccept", (PyCFunction) _py_recv_accept,
    METH_VARARGS, "" },

  { "recvChannelDestroy", (PyCFunction) _py_recv_destroy,
    METH_VARARGS, "" },
  */

  { "recv", (PyCFunction) py_recv,
    METH_VARARGS, "override to process data received over a channel"
    " belonging to this service" },

  { "getName", (PyCFunction) py_get_name,
    METH_NOARGS, "override to provide the service's name" },

  { "getDesc", (PyCFunction) py_get_desc,
    METH_NOARGS, "override to provide the service's description" },

  {NULL},
};


static PyGetSetDef tp_getset[] = {
  { "type", (getter) py_get_type, (setter) py_set_type,
    "read-only. The registered type number for this service", NULL },

  { "state", (getter) py_get_state, (setter) py_set_state,
    "read-only. The service's internal state", NULL },

  { "session", (getter) py_get_session, (setter) py_set_session,
    "read-only. The service's owning Session" },

  {NULL},
};


static PyObject *tp_new(PyTypeObject *t, PyObject *args, PyObject *kwds) {
  mwPyService *self;

  PyObject *sobj;
  guint32 type = 0;
  
  if(! PyArg_ParseTuple(args, "Oi", &sobj, &type))
    return NULL;

  if(! PyObject_IsInstance(sobj, (PyObject *) mwPySession_type()))
    return NULL;

  self = (mwPyService *) t->tp_alloc(t, 0);

  self->session = (mwPySession *) sobj;
  Py_INCREF(self->session);

  /* internally attaches to self->wrapper */
  mwServicePyWrap_new(self, type);

  return (PyObject *) self;
}


static void tp_dealloc(mwPyService *self) {
  /* this ends up calling mw_clear, freeing self->wrapped */
  mwService_free(MW_SERVICE(self->wrapper));
  self->wrapper = NULL;

  Py_XDECREF(self->session);
  self->session = NULL;

  self->ob_type->tp_free((PyObject *) self);
}


static PyTypeObject pyType_mwService = {
  PyObject_HEAD_INIT(NULL)
  0, /* ob_size */
  "_meanwhile.Service",
  sizeof(mwPyService),
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
  Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
  "base class for Meanwhile client services",
  0, /* tp_traverse */
  0, /* tp_clear */
  0, /* tp_richcompare */
  0, /* tp_weaklistoffset */
  0, /* tp_iter */
  0, /* tp_iternext */
  tp_methods,
  0, /* tp_members */
  tp_getset,
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


PyTypeObject *mwPyService_type() {
  if(! py_type) {
    g_message("readying type mwPyService");
    py_type = &pyType_mwService;
    Py_INCREF(py_type);
    PyType_Ready(py_type);
  }

  return py_type;
}

