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

#include "pyjit-value.h"

#include "pyjit-context.h"
#include "pyjit-insn.h"
#include "pyjit-function.h"
#include "pyjit-type.h"

PyDoc_STRVAR(value_doc, "Wrapper class for jit_value_t");

static PyObject *value_cache = NULL;

/* Slot implementations */

static void
value_dealloc(PyJitValue *self)
{
    if (self->weakreflist)
        PyObject_ClearWeakRefs((PyObject *)self);

    if (self->value) {
        PYJIT_BEGIN_ALLOW_EXCEPTION
        if (pyjit_weak_cache_delitem(value_cache, (long)self->value) < 0) {
            PYJIT_TRACE("this shouldn't have happened");
            abort();
        }
        PYJIT_END_ALLOW_EXCEPTION
    }

    Py_XDECREF(self->function);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

PYJIT_REPR_GENERIC(value_repr, PyJitValue, value)

PYJIT_HASH_GENERIC(value_hash, PyJitValue, PyJitValue_Verify, value)

static PyObject *
value_richcompare(PyJitValue *self, PyObject *other, int opid)
{
    PyJitFunction *jit_function;
    PyJitValue *jit_other;
    jit_function_t function;
    jit_value_t (*cmpfunc)(jit_function_t, jit_value_t, jit_value_t) = NULL;
    jit_value_t value;

    if (PyJitValue_Verify(self) < 0)
        return NULL;
    jit_function = PyJitFunction_CastAndVerify(self->function);
    if (!jit_function)
        return NULL;
    function = jit_function->function;

    jit_other = PyJitValue_CastAndVerify(other);
    if (!jit_other)
        return NULL;

    if (!function || !jit_value_get_function(jit_other->value)) {
        PyErr_SetString(
            PyExc_TypeError,
            "both values must be associated with a function");
        return NULL;
    }

    switch (opid) {
    case Py_LT:
        cmpfunc = jit_insn_lt;
        break;
    case Py_LE:
        cmpfunc = jit_insn_le;
        break;
    case Py_EQ:
        cmpfunc = jit_insn_eq;
        break;
    case Py_NE:
        cmpfunc = jit_insn_ne;
        break;
    case Py_GT:
        cmpfunc = jit_insn_gt;
        break;
    case Py_GE:
        cmpfunc = jit_insn_ge;
        break;
    default:
        break;
    }

    if (!cmpfunc) {
        PyErr_SetString(PyExc_TypeError, "invalid comparison operation");
        return NULL;
    }

    value = cmpfunc(function, self->value, jit_other->value);
    return PyJitValue_New(value, self->function);
}

/* Number protocol methods */

#define DEFINE_UNARY_NUMBER_METHOD(name)                    \
static PyObject *                                           \
value_##name(PyJitValue *self)                              \
{                                                           \
    return pyjit_insn_unary_method(                         \
        self->function, (PyObject *)self, jit_insn_##name); \
}

static jit_value_t
_value_create_constant(jit_function_t function, PyObject *o)
{
    /* TODO: Make sure to avoid overflows here. */
    jit_value_t retval = NULL;
    if (PyInt_Check(o)) {
        jit_nint value = PyInt_AS_LONG(o);
        retval = jit_value_create_nint_constant(
            function, jit_type_nint, value);
    }
    else if (PyLong_Check(o)) {
        jit_long value = PyLong_AsLong(o);
        retval = jit_value_create_long_constant(
            function, jit_type_long, value);
    }
    else if (PyFloat_Check(o)) {
        double value = PyFloat_AS_DOUBLE(o);
        retval = jit_value_create_float64_constant(
            function, jit_type_float64, value);
    }
    else {
        PyErr_Format(PyExc_TypeError,
                     "cannot marshal %.100s to numerical LibJIT constant",
                     Py_TYPE(o)->tp_name);
    }
    return retval;
}

static PyObject *
_value_binaryfunc(PyObject *a, PyObject *b, pyjit_binaryfunc binaryfunc)
{
    PyObject *op_a = a, *op_b = b, *function;
    PyJitValue *value_a, *value_b;

    value_a = PyJitValue_Cast(a);
    value_b = PyJitValue_Cast(b);

    /* XXX: We don't test the integrity of value_a and value_b here! */
    if (!value_a && !value_b) {
        return Py_NotImplemented;
    }
    else if (value_a && !value_b) {
        /* jit.Value OP obj */
        PyJitFunction *func;
        jit_value_t value;

        func = PyJitFunction_CastAndVerify(value_a->function);
        if (!func)
            return NULL;
        value = _value_create_constant(func->function, b);
        if (!value)
            return NULL;
        function = value_a->function;
        op_b = PyJitValue_New(value, function);
    }
    else if (!value_a && value_b) {
        /* obj OP jit.Value */
        PyJitFunction *func;
        jit_value_t value;

        func = PyJitFunction_CastAndVerify(value_b->function);
        if (!func)
            return NULL;
        value = _value_create_constant(func->function, a);
        if (!value)
            return NULL;
        function = value_b->function;
        op_a = PyJitValue_New(value, function);
    }
    else
        function = value_a->function;

    return pyjit_insn_binary_method(function, op_a, op_b, binaryfunc);
}

#define DEFINE_BINARY_NUMBER_METHOD(name)                   \
static PyObject *                                           \
value_##name(PyObject *self, PyObject *other)               \
{                                                           \
    return _value_binaryfunc(self, other, jit_insn_##name); \
}

DEFINE_BINARY_NUMBER_METHOD(add)
DEFINE_BINARY_NUMBER_METHOD(sub)
DEFINE_BINARY_NUMBER_METHOD(mul)
DEFINE_BINARY_NUMBER_METHOD(div)
DEFINE_BINARY_NUMBER_METHOD(rem)
DEFINE_UNARY_NUMBER_METHOD(neg)

/* For completeness' sake */
static PyObject *
value_pos(PyJitValue *self)
{
    return (PyObject *)self;
}

DEFINE_UNARY_NUMBER_METHOD(abs)
DEFINE_BINARY_NUMBER_METHOD(shl)
DEFINE_BINARY_NUMBER_METHOD(shr)
DEFINE_BINARY_NUMBER_METHOD(and)
DEFINE_BINARY_NUMBER_METHOD(or)
DEFINE_BINARY_NUMBER_METHOD(xor)
DEFINE_UNARY_NUMBER_METHOD(not)

#undef DEFINE_UNARY_NUMBER_METHOD
#undef DEFINE_BINARY_NUMBER_METHOD

static PyNumberMethods value_number_methods = {
    (binaryfunc)value_add,  /* nb_add */
    (binaryfunc)value_sub,  /* nb_subtract */
    (binaryfunc)value_mul,  /* nb_multiply */
    (binaryfunc)value_div,  /* nb_divide */
    (binaryfunc)value_rem,  /* nb_remainder */
    0,                      /* nb_divmod */
    0,                      /* nb_power */
    (unaryfunc)value_neg,   /* nb_negative */
    (unaryfunc)value_pos,   /* nb_positive */
    (unaryfunc)value_abs,   /* nb_absolute */
    0,                      /* nb_nonzero */
    (unaryfunc)value_not,   /* nb_invert */
    (binaryfunc)value_shl,  /* nb_lshift */
    (binaryfunc)value_shr,  /* nb_rshift */
    (binaryfunc)value_and,  /* nb_and */
    (binaryfunc)value_xor,  /* nb_xor */
    (binaryfunc)value_or    /* nb_or */
};

/* Regular methods */

static int
check_func_and_type(PyObject *func, PyObject *type)
{
    if (!PyJitFunction_Check(func)) {
        PyErr_Format(PyExc_TypeError,
                     "func must be of type %.100s, not %.100s",
                     pyjit_function_get_pytype()->tp_name,
                     Py_TYPE(func)->tp_name);
        return -1;
    }

    if (!PyJitType_Check(type)) {
        PyErr_Format(PyExc_TypeError,
                     "type_ must be of type %.100s, not %.100s",
                     pyjit_type_get_pytype()->tp_name, Py_TYPE(type)->tp_name);
        return -1;
    }

    return 0;
}

static PyObject *
value_create(void *null, PyObject *args, PyObject *kwargs)
{
    PyObject *func = NULL, *type = NULL;
    jit_value_t value;
    static char *kwlist[] = { "func", "type_", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OO:Value", kwlist, &func,
                                     &type))
        return NULL;

    if (check_func_and_type(func, type) < 0)
        return NULL;

    value = jit_value_create(((PyJitFunction *)func)->function,
                             ((PyJitType *)type)->type);
    return PyJitValue_New(value, func);
}

static PyObject *
value_create_nint_constant(void *null, PyObject *args, PyObject *kwargs)
{
    PyObject *func = NULL, *type = NULL;
    jit_nint const_value;
    jit_value_t value;
    static char *kwlist[] = { "func", "type_", "const_value", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OOl:Value", kwlist, &func,
                                     &type, &const_value))
        return NULL;

    if (check_func_and_type(func, type) < 0)
        return NULL;

    value = jit_value_create_nint_constant(
        ((PyJitFunction *)func)->function, ((PyJitType *)type)->type,
        const_value);
    return PyJitValue_New(value, func);
}

static PyObject *
value_create_float32_constant(void *null, PyObject *args, PyObject *kwargs)
{
    PyObject *func = NULL, *type = NULL;
    jit_float32 const_value;
    jit_value_t value;
    static char *kwlist[] = { "func", "type_", "const_value", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OOf:Value", kwlist, &func,
                                     &type, &const_value))
        return NULL;

    if (check_func_and_type(func, type) < 0)
        return NULL;

    value = jit_value_create_float32_constant(
        ((PyJitFunction *)func)->function, ((PyJitType *)type)->type,
        const_value);
    return PyJitValue_New(value, func);
}

static PyObject *
value_create_float64_constant(void *null, PyObject *args, PyObject *kwargs)
{
    PyObject *func = NULL, *type = NULL;
    jit_float32 const_value;
    jit_value_t value;
    static char *kwlist[] = { "func", "type_", "const_value", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OOf:Value", kwlist, &func,
                                     &type, &const_value))
        return NULL;

    if (check_func_and_type(func, type) < 0)
        return NULL;

    value = jit_value_create_float64_constant(
        ((PyJitFunction *)func)->function, ((PyJitType *)type)->type,
        const_value);
    return PyJitValue_New(value, func);
}

static PyObject *
value_get_param(void *null, PyObject *args, PyObject *kwargs)
{
    unsigned int param, num_params;
    PyObject *func = NULL;
    PyJitFunction *jit_function;
    jit_value_t value;
    static char *kwlist[] = { "func", "param", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OI:Value", kwlist, &func,
                                     &param))
        return NULL;

    jit_function = PyJitFunction_Cast(func);
    if (!jit_function) {
        pyjit_raise_type_error("func", pyjit_function_get_pytype(), func);
        return NULL;
    }
    if (!jit_function->function) {
        PyErr_SetString(PyExc_ValueError, "func is not initialized");
        return NULL;
    }

    /* FIXME: libjit doesn't perform range checks in jit_value_get_param(), so
     *        we have to do it manually here. A patch is available upstream.
     *        Remove this check if/once it has been merged.
     */
    num_params = jit_type_num_params(
        jit_function_get_signature(jit_function->function));
    if (param >= num_params) {
        PyErr_SetString(PyExc_IndexError, "invalid parameter index");
        return NULL;
    }
    value = jit_value_get_param(jit_function->function, param);
    if (!value) {
        PyErr_SetString(PyExc_ValueError, "invalid parameter index");
        return NULL;
    }
    return PyJitValue_New(value, func);
}

static PyObject *
value_get_struct_pointer(void *null, PyObject *args, PyObject *kwargs)
{
    PyObject *func = NULL;
    jit_value_t struct_pointer;
    static char *kwlist[] = { "func", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O:Value", kwlist, &func))
        return NULL;

    if (!PyJitFunction_Check(func)) {
        PyErr_Format(PyExc_TypeError,
                     "func must be of type %.100s, not %.100s",
                     pyjit_function_get_pytype()->tp_name,
                     Py_TYPE(func)->tp_name);
        return NULL;
    }

    struct_pointer = jit_value_get_struct_pointer(
        ((PyJitFunction *)func)->function);
    if (struct_pointer)
        return PyJitValue_New(struct_pointer, func);
    Py_RETURN_NONE;
}

#define DEFINE_BOOLEAN_METHOD(name)                         \
static PyObject *                                           \
value_##name(PyJitValue *self)                              \
{                                                           \
    return PyBool_FromLong(jit_value_##name(self->value));  \
}

DEFINE_BOOLEAN_METHOD(is_temporary)
DEFINE_BOOLEAN_METHOD(is_local)
DEFINE_BOOLEAN_METHOD(is_constant)
DEFINE_BOOLEAN_METHOD(is_parameter)

static PyObject *
value_ref(void *null, PyObject *args, PyObject *kwargs)
{
    PyObject *func = NULL, *value = NULL;
    static char *kwlist[] = { "func", "value", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OO:Value", kwlist, &func,
                                     &value))
        return NULL;

    if (!PyJitValue_Check(value)) {
        PyErr_Format(PyExc_TypeError,
                     "value must be of type %.100s, not %.100s",
                     pyjit_value_get_pytype()->tp_name,
                     Py_TYPE(value)->tp_name);
        return NULL;
    }

    jit_value_ref(((PyJitFunction *)func)->function,
                  ((PyJitValue *)value)->value);
    Py_RETURN_NONE;
}

static PyObject *
value_set_volatile(PyJitValue *self)
{
    jit_value_set_volatile(self->value);
    Py_RETURN_NONE;
}

DEFINE_BOOLEAN_METHOD(is_volatile)

static PyObject *
value_set_addressable(PyJitValue *self)
{
    jit_value_set_addressable(self->value);
    Py_RETURN_NONE;
}

DEFINE_BOOLEAN_METHOD(is_addressable)

static PyObject *
value_get_type(PyJitValue *self)
{
    return PyJitType_New(jit_value_get_type(self->value));
}

static PyObject *
value_get_function(PyJitValue *self)
{
    return PyJitFunction_New(jit_value_get_function(self->value));
}

static PyObject *
value_get_context(PyJitValue *self)
{
    return PyJitContext_New(jit_value_get_context(self->value));
}

static PyObject *
value_get_nint_constant(PyJitValue *self)
{
    return PyLong_FromLong(jit_value_get_nint_constant(self->value));
}

static PyObject *
value_get_long_constant(PyJitValue *self)
{
    return PyLong_FromLong(jit_value_get_long_constant(self->value));
}

static PyObject *
value_get_float32_constant(PyJitValue *self)
{
    return PyFloat_FromDouble(jit_value_get_float32_constant(self->value));
}

static PyObject *
value_get_float64_constant(PyJitValue *self)
{
    return PyFloat_FromDouble(jit_value_get_float64_constant(self->value));
}

DEFINE_BOOLEAN_METHOD(is_true)

#undef DEFINE_BOOLEAN_METHOD

static PyMethodDef value_methods[] = {
    /* XXX: Implementing the functionality of `create' in the constructor of
     *      jit.Value would make more sense (see jit.Function).
     */
    PYJIT_STATIC_METHOD_KW(value, create),
    PYJIT_STATIC_METHOD_KW(value, create_nint_constant),
    /* XXX: long and jit_nint might not always be identical. */
    PYJIT_METHOD_EX("create_long_constant", value_create_nint_constant,
                    METH_STATIC | METH_KEYWORDS),
    PYJIT_STATIC_METHOD_KW(value, create_float32_constant),
    PYJIT_STATIC_METHOD_KW(value, create_float64_constant),
    /* XXX: Python doesn't support long doubles */
    /* jit_value_create_nfloat_constant */
    /* TODO: Requires jit.Constant */
    /* jit_value_create_constant */
    PYJIT_STATIC_METHOD_KW(value, get_param),
    PYJIT_STATIC_METHOD_KW(value, get_struct_pointer),
    PYJIT_METHOD_NOARGS(value, is_temporary),
    PYJIT_METHOD_NOARGS(value, is_local),
    PYJIT_METHOD_NOARGS(value, is_constant),
    PYJIT_METHOD_NOARGS(value, is_parameter),
    PYJIT_STATIC_METHOD_KW(value, ref),
    PYJIT_METHOD_NOARGS(value, set_volatile),
    PYJIT_METHOD_NOARGS(value, is_volatile),
    PYJIT_METHOD_NOARGS(value, set_addressable),
    PYJIT_METHOD_NOARGS(value, is_addressable),
    PYJIT_METHOD_NOARGS(value, get_type),
    PYJIT_METHOD_NOARGS(value, get_function),
    /* TODO: Requires jit.Block */
    /* jit_value_get_block */
    PYJIT_METHOD_NOARGS(value, get_context),
    /* TODO: Requires jit.Constant */
    /* jit_value_get_constant */
    PYJIT_METHOD_NOARGS(value, get_nint_constant),
    PYJIT_METHOD_NOARGS(value, get_long_constant),
    PYJIT_METHOD_NOARGS(value, get_float32_constant),
    PYJIT_METHOD_NOARGS(value, get_float64_constant),
    PYJIT_METHOD_NOARGS(value, get_nint_constant),
    /* XXX: See above. */
    /* jit_value_get_nfloat_constant */
    PYJIT_METHOD_NOARGS(value, is_true),
    { NULL } /* Sentinel */
};

static PyTypeObject PyJitValue_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                                  /* ob_size */
    "jit.Value",                        /* tp_name */
    sizeof(PyJitValue),                 /* tp_basicsize */
    0,                                  /* tp_itemsize */
    (destructor)value_dealloc,          /* tp_dealloc */
    0,                                  /* tp_print */
    0,                                  /* tp_getattr */
    0,                                  /* tp_setattr */
    0,                                  /* tp_compare */
    (reprfunc)value_repr,               /* tp_repr */
    &value_number_methods,              /* tp_as_number */
    0,                                  /* tp_as_sequence */
    0,                                  /* tp_as_mapping */
    (hashfunc)value_hash,               /* tp_hash */
    0,                                  /* tp_call */
    0,                                  /* tp_str */
    0,                                  /* tp_getattro */
    0,                                  /* tp_setattro */
    0,                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT |
        Py_TPFLAGS_CHECKTYPES,          /* tp_flags */
    value_doc,                          /* tp_doc */
    0,                                  /* tp_traverse */
    0,                                  /* tp_clear */
    (richcmpfunc)value_richcompare,     /* tp_richcompare */
    offsetof(PyJitValue, weakreflist),  /* tp_weaklistoffset */
    0,                                  /* tp_iter */
    0,                                  /* tp_iternext */
    value_methods                       /* tp_methods */
};

const PyTypeObject *
pyjit_value_get_pytype(void)
{
    return &PyJitValue_Type;
}

int
pyjit_value_init(PyObject *module)
{
    if (PyType_Ready(&PyJitValue_Type) < 0)
        return -1;

    /* Set up the value cache. */
    value_cache = PyDict_New();
    if (!value_cache)
        return -1;

    Py_INCREF(&PyJitValue_Type);
    PyModule_AddObject(module, "Value", (PyObject *)&PyJitValue_Type);

    return 0;
}

static PyObject *
_value_new(PyTypeObject *type, jit_value_t value, PyObject *function)
{
    PyObject *object;
    PyJitValue *jit_value;

    object = PyType_GenericNew(type, NULL, NULL);
    if (!object)
        return NULL;
    jit_value = (PyJitValue *)object;
    jit_value->value = value;
    Py_INCREF(function);
    jit_value->function = function;

    /* Cache the value. */
    if (pyjit_weak_cache_setitem(value_cache, (long)value, object) < 0) {
        Py_DECREF(object);
        return NULL;
    }
    return object;
}

PyObject *
PyJitValue_New(jit_value_t value, PyObject *function)
{
    long numkey;
    PyObject *object;

    numkey = (long)value;
    object = pyjit_weak_cache_getitem(value_cache, numkey);
    if (!object)
        object = _value_new(&PyJitValue_Type, value, function);
    return object;
}

int
PyJitValue_Check(PyObject *o)
{
    return PyObject_IsInstance(o, (PyObject *)&PyJitValue_Type);
}

PyJitValue *
PyJitValue_Cast(PyObject *o)
{
    int r = PyJitValue_Check(o);
    if (r == 1)
        return (PyJitValue *)o;
    else if (r < 0 && PyErr_Occurred())
        PyErr_Clear();
    return NULL;
}

int
PyJitValue_Verify(PyJitValue *o)
{
    if (!o->function || !o->value) {
        PyErr_SetString(PyExc_ValueError, "value is not initialized");
        return -1;
    }
    return 0;
}

PyJitValue *
PyJitValue_CastAndVerify(PyObject *o)
{
    PyJitValue *value = PyJitValue_Cast(o);
    if (!value) {
        PyErr_Format(
            PyExc_TypeError, "expected instance of %.100s, not %.100s",
            pyjit_value_get_pytype()->tp_name, Py_TYPE(o)->tp_name);
        return NULL;
    }
    if (PyJitValue_Verify(value) < 0)
        return NULL;
    return value;
}

