// Local includes
#include "ndicapi.h"
#include "ndicapi_math.h"

// Export includes
#include "ndicapiExport.h"

// Python includes
#include <Python.h>

// Conditional definitions for Python-version-based compilation
#if PY_MAJOR_VERSION >= 3
  #define MOD_ERROR_VAL NULL
  #define MOD_SUCCESS_VAL(val) val
  #define MOD_INIT(name) PyMODINIT_FUNC PyInit_##name(void)
  #define MOD_DEF(ob, name, doc, methods) \
          static struct PyModuleDef moduledef = { \
            PyModuleDef_HEAD_INIT, name, doc, -1, methods, }; \
          ob = PyModule_Create(&moduledef);
  #define PyInt_FromLong PyLong_FromLong
  #define PyInt_Check PyLong_Check
  #define PyInt_AsLong PyLong_AsLong
  #define PyString_FromString PyUnicode_FromString
  #define PyString_FromStringAndSize PyUnicode_FromStringAndSize
  #define PyString_Format PyUnicode_Format
  #define PyString_AsString PyUnicode_AsUTF8
  #define PyIntObject PyLongObject
  //#define PY_INT_OBJECT_OB_IVAL(ob) PyLong_AsLong((PyObject*)(ob))
  #define PY_INT_OBJECT_OB_IVAL(ob) ob->ob_digit[0]
  #define cmpfunc PyAsyncMethods*
#else
  #define MOD_ERROR_VAL
  #define MOD_SUCCESS_VAL(val)
  #define MOD_INIT(name) void init##name(void)
  #define MOD_DEF(ob, name, doc, methods) \
          ob = Py_InitModule3(name, methods, doc);
  #define PY_INT_OBJECT_OB_IVAL(ob) ob->ob_ival
#endif

//--------------------------------------------------------------
// PyNdicapi structure
typedef struct
{
  PyObject_HEAD
  ndicapi* pl_ndicapi;
} PyNdicapi;

static void PyNdicapi_PyDelete(PyObject* self)
{
  ndicapi* pol;

  pol = ((PyNdicapi*)self)->pl_ndicapi;
  ndiCloseSerial(pol);
  PyMem_DEL(self);
}

static char* PyNdicapi_PrintHelper(PyObject* self, char* space)
{
  ndicapi* pol;

  pol = ((PyNdicapi*)self)->pl_ndicapi;

  sprintf(space, "<polaris object %p, %s>", (void*)pol, ndiGetSerialDeviceName(pol));

  return space;
}

static int PyNdicapi_PyPrint(PyObject* self, FILE* fp, int dummy)
{
  char space[256];

  PyNdicapi_PrintHelper(self, space);
  fprintf(fp, "%s", space);
  return 0;
}

static PyObject* PyNdicapi_PyString(PyObject* self)
{
  char space[256];

  PyNdicapi_PrintHelper(self, space);
  return PyString_FromString(space);
}

static PyObject* PyNdicapi_PyRepr(PyObject* self)
{
  char space[256];

  PyNdicapi_PrintHelper(self, space);
  return PyString_FromString(space);
}

static PyObject* PyNdicapi_PyGetAttr(PyObject* self, char* name)
{
  ndicapi* pol;

  pol = ((PyNdicapi*)self)->pl_ndicapi;
  PyErr_SetString(PyExc_AttributeError, name);
  return NULL;
}

static PyTypeObject PyNdicapiType =
{
  PyVarObject_HEAD_INIT(NULL, 0) /* (&PyType_Type) */
  "ndicapi",                                                  /* tp_name */
  sizeof(PyNdicapi),                                          /* tp_basicsize */
  0,                                                          /* tp_itemsize */
  (destructor)PyNdicapi_PyDelete,                             /* tp_dealloc */
  (printfunc)PyNdicapi_PyPrint,                               /* tp_print */
  (getattrfunc)PyNdicapi_PyGetAttr,                           /* tp_getattr */
  0,                                                          /* tp_setattr */
  (cmpfunc)0,                                                 /* tp_compare */
  (reprfunc)PyNdicapi_PyRepr,                                 /* tp_repr */
  0,                                                          /* tp_as_number  */
  0,                                                          /* tp_as_sequence */
  0,                                                          /* tp_as_mapping */
  (hashfunc)0,                                                /* tp_hash */
  (ternaryfunc)0,                                             /* tp_call */
  (reprfunc)PyNdicapi_PyString,                               /* tp_string */
  (getattrofunc)0,                                            /* tp_getattro */
  (setattrofunc)0,                                            /* tp_setattro */
  0,                                                          /* tp_as_buffer */
  0,                                                          /* tp_flags */
  "ndicapi: interface to an NDICAPI serial tracking system"   /* tp_doc */
};

int PyNdicapi_Check(PyObject* obj)
{
  return (obj->ob_type == &PyNdicapiType);
}

/*=================================================================
  bitfield type: this code is a ripoff of the python integer type
  that prints itself as a hexadecimal value
*/

typedef struct
{
  PyObject_HEAD
  unsigned long ob_ival;
} PyNDIBitfieldObject;

PyObject* PyNDIBitfield_FromUnsignedLong(unsigned long ival);

static void
bitfield_dealloc(PyIntObject* v)
{
  PyMem_DEL(v);
}

static int
bitfield_print(PyIntObject* v, FILE* fp, int flags/* Not used but required by interface */)
{
  fprintf(fp, "0x%lX", (unsigned long)PY_INT_OBJECT_OB_IVAL(v));
  return 0;
}

static PyObject*
bitfield_repr(PyIntObject* v)
{
  char buf[20];
  sprintf(buf, "0x%lX", (unsigned long)PY_INT_OBJECT_OB_IVAL(v));
  return PyString_FromString(buf);
}

static int
bitfield_compare(PyIntObject* v, PyIntObject* w)
{
  register unsigned long i = PY_INT_OBJECT_OB_IVAL(v);
  register unsigned long j = PY_INT_OBJECT_OB_IVAL(w);
  return (i < j) ? -1 : (i > j) ? 1 : 0;
}

static int
bitfield_nonzero(PyIntObject* v)
{
  return PY_INT_OBJECT_OB_IVAL(v) != 0;
}

static PyObject*
bitfield_invert(PyIntObject* v)
{
  return PyNDIBitfield_FromUnsignedLong(~(PY_INT_OBJECT_OB_IVAL(v)));
}

static PyObject*
bitfield_lshift(PyIntObject* v, PyIntObject* w)
{
  register long a, b;
  a = PY_INT_OBJECT_OB_IVAL(v);
  b = PY_INT_OBJECT_OB_IVAL(w);
  if (b < 0)
  {
    PyErr_SetString(PyExc_ValueError, "negative shift count");
    return NULL;
  }
  if (a == 0 || b == 0)
  {
    Py_INCREF(v);
    return (PyObject*) v;
  }
  if (b >= 8 * (long)sizeof(long))
  {
    return PyNDIBitfield_FromUnsignedLong(0L);
  }
  a = (unsigned long)a << b;
  return PyNDIBitfield_FromUnsignedLong(a);
}

static PyObject*
bitfield_rshift(PyIntObject* v, PyIntObject* w)
{
  register long a, b;
  a = PY_INT_OBJECT_OB_IVAL(v);
  b = PY_INT_OBJECT_OB_IVAL(w);
  if (b < 0)
  {
    PyErr_SetString(PyExc_ValueError, "negative shift count");
    return NULL;
  }
  if (a == 0 || b == 0)
  {
    Py_INCREF(v);
    return (PyObject*) v;
  }
  if (b >= 8 * (long)sizeof(long))
  {
    if (a < 0)
    { a = -1; }
    else
    { a = 0; }
  }
  else
  {
    if (a < 0)
    { a = ~(~(unsigned long)a >> b); }
    else
    { a = (unsigned long)a >> b; }
  }
  return PyNDIBitfield_FromUnsignedLong(a);
}

static PyObject*
bitfield_and(PyIntObject* v, PyIntObject* w)
{
  register unsigned long a, b;
  a = PY_INT_OBJECT_OB_IVAL(v);
  b = PY_INT_OBJECT_OB_IVAL(w);
  return PyNDIBitfield_FromUnsignedLong(a & b);
}

static PyObject*
bitfield_xor(PyIntObject* v, PyIntObject* w)
{
  register unsigned long a, b;
  a = PY_INT_OBJECT_OB_IVAL(v);
  b = PY_INT_OBJECT_OB_IVAL(w);
  return PyNDIBitfield_FromUnsignedLong(a ^ b);
}

static PyObject*
bitfield_or(PyIntObject* v, PyIntObject* w)
{
  register unsigned long a, b;
  a = PY_INT_OBJECT_OB_IVAL(v);
  b = PY_INT_OBJECT_OB_IVAL(w);
  return PyNDIBitfield_FromUnsignedLong(a | b);
}

static int
bitfield_coerce(PyObject** pv, PyObject** pw)
{
  if (PyInt_Check(*pw))
  {
    *pw = PyNDIBitfield_FromUnsignedLong(PyInt_AsLong(*pw));
    Py_INCREF(*pv);
    return 0;
  }
  else if (PyLong_Check(*pw))
  {
    *pw = PyNDIBitfield_FromUnsignedLong(PyLong_AsLong(*pw));
    Py_INCREF(*pv);
    return 0;
  }
  return 1; /* Can't do it */
}

static PyObject*
bitfield_int(PyIntObject* v)
{
  return PyInt_FromLong(PY_INT_OBJECT_OB_IVAL(v));
}

static PyObject*
bitfield_long(PyIntObject* v)
{
  return PyLong_FromLong(PY_INT_OBJECT_OB_IVAL(v));
}

static PyObject*
bitfield_float(PyIntObject* v)
{
  return PyFloat_FromDouble((double)(PY_INT_OBJECT_OB_IVAL(v)));
}

static PyObject*
bitfield_oct(PyIntObject* v)
{
  char buf[100];
  long x = PY_INT_OBJECT_OB_IVAL(v);
  if (x == 0)
  { strcpy(buf, "0"); }
  else
  { sprintf(buf, "0%lo", x); }
  return PyString_FromString(buf);
}

static PyObject*
bitfield_hex(PyIntObject* v)
{
  char buf[100];
  long x = PY_INT_OBJECT_OB_IVAL(v);
  sprintf(buf, "0x%lx", x);
  return PyString_FromString(buf);
}

static PyNumberMethods bitfield_as_number =
{
  (binaryfunc)0, /*nb_add*/
  (binaryfunc)0, /*nb_subtract*/
  (binaryfunc)0, /*nb_multiply*/
#if PY_MAJOR_VERSION <= 2
  (binaryfunc)0, /*nb_divide*/
#endif
  (binaryfunc)0, /*nb_remainder*/
  (binaryfunc)0, /*nb_divmod*/
  (ternaryfunc)0, /*nb_power*/
  (unaryfunc)0, /*nb_negative*/
  (unaryfunc)0, /*nb_positive*/
  (unaryfunc)0, /*nb_absolute*/
  (inquiry)bitfield_nonzero, /*nb_nonzero*/
  (unaryfunc)bitfield_invert, /*nb_invert*/
  (binaryfunc)bitfield_lshift, /*nb_lshift*/
  (binaryfunc)bitfield_rshift, /*nb_rshift*/
  (binaryfunc)bitfield_and, /*nb_and*/
  (binaryfunc)bitfield_xor, /*nb_xor*/
  (binaryfunc)bitfield_or, /*nb_or*/
#if PY_MAJOR_VERSION <= 2
  (coercion)bitfield_coerce, /*nb_coerce*/
#endif
  (unaryfunc)bitfield_int, /*nb_int*/
#if PY_MAJOR_VERSION >= 3
  (void *)0, /*nb_reserved*/
#endif
#if PY_MAJOR_VERSION <= 2
  (unaryfunc)bitfield_long, /*nb_long*/
#endif
  (unaryfunc)bitfield_float, /*nb_float*/
#if PY_MAJOR_VERSION <= 2
  (unaryfunc)bitfield_oct, /*nb_oct*/
  (unaryfunc)bitfield_hex, /*nb_hex*/
#endif
};

PyTypeObject PyNDIBitfield_Type =
{
  PyVarObject_HEAD_INIT(NULL, 0)  /* (&PyType_Type) */
  "bitfield",
  sizeof(PyIntObject),
  0,
  (destructor)bitfield_dealloc, /*tp_dealloc*/
  (printfunc)bitfield_print, /*tp_print*/
  0,    /*tp_getattr*/
  0,    /*tp_setattr*/
  (cmpfunc)bitfield_compare, /*tp_compare*/
  (reprfunc)bitfield_repr, /*tp_repr*/
  &bitfield_as_number,  /*tp_as_number*/
  0,    /*tp_as_sequence*/
  0,    /*tp_as_mapping*/
  (hashfunc)0, /*tp_hash*/
};

PyObject* PyNDIBitfield_FromUnsignedLong(unsigned long ival)
{
  return PyLong_FromUnsignedLong(ival);

  /* The implementation below has been commented out,
   * as it leads to double memory deallocation. The
   * PyNDIBitfieldObject does not seem to be used
   * anywhere else in the entire codebase either. So
   * the assumption is that the above implementation
   * of this function should not break any features.
   *
  PyNDIBitfieldObject* v;
  v = PyObject_NEW(PyNDIBitfieldObject, &PyNDIBitfield_Type);

  v->ob_ival = ival;
  return (PyObject*) v;
   */
}

/*=================================================================
  helper functions
*/

static PyObject* _ndiErrorHelper(int errnum, PyObject* rval)
{
  char errtext[512];

  if (errnum)
  {
    Py_DECREF(rval);
    if ((errnum & 0xff) == errnum)
    {
      sprintf(errtext, "POLARIS %#4.2x: %s", errnum, ndiErrorString(errnum));
    }
    else
    {
      sprintf(errtext, "Error %#6.4x: %s", errnum, ndiErrorString(errnum));
    }
    PyErr_SetString(PyExc_IOError, errtext);
    return NULL;
  }

  return rval;
}

static int _ndiConverter(PyObject* obj, ndicapi** polptr)
{
  if (PyNdicapi_Check(obj))
  {
    *polptr = ((PyNdicapi*)obj)->pl_ndicapi;
  }
  else
  {
    PyErr_SetString(PyExc_ValueError, "expected an NDICAPI object.");
    return 0;
  }
  return 1;
}

static PyObject* _PyString_FromChar(char value)
{
  return PyString_FromStringAndSize(&value, 1);
}

/*=================================================================
  methods
*/

static PyObject* Py_ndiHexToUnsignedLong(PyObject* module, PyObject* args)
{
  char* cp;
  int n;
  unsigned long result;

  if (PyArg_ParseTuple(args, "si:plHexToUnsignedLong", &cp, &n))
  {
    result = ndiHexToUnsignedLong(cp, n);
    return PyNDIBitfield_FromUnsignedLong(result);
  }

  return NULL;
}

static PyObject* Py_ndiSignedToLong(PyObject* module, PyObject* args)
{
  char* cp;
  int n;
  long result;

  if (PyArg_ParseTuple(args, "si:plSignedToLong", &cp, &n))
  {
    result = ndiSignedToLong(cp, n);
    return PyInt_FromLong(result);
  }

  return NULL;
}

static PyObject* Py_ndiHexEncode(PyObject* module, PyObject* args)
{
  char* result;
  void* data;
  char* cp;
  int m, n;
  PyObject* obj;

  if (PyArg_ParseTuple(args, "s#i:plHexEncode", &data, &m, &n))
  {
    cp = (char*)malloc(2 * n);
    if (m < n)
    {
      PyErr_SetString(PyExc_ValueError, "data string is not long enough");
      free(cp);
      return NULL;
    }
    result = ndiHexEncode(cp, data, n);
    obj = PyString_FromStringAndSize(result, 2 * n);
    free(cp);
    return obj;
  }

  return NULL;
}

static PyObject* Py_ndiHexDecode(PyObject* module, PyObject* args)
{
  void* result;
  void* data;
  char* cp;
  int m, n;
  PyObject* obj;

  if (PyArg_ParseTuple(args, "s#i:plHexDecode", &cp, &m, &n))
  {
    data = malloc(n);
    if (m < 2 * n)
    {
      PyErr_SetString(PyExc_ValueError, "encoded string is not long enough");
      free(data);
      return NULL;
    }
    result = ndiHexEncode((char*)data, cp, n);
    obj = PyString_FromStringAndSize((char*)result, n);
    free(data);
    return obj;
  }

  return NULL;
}

static PyObject* Py_ndiGetError(PyObject* module, PyObject* args)
{
  ndicapi* pol;
  int result;

  if (PyArg_ParseTuple(args, "O&:plGetError", &_ndiConverter, &pol))
  {
    result = ndiGetError(pol);
    return PyNDIBitfield_FromUnsignedLong(result);
  }

  return NULL;
}

static PyObject* Py_ndiErrorString(PyObject* module, PyObject* args)
{
  int errnum;
  const char* result;

  if (PyArg_ParseTuple(args, "i:plErrorString", &errnum))
  {
    result = ndiErrorString(errnum);
    return PyString_FromString(result);
  }

  return NULL;
}

static PyObject* Py_ndiDeviceName(PyObject* module, PyObject* args)
{
  int n;
  const char* result;

  if (PyArg_ParseTuple(args, "i:plDeviceName", &n))
  {
    result = ndiSerialDeviceName(n);
    if (result)
    {
      return PyString_FromString(result);
    }
    else
    {
      Py_INCREF(Py_None);
      return Py_None;
    }
  }

  return NULL;
}

static PyObject* Py_ndiProbe(PyObject* module, PyObject* args)
{
  char* device;
  int result;

  if (PyArg_ParseTuple(args, "s:plProbe", &device))
  {
    result = ndiSerialProbe(device, false);
    return PyNDIBitfield_FromUnsignedLong(result);
  }

  return NULL;
}

static PyObject* Py_ndiOpen(PyObject* module, PyObject* args)
{
  ndicapi* pol;
  char* device;
  PyNdicapi* self;

  if (PyArg_ParseTuple(args, "s:plOpen", &device))
  {
    pol = ndiOpenSerial(device);
    if (pol == NULL)
    {
      Py_INCREF(Py_None);
      return Py_None;
    }
    self = PyObject_NEW(PyNdicapi, &PyNdicapiType);
    self->pl_ndicapi = pol;
    Py_INCREF(self);
    return (PyObject*)self;
  }

  return NULL;
}

/* Open a networked tracker*/
static PyObject* Py_ndiOpenNetwork(PyObject* module, PyObject* args)
{
  ndicapi* pol;
  char* hostname;
  int port;
  PyNdicapi* self;

  if (PyArg_ParseTuple(args, "si:plOpenNetwork", &hostname, &port))
  {
    pol = ndiOpenNetwork(hostname, port);
    if (pol == NULL)
    {
      Py_INCREF(Py_None);
      return Py_None;
    }
    self = PyObject_NEW(PyNdicapi, &PyNdicapiType);
    self->pl_ndicapi = pol;
    Py_INCREF(self);
    return (PyObject*)self;
  }

  return NULL;
}

static PyObject* Py_ndiGetDeviceName(PyObject* module, PyObject* args)
{
  ndicapi* pol;
  char* result;

  if (PyArg_ParseTuple(args, "O&:plGetDeviceName", &_ndiConverter, &pol))
  {
    result = ndiGetSerialDeviceName(pol);
    if (result == NULL)
    {
      Py_INCREF(Py_None);
      return Py_None;
    }
    return PyString_FromString(result);
  }

  return NULL;
}

static PyObject* Py_ndiClose(PyObject* module, PyObject* args)
{
  ndicapi* pol;

  if (PyArg_ParseTuple(args, "O&:plClose", &_ndiConverter, &pol))
  {
    ndiSerialClose(pol->SerialDevice);
    Py_INCREF(Py_None);
    return Py_None;
  }

  return NULL;
}

/* close a networked tracker */
static PyObject* Py_ndiCloseNetwork(PyObject* module, PyObject* args)
{
  ndicapi* pol;

  if (PyArg_ParseTuple(args, "O&:plClose", &_ndiConverter, &pol))
  {
    ndiCloseNetwork(pol);
    Py_INCREF(Py_None);
    return Py_None;
  }

  return NULL;
}

static PyObject* Py_ndiSetThreadMode(PyObject* module, PyObject* args)
{
  ndicapi* pol;
  int mode;

  if (PyArg_ParseTuple(args, "O&i:plSetThreadMode", &_ndiConverter, &pol,
                       &mode))
  {
    ndiSetThreadMode(pol, mode);
    Py_INCREF(Py_None);
    return Py_None;
  }

  return NULL;
}

static PyObject* Py_ndiCommand(PyObject* module, PyObject* args)
{
  int n;
  ndicapi* pol;
  char* format;
  char* result;
  PyObject* initial;
  PyObject* remainder;
  PyObject* newstring = NULL;
  PyObject* obj;

  if ((n = PySequence_Length(args)) < 2)
  {
    PyErr_SetString(PyExc_TypeError,
                    "plCommand requires at least 2 arguments");
    return NULL;
  }

  remainder = PySequence_GetSlice(args, 2, n);
  initial = PySequence_GetSlice(args, 0, 2);

  if (!PyArg_ParseTuple(initial, "O&z:plCommand",
                        &_ndiConverter, &pol, &format))
  {
    Py_DECREF(initial);
    Py_DECREF(remainder);
    return NULL;
  }

  if (format != NULL)
  {
    obj = PySequence_GetItem(args, 1);
    newstring = PyString_Format(obj, remainder);
    Py_DECREF(obj);
    Py_DECREF(initial);
    Py_DECREF(remainder);

    if (newstring == NULL)
    {
      return NULL;
    }

    result = ndiCommand(pol, "%s", PyString_AsString(newstring));
  }
  else
  {
    result = ndiCommand(pol, NULL);
  }

  if (newstring != NULL)
  {
    Py_DECREF(newstring);
  }

  if (result == NULL)
  {
    Py_INCREF(Py_None);
    obj = Py_None;
  }
  else
  {
    obj = PyString_FromString(result);
  }

  return _ndiErrorHelper(ndiGetError(pol), obj);
}

static PyObject* Py_ndiCommand2(PyObject* module, const char* format, PyObject* args)
{
  int i, n;
  PyObject* newargs;
  PyObject* obj;

  if ((n = PySequence_Length(args)) < 1)
  {
    PyErr_SetString(PyExc_TypeError,
                    "plCommand requires at least 2 arguments");
    return NULL;
  }

  newargs = PyTuple_New(n + 1);
  obj = PySequence_GetItem(args, 0);
  Py_INCREF(obj);
  PyTuple_SET_ITEM(newargs, 0, obj);

  if (format != NULL)
  {
    obj = PyString_FromString(format);
  }
  else
  {
    Py_INCREF(Py_None);
    obj = Py_None;
  }
  PyTuple_SET_ITEM(newargs, 1, obj);

  for (i = 1; i < n; i++)
  {
    obj = PySequence_GetItem(args, i);
    Py_INCREF(obj);
    PyTuple_SET_ITEM(newargs, i + 1, obj);
  }

  obj = Py_ndiCommand(module, newargs);

  Py_DECREF(newargs);

  return obj;
}

#define PyCommandMacro(name,format) \
  static PyObject *Py_##name(PyObject *module, PyObject *args) \
  { \
    return Py_ndiCommand2(module, format, args); \
  }

PyCommandMacro(ndiRESET, NULL)
PyCommandMacro(ndiINIT, "INIT:")
PyCommandMacro(ndiCOMM, "COMM:%d%03d%d")
PyCommandMacro(ndiPVWR, "PVWR:%c%04X%.128s")
PyCommandMacro(ndiPVCLR, "PVCLR:%c")
PyCommandMacro(ndiPINIT, "PINIT:%c")
PyCommandMacro(ndiPENA, "PENA:%c%c")
PyCommandMacro(ndiPDIS, "PDIS:%c")
PyCommandMacro(ndiTSTART, "TSTART:")
PyCommandMacro(ndiTSTOP, "TSTOP:")
PyCommandMacro(ndiGX, "GX:%04X")
PyCommandMacro(ndiBX, "BX:%04X")
PyCommandMacro(ndiTX, "TX:%04X")
PyCommandMacro(ndiLED, "LED:%c%d%c")
PyCommandMacro(ndiBEEP, "BEEP:%i")
PyCommandMacro(ndiVER, "VER:%d")
PyCommandMacro(ndiVSEL, "VSEL:%d")
PyCommandMacro(ndiSFLIST, "SFLIST:%02X")
PyCommandMacro(ndiPSTAT, "PSTAT:%04X")
PyCommandMacro(ndiSSTAT, "SSTAT:%04X")

PyCommandMacro(ndiPPRD, "PPRD:%c%04X")
PyCommandMacro(ndiPPWR, "PPWR:%c%04X%.128s")
PyCommandMacro(ndiPURD, "PURD:%c%04X")
PyCommandMacro(ndiPUWR, "PPWR:%c%04X%.128s")
PyCommandMacro(ndiPSEL, "PSEL:%c%s")
PyCommandMacro(ndiPSRCH, "PSRCH:%c")
PyCommandMacro(ndiPVTIP, "PVTIP:%c%d%d")
PyCommandMacro(ndiTCTST, "TCTST:%c")
PyCommandMacro(ndiTTCFG, "TTCFG:%c")

PyCommandMacro(ndiDSTART, "DSTART:")
PyCommandMacro(ndiDSTOP, "DSTOP:")
PyCommandMacro(ndiIRINIT, "IRINIT:")
PyCommandMacro(ndiIRCHK, "IRCHK:%04X")
PyCommandMacro(ndiIRED, "IRED:%c%08X")
PyCommandMacro(ndi3D, "3D:%c%d")


static PyObject* Py_ndiPVWRFromFile(PyObject* module, PyObject* args)
{
  int port;
  int result;
  char* filename;
  ndicapi* pol;

  if (PyArg_ParseTuple(args, "O&is:plPVWRFromFile",
                       &_ndiConverter, &pol, &port, &filename))
  {
    result = ndiPVWRFromFile(pol, port, filename);
    return PyNDIBitfield_FromUnsignedLong(result);
  }

  return NULL;
}

static PyObject* Py_ndiGetGXTransform(PyObject* module, PyObject* args)
{
  char port;
  int result;
  double transform[8];
  ndicapi* pol;

  if (PyArg_ParseTuple(args, "O&c:plGetGXTransform",
                       &_ndiConverter, &pol, &port))
  {
    result = ndiGetGXTransform(pol, port, transform);

    if (result == NDI_MISSING)
    {
      return PyString_FromString("MISSING");
    }
    else if (result == NDI_DISABLED)
    {
      return PyString_FromString("DISABLED");
    }

    return Py_BuildValue("(dddddddd)", transform[0], transform[1],
                         transform[2], transform[3], transform[4],
                         transform[5], transform[6], transform[7]);
  }

  return NULL;
}

static PyObject* Py_ndiGetBXTransform(PyObject* module, PyObject* args)
{
  char port;
  int result;
  float transform[8];
  ndicapi* pol;

  if (PyArg_ParseTuple(args, "O&c:plGetBXTransform",
                       &_ndiConverter, &pol, &port))
  {
    result = ndiGetBXTransform(pol, port, transform);

    if (result == NDI_MISSING)
    {
      return PyString_FromString("MISSING");
    }
    else if (result == NDI_DISABLED)
    {
      return PyString_FromString("DISABLED");
    }

    return Py_BuildValue("(dddddddd)", transform[0], transform[1],
                         transform[2], transform[3], transform[4],
                         transform[5], transform[6], transform[7]);
  }

  return NULL;
}

static PyObject* Py_ndiGetTXTransform(PyObject* module, PyObject* args)
{
  char port;
  int result;
  double transform[8];
  ndicapi* pol;

  if (PyArg_ParseTuple(args, "O&c:plGetTXTransform",
                       &_ndiConverter, &pol, &port))
  {
    result = ndiGetTXTransform(pol, port, transform);

    if (result == NDI_MISSING)
    {
      return PyString_FromString("MISSING");
    }
    else if (result == NDI_DISABLED)
    {
      return PyString_FromString("DISABLED");
    }

    return Py_BuildValue("(dddddddd)", transform[0], transform[1],
                         transform[2], transform[3], transform[4],
                         transform[5], transform[6], transform[7]);
  }

  return NULL;
}

static PyObject* Py_ndiGetGXPortStatus(PyObject* module, PyObject* args)
{
  char port;
  int result;
  ndicapi* pol;

  if (PyArg_ParseTuple(args, "O&c:plGetGXPortStatus",
                       &_ndiConverter, &pol, &port))
  {
    result = ndiGetGXPortStatus(pol, port);
    return PyNDIBitfield_FromUnsignedLong(result);
  }

  return NULL;
}


static PyObject* Py_ndiGetBXPortStatus(PyObject* module, PyObject* args)
{
  char port;
  int result;
  ndicapi* pol;

  if (PyArg_ParseTuple(args, "O&c:plGetBXPortStatus",
                       &_ndiConverter, &pol, &port))
  {
    result = ndiGetBXPortStatus(pol, port);
    return PyNDIBitfield_FromUnsignedLong(result);
  }

  return NULL;
}

static PyObject* Py_ndiGetTXPortStatus(PyObject* module, PyObject* args)
{
  char port;
  int result;
  ndicapi* pol;

  if (PyArg_ParseTuple(args, "O&c:plGetTXPortStatus",
                       &_ndiConverter, &pol, &port))
  {
    result = ndiGetTXPortStatus(pol, port);
    return PyNDIBitfield_FromUnsignedLong(result);
  }

  return NULL;
}


static PyObject* Py_ndiGetGXSystemStatus(PyObject* module, PyObject* args)
{
  int result;
  ndicapi* pol;

  if (PyArg_ParseTuple(args, "O&:plGetGXSystemStatus",
                       &_ndiConverter, &pol))
  {
    result = ndiGetGXSystemStatus(pol);
    return PyNDIBitfield_FromUnsignedLong(result);
  }

  return NULL;
}


static PyObject* Py_ndiGetBXSystemStatus(PyObject* module, PyObject* args)
{
  int result;
  ndicapi* pol;

  if (PyArg_ParseTuple(args, "O&:plGetBXSystemStatus",
                       &_ndiConverter, &pol))
  {
    result = ndiGetBXSystemStatus(pol);
    return PyNDIBitfield_FromUnsignedLong(result);
  }

  return NULL;
}

static PyObject* Py_ndiGetTXSystemStatus(PyObject* module, PyObject* args)
{
  int result;
  ndicapi* pol;

  if (PyArg_ParseTuple(args, "O&:plGetTXSystemStatus",
                       &_ndiConverter, &pol))
  {
    result = ndiGetTXSystemStatus(pol);
    return PyNDIBitfield_FromUnsignedLong(result);
  }

  return NULL;
}



static PyObject* Py_ndiGetGXToolInfo(PyObject* module, PyObject* args)
{
  char port;
  int result;
  ndicapi* pol;

  if (PyArg_ParseTuple(args, "O&c:plGetGXToolInfo",
                       &_ndiConverter, &pol, &port))
  {
    result = ndiGetGXToolInfo(pol, port);
    return PyNDIBitfield_FromUnsignedLong(result);
  }

  return NULL;
}

static PyObject* Py_ndiGetGXMarkerInfo(PyObject* module, PyObject* args)
{
  char port;
  char marker;
  int result;
  ndicapi* pol;

  if (PyArg_ParseTuple(args, "O&ci:plGetGXMarkerInfo",
                       &_ndiConverter, &pol, &port, &marker))
  {
    result = ndiGetGXMarkerInfo(pol, port, marker);
    return PyNDIBitfield_FromUnsignedLong(result);
  }

  return NULL;
}

static PyObject* Py_ndiGetGXSingleStray(PyObject* module, PyObject* args)
{
  char port;
  int result;
  double coord[3];
  ndicapi* pol;

  if (PyArg_ParseTuple(args, "O&c:plGetGXSingleStray",
                       &_ndiConverter, &pol, &port))
  {
    result = ndiGetGXSingleStray(pol, port, coord);

    if (result == NDI_MISSING)
    {
      return PyString_FromString("MISSING");
    }
    else if (result == NDI_DISABLED)
    {
      return PyString_FromString("DISABLED");
    }

    return Py_BuildValue("(ddd)", coord[0], coord[1], coord[2]);
  }

  return NULL;
}

static PyObject* Py_ndiGetGXFrame(PyObject* module, PyObject* args)
{
  char port;
  unsigned long result;
  ndicapi* pol;

  if (PyArg_ParseTuple(args, "O&c:plGetGXFrame",
                       &_ndiConverter, &pol, &port))
  {
    result = ndiGetGXFrame(pol, port);
    return PyLong_FromUnsignedLong(result);
  }

  return NULL;
}

static PyObject* Py_ndiGetBXFrame(PyObject* module, PyObject* args)
{
  char port;
  unsigned long result;
  ndicapi* pol;

  if (PyArg_ParseTuple(args, "O&c:plGetBXFrame",
                       &_ndiConverter, &pol, &port))
  {
    result = ndiGetBXFrame(pol, port);
    return PyLong_FromUnsignedLong(result);
  }

  return NULL;
}

static PyObject* Py_ndiGetTXFrame(PyObject* module, PyObject* args)
{
  char port;
  unsigned long result;
  ndicapi* pol;

  if (PyArg_ParseTuple(args, "O&c:plGetTXFrame",
                       &_ndiConverter, &pol, &port))
  {
    result = ndiGetTXFrame(pol, port);
    return PyLong_FromUnsignedLong(result);
  }

  return NULL;
}


static PyObject* Py_ndiGetGXNumberOfPassiveStrays(PyObject* module,
    PyObject* args)
{
  int result;
  ndicapi* pol;

  if (PyArg_ParseTuple(args, "O&:plGetGXNumberOfPassiveStrays",
                       &_ndiConverter, &pol))
  {
    result = ndiGetGXNumberOfPassiveStrays(pol);
    return PyInt_FromLong(result);
  }

  return NULL;
}

static PyObject* Py_ndiGetGXPassiveStray(PyObject* module, PyObject* args)
{
  int result;
  int i;
  double coord[3];
  ndicapi* pol;

  if (PyArg_ParseTuple(args, "O&i:plGetGXPassiveStray",
                       &_ndiConverter, &pol, &i))
  {
    result = ndiGetGXPassiveStray(pol, i, coord);

    if (result == NDI_MISSING)
    {
      return PyString_FromString("MISSING");
    }
    else if (result == NDI_DISABLED)
    {
      return PyString_FromString("DISABLED");
    }

    return Py_BuildValue("(ddd)", coord[0], coord[1], coord[2]);
  }

  return NULL;
}

static PyObject* Py_ndiGetPSTATPortStatus(PyObject* module, PyObject* args)
{
  char port;
  int result;
  ndicapi* pol;

  if (PyArg_ParseTuple(args, "O&c:plGetPSTATPortStatus",
                       &_ndiConverter, &pol, &port))
  {
    result = ndiGetPSTATPortStatus(pol, port);
    return PyNDIBitfield_FromUnsignedLong(result);
  }

  return NULL;
}

static PyObject* Py_ndiGetPSTATToolInfo(PyObject* module, PyObject* args)
{
  char port;
  char result[30];
  ndicapi* pol;

  if (PyArg_ParseTuple(args, "O&c:plGetPSTATToolInfo",
                       &_ndiConverter, &pol, &port))
  {
    if (ndiGetPSTATToolInfo(pol, port, result) != NDI_UNOCCUPIED)
    {
      return PyString_FromStringAndSize(result, 30);
    }
    return PyString_FromString("UNOCCUPIED");
  }

  return NULL;
}

static PyObject* Py_ndiGetPSTATCurrentTest(PyObject* module, PyObject* args)
{
  char port;
  unsigned long result;
  ndicapi* pol;

  if (PyArg_ParseTuple(args, "O&c:plGetPSTATCurrentTest",
                       &_ndiConverter, &pol, &port))
  {
    result = ndiGetPSTATCurrentTest(pol, port);
    return PyLong_FromUnsignedLong(result);
  }

  return NULL;
}

static PyObject* Py_ndiGetPSTATPartNumber(PyObject* module, PyObject* args)
{
  char port;
  char result[20];
  ndicapi* pol;

  if (PyArg_ParseTuple(args, "O&c:plGetPSTATPartNumber",
                       &_ndiConverter, &pol, &port))
  {
    if (ndiGetPSTATPartNumber(pol, port, result) != NDI_OKAY)
    {
      Py_INCREF(Py_None);
      return Py_None;
    }
    return PyString_FromStringAndSize(result, 20);
  }

  return NULL;
}

static PyObject* Py_ndiGetPSTATAccessories(PyObject* module, PyObject* args)
{
  char port;
  int result;
  ndicapi* pol;

  if (PyArg_ParseTuple(args, "O&c:plGetPSTATAccessories",
                       &_ndiConverter, &pol, &port))
  {
    result = ndiGetPSTATAccessories(pol, port);
    return PyNDIBitfield_FromUnsignedLong(result);
  }

  return NULL;
}

static PyObject* Py_ndiGetPSTATMarkerType(PyObject* module, PyObject* args)
{
  char port;
  int result;
  ndicapi* pol;

  if (PyArg_ParseTuple(args, "O&c:plGetPSTATMarkerType",
                       &_ndiConverter, &pol, &port))
  {
    result = ndiGetPSTATMarkerType(pol, port);
    return PyNDIBitfield_FromUnsignedLong(result);
  }

  return NULL;
}

static PyObject* Py_ndiGetSSTATControl(PyObject* module, PyObject* args)
{
  int result;
  ndicapi* pol;

  if (PyArg_ParseTuple(args, "O&:plGetSSTATControl",
                       &_ndiConverter, &pol))
  {
    result = ndiGetSSTATControl(pol);
    return PyNDIBitfield_FromUnsignedLong(result);
  }

  return NULL;
}

static PyObject* Py_ndiGetSSTATSensors(PyObject* module, PyObject* args)
{
  int result;
  ndicapi* pol;

  if (PyArg_ParseTuple(args, "O&:plGetSSTATSensors",
                       &_ndiConverter, &pol))
  {
    result = ndiGetSSTATSensors(pol);
    return PyNDIBitfield_FromUnsignedLong(result);
  }

  return NULL;
}

static PyObject* Py_ndiGetSSTATTIU(PyObject* module, PyObject* args)
{
  int result;
  ndicapi* pol;

  if (PyArg_ParseTuple(args, "O&:plGetSSTATTIU",
                       &_ndiConverter, &pol))
  {
    result = ndiGetSSTATTIU(pol);
    return PyNDIBitfield_FromUnsignedLong(result);
  }

  return NULL;
}

static PyObject* Py_ndiGetIRCHKDetected(PyObject* module, PyObject* args)
{
  int result;
  ndicapi* pol;

  if (PyArg_ParseTuple(args, "O&:plGetIRCHKDetected",
                       &_ndiConverter, &pol))
  {
    result = ndiGetIRCHKDetected(pol);
    return PyNDIBitfield_FromUnsignedLong(result);
  }

  return NULL;
}

static PyObject* Py_ndiGetIRCHKNumberOfSources(PyObject* module, PyObject* args)
{
  int result;
  int side;
  ndicapi* pol;

  if (PyArg_ParseTuple(args, "O&i:plGetIRCHKNumberOfSources",
                       &_ndiConverter, &pol, &side))
  {
    result = ndiGetIRCHKNumberOfSources(pol, side);
    return PyInt_FromLong(result);
  }

  return NULL;
}

static PyObject* Py_ndiGetIRCHKSourceXY(PyObject* module, PyObject* args)
{
  int result;
  int side;
  int i;
  double xy[2];
  ndicapi* pol;

  if (PyArg_ParseTuple(args, "O&ii:plGetIRCHKSourceXY",
                       &_ndiConverter, &pol, &side, &i))
  {
    result = ndiGetIRCHKSourceXY(pol, side, i, xy);
    if (result != NDI_OKAY)
    {
      Py_INCREF(Py_None);
      return Py_None;
    }
    return Py_BuildValue("(ff)", xy[0], xy[1]);
  }

  return NULL;
}

static PyObject* Py_ndiGetPHRQHandle (PyObject* module, PyObject* args)
{
  int result;
  ndicapi* pol;

  if (PyArg_ParseTuple(args, "O&:plGetPHRQHandle",
                       &_ndiConverter, &pol))
  {
    result =  ndiGetPHRQHandle(pol);
    return PyInt_FromLong(result);
  }
  return NULL;
}

static PyObject* Py_ndiGetPHSRNumberOfHandles (PyObject* module, PyObject* args)
{
  int result;
  ndicapi* pol;

  if (PyArg_ParseTuple(args, "O&:plGetPHSRNumberOfHandles",
                       &_ndiConverter, &pol))
  {
    result =  ndiGetPHSRNumberOfHandles(pol);
    return PyInt_FromLong(result);
  }
  return NULL;
}

static PyObject* Py_ndiGetPHSRHandle (PyObject* module, PyObject* args)
{
  int result;
  ndicapi* pol;
  int i;

  if (PyArg_ParseTuple(args, "O&i:plGetPHSRHandle",
                       &_ndiConverter, &pol, &i))
  {
    result =  ndiGetPHSRHandle(pol,i);
    return PyInt_FromLong(result);
  }
  return NULL;
}

static PyObject* Py_ndiRelativeTransform(PyObject* module, PyObject* args)
{
  double a[8];
  double b[8];

  if (PyArg_ParseTuple(args, "(dddddddd)(dddddddd):ndiRelativeTransform",
                       &a[0], &a[1], &a[2], &a[3],
                       &a[4], &a[5], &a[6], &a[7],
                       &b[0], &b[1], &b[2], &b[3],
                       &b[4], &b[5], &b[6], &b[7]))
  {
    ndiRelativeTransform(a, b, a);

    return Py_BuildValue("(dddddddd)", a[0], a[1], a[2], a[3],
                         a[4], a[5], a[6], a[7]);
  }

  return NULL;
}

static PyObject* Py_ndiTransformToMatrixd(PyObject* module, PyObject* args)
{
  double a[8];
  double c[16];

  if (PyArg_ParseTuple(args, "(dddddddd):ndiTransformToMatrixd",
                       &a[0], &a[1], &a[2], &a[3],
                       &a[4], &a[5], &a[6], &a[7]))
  {
    ndiTransformToMatrixd(a, c);

    return Py_BuildValue("(dddddddddddddddd)",
                         c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7],
                         c[8], c[9], c[10], c[11], c[12], c[13], c[14], c[15]);
  }

  return NULL;
}

static PyObject* Py_ndiTransformToMatrixf(PyObject* module, PyObject* args)
{
  float a[8];
  float c[16];

  if (PyArg_ParseTuple(args, "(ffffffff):ndiTransformToMatrixf",
                       &a[0], &a[1], &a[2], &a[3],
                       &a[4], &a[5], &a[6], &a[7]))
  {
    ndiTransformToMatrixf(a, c);

    return Py_BuildValue("(ffffffffffffffff)",
                         c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7],
                         c[8], c[9], c[10], c[11], c[12], c[13], c[14], c[15]);
  }

  return NULL;
}

static PyObject* Py_ndiAnglesFromMatrixd(PyObject* module, PyObject* args)
{
  double a[16];
  double c[3];

  if (PyArg_ParseTuple(args, "(dddddddddddddddd):ndiAnglesFromMatrixd",
                       &a[0], &a[1], &a[2], &a[3],
                       &a[4], &a[5], &a[6], &a[7],
                       &a[8], &a[9], &a[10], &a[11],
                       &a[12], &a[13], &a[14], &a[15]))
  {
    ndiAnglesFromMatrixd(c, a);

    return Py_BuildValue("(ddd)", c[0], c[1], c[2]);
  }

  return NULL;
}

static PyObject* Py_ndiAnglesFromMatrixf(PyObject* module, PyObject* args)
{
  float a[16];
  float c[3];

  if (PyArg_ParseTuple(args, "(ffffffffffffffff):ndiAnglesFromMatrixf",
                       &a[0], &a[1], &a[2], &a[3],
                       &a[4], &a[5], &a[6], &a[7],
                       &a[8], &a[9], &a[10], &a[11],
                       &a[12], &a[13], &a[14], &a[15]))
  {
    ndiAnglesFromMatrixf(c, a);

    return Py_BuildValue("(fff)", c[0], c[1], c[2]);
  }

  return NULL;
}

static PyObject* Py_ndiCoordsFromMatrixd(PyObject* module, PyObject* args)
{
  double a[16];
  double c[3];

  if (PyArg_ParseTuple(args, "(dddddddddddddddd):ndiCoordsFromMatrixd",
                       &a[0], &a[1], &a[2], &a[3],
                       &a[4], &a[5], &a[6], &a[7],
                       &a[8], &a[9], &a[10], &a[11],
                       &a[12], &a[13], &a[14], &a[15]))
  {
    ndiCoordsFromMatrixd(c, a);

    return Py_BuildValue("(ddd)", c[0], c[1], c[2]);
  }

  return NULL;
}

static PyObject* Py_ndiCoordsFromMatrixf(PyObject* module, PyObject* args)
{
  float a[16];
  float c[3];

  if (PyArg_ParseTuple(args, "(ffffffffffffffff):ndiCoordsFromMatrixf",
                       &a[0], &a[1], &a[2], &a[3],
                       &a[4], &a[5], &a[6], &a[7],
                       &a[8], &a[9], &a[10], &a[11],
                       &a[12], &a[13], &a[14], &a[15]))
  {
    ndiCoordsFromMatrixf(c, a);

    return Py_BuildValue("(fff)", c[0], c[1], c[2]);
  }

  return NULL;
}

/*=================================================================
  Module definition
*/
#define Py_NDIMethodMacro(name) {#name,  Py_##name,  METH_VARARGS}

static PyMethodDef NdicapiMethods[] =
{
  Py_NDIMethodMacro(ndiHexToUnsignedLong),
  Py_NDIMethodMacro(ndiSignedToLong),
  Py_NDIMethodMacro(ndiHexEncode),
  Py_NDIMethodMacro(ndiHexDecode),

  Py_NDIMethodMacro(ndiDeviceName),
  Py_NDIMethodMacro(ndiProbe),
  Py_NDIMethodMacro(ndiOpen),
  Py_NDIMethodMacro(ndiOpenNetwork),
  Py_NDIMethodMacro(ndiGetDeviceName),
  Py_NDIMethodMacro(ndiClose),
  Py_NDIMethodMacro(ndiCloseNetwork),
  Py_NDIMethodMacro(ndiSetThreadMode),
  Py_NDIMethodMacro(ndiCommand),

  Py_NDIMethodMacro(ndiGetError),
  Py_NDIMethodMacro(ndiErrorString),

  Py_NDIMethodMacro(ndiRESET),
  Py_NDIMethodMacro(ndiINIT),
  Py_NDIMethodMacro(ndiCOMM),

  Py_NDIMethodMacro(ndiPVWRFromFile),

  Py_NDIMethodMacro(ndiPVWR),
  Py_NDIMethodMacro(ndiPVCLR),
  Py_NDIMethodMacro(ndiPINIT),
  Py_NDIMethodMacro(ndiPENA),
  Py_NDIMethodMacro(ndiPDIS),
  Py_NDIMethodMacro(ndiTSTART),
  Py_NDIMethodMacro(ndiTSTOP),
  Py_NDIMethodMacro(ndiGX),

  Py_NDIMethodMacro(ndiGetGXTransform),
  Py_NDIMethodMacro(ndiGetGXPortStatus),
  Py_NDIMethodMacro(ndiGetGXSystemStatus),
  Py_NDIMethodMacro(ndiGetGXToolInfo),
  Py_NDIMethodMacro(ndiGetGXMarkerInfo),
  Py_NDIMethodMacro(ndiGetGXSingleStray),
  Py_NDIMethodMacro(ndiGetGXFrame),
  Py_NDIMethodMacro(ndiGetGXNumberOfPassiveStrays),
  Py_NDIMethodMacro(ndiGetGXPassiveStray),

  Py_NDIMethodMacro(ndiBX),

  Py_NDIMethodMacro(ndiGetBXTransform),
  Py_NDIMethodMacro(ndiGetBXPortStatus),
  Py_NDIMethodMacro(ndiGetBXSystemStatus),
  //Py_NDIMethodMacro(ndiGetBXToolInfo),
  //Py_NDIMethodMacro(ndiGetBXMarkerInfo),
  //Py_NDIMethodMacro(ndiGetBXSingleStray),
  Py_NDIMethodMacro(ndiGetBXFrame),
  //Py_NDIMethodMacro(ndiGetBXNumberOfPassiveStrays),
  //Py_NDIMethodMacro(ndiGetBXPassiveStray),

  Py_NDIMethodMacro(ndiTX),

  Py_NDIMethodMacro(ndiGetTXTransform),
  Py_NDIMethodMacro(ndiGetTXPortStatus),
  Py_NDIMethodMacro(ndiGetTXSystemStatus),
  //Py_NDIMethodMacro(ndiGetTXToolInfo),
  //Py_NDIMethodMacro(ndiGetTXMarkerInfo),
  //Py_NDIMethodMacro(ndiGetTXSingleStray),
  Py_NDIMethodMacro(ndiGetTXFrame),
  //Py_NDIMethodMacro(ndiGetTXNumberOfPassiveStrays),
  //Py_NDIMethodMacro(ndiGetTXPassiveStray),


  Py_NDIMethodMacro(ndiLED),
  Py_NDIMethodMacro(ndiBEEP),
  Py_NDIMethodMacro(ndiVER),
  Py_NDIMethodMacro(ndiSFLIST),
  Py_NDIMethodMacro(ndiVSEL),
  Py_NDIMethodMacro(ndiPSTAT),

  Py_NDIMethodMacro(ndiGetPSTATPortStatus),
  Py_NDIMethodMacro(ndiGetPSTATToolInfo),
  Py_NDIMethodMacro(ndiGetPSTATCurrentTest),
  Py_NDIMethodMacro(ndiGetPSTATPartNumber),
  Py_NDIMethodMacro(ndiGetPSTATAccessories),
  Py_NDIMethodMacro(ndiGetPSTATMarkerType),

  Py_NDIMethodMacro(ndiSSTAT),

  Py_NDIMethodMacro(ndiGetSSTATControl),
  Py_NDIMethodMacro(ndiGetSSTATSensors),
  Py_NDIMethodMacro(ndiGetSSTATTIU),

  Py_NDIMethodMacro(ndiPPRD),
  Py_NDIMethodMacro(ndiPPWR),
  Py_NDIMethodMacro(ndiPURD),
  Py_NDIMethodMacro(ndiPUWR),
  Py_NDIMethodMacro(ndiPSEL),
  Py_NDIMethodMacro(ndiPSRCH),
  Py_NDIMethodMacro(ndiPVTIP),
  Py_NDIMethodMacro(ndiTCTST),
  Py_NDIMethodMacro(ndiTTCFG),

  Py_NDIMethodMacro(ndiDSTART),
  Py_NDIMethodMacro(ndiDSTOP),
  Py_NDIMethodMacro(ndiIRINIT),
  Py_NDIMethodMacro(ndiIRCHK),

  Py_NDIMethodMacro(ndiGetIRCHKDetected),
  Py_NDIMethodMacro(ndiGetIRCHKNumberOfSources),
  Py_NDIMethodMacro(ndiGetIRCHKSourceXY),

  Py_NDIMethodMacro(ndiGetPHRQHandle),
  Py_NDIMethodMacro(ndiGetPHSRNumberOfHandles),
  Py_NDIMethodMacro(ndiGetPHSRHandle),

  Py_NDIMethodMacro(ndiIRED),
  Py_NDIMethodMacro(ndi3D),

  Py_NDIMethodMacro(ndiRelativeTransform),
  Py_NDIMethodMacro(ndiTransformToMatrixd),
  Py_NDIMethodMacro(ndiTransformToMatrixf),
  Py_NDIMethodMacro(ndiAnglesFromMatrixd),
  Py_NDIMethodMacro(ndiAnglesFromMatrixf),
  Py_NDIMethodMacro(ndiCoordsFromMatrixd),
  Py_NDIMethodMacro(ndiCoordsFromMatrixf),

  {NULL, NULL}
};

#ifdef __cplusplus
extern "C" {
#endif

#define Py_NDIBitfieldMacro(a) \
     PyDict_SetItemString(dict, #a, PyNDIBitfield_FromUnsignedLong(a))
#define Py_NDIConstantMacro(a) \
     PyDict_SetItemString(dict, #a, PyInt_FromLong(a))
#define Py_NDIErrcodeMacro(a) \
     PyDict_SetItemString(dict, #a, PyNDIBitfield_FromUnsignedLong(a))
#define Py_NDICharMacro(a) \
     PyDict_SetItemString(dict, #a, _PyString_FromChar(a))

ndicapiExport MOD_INIT(ndicapy)
{
  PyObject* module = NULL;
  PyObject* dict = NULL;

#if PY_MAJOR_VERSION <= 2
  PyNdicapiType.ob_type = &PyType_Type;
  PyNDIBitfield_Type.ob_type = &PyType_Type;
#else
  #if PY_MINOR_VERSION < 11
    Py_TYPE(&PyNdicapiType) = &PyType_Type;
    Py_TYPE(&PyNDIBitfield_Type) = &PyType_Type;
  #else
    Py_SET_TYPE(&PyNdicapiType, &PyType_Type);
    Py_SET_TYPE(&PyNDIBitfield_Type, &PyType_Type);
  #endif
#endif

  MOD_DEF(module, "ndicapy", NULL, NdicapiMethods);
  if (module == NULL)
    return MOD_ERROR_VAL;
  dict = PyModule_GetDict(module);
  if (dict == NULL)
    return MOD_ERROR_VAL;

  Py_NDIConstantMacro(NDICAPI_MAJOR_VERSION);
  Py_NDIConstantMacro(NDICAPI_MINOR_VERSION);

  Py_NDIConstantMacro(NDI_OKAY);

  Py_NDIErrcodeMacro(NDI_INVALID);
  Py_NDIErrcodeMacro(NDI_TOO_LONG);
  Py_NDIErrcodeMacro(NDI_TOO_SHORT);
  Py_NDIErrcodeMacro(NDI_BAD_COMMAND_CRC);
  Py_NDIErrcodeMacro(NDI_INTERN_TIMEOUT);
  Py_NDIErrcodeMacro(NDI_COMM_FAIL);
  Py_NDIErrcodeMacro(NDI_PARAMETERS);
  Py_NDIErrcodeMacro(NDI_INVALID_PORT);
  Py_NDIErrcodeMacro(NDI_INVALID_MODE);
  Py_NDIErrcodeMacro(NDI_INVALID_LED);
  Py_NDIErrcodeMacro(NDI_LED_STATE);
  Py_NDIErrcodeMacro(NDI_BAD_MODE);
  Py_NDIErrcodeMacro(NDI_NO_TOOL);
  Py_NDIErrcodeMacro(NDI_PORT_NOT_INIT);
  Py_NDIErrcodeMacro(NDI_PORT_DISABLED);
  Py_NDIErrcodeMacro(NDI_INITIALIZATION);
  Py_NDIErrcodeMacro(NDI_TSTOP_FAIL);
  Py_NDIErrcodeMacro(NDI_TSTART_FAIL);
  Py_NDIErrcodeMacro(NDI_PINIT_FAIL);
  Py_NDIErrcodeMacro(NDI_INVALID_PS_PARAM);
  Py_NDIErrcodeMacro(NDI_INIT_FAIL);
  Py_NDIErrcodeMacro(NDI_DSTART_FAIL);
  Py_NDIErrcodeMacro(NDI_DSTOP_FAIL);
  Py_NDIErrcodeMacro(NDI_RESERVED18);
  Py_NDIErrcodeMacro(NDI_FIRMWARE_VER);
  Py_NDIErrcodeMacro(NDI_INTERNAL);
  Py_NDIErrcodeMacro(NDI_RESERVED1B);
  Py_NDIErrcodeMacro(NDI_IRED_FAIL);
  Py_NDIErrcodeMacro(NDI_RESERVED1D);
  Py_NDIErrcodeMacro(NDI_SROM_READ);
  Py_NDIErrcodeMacro(NDI_SROM_WRITE);
  Py_NDIErrcodeMacro(NDI_RESERVED20);
  Py_NDIErrcodeMacro(NDI_PORT_CURRENT);
  Py_NDIErrcodeMacro(NDI_WAVELENGTH);
  Py_NDIErrcodeMacro(NDI_PARAMETER_RANGE);
  Py_NDIErrcodeMacro(NDI_VOLUME);
  Py_NDIErrcodeMacro(NDI_FEATURES);
  Py_NDIErrcodeMacro(NDI_RESERVED26);
  Py_NDIErrcodeMacro(NDI_RESERVED27);
  Py_NDIErrcodeMacro(NDI_TOO_MANY_TOOLS);
  Py_NDIErrcodeMacro(NDI_RESERVED29);
  Py_NDIErrcodeMacro(NDI_HEAP_FULL);
  Py_NDIErrcodeMacro(NDI_HANDLE_NOT_ALLOC);
  Py_NDIErrcodeMacro(NDI_HANDLE_EMPTY);
  Py_NDIErrcodeMacro(NDI_HANDLES_FULL);
  Py_NDIErrcodeMacro(NDI_INCOMP_FIRM_VER);
  Py_NDIErrcodeMacro(NDI_INV_PORT_DESC);
  Py_NDIErrcodeMacro(NDI_PORT_HAS_HANDLE);
  Py_NDIErrcodeMacro(NDI_RESERVED31);
  Py_NDIErrcodeMacro(NDI_INVALID_OP);
  Py_NDIErrcodeMacro(NDI_NO_FEATURE);
  Py_NDIErrcodeMacro(NDI_NO_USER_PARAM);
  Py_NDIErrcodeMacro(NDI_BAD_VALUE);
  Py_NDIErrcodeMacro(NDI_USER_PARAM_VALUE_RANGE);
  Py_NDIErrcodeMacro(NDI_USER_PARAM_INDEX_RANGE);
  Py_NDIErrcodeMacro(NDI_BAD_USER_PARAM_SIZE);
  Py_NDIErrcodeMacro(NDI_PERM_DENIED);
  Py_NDIErrcodeMacro(NDI_RESERVED3A);
  Py_NDIErrcodeMacro(NDI_FILE_NOT_FOUND);
  Py_NDIErrcodeMacro(NDI_FERR_WRITE);
  Py_NDIErrcodeMacro(NDI_FERR_READ);
  Py_NDIErrcodeMacro(NDI_RESERVED3E);
  Py_NDIErrcodeMacro(NDI_RESERVED3F);
  Py_NDIErrcodeMacro(NDI_DEF_FILE_ERR);
  Py_NDIErrcodeMacro(NDI_BAD_CHARACTERISTICS);
  Py_NDIErrcodeMacro(NDI_NO_DEVICE);

  Py_NDIErrcodeMacro(NDI_ENVIRONMENT);

  //Py_NDIErrcodeMacro(NDI_EPROM_READ);
  //Py_NDIErrcodeMacro(NDI_EPROM_WRITE);
  //Py_NDIErrcodeMacro(NDI_EPROM_ERASE);

  Py_NDIErrcodeMacro(NDI_BAD_CRC);
  Py_NDIErrcodeMacro(NDI_OPEN_ERROR);
  Py_NDIErrcodeMacro(NDI_BAD_COMM);
  Py_NDIErrcodeMacro(NDI_TIMEOUT);
  Py_NDIErrcodeMacro(NDI_WRITE_ERROR);
  Py_NDIErrcodeMacro(NDI_READ_ERROR);
  Py_NDIErrcodeMacro(NDI_PROBE_FAIL);

  Py_NDIConstantMacro(NDI_9600);
  Py_NDIConstantMacro(NDI_14400);
  Py_NDIConstantMacro(NDI_19200);
  Py_NDIConstantMacro(NDI_38400);
  Py_NDIConstantMacro(NDI_57600);
  Py_NDIConstantMacro(NDI_115200);
  Py_NDIConstantMacro(NDI_921600);
  Py_NDIConstantMacro(NDI_1228739);
  Py_NDIConstantMacro(NDI_230400);

  Py_NDIConstantMacro(NDI_8N1);
  Py_NDIConstantMacro(NDI_8N2);
  Py_NDIConstantMacro(NDI_8O1);
  Py_NDIConstantMacro(NDI_8O2);
  Py_NDIConstantMacro(NDI_8E1);
  Py_NDIConstantMacro(NDI_8E2);
  Py_NDIConstantMacro(NDI_7N1);
  Py_NDIConstantMacro(NDI_7N2);
  Py_NDIConstantMacro(NDI_7O1);
  Py_NDIConstantMacro(NDI_7O2);
  Py_NDIConstantMacro(NDI_7E1);
  Py_NDIConstantMacro(NDI_7E2);

  Py_NDIConstantMacro(NDI_NOHANDSHAKE);
  Py_NDIConstantMacro(NDI_HANDSHAKE);

  Py_NDICharMacro(NDI_STATIC);
  Py_NDICharMacro(NDI_DYNAMIC);
  Py_NDICharMacro(NDI_BUTTON_BOX);

  Py_NDIBitfieldMacro(NDI_XFORMS_AND_STATUS);
  Py_NDIBitfieldMacro(NDI_ADDITIONAL_INFO);
  Py_NDIBitfieldMacro(NDI_SINGLE_STRAY);
  Py_NDIBitfieldMacro(NDI_FRAME_NUMBER);
  Py_NDIBitfieldMacro(NDI_PASSIVE);
  Py_NDIBitfieldMacro(NDI_PASSIVE_EXTRA);
  Py_NDIBitfieldMacro(NDI_PASSIVE_STRAY);

  Py_NDIConstantMacro(NDI_DISABLED);
  Py_NDIConstantMacro(NDI_MISSING);

  Py_NDIBitfieldMacro(NDI_TOOL_IN_PORT);
  Py_NDIBitfieldMacro(NDI_SWITCH_1_ON);
  Py_NDIBitfieldMacro(NDI_SWITCH_2_ON);
  Py_NDIBitfieldMacro(NDI_SWITCH_3_ON);
  Py_NDIBitfieldMacro(NDI_INITIALIZED);
  Py_NDIBitfieldMacro(NDI_ENABLED);
  Py_NDIBitfieldMacro(NDI_OUT_OF_VOLUME);
  Py_NDIBitfieldMacro(NDI_PARTIALLY_IN_VOLUME);
  Py_NDIBitfieldMacro(NDI_CURRENT_DETECT);

  Py_NDIBitfieldMacro(NDI_COMM_SYNC_ERROR);
  Py_NDIBitfieldMacro(NDI_TOO_MUCH_EXTERNAL_INFRARED);
  Py_NDIBitfieldMacro(NDI_COMM_CRC_ERROR);

  Py_NDIBitfieldMacro(NDI_BAD_TRANSFORM_FIT);
  Py_NDIBitfieldMacro(NDI_NOT_ENOUGH_MARKERS);
  Py_NDIBitfieldMacro(NDI_TOOL_FACE_USED);

  Py_NDIBitfieldMacro(NDI_MARKER_MISSING);
  Py_NDIBitfieldMacro(NDI_MARKER_EXCEEDED_MAX_ANGLE);
  Py_NDIBitfieldMacro(NDI_MARKER_EXCEEDED_MAX_ERROR);
  Py_NDIBitfieldMacro(NDI_MARKER_USED);

  Py_NDICharMacro(NDI_BLANK);
  Py_NDICharMacro(NDI_FLASH);
  Py_NDICharMacro(NDI_SOLID);

  Py_NDIBitfieldMacro(NDI_BASIC);
  Py_NDIBitfieldMacro(NDI_TESTING);
  Py_NDIBitfieldMacro(NDI_PART_NUMBER);
  Py_NDIBitfieldMacro(NDI_ACCESSORIES);
  Py_NDIBitfieldMacro(NDI_MARKER_TYPE);

  Py_NDIConstantMacro(NDI_UNOCCUPIED);

  Py_NDIBitfieldMacro(NDI_TOOL_IN_PORT_SWITCH);
  Py_NDIBitfieldMacro(NDI_SWITCH_1);
  Py_NDIBitfieldMacro(NDI_SWITCH_2);
  Py_NDIBitfieldMacro(NDI_SWITCH_3);
  Py_NDIBitfieldMacro(NDI_TOOL_TRACKING_LED);
  Py_NDIBitfieldMacro(NDI_LED_1);
  Py_NDIBitfieldMacro(NDI_LED_2);
  Py_NDIBitfieldMacro(NDI_LED_3);

  Py_NDIBitfieldMacro(NDI_950NM);
  Py_NDIBitfieldMacro(NDI_850NM);

  Py_NDIBitfieldMacro(NDI_NDI_ACTIVE);
  Py_NDIBitfieldMacro(NDI_NDI_CERAMIC);
  Py_NDIBitfieldMacro(NDI_PASSIVE_ANY);
  Py_NDIBitfieldMacro(NDI_PASSIVE_SPHERE);
  Py_NDIBitfieldMacro(NDI_PASSIVE_DISC);

  Py_NDIBitfieldMacro(NDI_CONTROL);
  Py_NDIBitfieldMacro(NDI_SENSORS);
  Py_NDIBitfieldMacro(NDI_TIU);

  Py_NDIBitfieldMacro(NDI_EPROM_CODE_CHECKSUM);
  Py_NDIBitfieldMacro(NDI_EPROM_SYSTEM_CHECKSUM);

  Py_NDIBitfieldMacro(NDI_LEFT_ROM_CHECKSUM);
  Py_NDIBitfieldMacro(NDI_LEFT_SYNC_TYPE_1);
  Py_NDIBitfieldMacro(NDI_LEFT_SYNC_TYPE_2);
  Py_NDIBitfieldMacro(NDI_RIGHT_ROM_CHECKSUM);
  Py_NDIBitfieldMacro(NDI_RIGHT_SYNC_TYPE_1);
  Py_NDIBitfieldMacro(NDI_RIGHT_SYNC_TYPE_2);

  Py_NDIBitfieldMacro(NDI_ROM_CHECKSUM);
  Py_NDIBitfieldMacro(NDI_OPERATING_VOLTAGES);
  Py_NDIBitfieldMacro(NDI_MARKER_SEQUENCING);
  Py_NDIBitfieldMacro(NDI_SYNC);
  Py_NDIBitfieldMacro(NDI_COOLING_FAN);
  Py_NDIBitfieldMacro(NDI_INTERNAL_ERROR);

  Py_NDIBitfieldMacro(NDI_DETECTED);
  Py_NDIBitfieldMacro(NDI_SOURCES);

  Py_NDIConstantMacro(NDI_LEFT);
  Py_NDIConstantMacro(NDI_RIGHT);

  return MOD_SUCCESS_VAL(module);
}

#ifdef __cplusplus
}
#endif
