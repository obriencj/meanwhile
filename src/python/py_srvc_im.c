
#include <Python.h>
#include <structmember.h>

#include <glib.h>
#include <string.h>

#include "py_meanwhile.h"
#include "../common.h"
#include "../mw_debug.h"
#include "../service.h"
#include "../srvc_im.h"


#define ON_TEXT    "onText"
#define ON_HTML    "onHtml"
#define ON_TYPING  "onTyping"
#define ON_ERROR   "onError"


/*
  Incoming IM message:
  Session -> py_service -> Python -> py_srvc_im -> srvc_im -> handler -> Python

  Outgoing IM message:
  Python -> py_srvc_im -> srvc_im -> Session
*/


static void mw_got_text(struct mwServiceIm *srvc,
		      struct mwIdBlock *from, const char *text) {

  struct mwServiceImHandler *h;
  struct pyObj_mwService *self;
  PyObject *robj = NULL;
  PyObject *a, *b, *c, *t, *z;

  h = mwServiceIm_getHandler(srvc);
  self = h->data;

  g_message("mw_got_text (%s,%s)%s",
	    from->user, from->community, text);

  /* getting crazy here, just messing around to try and fix a segfault
     in PyObject_Malloc */
  a = PyString_SafeFromString(from->user);
  b = PyString_SafeFromString(from->community);
  c = PyString_SafeFromString(text);
  t = PyTuple_New(2);
  PyTuple_SetItem(t, 0, a);
  PyTuple_SetItem(t, 1, b);
  z = PyString_SafeFromString(ON_TEXT);

  robj = PyObject_CallMethodObjArgs((PyObject *) self, z, t, c, NULL);

  /*
  if(! robj) {
    g_message("CallMethod failed");
    if(PyErr_Occurred()) PyErr_Print();
  }
  */

  /* Py_DECREF(a);
     Py_DECREF(b); */
  Py_DECREF(c);
  Py_DECREF(t);
  Py_DECREF(z);
  Py_XDECREF(robj);
}


static void mw_got_html(struct mwServiceIm *srvc,
		      struct mwIdBlock *from, const char *html) {

  struct mwServiceImHandler *h;
  struct pyObj_mwService *self;
  PyObject *robj = NULL;
  PyObject *a, *b, *c;

  h = mwServiceIm_getHandler(srvc);
  self = h->data;

  g_message("mw_got_html (%s,%s)%s",
	    from->user, from->community, html);

  a = PyString_SafeFromString(from->user);
  b = PyString_SafeFromString(from->community);
  c = PyString_SafeFromString(html);

  robj = PyObject_CallMethod((PyObject *) self, ON_HTML,
			     "(OO)O", a, b, c);

  Py_DECREF(a);
  Py_DECREF(b);
  Py_DECREF(c);
  Py_XDECREF(robj);
}


static void mw_got_typing(struct mwServiceIm *srvc,
			struct mwIdBlock *from, gboolean typing) {

  struct mwServiceImHandler *h;
  struct pyObj_mwService *self;
  PyObject *robj = NULL;
  PyObject *a, *b, *c;

  h = mwServiceIm_getHandler(srvc);
  self = h->data;

  g_message("mw_got_typing (%s,%s)%i",
	    from->user, from->community, typing);

  a = PyString_SafeFromString(from->user);
  b = PyString_SafeFromString(from->community);
  c = PyInt_FromLong((int) typing);

  robj = PyObject_CallMethod((PyObject *) self, ON_TYPING,
			     "(OO)O", a, b, c);

  /*
  if(! robj) {
    g_message("CallMethod failed");
    if(PyErr_Occurred()) PyErr_Print();
  }
  */

  Py_DECREF(a);
  Py_DECREF(b);
  Py_DECREF(c);
  Py_XDECREF(robj);
}


static void mw_got_error(struct mwServiceIm *srvc,
			 struct mwIdBlock *user, guint32 error) {

  struct mwServiceImHandler *h;
  struct pyObj_mwService *self;
  PyObject *robj = NULL;
  PyObject *a, *b;

  h = mwServiceIm_getHandler(srvc);
  self = h->data;

  g_message("mw_got_error (%s,%s)0x%08x",
	    user->user, user->community, error);

  a = PyString_SafeFromString(user->user);
  b = PyString_SafeFromString(user->community);

  robj = PyObject_CallMethod((PyObject *) self, ON_ERROR,
			     "(OO)l", a, b, error);

  Py_DECREF(a);
  Py_DECREF(b);
  Py_XDECREF(robj);
}


static void mw_clear(struct mwServiceIm *srvc) {
  g_free(mwServiceIm_getHandler(srvc));
}


static PyObject *py_got_text(mwPyService *self, PyObject *args) {
  Py_INCREF(Py_None);
  return Py_None;
}


static PyObject *py_got_html(mwPyService *self, PyObject *args) {
  Py_INCREF(Py_None);
  return Py_None;
}


static PyObject *py_got_typing(mwPyService *self, PyObject *args) {
  Py_INCREF(Py_None);
  return Py_None;
}


static PyObject *py_got_error(mwPyService *self, PyObject *args) {
  Py_INCREF(Py_None);
  return Py_None;
}


static PyObject *py_send_text(mwPyService *self, PyObject *args) {
  struct mwIdBlock id = { 0, 0 };
  const char *text = NULL;
  struct mwServiceIm *srvc_im;
  int ret;

  PyObject *a, *b, *c;

  if(! PyArg_ParseTuple(args, "(OO)O", &a, &b, &c))
    return NULL;

  id.user = (char *) PyString_SafeAsString(a);
  id.community = (char *) PyString_SafeAsString(b);
  text = PyString_SafeAsString(c);

  srvc_im = (struct mwServiceIm *) self->wrapped;
  ret = mwServiceIm_sendText(srvc_im, &id, text);
  /* don't free text or clear id, those strings are borrowed */

  return PyInt_FromLong(ret);
}


static PyObject *py_send_html(mwPyService *self, PyObject *args) {
  struct mwIdBlock id = { 0, 0 };
  char *text = NULL;
  struct mwServiceIm *srvc_im;
  int ret;

  if(! PyArg_ParseTuple(args, "(ss)s", &id.user, &id.community, &text))
    return NULL;

  srvc_im = (struct mwServiceIm *) self->wrapped;
  ret = mwServiceIm_sendHtml(srvc_im, &id, text);
  /* don't free text or clear id, those strings are borrowed */

  return PyInt_FromLong(ret);
}


static struct PyMethodDef tp_methods[] = {
  { ON_TEXT, (PyCFunction) py_got_text,
    METH_VARARGS, "override to receive text messages" },

  { ON_HTML, (PyCFunction) py_got_html,
    METH_VARARGS, "override to receive HTML formatted messages" },

  { ON_TYPING, (PyCFunction) py_got_typing,
    METH_VARARGS, "override to receive typing notification" },

  { ON_ERROR, (PyCFunction) py_got_error,
    METH_VARARGS, "override to handle errors" },

  { "sendText", (PyCFunction) py_send_text,
    METH_VARARGS, "send a text message" },

  { "sendHtml", (PyCFunction) py_send_html,
    METH_VARARGS, "send an HTML formatted message" },

  {NULL}
};


static PyObject *tp_new(PyTypeObject *t, PyObject *args, PyObject *kwds) {
  mwPyService *self;
  mwPySession *sessobj;
  struct mwSession *session;
  struct mwServiceImHandler *handler;
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
  handler = g_new0(struct mwServiceImHandler, 1);
  handler->got_text = mw_got_text;
  handler->got_html = mw_got_html;
  handler->got_typing = mw_got_typing;
  handler->got_error = mw_got_error;
  handler->clear = mw_clear;
  handler->data = self;

  /* create the im service */
  srvc_im = mwServiceIm_new(session, handler);

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

