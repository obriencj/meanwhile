
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
#include "py_meanwhile.h"
#include "../mw_service.h"
#include "../mw_session.h"
#include "../mw_srvc_aware.h"
#include "../mw_srvc_conf.h"
#include "../mw_srvc_im.h"
#include "../mw_srvc_resolve.h"


#ifndef PyMODINIT_FUNC
#define PyMODINIT_FUNC void
#endif


#define INT_CONSTANT(mod, name, val) \
  PyModule_AddIntConstant((mod), (name), (val))


static struct PyMethodDef py_methods[] = {
  { NULL }
};


PyMODINIT_FUNC init_meanwhile() {
  PyObject *m;

  m = Py_InitModule3("_meanwhile", py_methods, "Meanwhile client module");

  /* service state constants */
  INT_CONSTANT(m, "SERVICE_STARTING", mwServiceState_STARTING);
  INT_CONSTANT(m, "SERVICE_STARTED", mwServiceState_STARTED);
  INT_CONSTANT(m, "SERVICE_STOPPING", mwServiceState_STOPPING);
  INT_CONSTANT(m, "SERVICE_STOPPED", mwServiceState_STOPPED);
  INT_CONSTANT(m, "SERVICE_ERROR", mwServiceState_ERROR);
  INT_CONSTANT(m, "SERVICE_UNKNOWN", mwServiceState_UNKNOWN);

  /* session state constants */
  INT_CONSTANT(m, "SESSION_STARTING", mwSession_STARTING);
  INT_CONSTANT(m, "SESSION_HANDSHAKE", mwSession_HANDSHAKE);
  INT_CONSTANT(m, "SESSION_HANDSHAKE_ACK", mwSession_HANDSHAKE_ACK);
  INT_CONSTANT(m, "SESSION_LOGIN", mwSession_LOGIN);
  INT_CONSTANT(m, "SESSION_LOGIN_REDIR", mwSession_LOGIN_REDIR);
  INT_CONSTANT(m, "SESSION_LOGIN_ACK", mwSession_LOGIN_ACK);
  INT_CONSTANT(m, "SESSION_STARTED", mwSession_STARTED);
  INT_CONSTANT(m, "SESSION_STOPPING", mwSession_STOPPING);
  INT_CONSTANT(m, "SESSION_STOPPED", mwSession_STOPPED);
  INT_CONSTANT(m, "SESSION_UNKNOWN", mwSession_UNKNOWN);

  /* user state constants */
  INT_CONSTANT(m, "STATUS_ACTIVE", mwStatus_ACTIVE);
  INT_CONSTANT(m, "STATUS_IDLE", mwStatus_IDLE);
  INT_CONSTANT(m, "STATUS_AWAY", mwStatus_AWAY);
  INT_CONSTANT(m, "STATUS_BUSY", mwStatus_BUSY);

  /* awareness types */
  INT_CONSTANT(m, "AWARE_SERVER", mwAware_SERVER);
  INT_CONSTANT(m, "AWARE_USER", mwAware_USER);
  INT_CONSTANT(m, "AWARE_GROUP", mwAware_GROUP);

  /* IM client types */
  INT_CONSTANT(m, "IM_CLIENT_PLAIN", mwImClient_PLAIN);
  INT_CONSTANT(m, "IM_CLIENT_NOTESBUDDY", mwImClient_NOTESBUDDY);

  /* IM message types */
  INT_CONSTANT(m, "IM_PLAIN", mwImSend_PLAIN);
  INT_CONSTANT(m, "IM_TYPING", mwImSend_TYPING);
  INT_CONSTANT(m, "IM_HTML", mwImSend_HTML);
  INT_CONSTANT(m, "IM_SUBJECT", mwImSend_SUBJECT);
  INT_CONSTANT(m, "IM_MIME", mwImSend_MIME);

  /* IM conversation states */
  INT_CONSTANT(m, "CONVERSATION_CLOSED", mwConversation_CLOSED);
  INT_CONSTANT(m, "CONVERSATION_PENDING", mwConversation_PENDING);
  INT_CONSTANT(m, "CONVERSATION_OPEN", mwConversation_OPEN);
  INT_CONSTANT(m, "CONVERSATION_UNKNOWN", mwConversation_UNKNOWN);

  /* conference states */
  INT_CONSTANT(m, "CONFERENCE_NEW", mwConference_NEW);
  INT_CONSTANT(m, "CONFERENCE_PENDING", mwConference_PENDING);
  INT_CONSTANT(m, "CONFERENCE_INVITED", mwConference_INVITED);
  INT_CONSTANT(m, "CONFERENCE_OPEN", mwConference_OPEN);
  INT_CONSTANT(m, "CONFERENCE_CLOSING", mwConference_CLOSING);
  INT_CONSTANT(m, "CONFERENCE_ERROR", mwConference_ERROR);
  INT_CONSTANT(m, "CONFERENCE_UNKNOWN", mwConference_UNKNOWN);

  /* resolve options */
  INT_CONSTANT(m, "RESOLVE_UNIQUE", mwResolveFlag_UNIQUE);
  INT_CONSTANT(m, "RESOLVE_FIRST", mwResolveFlag_FIRST);
  INT_CONSTANT(m, "RESOLVE_ALL_DIRS", mwResolveFlag_ALL_DIRS);
  INT_CONSTANT(m, "RESOLVE_USERS", mwResolveFlag_USERS);

  /* resolve result codes */
  INT_CONSTANT(m, "RESOLVED_SUCCESS", mwResolveCode_SUCCESS);
  INT_CONSTANT(m, "RESOLVED_PARTIAL", mwResolveCode_PARTIAL);
  INT_CONSTANT(m, "RESOLVED_MULTIPLE", mwResolveCode_MULTIPLE);
  INT_CONSTANT(m, "RESOLVED_BAD_FORMAT", mwResolveCode_BAD_FORMAT);

  /* basic classes */
  PyModule_AddObject(m, "Channel", (PyObject *) mwPyChannel_type());
  PyModule_AddObject(m, "Service", (PyObject *) mwPyService_type());
  PyModule_AddObject(m, "Session", (PyObject *) mwPySession_type());

  /* service classes */
  PyModule_AddObject(m, "ServiceAware", (PyObject *) mwPyServiceAware_type());
  PyModule_AddObject(m, "ServiceConference",
		     (PyObject *) mwPyServiceConference_type());
  PyModule_AddObject(m, "ServiceIm", (PyObject *) mwPyServiceIm_type());
  PyModule_AddObject(m, "ServiceResolve",
		     (PyObject *) mwPyServiceResolve_type());
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

