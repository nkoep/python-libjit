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

#include "pyjit-type.h"

PyDoc_STRVAR(type_doc, "Wrapper class for jit_type_t");

/* Slot implementations */

static PyObject *type_cache = NULL;

static void
type_dealloc(PyJitType *self)
{
    if (self->weakreflist)
        PyObject_ClearWeakRefs((PyObject *)self);

    if (self->type) {
        PYJIT_BEGIN_ALLOW_EXCEPTION
        if (pyjit_weak_cache_delitem(type_cache, (long)self->type) < 0) {
            PYJIT_TRACE("this shouldn't have happened");
            abort();
        }
        PYJIT_END_ALLOW_EXCEPTION

        jit_type_free(self->type);
    }

    Py_TYPE(self)->tp_free((PyObject *)self);
}

PYJIT_REPR_GENERIC(type_repr, PyJitType, type)

PYJIT_HASH_GENERIC(type_hash, PyJitType, PyJitType_Verify, type)

/* Regular methods */

static PyObject *
_type_create_aggregate_type(
    PyObject *args, PyObject *kwargs,
    jit_type_t (*createfunc)(jit_type_t *, unsigned int, int))
{
    int incref = 1, r;
    unsigned int i, num_fields;
    PyObject *fields = NULL, *retval = NULL;
    jit_type_t *jit_fields;
    static char *kwlist[] = { "fields", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O:Type", kwlist, &fields))
        return NULL;

    r = PySequence_Check(fields);
    if (r < 0) {
        return NULL;
    }
    else if (r == 0) {
        PyErr_Format(PyExc_TypeError, "fields must be a sequence, not %.100s",
                     Py_TYPE(fields)->tp_name);
        return NULL;
    }

    num_fields = (unsigned int)PySequence_Length(fields);
    jit_fields = PyMem_New(jit_type_t, num_fields);
    for (i = 0; i < num_fields; i++) {
        PyObject *item = PySequence_ITEM(fields, i);
        PyJitType *jit_item;
        if (!item)
            break;
        jit_item = PyJitType_Cast(item);
        if (!jit_item) {
            PyErr_Format(
                PyExc_TypeError,
                "sequence elements must be of type %.100s, not %.100s",
                pyjit_type_get_pytype()->tp_name, Py_TYPE(item)->tp_name);
            Py_DECREF(item);
            break;
        }
        if (PyJitType_Verify(jit_item) < 0) {
            Py_DECREF(item);
            break;
        }
        jit_fields[i] = jit_item->type;
        Py_DECREF(item);
    }

    /* We didn't break from the loop prematurely, so we're good to go. */
    if (i == num_fields) {
        jit_type_t type = createfunc(jit_fields, num_fields, incref);
        if (type) {
            retval = PyJitType_New(type);
        }
        else {
            PyErr_Format(PyExc_MemoryError,
                         "memory allocation inside LibJIT failed");
        }
    }

    PyMem_Free(jit_fields);
    return retval;
}

static PyObject *
type_create_struct(void *null, PyObject *args, PyObject *kwargs)
{
    return _type_create_aggregate_type(args, kwargs, jit_type_create_struct);
}

static PyObject *
type_create_union(void *null, PyObject *args, PyObject *kwargs)
{
    return _type_create_aggregate_type(args, kwargs, jit_type_create_union);
}

static PyObject *
type_create_signature(void *null, PyObject *args, PyObject *kwargs)
{
    int abi, incref = 1, r;
    unsigned int i, num_params;
    PyObject *return_type = NULL, *params = NULL, *retval = NULL;
    jit_type_t jit_return_type, *jit_params;
    static char *kwlist[] = { "abi", "return_type", "params", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "iOO:Type", kwlist, &abi,
                                     &return_type, &params))
        return NULL;

    if (return_type == Py_None) {
        jit_return_type = jit_type_void;
    }
    else {
        PyJitType *rt = PyJitType_Cast(return_type);
        if (!rt) {
            pyjit_raise_type_error("return_type", pyjit_type_get_pytype(),
                                   return_type);
            return NULL;
        }
        if (PyJitType_Verify(rt) < 0)
            return NULL;
        jit_return_type = rt->type;
    }

    r = PySequence_Check(params);
    if (r < 0) {
        return NULL;
    }
    else if (r == 0) {
        PyErr_Format(PyExc_TypeError, "params must be a sequence, not %.100s",
                     Py_TYPE(params)->tp_name);
        return NULL;
    }

    num_params = (unsigned int)PySequence_Length(params);
    jit_params = PyMem_New(jit_type_t, num_params);
    for (i = 0; i < num_params; i++) {
        PyObject *item = PySequence_ITEM(params, i);
        PyJitType *jit_item;
        if (!item)
            break;
        jit_item = PyJitType_Cast(item);
        if (!jit_item) {
            PyErr_Format(
                PyExc_TypeError,
                "sequence elements must be of type %.100s, not %.100s",
                pyjit_type_get_pytype()->tp_name, Py_TYPE(item)->tp_name);
            Py_DECREF(item);
            break;
        }
        if (PyJitType_Verify(jit_item) < 0) {
            Py_DECREF(item);
            break;
        }
        jit_params[i] = jit_item->type;
        Py_DECREF(item);
    }

    /* We didn't break from the loop prematurely, so we're good to go. */
    if (i == num_params) {
        jit_type_t signature = jit_type_create_signature(
            abi, jit_return_type, jit_params, num_params, incref);
        if (signature) {
            retval = PyJitType_New(signature);
        }
        else {
            PyErr_Format(PyExc_MemoryError,
                         "memory allocation inside LibJIT failed");
        }
    }

    PyMem_Free(jit_params);
    return retval;
}

static PyObject *
type_create_pointer(PyJitType *self)
{
    int incref = 1;
    jit_type_t type;

    if (PyJitType_Verify(self) < 0)
        return NULL;

    type = jit_type_create_pointer(self->type, incref);
    if (!type) {
        PyErr_SetString(PyExc_MemoryError,
                        "memory allocation inside LibJIT failed");
        return NULL;
    }
    return PyJitType_New(type);
}

static PyObject *
type_create_tagged(PyJitType *self, PyObject *args, PyObject *kwargs)
{
    int kind, incref = 1;
    PyObject *data = NULL;
    jit_type_t type;
    static char *kwlist[] = { "kind", "data", NULL };

    if (PyJitType_Verify(self) < 0)
        return NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "i|O:Type", kwlist, &kind,
                                     &data))
        return NULL;

    if (data)
        Py_INCREF(data);
    type = jit_type_create_tagged(
        self->type, kind, data, pyjit_meta_free_func, incref);
    if (!type) {
        PyErr_SetString(PyExc_MemoryError,
                        "memory allocation inside LibJIT failed");
        Py_DECREF(data);
        return NULL;
    }
    return PyJitType_New(type);
}

static PyObject *
type_set_names(PyJitType *self, PyObject *args, PyObject *kwargs)
{
    int r;
    PyObject *names = NULL, *retval = NULL;
    unsigned int i, num_names;
    char **cnames;
    static char *kwlist[] = { "names", NULL };

    if (PyJitType_Verify(self) < 0)
        return NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O:Type", kwlist, &names))
        return NULL;

    r = PySequence_Check(names);
    if (r < 0) {
        return NULL;
    }
    else if (r == 0) {
        PyErr_Format(PyExc_TypeError, "names must be a sequence, not %.100s",
                     Py_TYPE(names)->tp_name);
        return NULL;
    }

    num_names = (unsigned int)PySequence_Length(names);
    cnames = PyMem_New(char *, num_names);
    for (i = 0; i < num_names; i++) {
        PyObject *item = PySequence_ITEM(names, i);
        if (!item)
            break;
        r = PyString_Check(item);
        if (r < 0) {
            Py_DECREF(item);
            break;
        }
        else if (r == 0) {
            PyErr_Format(
                PyExc_TypeError,
                "sequence elements must be strings, not %.100s",
                Py_TYPE(item)->tp_name);
            Py_DECREF(item);
            break;
        }
        cnames[i] = PyString_AsString(item);
        Py_DECREF(item);
    }

    if (i == num_names) {
        retval = PyBool_FromLong(
            jit_type_set_names(self->type, cnames, num_names));
    }
    PyMem_Free(cnames);
    return retval;
}

static PyObject *
type_set_size_and_alignment(PyJitType *self, PyObject *args, PyObject *kwargs)
{
    long size, alignment;
    static char *kwlist[] = { "size", "alignment", NULL };

    if (PyJitType_Verify(self) < 0)
        return NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "ll:Type", kwlist, &size,
                                     &alignment))
        return NULL;

    jit_type_set_size_and_alignment(self->type, size, alignment);
    Py_RETURN_NONE;
}

static PyObject *
type_set_offset(PyJitType *self, PyObject *args, PyObject *kwargs)
{
    unsigned int field_index;
    unsigned long offset;
    static char *kwlist[] = { "field_index", "offset", NULL };

    if (PyJitType_Verify(self) < 0)
        return NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "Ik:Type", kwlist,
                                     &field_index, &offset))
        return NULL;

    jit_type_set_offset(self->type, field_index, offset);
    Py_RETURN_NONE;
}

static PyObject *
type_get_kind(PyJitType *self)
{
    if (PyJitType_Verify(self) < 0)
        return NULL;
    return PyLong_FromLong(jit_type_get_kind(self->type));
}

/* The following two functions actually return unsigned values. However, the
 * `jit_type_set_size_and_alignment' function accepts signed values and
 * special-cases -1 which is also returned if `jit_type_get_{size,alignment}'
 * is called afterwards. To produce sensible values for the interpreter we
 * therefore need to cast the return values to signed integers again.
 */
static PyObject *
type_get_size(PyJitType *self)
{
    if (PyJitType_Verify(self) < 0)
        return NULL;
    /* jit_nuint == unsigned long */
    return PyLong_FromLong((jit_nint)jit_type_get_size(self->type));
}

static PyObject *
type_get_alignment(PyJitType *self)
{
    if (PyJitType_Verify(self) < 0)
        return NULL;
    /* jit_nuint == unsigned long */
    return PyLong_FromLong((jit_nint)jit_type_get_alignment(self->type));
}

static PyObject *
_type_num_elems(PyJitType *self, unsigned int (*elemfunc)(jit_type_t))
{
    if (PyJitType_Verify(self) < 0)
        return NULL;
    return PyLong_FromUnsignedLong(elemfunc(self->type));
}

static PyObject *
type_num_fields(PyJitType *self)
{
    return _type_num_elems(self, jit_type_num_fields);
}

static int
_type_get_field_offset(PyJitType *self, PyObject *args, PyObject *kwargs)
{
    unsigned int field_index;
    static char *kwlist[] = { "field_index", NULL };

    if (PyJitType_Verify(self) < 0)
        return -1;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "I:Type", kwlist,
                                     &field_index))
        return -1;

    return (unsigned int)field_index;
}

static PyObject *
type_get_field(PyJitType *self, PyObject *args, PyObject *kwargs)
{
    int field_index;
    jit_type_t type;


    field_index = _type_get_field_offset(self, args, kwargs);
    if (field_index < 0)
        return NULL;
    type = jit_type_get_field(self->type, field_index);
    if (type)
        return PyJitType_New(type);
    Py_RETURN_NONE;
}

static PyObject *
type_get_offset(PyJitType *self, PyObject *args, PyObject *kwargs)
{
    int field_index;
    /* jit_nuint == unsigned long */
    unsigned long offset;

    field_index = _type_get_field_offset(self, args, kwargs);
    if (field_index < 0)
        return NULL;
    offset = jit_type_get_offset(self->type, field_index);
    return PyLong_FromUnsignedLong(offset);
}

static PyObject *
type_get_name(PyJitType *self, PyObject *args, PyObject *kwargs)
{
    unsigned int index;
    const char *name;
    static char *kwlist[] = { "index", NULL };

    if (PyJitType_Verify(self) < 0)
        return NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "I:Type", kwlist, &index))
        return NULL;

    name = jit_type_get_name(self->type, index);
    if (name)
        return PyString_FromString(name);
    Py_RETURN_NONE;
}

static PyObject *
type_find_name(PyJitType *self, PyObject *args, PyObject *kwargs)
{
    unsigned int index;
    const char *name;
    static char *kwlist[] = { "name", NULL };

    if (PyJitType_Verify(self) < 0)
        return NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s:Type", kwlist, &name))
        return NULL;

    index = jit_type_find_name(self->type, name);
    return PyLong_FromUnsignedLong(index);
}

static PyObject *
type_num_params(PyJitType *self)
{
    return _type_num_elems(self, jit_type_num_params);
}

static PyObject *
_type_method_noargs(PyJitType *self, jit_type_t (*func)(jit_type_t))
{
    jit_type_t type;

    if (PyJitType_Verify(self) < 0)
        return NULL;
    type = func(self->type);
    if (type)
        return PyJitType_New(type);
    Py_RETURN_NONE;
}

static PyObject *
type_get_return(PyJitType *self)
{
    return _type_method_noargs(self, jit_type_get_return);
}

static PyObject *
type_get_param(PyJitType *self, PyObject *args, PyObject *kwargs)
{
    unsigned int param_index;
    jit_type_t type;
    static char *kwlist[] = { "param_index", NULL };

    if (PyJitType_Verify(self) < 0)
        return NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "I:Type", kwlist,
                                     &param_index))
        return NULL;

    type = jit_type_get_param(self->type, param_index);
    if (type)
        return PyJitType_New(type);
    Py_RETURN_NONE;
}

static PyObject *
type_get_abi(PyJitType *self)
{
    if (PyJitType_Verify(self) < 0)
        return NULL;
    return PyLong_FromLong(jit_type_get_abi(self->type));
}

static PyObject *
type_get_ref(PyJitType *self)
{
    return _type_method_noargs(self, jit_type_get_ref);
}

static PyObject *
type_get_tagged_type(PyJitType *self)
{
    return _type_method_noargs(self, jit_type_get_tagged_type);
}

static PyObject *
type_set_tagged_type(PyJitType *self, PyObject *args, PyObject *kwargs)
{
    int incref = 1;
    PyObject *underlying;
    PyJitType *jit_underlying;
    static char *kwlist[] = { "underlying", NULL };

    if (PyJitType_Verify(self) < 0)
        return NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O:Type", kwlist,
                                     &underlying))
        return NULL;

    jit_underlying = PyJitType_Cast(underlying);
    if (!jit_underlying) {
        pyjit_raise_type_error("underlying", pyjit_type_get_pytype(),
                               underlying);
        return NULL;
    }
    if (PyJitType_Verify(jit_underlying) < 0)
        return NULL;

    jit_type_set_tagged_type(self->type, jit_underlying->type, incref);
    Py_RETURN_NONE;
}

/* This erroneously appears as `jit_type_get_tagged_type()' in libjit's
 * documentation. A patch is available upstream.
 */
static PyObject *
type_get_tagged_kind(PyJitType *self)
{
    if (PyJitType_Verify(self) < 0)
        return NULL;
    return PyLong_FromLong(jit_type_get_tagged_kind(self->type));
}

static PyObject *
type_get_tagged_data(PyJitType *self)
{
    PyObject *data;

    if (PyJitType_Verify(self) < 0)
        return NULL;
    data = (PyObject *)jit_type_get_tagged_data(self->type);
    if (data) {
        Py_INCREF(data);
        return data;
    }
    Py_RETURN_NONE;
}

static PyObject *
type_set_tagged_data(PyJitType *self, PyObject *args, PyObject *kwargs)
{
    PyObject *data = NULL;
    static char *kwlist[] = { "data", NULL };

    if (PyJitType_Verify(self) < 0)
        return NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O:Type", kwlist, &data))
        return NULL;

    Py_INCREF(data);
    jit_type_set_tagged_data(self->type, data, pyjit_meta_free_func);
    Py_RETURN_NONE;
}

#define DEFINE_BOOLEAN_METHOD(name)                         \
static PyObject *                                           \
type_##name(PyJitType *self)                                \
{                                                           \
    if (PyJitType_Verify(self) < 0)                         \
        return NULL;                                        \
    return PyBool_FromLong(jit_type_##name(self->type));    \
}

DEFINE_BOOLEAN_METHOD(is_primitive)
DEFINE_BOOLEAN_METHOD(is_struct)
DEFINE_BOOLEAN_METHOD(is_union)
DEFINE_BOOLEAN_METHOD(is_signature)
DEFINE_BOOLEAN_METHOD(is_pointer)
DEFINE_BOOLEAN_METHOD(is_tagged)

static PyObject *
type_best_alignment(void *null)
{
    return PyLong_FromUnsignedLong(jit_type_best_alignment());
}

static PyObject *
type_normalize(PyJitType *self)
{
    return _type_method_noargs(self, jit_type_normalize);
}

static PyObject *
type_remove_tags(PyJitType *self)
{
    return _type_method_noargs(self, jit_type_remove_tags);
}

static PyObject *
type_promote_int(PyJitType *self)
{
    return _type_method_noargs(self, jit_type_promote_int);
}

DEFINE_BOOLEAN_METHOD(return_via_pointer)

#undef DEFINE_BOOLEAN_METHOD

static PyObject *
type_has_tag(PyJitType *self, PyObject *args, PyObject *kwargs)
{
    int kind;
    static char *kwlist[] = { "kind", NULL };

    if (PyJitType_Verify(self) < 0)
        return NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "i:Type", kwlist, &kind))
        return NULL;

    return PyBool_FromLong(jit_type_has_tag(self->type, kind));
}

static PyMethodDef type_methods[] = {
    PYJIT_STATIC_METHOD_KW(type, create_struct),
    PYJIT_STATIC_METHOD_KW(type, create_union),
    PYJIT_STATIC_METHOD_KW(type, create_signature),
    PYJIT_METHOD_NOARGS(type, create_pointer),
    PYJIT_METHOD_KW(type, create_tagged),
    PYJIT_METHOD_KW(type, set_names),
    PYJIT_METHOD_KW(type, set_size_and_alignment),
    PYJIT_METHOD_KW(type, set_offset),
    PYJIT_METHOD_NOARGS(type, get_kind),
    PYJIT_METHOD_NOARGS(type, get_size),
    PYJIT_METHOD_NOARGS(type, get_alignment),
    PYJIT_METHOD_NOARGS(type, num_fields),
    PYJIT_METHOD_KW(type, get_field),
    PYJIT_METHOD_KW(type, get_offset),
    PYJIT_METHOD_KW(type, get_name),
    PYJIT_METHOD_KW(type, find_name),
    PYJIT_METHOD_NOARGS(type, num_params),
    PYJIT_METHOD_NOARGS(type, get_return),
    PYJIT_METHOD_KW(type, get_param),
    PYJIT_METHOD_NOARGS(type, get_abi),
    PYJIT_METHOD_NOARGS(type, get_ref),
    PYJIT_METHOD_NOARGS(type, get_tagged_type),
    PYJIT_METHOD_KW(type, set_tagged_type),
    PYJIT_METHOD_NOARGS(type, get_tagged_kind),
    PYJIT_METHOD_NOARGS(type, get_tagged_data),
    PYJIT_METHOD_KW(type, set_tagged_data),
    PYJIT_METHOD_NOARGS(type, is_primitive),
    PYJIT_METHOD_NOARGS(type, is_struct),
    PYJIT_METHOD_NOARGS(type, is_union),
    PYJIT_METHOD_NOARGS(type, is_signature),
    PYJIT_METHOD_NOARGS(type, is_pointer),
    PYJIT_METHOD_NOARGS(type, is_tagged),
    PYJIT_STATIC_METHOD_NOARGS(type, best_alignment),
    PYJIT_METHOD_NOARGS(type, normalize),
    PYJIT_METHOD_NOARGS(type, remove_tags),
    PYJIT_METHOD_NOARGS(type, promote_int),
    PYJIT_METHOD_NOARGS(type, return_via_pointer),
    PYJIT_METHOD_KW(type, has_tag),
    { NULL } /* Sentinel */
};

static PyTypeObject PyJitType_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                                  /* ob_size */
    "jit.Type",                         /* tp_name */
    sizeof(PyJitType),                  /* tp_basicsize */
    0,                                  /* tp_itemsize */
    (destructor)type_dealloc,           /* tp_dealloc */
    0,                                  /* tp_print */
    0,                                  /* tp_getattr */
    0,                                  /* tp_setattr */
    0,                                  /* tp_compare */
    (reprfunc)type_repr,                /* tp_repr */
    0,                                  /* tp_as_number */
    0,                                  /* tp_as_sequence */
    0,                                  /* tp_as_mapping */
    (hashfunc)type_hash,                /* tp_hash */
    0,                                  /* tp_call */
    0,                                  /* tp_str */
    0,                                  /* tp_getattro */
    0,                                  /* tp_setattro */
    0,                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,                 /* tp_flags */
    type_doc,                           /* tp_doc */
    0,                                  /* tp_traverse */
    0,                                  /* tp_clear */
    0,                                  /* tp_richcompare */
    offsetof(PyJitType, weakreflist),   /* tp_weaklistoffset */
    0,                                  /* tp_iter */
    0,                                  /* tp_iternext */
    type_methods                        /* tp_methods */
};

int
pyjit_type_init(PyObject *module)
{
    PyObject *dict;

    if (PyType_Ready(&PyJitType_Type) < 0)
        return -1;

    /* Set up the type cache. */
    type_cache = PyDict_New();
    if (!type_cache)
        return -1;

    /* Register primitive types in the class. */
    dict = PyJitType_Type.tp_dict;

#define REGISTER_TYPE(id)                                           \
do {                                                                \
    char name[] = #id;                                              \
    PyObject *key = PyString_FromString(pyjit_strtoupper(name));    \
    PyObject *o = PyJitType_New(jit_type_##id);                     \
    int r = PyDict_SetItem(dict, key, o);                           \
    Py_DECREF(key);                                                 \
    Py_DECREF(o);                                                   \
    if (r < 0)                                                      \
        return -1;                                                  \
} while (0)

    /* Platform-agnostic types, e.g. jit_type_int always describes 32-bit
     * wide signed integers
     */
    REGISTER_TYPE(void);
    REGISTER_TYPE(sbyte);
    REGISTER_TYPE(ubyte);
    REGISTER_TYPE(ushort);
    REGISTER_TYPE(short);
    REGISTER_TYPE(int);
    REGISTER_TYPE(uint);
    REGISTER_TYPE(nint);
    REGISTER_TYPE(nuint);
    REGISTER_TYPE(long);
    REGISTER_TYPE(ulong);
    REGISTER_TYPE(float32);
    REGISTER_TYPE(float64);
    REGISTER_TYPE(nfloat);
    REGISTER_TYPE(void_ptr);

    /* System types, e.g. jit_type_sys_int may describe a 32- or 64-bit signed
     * integer depending on the platform's architecture
     */
    REGISTER_TYPE(sys_bool);
    REGISTER_TYPE(sys_char);
    REGISTER_TYPE(sys_schar);
    REGISTER_TYPE(sys_uchar);
    REGISTER_TYPE(sys_short);
    REGISTER_TYPE(sys_ushort);
    REGISTER_TYPE(sys_int);
    REGISTER_TYPE(sys_uint);
    REGISTER_TYPE(sys_long);
    REGISTER_TYPE(sys_ulong);
    REGISTER_TYPE(sys_longlong);
    REGISTER_TYPE(sys_ulonglong);
    REGISTER_TYPE(sys_float);
    REGISTER_TYPE(sys_double);
    REGISTER_TYPE(sys_long_double);

#undef REGISTER_TYPE

    Py_INCREF(&PyJitType_Type);
    PyModule_AddObject(module, "Type", (PyObject *)&PyJitType_Type);

    return 0;
}

const PyTypeObject *
pyjit_type_get_pytype(void)
{
    return &PyJitType_Type;
}

PyObject *
PyJitType_New(jit_type_t type)
{
    long numkey;
    PyObject *object;

    numkey = (long)type;
    object = pyjit_weak_cache_getitem(type_cache, numkey);
    if (!object) {
        object = PyType_GenericNew(&PyJitType_Type, NULL, NULL);
        if (!object)
            return NULL;
        ((PyJitType *)object)->type = type;

        /* Cache the type. */
        if (pyjit_weak_cache_setitem(type_cache, numkey, object) < 0) {
            Py_DECREF(object);
            return NULL;
        }
    }
    return object;
}

int
PyJitType_Check(PyObject *o)
{
    return PyObject_IsInstance(o, (PyObject *)&PyJitType_Type);
}

PyJitType *
PyJitType_Cast(PyObject *o)
{
    int r = PyJitType_Check(o);
    if (r == 1)
        return (PyJitType *)o;
    else if (r < 0 && PyErr_Occurred())
        PyErr_Clear();
    return NULL;
}

int
PyJitType_Verify(PyJitType *o)
{
    if (!o->type) {
        PyErr_SetString(PyExc_ValueError, "type is not initialized");
        return -1;
    }
    return 0;
}

PyJitType *
PyJitType_CastAndVerify(PyObject *o)
{
    PyJitType *type = PyJitType_Cast(o);
    if (!type) {
        PyErr_Format(
            PyExc_TypeError, "expected instance of %.100s, not %.100s",
            pyjit_type_get_pytype()->tp_name, Py_TYPE(o)->tp_name);
        return NULL;
    }
    if (PyJitType_Verify(type) < 0)
        return NULL;
    return type;
}

