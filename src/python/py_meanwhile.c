

#include <Python.h>
#include "py_meanwhile.h"


#ifndef PyMODINIT_FUNC
#define PyMODINIT_FUNC void
#endif


static struct PyMethodDef _methods[] = {
  {NULL}
};


PyMODINIT_FUNC init_meanwhile() {
  PyObject *m;

  m = Py_InitModule3("_meanwhile", _methods, "Meanwhile client module");

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
