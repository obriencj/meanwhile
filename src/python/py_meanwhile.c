

#include <Python.h>
#include "py_meanwhile.h"
#include "../service.h"
#include "../session.h"


#ifndef PyMODINIT_FUNC
#define PyMODINIT_FUNC void
#endif


static struct PyMethodDef _methods[] = {
  {NULL}
};


PyMODINIT_FUNC init_meanwhile() {
  PyObject *m;

  m = Py_InitModule3("_meanwhile", _methods, "Meanwhile client module");

  PyModule_AddIntConstant(m, "SERVICE_STARTING", mwServiceState_STARTING);
  PyModule_AddIntConstant(m, "SERVICE_STARTED", mwServiceState_STARTED);
  PyModule_AddIntConstant(m, "SERVICE_STOPPING", mwServiceState_STOPPING);
  PyModule_AddIntConstant(m, "SERVICE_STOPPED", mwServiceState_STOPPED);
  PyModule_AddIntConstant(m, "SERVICE_ERROR", mwServiceState_ERROR);
  PyModule_AddIntConstant(m, "SERVICE_UNKNOWN", mwServiceState_UNKNOWN);

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

  PyModule_AddObject(m, "Channel", (PyObject *) mwPyChannel_type());
  PyModule_AddObject(m, "Service", (PyObject *) mwPyService_type());
  PyModule_AddObject(m, "Session", (PyObject *) mwPySession_type());

  PyModule_AddObject(m, "ServiceIm", (PyObject *) mwPyServiceIm_type());
  PyModule_AddObject(m, "ServiceStorage",
		     (PyObject *) mwPyServiceStorage_type());
}


PyObject *PyString_SafeFromString(const char *text) {
  if(text) {
    return PyString_FromString(text);

  } else {
    Py_INCREF(Py_None);
    return Py_None;
  }
}


const char *PyString_SafeAsString(PyObject *str) {
  char *ret = NULL;

  if(str && (str != Py_None))
    ret = PyString_AsString(str);

  return ret;
}
