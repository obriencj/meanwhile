
#include <Python.h>
#include <structmember.h>

#include <glib.h>
#include <string.h>

#include "py_meanwhile.h"
#include "../mw_common.h"
#include "../mw_debug.h"
#include "../mw_service.h"
#include "../mw_srvc_conf.h"


#define ON_TEXT       "onText"
#define ON_PEER_JOIN  "onPeerJoin"
#define ON_PEER_PART  "onPeerPart"
#define ON_TYPING     "onTyping"
#define ON_INVITED    "onInvited"
#define ON_OPENED     "onOpened"
#define ON_CLOSING    "onClosing"


/* 
   done similarly to the IM service, but with each conference
   identified by the conference_name, which is generated at creation
   and immutable. 
*/


static void mw_on_invited(struct mwConference *conf,
			  struct mwLoginInfo *inviter, const char *invite) {

  struct pyObj_mwService *self;
  PyObject *robj;
  PyObject *a, *b, *c, *d;

  self = mwService_getClientData(MW_SERVICE(mwConference_getService(conf)));

  a = PyString_SafeFromString(mwConference_getName(conf));
  b = PyString_SafeFromString(inviter->user_id);
  c = PyString_SafeFromString(inviter->community);
  d = PyString_SafeFromString(invite);

  robj = PyObject_CallMethod((PyObject *) self, ON_INVITED,
			     "N(NN)N", a, b, c, d);
  Py_XDECREF(robj);
}


static void mw_conf_opened(struct mwConference *conf, GList *members) {
  struct pyObj_mwService *self;
  PyObject *robj;
  PyObject *a, *m;

  self = mwService_getClientData(MW_SERVICE(mwConference_getService(conf)));

  a = PyString_SafeFromString(mwConference_getName(conf));
  m = PyList_New(0);
  
  while(members) {
    struct mwLoginInfo *i = members->data;
    PyObject *t = PyTuple_New(2);

    PyTuple_SetItem(t, 0, PyString_SafeFromString(i->user_id));
    PyTuple_SetItem(t, 1, PyString_SafeFromString(i->community));

    PyList_Append(m, t);
    members = members->next;
  }

  robj = PyObject_CallMethod((PyObject *) self, ON_OPENED,
			     "NN", a, m);
  Py_XDECREF(robj);
}


static void mw_conf_closed(struct mwConference *conf, guint32 reason) {
  struct pyObj_mwService *self;
  PyObject *robj;
  PyObject *a, *b;

  self = mwService_getClientData(MW_SERVICE(mwConference_getService(conf)));

  a = PyString_SafeFromString(mwConference_getName(conf));
  b = PyInt_FromLong(reason);

  robj = PyObject_CallMethod((PyObject *) self, ON_CLOSING,
			     "NN", a, b);
  Py_XDECREF(robj);
}


static void mw_on_peer_joined(struct mwConference *conf,
			      struct mwLoginInfo *who) {

  struct pyObj_mwService *self;
  PyObject *robj;
  PyObject *a, *b, *c;

  self = mwService_getClientData(MW_SERVICE(mwConference_getService(conf)));

  a = PyString_SafeFromString(mwConference_getName(conf));
  b = PyString_SafeFromString(who->user_id);
  c = PyString_SafeFromString(who->community);

  robj = PyObject_CallMethod((PyObject *) self, ON_PEER_JOIN,
			     "N(NN)", a, b, c);
  Py_XDECREF(robj);
}


static void mw_on_peer_parted(struct mwConference *conf,
			      struct mwLoginInfo *who) {

  struct pyObj_mwService *self;
  PyObject *robj;
  PyObject *a, *b, *c;

  self = mwService_getClientData(MW_SERVICE(mwConference_getService(conf)));

  a = PyString_SafeFromString(mwConference_getName(conf));
  b = PyString_SafeFromString(who->user_id);
  c = PyString_SafeFromString(who->community);

  robj = PyObject_CallMethod((PyObject *) self, ON_PEER_PART,
			     "N(NN)", a, b, c);
  Py_XDECREF(robj);
}


static void mw_on_text(struct mwConference *conf,
		       struct mwLoginInfo *who, const char *what) {

  struct pyObj_mwService *self;
  PyObject *robj;
  PyObject *a, *b, *c, *d;

  self = mwService_getClientData(MW_SERVICE(mwConference_getService(conf)));

  a = PyString_SafeFromString(mwConference_getName(conf));
  b = PyString_SafeFromString(who->user_id);
  c = PyString_SafeFromString(who->community);
  d = PyString_SafeFromString(what);

  robj = PyObject_CallMethod((PyObject *) self, ON_TEXT,
			     "N(NN)N", a, b, c, d);
  Py_XDECREF(robj);
}


static void mw_on_typing(struct mwConference *conf,
			 struct mwLoginInfo *who, gboolean typing) {
  
  struct pyObj_mwService *self;
  PyObject *robj;
  PyObject *a, *b, *c, *d;

  self = mwService_getClientData(MW_SERVICE(mwConference_getService(conf)));

  a = PyString_SafeFromString(mwConference_getName(conf));
  b = PyString_SafeFromString(who->user_id);
  c = PyString_SafeFromString(who->community);
  d = PyInt_FromLong(typing);

  robj = PyObject_CallMethod((PyObject *) self, ON_TYPING,
			     "N(NN)N", a, b, c, d);
  Py_XDECREF(robj);
}


static void mw_clear(struct mwServiceConference *srvc) {
  g_free(mwServiceConference_getHandler(srvc));
}


static PyObject *py_conf_new(mwPyService *self, PyObject *args) {
  mw_return_none();
}


static PyObject *py_conf_open(mwPyService *self, PyObject *args) {
  mw_return_none();
}


static PyObject *py_conf_close(mwPyService *self, PyObject *args) {
  mw_return_none();
}


static PyObject *py_send_text(mwPyService *self, PyObject *args) {
  mw_return_none();
}


static PyObject *py_send_typing(mwPyService *self, PyObject *args) {
  mw_return_none();
}


static struct PyMethodDef tp_methods[] = {
  { ON_TEXT, MW_METH_VARARGS_NONE, METH_VARARGS,
    "override to receive text messages" },

  { ON_PEER_JOIN, MW_METH_VARARGS_NONE, METH_VARARGS,
    "override to receive notification of peers joining" },

  { ON_PEER_PART, MW_METH_VARARGS_NONE, METH_VARARGS,
    "override to receive notification of peers leaving" },

  { ON_TYPING, MW_METH_VARARGS_NONE, METH_VARARGS,
    "override to receive typing notification" },

  { ON_INVITED, MW_METH_VARARGS_NONE, METH_VARARGS,
    "override to receive invitation notices" },

  { ON_OPENED, MW_METH_VARARGS_NONE, METH_VARARGS,
    "override to receive notification of successful conference joining" },

  { ON_CLOSING, MW_METH_VARARGS_NONE, METH_VARARGS,
    "override to receive notification of conference closing" },

  { "createConference", (PyCFunction) py_conf_new, METH_VARARGS,
    "create an outgoing conference" },
  
  { "openConference", (PyCFunction) py_conf_open, METH_VARARGS,
    "open an outgoing conference or accept an invitation to a conference" },

  { "closeConference", (PyCFunction) py_conf_close, METH_VARARGS,
    "close a conference" },

  { "sendText", (PyCFunction) py_send_text, METH_VARARGS,
    "send a text message" },

  { "sendTyping", (PyCFunction) py_send_typing, METH_VARARGS,
    "send typing notification" },

  { NULL }
};


static PyObject *tp_new(PyTypeObject *t, PyObject *args, PyObject *kwds) {
  mwPyService *self;
  mwPySession *sessobj;
  struct mwSession *session;
  struct mwConferenceHandler *handler;
  struct mwServiceConference *srvc_conf;
  
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

  /* handler with our call-backs */
  handler = g_new0(struct mwConferenceHandler, 1);
  handler->on_invited = mw_on_invited;
  handler->conf_opened = mw_conf_opened;
  handler->conf_closed = mw_conf_closed;
  handler->on_peer_joined = mw_on_peer_joined;
  handler->on_peer_parted = mw_on_peer_parted;
  handler->on_text = mw_on_text;
  handler->on_typing = mw_on_typing;
  handler->clear = mw_clear;
  
  /* create the conference service */
  srvc_conf = mwServiceConference_new(session, handler);

  /* store the self object on the service's client data slot */
  mwService_setClientData(MW_SERVICE(srvc_conf), self, NULL);

  /* create a python wrapper service built around this instance */
  /* sets self->wrapper and self->service */
  mwServicePyWrap_wrap(self, MW_SERVICE(srvc_conf));
  
  return (PyObject *) self;
}


static PyTypeObject pyType_mwServiceConference = {
  PyObject_HEAD_INIT(NULL)
  0, /* ob_size */
  "_meanwhile.ServiceConference",
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
  "Meanwhile client service for conferencing",
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


PyTypeObject *mwPyServiceConference_type() {
  if(! py_type) {
    g_message("readying type mwPyServiceConference");

    py_type = &pyType_mwServiceConference;
    py_type->tp_base = mwPyService_type();

    Py_INCREF(py_type);
    PyType_Ready(py_type);
  }

  return py_type;
}

