

#include <Python.h>
#include "py_meanwhile.h"
#include "../mw_service.h"
#include "../mw_session.h"
#include "../mw_srvc_aware.h"
#include "../mw_srvc_conf.h"
#include "../mw_srvc_im.h"


#ifndef PyMODINIT_FUNC
#define PyMODINIT_FUNC void
#endif


static struct PyMethodDef py_methods[] = {
  { NULL }
};


PyMODINIT_FUNC init_meanwhile() {
  PyObject *m;

  m = Py_InitModule3("_meanwhile", py_methods, "Meanwhile client module");

  /* service state constants */
  PyModule_AddIntConstant(m, "SERVICE_STARTING", mwServiceState_STARTING);
  PyModule_AddIntConstant(m, "SERVICE_STARTED", mwServiceState_STARTED);
  PyModule_AddIntConstant(m, "SERVICE_STOPPING", mwServiceState_STOPPING);
  PyModule_AddIntConstant(m, "SERVICE_STOPPED", mwServiceState_STOPPED);
  PyModule_AddIntConstant(m, "SERVICE_ERROR", mwServiceState_ERROR);
  PyModule_AddIntConstant(m, "SERVICE_UNKNOWN", mwServiceState_UNKNOWN);

  /* session state constants */
  PyModule_AddIntConstant(m, "SESSION_STARTING", mwSession_STARTING);
  PyModule_AddIntConstant(m, "SESSION_HANDSHAKE", mwSession_HANDSHAKE);
  PyModule_AddIntConstant(m, "SESSION_HANDSHAKE_ACK", mwSession_HANDSHAKE_ACK);
  PyModule_AddIntConstant(m, "SESSION_LOGIN", mwSession_LOGIN);
  PyModule_AddIntConstant(m, "SESSION_LOGIN_REDIR", mwSession_LOGIN_REDIR);
  PyModule_AddIntConstant(m, "SESSION_LOGIN_ACK", mwSession_LOGIN_ACK);
  PyModule_AddIntConstant(m, "SESSION_STARTED", mwSession_STARTED);
  PyModule_AddIntConstant(m, "SESSION_STOPPING", mwSession_STOPPING);
  PyModule_AddIntConstant(m, "SESSION_STOPPED", mwSession_STOPPED);
  PyModule_AddIntConstant(m, "SESSION_UNKNOWN", mwSession_UNKNOWN);

  /* user state constants */
  PyModule_AddIntConstant(m, "STATUS_ACTIVE", mwStatus_ACTIVE);
  PyModule_AddIntConstant(m, "STATUS_IDLE", mwStatus_IDLE);
  PyModule_AddIntConstant(m, "STATUS_AWAY", mwStatus_AWAY);
  PyModule_AddIntConstant(m, "STATUS_BUSY", mwStatus_BUSY);

  /* awareness types */
  PyModule_AddIntConstant(m, "AWARE_SERVER", mwAware_SERVER);
  PyModule_AddIntConstant(m, "AWARE_USER", mwAware_USER);
  PyModule_AddIntConstant(m, "AWARE_GROUP", mwAware_GROUP);

  /* IM message types */
  PyModule_AddIntConstant(m, "IM_PLAIN", mwImSend_PLAIN);
  PyModule_AddIntConstant(m, "IM_TYPING", mwImSend_TYPING);
  PyModule_AddIntConstant(m, "IM_HTML", mwImSend_HTML);
  PyModule_AddIntConstant(m, "IM_SUBJECT", mwImSend_SUBJECT);
  PyModule_AddIntConstant(m, "IM_MIME", mwImSend_MIME);

  /* IM conversation states */
  PyModule_AddIntConstant(m, "CONVERSATION_CLOSED", mwConversation_CLOSED);
  PyModule_AddIntConstant(m, "CONVERSATION_PENDING", mwConversation_PENDING);
  PyModule_AddIntConstant(m, "CONVERSATION_OPEN", mwConversation_OPEN);
  PyModule_AddIntConstant(m, "CONVERSATION_UNKNOWN", mwConversation_UNKNOWN);

  /* conference states */
  PyModule_AddIntConstant(m, "CONFERENCE_NEW", mwConference_NEW);
  PyModule_AddIntConstant(m, "CONFERENCE_PENDING", mwConference_PENDING);
  PyModule_AddIntConstant(m, "CONFERENCE_INVITED", mwConference_INVITED);
  PyModule_AddIntConstant(m, "CONFERENCE_OPEN", mwConference_OPEN);
  PyModule_AddIntConstant(m, "CONFERENCE_CLOSING", mwConference_CLOSING);
  PyModule_AddIntConstant(m, "CONFERENCE_ERROR", mwConference_ERROR);
  PyModule_AddIntConstant(m, "CONFERENCE_UNKNOWN", mwConference_UNKNOWN);

  /* basic classes */
  PyModule_AddObject(m, "Channel", (PyObject *) mwPyChannel_type());
  PyModule_AddObject(m, "Service", (PyObject *) mwPyService_type());
  PyModule_AddObject(m, "Session", (PyObject *) mwPySession_type());

  /* service classes */
  PyModule_AddObject(m, "ServiceAware", (PyObject *) mwPyServiceAware_type());
  PyModule_AddObject(m, "ServiceConference",
		     (PyObject *) mwPyServiceConference_type());
  PyModule_AddObject(m, "ServiceIm", (PyObject *) mwPyServiceIm_type());
  PyModule_AddObject(m, "ServiceStorage",
		     (PyObject *) mwPyServiceStorage_type());
}


PyObject *PyString_SafeFromString(const char *text) {
  if(text) {
    return PyString_FromString(text);

  } else {
    mw_return_none();
  }
}


const char *PyString_SafeAsString(PyObject *str) {
  char *ret = NULL;

  if(str && (str != Py_None))
    ret = PyString_AsString(str);

  return ret;
}


PyObject *mw_noargs_none(PyObject *obj) {
  mw_return_none();
}


PyObject *mw_varargs_none(PyObject *obj, PyObject *args) {
  mw_return_none();
}

