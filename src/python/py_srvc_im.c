
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
#include <string.h>

#include "py_meanwhile.h"
#include "../mw_common.h"
#include "../mw_debug.h"
#include "../mw_service.h"
#include "../mw_srvc_im.h"


#define ON_TEXT     "onText"
#define ON_HTML     "onHtml"
#define ON_MIME     "onMime"
#define ON_SUBJECT  "onSubject"
#define ON_TYPING   "onTyping"
#define ON_CLOSED   "onClosed"
#define ON_OPENED   "onOpened"


/*
  Incoming IM message:
  Session -> py_service -> Python -> py_srvc_im -> srvc_im -> handler -> Python

  Outgoing IM message:
  Python -> py_srvc_im -> srvc_im -> Session
*/


static void mw_got_text(struct mwServiceIm *srvc,
			struct mwIdBlock *from, const char *text) {

  struct pyObj_mwService *self;
  PyObject *robj = NULL;
  PyObject *a, *b, *c;

  self = mwService_getClientData(MW_SERVICE(srvc));

  a = PyString_SafeFromString(from->user);
  b = PyString_SafeFromString(from->community);
  c = PyString_SafeFromString(text);

  robj = PyObject_CallMethod((PyObject *) self, ON_TEXT,
			     "(NN)N", a, b, c);
  Py_XDECREF(robj);
}


static void mw_got_html(struct mwServiceIm *srvc,
			struct mwIdBlock *from, const char *html) {

  struct pyObj_mwService *self;
  PyObject *robj = NULL;
  PyObject *a, *b, *c;

  self = mwService_getClientData(MW_SERVICE(srvc));

  a = PyString_SafeFromString(from->user);
  b = PyString_SafeFromString(from->community);
  c = PyString_SafeFromString(html);

  robj = PyObject_CallMethod((PyObject *) self, ON_HTML,
			     "(NN)N", a, b, c);
  Py_XDECREF(robj);
}


static void mw_got_mime(struct mwServiceIm *srvc,
			struct mwIdBlock *from, struct mwOpaque *mime) {

  struct pyObj_mwService *self;
  PyObject *robj = NULL;
  PyObject *a, *b, *c;

  self = mwService_getClientData(MW_SERVICE(srvc));

  a = PyString_SafeFromString(from->user);
  b = PyString_SafeFromString(from->community);
  c = PyBuffer_FromMemory(mime->data, mime->len);

  robj = PyObject_CallMethod((PyObject *) self, ON_MIME,
			     "(NN)N", a, b, c);
  Py_XDECREF(robj);
}


static void mw_got_subject(struct mwServiceIm *srvc,
			   struct mwIdBlock *from, const char *subj) {

  struct pyObj_mwService *self;
  PyObject *robj = NULL;
  PyObject *a, *b, *c;

  self = mwService_getClientData(MW_SERVICE(srvc));

  a = PyString_SafeFromString(from->user);
  b = PyString_SafeFromString(from->community);
  c = PyString_SafeFromString(subj);

  robj = PyObject_CallMethod((PyObject *) self, ON_SUBJECT,
			     "(NN)N", a, b, c);
  Py_XDECREF(robj);
}


static void mw_got_typing(struct mwServiceIm *srvc,
			  struct mwIdBlock *from, gboolean typing) {

  struct pyObj_mwService *self;
  PyObject *robj = NULL;
  PyObject *a, *b;

  self = mwService_getClientData(MW_SERVICE(srvc));

  a = PyString_SafeFromString(from->user);
  b = PyString_SafeFromString(from->community);

  robj = PyObject_CallMethod((PyObject *) self, ON_TYPING,
			     "(NN)l", a, b, typing);
  Py_XDECREF(robj);
}


static void mw_conversation_recv(struct mwConversation *conv,
				 enum mwImSendType type, gconstpointer msg) {

  struct mwIdBlock *idb;
  struct mwServiceIm *srvc;

  g_return_if_fail(conv != NULL);

  idb = mwConversation_getTarget(conv);
  srvc = mwConversation_getService(conv);

  switch(type) {
  case mwImSend_PLAIN:
    mw_got_text(srvc, idb, msg);
    break;
  case mwImSend_HTML:
    mw_got_html(srvc, idb, msg);
    break;
  case mwImSend_MIME:
    mw_got_mime(srvc, idb, (struct mwOpaque *) msg);
    break;
  case mwImSend_SUBJECT:
    mw_got_subject(srvc, idb, msg);
    break;
  case mwImSend_TYPING:
    mw_got_typing(srvc, idb, GPOINTER_TO_INT(msg));
    break;
  default:
    ; /* erm... */
  }
}


static void mw_conversation_opened(struct mwConversation *conv) {
  struct mwServiceIm *srvc;
  struct pyObj_mwService *self;
  struct mwIdBlock *idb;
  PyObject *robj = NULL;
  PyObject *t;

  srvc = mwConversation_getService(conv);
  self = mwService_getClientData(MW_SERVICE(srvc));
  idb = mwConversation_getTarget(conv);
  
  t = PyTuple_New(2);
  PyTuple_SetItem(t, 0, PyString_SafeFromString(idb->user));
  PyTuple_SetItem(t, 1, PyString_SafeFromString(idb->community));

  /* why the hell do I have to double-tuple to make this work? */
  robj = PyObject_CallMethod((PyObject *) self, ON_OPENED, "(N)", t);

  Py_XDECREF(robj);
}


static void mw_conversation_closed(struct mwConversation *conv, guint32 err) {
  struct mwServiceIm *srvc;
  struct pyObj_mwService *self;
  struct mwIdBlock *idb;
  PyObject *robj = NULL;
  PyObject *a, *b;
  
  srvc = mwConversation_getService(conv);
  self = mwService_getClientData(MW_SERVICE(srvc));
  idb = mwConversation_getTarget(conv);

  a = PyString_SafeFromString(idb->user);
  b = PyString_SafeFromString(idb->community);

  robj = PyObject_CallMethod((PyObject *) self, ON_CLOSED, 
			     "(NN)l", a, b, err);
  Py_XDECREF(robj); 
}


static void mw_clear(struct mwServiceIm *srvc) {
  g_free(mwServiceIm_getHandler(srvc));
}


static PyObject *py_send_text(mwPyService *self, PyObject *args) {
  struct mwIdBlock id = { 0, 0 };
  const char *text = NULL;
  struct mwServiceIm *srvc_im;
  struct mwConversation *conv;

  PyObject *a, *b, *c;

  if(! PyArg_ParseTuple(args, "(OO)O", &a, &b, &c))
    return NULL;

  id.user = (char *) PyString_SafeAsString(a);
  id.community = (char *) PyString_SafeAsString(b);
  text = PyString_SafeAsString(c);

  srvc_im = (struct mwServiceIm *) self->wrapped;
  conv = mwServiceIm_findConversation(srvc_im, &id);
  
  if(!conv || !MW_CONVO_IS_OPEN(conv)) {
    mw_raise("conversation not currently open", NULL);
  } else if(! mwConversation_supports(conv, mwImSend_PLAIN)) {
    mw_raise("conversation does not support sending plain text", NULL);
  }

  /* don't free text or clear id, those strings are borrowed */
  if(mwConversation_send(conv, mwImSend_PLAIN, text)) {
    mw_raise("error sending text message over conversation", NULL);
  } else {
    mw_return_none();
  }
}


static PyObject *py_send_html(mwPyService *self, PyObject *args) {
  struct mwIdBlock id = { 0, 0 };
  const char *text = NULL;
  struct mwServiceIm *srvc_im;
  struct mwConversation *conv;

  PyObject *a, *b, *c;

  if(! PyArg_ParseTuple(args, "(OO)O", &a, &b, &c))
    return NULL;

  id.user = (char *) PyString_SafeAsString(a);
  id.community = (char *) PyString_SafeAsString(b);
  text = PyString_SafeAsString(c);

  srvc_im = (struct mwServiceIm *) self->wrapped;
  conv = mwServiceIm_findConversation(srvc_im, &id);

  if(!conv || !MW_CONVO_IS_OPEN(conv)) {
    mw_raise("conversation not currently open", NULL);
  } else if(! mwConversation_supports(conv, mwImSend_HTML)) {
    mw_raise("conversation does not support sending HTML", NULL);
  }

  /* don't free text or clear id, those strings are borrowed */
  if(mwConversation_send(conv, mwImSend_HTML, text)) {
    mw_raise("error sending HTML message over conversation", NULL);
  } else {
    mw_return_none();
  }
}


static PyObject *py_send_mime(mwPyService *self, PyObject *args) {
  struct mwIdBlock id = { 0, 0 };
  struct mwOpaque data = { 0, 0 };
  struct mwServiceIm *srvc_im;
  struct mwConversation *conv;

  PyObject *a, *b;

  if(! PyArg_ParseTuple(args, "(OO)t#", &a, &b, &data.data, &data.len))
    return NULL;

  id.user = (char *) PyString_SafeAsString(a);
  id.community = (char *) PyString_SafeAsString(b);

  srvc_im = (struct mwServiceIm *) self->wrapped;
  conv = mwServiceIm_findConversation(srvc_im, &id);

  if(!conv || !MW_CONVO_IS_OPEN(conv)) {
    mw_raise("conversation not currently open", NULL);
  } else if(! mwConversation_supports(conv, mwImSend_MIME)) {
    mw_raise("conversation does not support sending MIME", NULL);
  }

  if(mwConversation_send(conv, mwImSend_MIME, &data)) {
    mw_raise("error sending MIME message over conversation", NULL);
  } else {
    mw_return_none();
  }
}


static PyObject *py_send_subject(mwPyService *self, PyObject *args) {
  struct mwIdBlock id = { 0, 0 };
  const char *text = NULL;
  struct mwServiceIm *srvc_im;
  struct mwConversation *conv;

  PyObject *a, *b, *c;

  if(! PyArg_ParseTuple(args, "(OO)O", &a, &b, &c))
    return NULL;

  id.user = (char *) PyString_SafeAsString(a);
  id.community = (char *) PyString_SafeAsString(b);
  text = PyString_SafeAsString(c);

  srvc_im = (struct mwServiceIm *) self->wrapped;
  conv = mwServiceIm_findConversation(srvc_im, &id);

  if(!conv || !MW_CONVO_IS_OPEN(conv)) {
    mw_raise("conversation not currently open", NULL);
  } else if(! mwConversation_supports(conv, mwImSend_SUBJECT)) {
    mw_raise("conversation does not support subjects", NULL);
  }

  /* don't free text or clear id, those strings are borrowed */
  if(mwConversation_send(conv, mwImSend_SUBJECT, text)) {
    mw_raise("error sending subject over conversation", NULL);
  } else {
    mw_return_none();
  }
}


static PyObject *py_send_typing(mwPyService *self, PyObject *args) {
  struct mwIdBlock id = { 0, 0 };
  gboolean typing = FALSE;
  struct mwServiceIm *srvc_im;
  struct mwConversation *conv;

  PyObject *a, *b;

  if(! PyArg_ParseTuple(args, "(OO)l", &a, &b, &typing))
    return NULL;

  id.user = (char *) PyString_SafeAsString(a);
  id.community = (char *) PyString_SafeAsString(b);

  srvc_im = (struct mwServiceIm *) self->wrapped;
  conv = mwServiceIm_findConversation(srvc_im, &id);

  if(!conv || !MW_CONVO_IS_OPEN(conv)) {
    mw_raise("conversation not currently open", NULL);
  } else if(! mwConversation_supports(conv, mwImSend_TYPING)) {
    mw_raise("conversation does not support typing notification", NULL);
  }

  /* don't clead id, it has borrowed strings */
  if(mwConversation_send(conv, mwImSend_TYPING, GINT_TO_POINTER(typing))) {
    mw_raise("error sending typing over conversation", NULL);
  } else {
    mw_return_none();
  }
}


static PyObject *py_convo_open(mwPyService *self, PyObject *args) {
  struct mwIdBlock id = { 0, 0 };
  struct mwServiceIm *srvc_im;
  struct mwConversation *conv;

  PyObject *a, *b;

  if(! PyArg_ParseTuple(args, "(OO)", &a, &b))
    return NULL;

  id.user = (char *) PyString_SafeAsString(a);
  id.community = (char *) PyString_SafeAsString(b);

  srvc_im = (struct mwServiceIm *) self->wrapped;
  conv = mwServiceIm_getConversation(srvc_im, &id);

  if(! MW_CONVO_IS_CLOSED(conv)) {
    mw_raise("conversation is already open or pending", NULL);
  } else {
    mwConversation_open(conv);
    mw_return_none();
  }
}


static PyObject *py_convo_close(mwPyService *self, PyObject *args) {
  struct mwIdBlock id = { 0, 0 };
  struct mwServiceIm *srvc_im;
  struct mwConversation *conv;
  guint32 err;

  PyObject *a, *b;

  if(! PyArg_ParseTuple(args, "(OO)l", &a, &b, &err))
    return NULL;

  id.user = (char *) PyString_SafeAsString(a);
  id.community = (char *) PyString_SafeAsString(b);

  srvc_im = (struct mwServiceIm *) self->wrapped;
  conv = mwServiceIm_findConversation(srvc_im, &id);

  if(!conv || MW_CONVO_IS_CLOSED(conv)) {
    mw_raise("conversation is not open or pending", NULL);
  } else {
    /** @todo maybe want to free the conversation as well, unsure */
    mwConversation_close(conv, err);
    mw_return_none();
  }
}


static PyObject *py_convo_get_state(mwPyService *self, PyObject *args) {
  struct mwIdBlock id = { 0, 0 };
  struct mwServiceIm *srvc_im;
  struct mwConversation *conv;

  PyObject *a, *b;

  if(! PyArg_ParseTuple(args, "(OO)", &a, &b))
    return NULL;

  id.user = (char *) PyString_SafeAsString(a);
  id.community = (char *) PyString_SafeAsString(b);

  srvc_im = (struct mwServiceIm *) self->wrapped;
  conv = mwServiceIm_findConversation(srvc_im, &id);

  if(! conv) {
    return PyInt_FromLong(mwConversation_CLOSED);
  } else {
    return PyInt_FromLong(mwConversation_getState(conv));
  }
}


static PyObject *py_convo_supports(mwPyService *self, PyObject *args) {
  struct mwIdBlock id = { 0, 0 };
  struct mwServiceIm *srvc_im;
  struct mwConversation *conv;
  enum mwImSendType feature;

  PyObject *a, *b;

  if(! PyArg_ParseTuple(args, "(OO)l", &a, &b, &feature))
    return NULL;

  id.user = (char *) PyString_SafeAsString(a);
  id.community = (char *) PyString_SafeAsString(b);

  srvc_im = (struct mwServiceIm *) self->wrapped;
  conv = mwServiceIm_findConversation(srvc_im, &id);

  if(! conv) {
    return PyInt_FromLong(FALSE);
  } else {
    return PyInt_FromLong(mwConversation_supports(conv, feature));
  }
}


static PyObject *py_supports(mwPyService *self, PyObject *args) {
  struct mwServiceIm *srvc;
  enum mwImSendType feature;

  if(! PyArg_ParseTuple(args, "l", &feature))
    return NULL;

  srvc = (struct mwServiceIm *) self->wrapped;

  return PyInt_FromLong(mwServiceIm_supports(srvc, feature));
}


static PyObject *py_get_client_type(mwPyService *self, gpointer data) {
  struct mwServiceIm *srvc;
  enum mwImClientType type;

  srvc = (struct mwServiceIm *) self->wrapped;
  type = mwServiceIm_getClientType(srvc);

  return PyInt_FromLong(type);
}


static int py_set_client_type(mwPyService *self, PyObject *val,
			      gpointer data) {

  struct mwServiceIm *srvc;
  enum mwImClientType type;

  if(! PyInt_Check(val))
    return -1;

  srvc = (struct mwServiceIm *) self->wrapped;
  type = PyInt_AsLong(val);

  mwServiceIm_setClientType(srvc, type);
  return 0;
}


static struct PyMethodDef tp_methods[] = {
  { ON_TEXT, MW_METH_VARARGS_NONE, METH_VARARGS,
    "override to receive text messages" },

  { ON_HTML, MW_METH_VARARGS_NONE, METH_VARARGS,
    "override to receive HTML formatted messages" },

  { ON_MIME, MW_METH_VARARGS_NONE, METH_VARARGS,
    "override to receive MIME encoded messages" },

  { ON_SUBJECT, MW_METH_VARARGS_NONE, METH_VARARGS,
    "override to handle conversation subjects" },

  { ON_TYPING, MW_METH_VARARGS_NONE, METH_VARARGS,
    "override to receive typing notification" },

  { ON_OPENED, MW_METH_VARARGS_NONE, METH_VARARGS,
    "override to handle conversation openings" },

  { ON_CLOSED, MW_METH_VARARGS_NONE, METH_VARARGS,
    "override to handle conversation closings" },

  { "sendText", (PyCFunction) py_send_text, METH_VARARGS,
    "send a text message" },

  { "sendHtml", (PyCFunction) py_send_html, METH_VARARGS,
    "send an HTML formatted message" },

  { "sendMime", (PyCFunction) py_send_mime, METH_VARARGS,
    "send a MIME encoded message" },

  { "sendSubject", (PyCFunction) py_send_subject, METH_VARARGS,
    "send the conversation subject" },

  { "sendTyping", (PyCFunction) py_send_typing, METH_VARARGS,
    "send typing notification" },

  { "openConversation", (PyCFunction) py_convo_open, METH_VARARGS,
    "open a conversation to target" },

  { "closeConversation", (PyCFunction) py_convo_close, METH_VARARGS,
    "close a conversation" },

  { "conversationState", (PyCFunction) py_convo_get_state, METH_VARARGS,
    "is conversation open?" },

  { "conversationSupports", (PyCFunction) py_convo_supports, METH_VARARGS,
    "does conversation support message type?" },

  { "supports", (PyCFunction) py_supports, METH_VARARGS,
    "does service support message type?" },

  { NULL }
};


static PyGetSetDef tp_getset[] = {
  { "clientType", (getter) py_get_client_type, (setter) py_set_client_type,
    "the client type identifier. Used to determine supported features"
    "in newly created conversations", NULL },

  { NULL }
};



static PyObject *tp_new(PyTypeObject *t, PyObject *args, PyObject *kwds) {
  mwPyService *self;
  mwPySession *sessobj;
  struct mwSession *session;
  struct mwImHandler *handler;
  struct mwServiceIm *srvc_im;
  
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
  handler = g_new0(struct mwImHandler, 1);
  handler->conversation_recv = mw_conversation_recv;
  handler->conversation_opened = mw_conversation_opened;
  handler->conversation_closed = mw_conversation_closed;
  handler->clear = mw_clear;

  /* create the im service */
  srvc_im = mwServiceIm_new(session, handler);

  /* store the self object on the service's client data slot */
  mwService_setClientData(MW_SERVICE(srvc_im), self, NULL);

  /* create a python wrapper service built around this instance */
  /* sets self->wrapper and self->service */
  mwServicePyWrap_wrap(self, MW_SERVICE(srvc_im));
  
  return (PyObject *) self;
}


static PyTypeObject pyType_mwServiceIm = {
  PyObject_HEAD_INIT(NULL)
  0, /* ob_size */
  "_meanwhile.ServiceIm",
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
  "Meanwhile client service for instant messaging",
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


PyTypeObject *mwPyServiceIm_type() {
  if(! py_type) {
    g_message("readying type mwPyServiceIm");

    py_type = &pyType_mwServiceIm;
    py_type->tp_base = mwPyService_type();

    Py_INCREF(py_type);
    PyType_Ready(py_type);
  }

  return py_type;
}

