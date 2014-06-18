/* python-libjit, Copyright 2014 Niklas Koep
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "pyjit-context.h"

PyDoc_STRVAR(context_doc, "Wrapper class for jit_context_t");

static PyObject *context_cache = NULL;

/* Slot implementations */

static void
context_dealloc(PyJitContext *self)
{
    if (self->weakreflist)
        PyObject_ClearWeakRefs((PyObject *)self);

    if (self->context) {
        PYJIT_BEGIN_ALLOW_EXCEPTION
        if (pyjit_weak_cache_delitem(context_cache, (long)self->context) < 0) {
            PYJIT_TRACE("this shouldn't have happened");
            abort();
        }
        PYJIT_END_ALLOW_EXCEPTION

        jit_context_destroy(self->context);
    }
    Py_TYPE(self)->tp_free((PyObject *)self);
}

PYJIT_REPR_GENERIC(context_repr, PyJitContext, context)

PYJIT_HASH_GENERIC(context_hash, PyJitContext, PyJitContext_Verify, context)

static int
context_init(PyJitContext *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = { NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, ":Context", kwlist))
        return -1;

    self->context = jit_context_create();

    /* Cache the context. */
    return pyjit_weak_cache_setitem(
        context_cache, (long)self->context, (PyObject *)self);
}

/* Regular methods */

static PyObject *context_enter(PyJitContext *self); /* Forward */

static PyObject *
context_build_start(PyJitContext *self)
{
    if (!context_enter(self))
        return NULL;
    Py_RETURN_NONE;
}

static PyObject *
context_build_end(PyJitContext *self)
{
    if (PyJitContext_Verify(self) < 0)
        return NULL;
    jit_context_build_end(self->context);
    Py_RETURN_NONE;
}

static PyObject *
context_set_meta(PyJitContext *self, PyObject *args, PyObject *kwargs)
{
    int type, retval;
    PyObject *data;
    static char *kwlist[] = { "type_", "data", NULL };

    if (PyJitContext_Verify(self) < 0)
        return NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "iO:Context", kwlist, &type,
                                     &data))
        return NULL;

    Py_INCREF(data);
    retval = jit_context_set_meta(self->context, type, data,
                                  pyjit_meta_free_func);
    return PyBool_FromLong(retval);
}

static PyObject *
context_set_meta_numeric(PyJitContext *self, PyObject *args, PyObject *kwargs)
{
    int type, retval;
    jit_nuint data;
    static char *kwlist[] = { "type_", "data", NULL };

    if (PyJitContext_Verify(self) < 0)
        return NULL;

    /* jit_nuint == unsigned long -> k */
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "ik:Context", kwlist, &type,
                                     &data))
        return NULL;

    retval = jit_context_set_meta_numeric(self->context, type, data);
    return PyBool_FromLong(retval);
}

static int
_context_get_meta_type(
    PyJitContext *self, PyObject *args, PyObject *kwargs, int *type)
{
    static char *kwlist[] = { "type_", NULL };

    if (PyJitContext_Verify(self) < 0)
        return -1;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "i:Context", kwlist, type))
        return -1;

    return 0;
}

static PyObject *
context_get_meta(PyJitContext *self, PyObject *args, PyObject *kwargs)
{
    int type;
    PyObject *retval;

    if (_context_get_meta_type(self, args, kwargs, &type) < 0)
        return NULL;

    retval = (PyObject *)jit_context_get_meta(self->context, type);
    if (retval) {
        Py_INCREF(retval);
        return retval;
    }
    Py_RETURN_NONE;
}

static PyObject *
context_get_meta_numeric(PyJitContext *self, PyObject *args, PyObject *kwargs)
{
    int type;
    jit_nuint retval;

    if (_context_get_meta_type(self, args, kwargs, &type) < 0)
        return NULL;

    retval = jit_context_get_meta_numeric(self->context, type);
    return PyLong_FromUnsignedLong(retval);
}

static PyObject *
context_free_meta(PyJitContext *self, PyObject *args, PyObject *kwargs)
{
    int type;

    if (_context_get_meta_type(self, args, kwargs, &type) < 0)
        return NULL;

    jit_context_free_meta(self->context, type);
    Py_RETURN_NONE;
}

static PyObject *
context_enter(PyJitContext *self)
{
    if (PyJitContext_Verify(self) < 0)
        return NULL;
    jit_context_build_start(self->context);
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyMethodDef context_methods[] = {
    PYJIT_METHOD_NOARGS(context, build_start),
    PYJIT_METHOD_NOARGS(context, build_end),
    PYJIT_METHOD_KW(context, set_meta),
    PYJIT_METHOD_KW(context, set_meta_numeric),
    PYJIT_METHOD_KW(context, get_meta),
    PYJIT_METHOD_KW(context, get_meta_numeric),
    PYJIT_METHOD_KW(context, free_meta),
    PYJIT_METHOD_EX("__enter__", context_enter, METH_NOARGS),
    PYJIT_METHOD_EX("__exit__", context_build_end, METH_VARARGS),

    /* TODO: Re-export jit_function_next, etc. here */
    { NULL } /* Sentinel */
};

static PyTypeObject PyJitContext_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                                      /* ob_size */
    "jit.Context",                          /* tp_name */
    sizeof(PyJitContext),                   /* tp_basicsize */
    0,                                      /* tp_itemsize */
    (destructor)context_dealloc,            /* tp_dealloc */
    0,                                      /* tp_print */
    0,                                      /* tp_getattr */
    0,                                      /* tp_setattr */
    0,                                      /* tp_compare */
    (reprfunc)context_repr,                 /* tp_repr */
    0,                                      /* tp_as_number */
    0,                                      /* tp_as_sequence */
    0,                                      /* tp_as_mapping */
    (hashfunc)context_hash,                 /* tp_hash */
    0,                                      /* tp_call */
    0,                                      /* tp_str */
    0,                                      /* tp_getattro */
    0,                                      /* tp_setattro */
    0,                                      /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT |
        Py_TPFLAGS_BASETYPE,                /* tp_flags */
    context_doc,                            /* tp_doc */
    0,                                      /* tp_traverse */
    0,                                      /* tp_clear */
    0,                                      /* tp_richcompare */
    offsetof(PyJitContext, weakreflist),    /* tp_weaklistoffset */
    /* TODO: Add support for iterating over a context to yield
     *       jit.FunctionS.
     */
    0,                                      /* tp_iter */
    0,                                      /* tp_iternext */
    context_methods,                        /* tp_methods */
    0,                                      /* tp_members */
    0,                                      /* tp_getset */
    0,                                      /* tp_base */
    0,                                      /* tp_dict */
    0,                                      /* tp_descr_get */
    0,                                      /* tp_descr_set */
    0,                                      /* tp_dictoffset */
    (initproc)context_init,                 /* tp_init */
    0,                                      /* tp_alloc */
    PyType_GenericNew                       /* tp_new */
};

int
pyjit_context_init(PyObject *module)
{
    if (PyType_Ready(&PyJitContext_Type) < 0)
        return -1;

    /* Set up the context cache. */
    context_cache = PyDict_New();
    if (!context_cache)
        return -1;

    Py_INCREF(&PyJitContext_Type);
    PyModule_AddObject(module, "Context", (PyObject *)&PyJitContext_Type);

    return 0;
}

const PyTypeObject *
pyjit_context_get_pytype(void)
{
    return &PyJitContext_Type;
}

PyObject *
PyJitContext_New(jit_context_t context)
{
    long numkey;
    PyObject *object;

    numkey = (long)context;
    object = pyjit_weak_cache_getitem(context_cache, numkey);
    if (!object) {
       PyJitContext *ctx = PyObject_New(PyJitContext, &PyJitContext_Type);
       ctx->context = context;
       object = (PyObject *)ctx;
       if (pyjit_weak_cache_setitem(context_cache, numkey, object) < 0) {
           Py_DECREF(object);
           return NULL;
       }
    }
    return object;
}

int
PyJitContext_Check(PyObject *o)
{
    return PyObject_IsInstance(o, (PyObject *)&PyJitContext_Type);
}

PyJitContext *
PyJitContext_Cast(PyObject *o)
{
    int r = PyJitContext_Check(o);
    if (r == 1)
        return (PyJitContext *)o;
    else if (r < 0 && PyErr_Occurred())
        PyErr_Clear();
    return NULL;
}

int
PyJitContext_Verify(PyJitContext *o)
{
    if (!o->context) {
        PyErr_SetString(PyExc_ValueError, "context is not initialized");
        return -1;
    }
    return 0;
}

PyJitContext *
PyJitContext_CastAndVerify(PyObject *o)
{
    PyJitContext *context = PyJitContext_Cast(o);
    if (!context) {
        PyErr_Format(
            PyExc_TypeError, "expected instance of %.100s, not %.100s",
            pyjit_context_get_pytype()->tp_name, Py_TYPE(o)->tp_name);
        return NULL;
    }
    if (PyJitContext_Verify(context) < 0)
        return NULL;
    return context;
}

