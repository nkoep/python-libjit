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

#include "pyjit-function.h"

#include "pyjit-context.h"
#include "pyjit-insn.h"
#include "pyjit-marshal.h"
#include "pyjit-type.h"
#include "pyjit-value.h"

PyDoc_STRVAR(function_doc, "Wrapper class for jit_function_t");

static PyObject *function_cache = NULL;

/* Slot implementations */

static void
function_dealloc(PyJitFunction *self)
{
    if (self->weakreflist)
        PyObject_ClearWeakRefs((PyObject *)self);

    if (self->function) {
        PYJIT_BEGIN_ALLOW_EXCEPTION
        if (pyjit_weak_cache_delitem(function_cache,
                                     (long)self->function) < 0) {
            PYJIT_TRACE("this shouldn't have happened");
            abort();
        }
        PYJIT_END_ALLOW_EXCEPTION

        jit_function_abandon(self->function);
    }

    Py_XDECREF(self->context);
    Py_XDECREF(self->signature);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

PYJIT_REPR_GENERIC(function_repr, PyJitFunction, function)

PYJIT_HASH_GENERIC(function_hash, PyJitFunction, PyJitFunction_Verify,
                   function)

/* Forward */
static PyObject *function_compile(PyJitFunction *self);
static PyObject *function_apply(
    PyJitFunction *self, PyObject *args, PyObject *kwargs);

static PyObject *
function_call(PyJitFunction *self, PyObject *args, PyObject *kwargs)
{
    PyObject *args_, *retval;

    if (kwargs != NULL) {
        if (PyDict_Check(kwargs) && PyDict_Size(kwargs) > 0) {
            PyErr_SetString(PyExc_TypeError,
                            "function does not accept keyword arguments");
            return NULL;
        }
    }

    if (PyJitFunction_Verify(self) < 0)
        return NULL;

    if (!function_compile(self))
        return NULL;

    /* Wrap the argument tuple in another tuple so that the argument to
     * jit.Function.apply_ appears to be a sequence once args_ gets unpacked by
     * the callee.
     */
    args_ = Py_BuildValue("(O)", args);
    retval = function_apply(self, args_, NULL);
    Py_DECREF(args_);
    return retval;
}

/* TODO: Write a helper function to cast and verify that objects are properly
 *       initialized and raise exceptions if not to reduce the boilerplate code
 *       we have to write for every function.
 */
static int
function_init(PyJitFunction *self, PyObject *args, PyObject *kwargs)
{
    PyObject *context = NULL, *signature = NULL, *parent = NULL;
    PyJitContext *jit_context;
    PyJitType *jit_signature;
    static char *kwlist[] = { "context", "signature", "parent", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OO|O:Function", kwlist,
                                     &context, &signature, &parent))
        return -1;

    jit_context = PyJitContext_Cast(context);
    if (!jit_context) {
        pyjit_raise_type_error("context", pyjit_context_get_pytype(), context);
        return -1;
    }

    jit_signature = PyJitType_Cast(signature);
    if (!jit_signature) {
        pyjit_raise_type_error("signature", pyjit_type_get_pytype(),
                               signature);
        return -1;
    }

    if (parent) {
        PyJitFunction *jit_parent = PyJitFunction_Cast(parent);
        if (!jit_parent) {
            pyjit_raise_type_error("parent", pyjit_function_get_pytype(),
                                   parent);
            return -1;
        }
        self->function = jit_function_create_nested(
            jit_context->context, jit_signature->type, jit_parent->function);
    }
    else {
        self->function = jit_function_create(
            jit_context->context, jit_signature->type);
    }

    Py_INCREF(context);
    self->context = context;
    Py_INCREF(signature);
    self->signature = signature;

    /* Cache the function. */
    return pyjit_weak_cache_setitem(
        function_cache, (long)self->function, (PyObject *)self);
}

/* Regular methods */

static PyObject *
function_get_context(PyJitFunction *self)
{
    jit_context_t context = jit_function_get_context(self->function);
    if (context)
        return PyJitContext_New(context);
    Py_RETURN_NONE;
}

static PyObject *
function_get_signature(PyJitFunction *self)
{
    jit_type_t signature = jit_function_get_signature(self->function);
    if (signature)
        return PyJitType_New(signature);
    Py_RETURN_NONE;
}

static PyObject *
function_set_meta(PyJitFunction *self, PyObject *args, PyObject *kwargs)
{
    int type, retval;
    PyObject *data = NULL, *build_only = NULL;
    static char *kwlist[] = { "type_", "data", "build_only", NULL };

    if (PyJitFunction_Verify(self) < 0)
        return NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "iOO:Function", kwlist,
                                     &type, &data, &build_only))
        return NULL;

    Py_INCREF(data);
    retval = jit_function_set_meta(
        self->function, type, data, pyjit_meta_free_func,
        PyObject_IsTrue(build_only));
    return PyBool_FromLong(retval);
}

static int
_function_get_meta_type(
    PyJitFunction *self, PyObject *args, PyObject *kwargs, int *type)
{
    static char *kwlist[] = { "type_", NULL };

    if (PyJitFunction_Verify(self) < 0)
        return -1;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "i:Function", kwlist, type))
        return -1;

    return 0;
}

static PyObject *
function_get_meta(PyJitFunction *self, PyObject *args, PyObject *kwargs)
{
    int type;
    PyObject *retval;

    if (_function_get_meta_type(self, args, kwargs, &type) < 0)
        return NULL;

    retval = (PyObject *)jit_function_get_meta(self->function, type);
    if (retval) {
        Py_INCREF(retval);
        return retval;
    }
    Py_RETURN_NONE;
}

static PyObject *
function_free_meta(PyJitFunction *self, PyObject *args, PyObject *kwargs)
{
    int type;

    if (_function_get_meta_type(self, args, kwargs, &type) < 0)
        return NULL;

    jit_function_free_meta(self->function, type);
    Py_RETURN_NONE;
}

static PyObject *
_function_iter(
    PyObject *args, PyObject *kwargs,
    jit_function_t (*iterfunc)(jit_context_t, jit_function_t))
{
    PyObject *context = NULL, *prev = NULL;
    PyJitContext *jit_context;
    jit_function_t jit_prev = NULL, function;
    static char *kwlist[] = { "context", "prev", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|O:Function", kwlist,
                                     &context, &prev))
        return NULL;

    jit_context = PyJitContext_CastAndVerify(context);
    if (!jit_context)
        return NULL;

    if (prev && prev != Py_None) {
        PyJitFunction *jit_function = PyJitFunction_CastAndVerify(prev);
        if (!jit_function)
            return NULL;
        jit_prev = jit_function->function;
    }

    function = iterfunc(jit_context->context, jit_prev);
    if (function)
        return PyJitFunction_New(function);
    Py_RETURN_NONE;
}

static PyObject *
function_next(void *null, PyObject *args, PyObject *kwargs)
{
    return _function_iter(args, kwargs, jit_function_next);
}

static PyObject *
function_previous(void *null, PyObject *args, PyObject *kwargs)
{
    return _function_iter(args, kwargs, jit_function_previous);
}

/* ... */

static PyObject *
function_get_nested_parent(PyJitFunction *self)
{
    jit_function_t parent;

    if (PyJitFunction_Verify(self) < 0)
        return NULL;
    parent = jit_function_get_nested_parent(self->function);
    if (parent)
        return PyJitFunction_New(parent);
    Py_RETURN_NONE;
}

static PyObject *
function_is_compiled(PyJitFunction *self)
{
    if (PyJitFunction_Verify(self) < 0)
        return NULL;
    return PyBool_FromLong(jit_function_is_compiled(self->function));
}

/* ... */

static PyObject *
function_to_closure(PyJitFunction *self)
{
    void *closure;

    if (PyJitFunction_Verify(self) < 0)
        return NULL;
    if (!jit_supports_closures()) {
        PyErr_SetString(PyExc_RuntimeError,
                        "closures are not supported on this platform");
        return NULL;
    }
    closure = jit_function_to_closure(self->function);
    if (closure)
        return PyLong_FromVoidPtr(closure);
    Py_RETURN_NONE;
}

/* ... */

static PyObject *
function_compile(PyJitFunction *self)
{
    if (PyJitFunction_Verify(self) < 0)
        return NULL;

    if (!jit_function_compile(self->function)) {
        PyErr_SetString(PyExc_RuntimeError, "failed to compile function");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
function_apply(PyJitFunction *self, PyObject *args, PyObject *kwargs)
{
    PyObject *args_ = NULL, *retval = NULL;
    int r;
    unsigned int num_params, num_params_given;
    void **jit_args = NULL, *return_area = NULL;
    jit_type_t signature, return_type;
    static char *kwlist[] = { "args", NULL };

    if (PyJitFunction_Verify(self) < 0)
        return NULL;

    if (!jit_function_is_compiled(self->function)) {
        PyErr_SetString(PyExc_ValueError, "function is not compiled");
        return NULL;
    }

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O:Function", kwlist,
                                     &args_))
        return NULL;

    r = PySequence_Check(args_);
    if (r < 0) {
        return NULL;
    }
    else if (r == 0) {
        PyErr_Format(PyExc_TypeError, "args must be a sequence, not %.100s",
                     Py_TYPE(args_)->tp_name);
        return NULL;
    }

    signature = jit_function_get_signature(self->function);
    num_params = jit_type_num_params(signature);
    num_params_given = (unsigned int)PySequence_Length(args_);
    if (num_params != num_params_given) {
        PyErr_Format(PyExc_TypeError, "function expected %u arguments, got %u",
                     num_params, num_params_given);
        return NULL;
    }

    /* Marshal Python arguments to appropriate C types. */
    if (pyjit_marshal_arg_list_from_py(args_, signature, &jit_args) < 0)
        return NULL;

    /* Allocate space for the return value. */
    return_type = jit_type_get_return(signature);
    return_area = PyMem_Malloc(jit_type_get_size(return_type));

    if (!jit_function_apply(self->function, jit_args, return_area)) {
        PyErr_SetString(PyExc_RuntimeError, "failed to apply function");
    }
    else {
        /* This may raise an exception, but at this point the failure and
         * success paths are identical anyway so we don't have to explicitly
         * check `retval'.
         */
        retval = pyjit_marshal_arg_to_py(return_type, return_area);
    }

    pyjit_marshal_free_arg_list(jit_args, num_params);
    PyMem_Free(return_area);

    return retval;
}

/* Re-exported methods of jit.Value */
static PyObject *
function_value_get_param(PyJitFunction *self, PyObject *args, PyObject *kwargs)
{
    unsigned int param, num_params;
    jit_value_t value;
    static char *kwlist[] = { "param", NULL };

    if (PyJitFunction_Verify(self) < 0)
        return NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "I:Function", kwlist,
                                     &param))
        return NULL;

    /* FIXME: libjit doesn't perform range checks in jit_value_get_param(), so
     *        we have to do it manually here. A patch is available upstream.
     *        Remove this check if/once it has been merged.
     */
    num_params = jit_type_num_params(
        jit_function_get_signature(self->function));
    if (param >= num_params) {
        PyErr_SetString(PyExc_IndexError, "invalid parameter index");
        return NULL;
    }
    value = jit_value_get_param(self->function, param);
    if (!value) {
        PyErr_SetString(PyExc_ValueError, "invalid parameter index");
        return NULL;
    }
    return PyJitValue_New(value, (PyObject *)self);
}

/* Re-exported methods of jit.Insn */
static PyObject *
_function_insn_unaryfunc(PyJitFunction *self, PyObject *args, PyObject *kwargs,
                         pyjit_unaryfunc unaryfunc)
{
    PyObject *value1 = NULL;
    static char *kwlist[] = { "value1", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O:Function", kwlist,
                                     &value1))
        return NULL;
    return pyjit_insn_unary_method((PyObject *)self, value1, unaryfunc);
}

#define DEFINE_UNARY_METHOD(name)                                           \
static PyObject *                                                           \
function_insn_##name(PyJitFunction *self, PyObject *args, PyObject *kwargs) \
{                                                                           \
    return _function_insn_unaryfunc(self, args, kwargs, jit_insn_##name);   \
}

static PyObject *
_function_insn_binaryfunc(PyJitFunction *self, PyObject *args,
                          PyObject *kwargs, pyjit_binaryfunc binaryfunc)
{
    PyObject *value1 = NULL, *value2 = NULL;
    static char *kwlist[] = { "value1", "value2", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OO:Function", kwlist,
                                     &value1, &value2))
        return NULL;
    return pyjit_insn_binary_method((PyObject *)self, value1, value2,
                                    binaryfunc);
}

#define DEFINE_BINARY_METHOD(name)                                          \
static PyObject *                                                           \
function_insn_##name(PyJitFunction *self, PyObject *args, PyObject *kwargs) \
{                                                                           \
    return _function_insn_binaryfunc(self, args, kwargs, jit_insn_##name);  \
}

DEFINE_BINARY_METHOD(add)
DEFINE_BINARY_METHOD(add_ovf)
DEFINE_BINARY_METHOD(sub)
DEFINE_BINARY_METHOD(sub_ovf)
DEFINE_BINARY_METHOD(mul)
DEFINE_BINARY_METHOD(mul_ovf)
DEFINE_BINARY_METHOD(div)
DEFINE_BINARY_METHOD(rem)
DEFINE_BINARY_METHOD(rem_ieee)
DEFINE_UNARY_METHOD(neg)
DEFINE_BINARY_METHOD(and)
DEFINE_BINARY_METHOD(or)
DEFINE_BINARY_METHOD(xor)
DEFINE_UNARY_METHOD(not)
DEFINE_BINARY_METHOD(shl)
DEFINE_BINARY_METHOD(shr)
DEFINE_BINARY_METHOD(ushr)
DEFINE_BINARY_METHOD(sshr)
DEFINE_BINARY_METHOD(eq)
DEFINE_BINARY_METHOD(ne)
DEFINE_BINARY_METHOD(lt)
DEFINE_BINARY_METHOD(le)
DEFINE_BINARY_METHOD(gt)
DEFINE_BINARY_METHOD(ge)
DEFINE_BINARY_METHOD(cmpl)
DEFINE_BINARY_METHOD(cmpg)
DEFINE_UNARY_METHOD(to_bool)
DEFINE_UNARY_METHOD(to_not_bool)
DEFINE_UNARY_METHOD(acos)
DEFINE_UNARY_METHOD(asin)
DEFINE_UNARY_METHOD(atan)
DEFINE_BINARY_METHOD(atan2)
DEFINE_UNARY_METHOD(cos)
DEFINE_UNARY_METHOD(cosh)
DEFINE_UNARY_METHOD(exp)
DEFINE_UNARY_METHOD(log)
DEFINE_UNARY_METHOD(log10)
DEFINE_BINARY_METHOD(pow)
DEFINE_UNARY_METHOD(sin)
DEFINE_UNARY_METHOD(sinh)
DEFINE_UNARY_METHOD(sqrt)
DEFINE_UNARY_METHOD(tan)
DEFINE_UNARY_METHOD(tanh)
DEFINE_UNARY_METHOD(ceil)
DEFINE_UNARY_METHOD(floor)
DEFINE_UNARY_METHOD(rint)
DEFINE_UNARY_METHOD(round)
DEFINE_UNARY_METHOD(trunc)
DEFINE_UNARY_METHOD(is_nan)
DEFINE_UNARY_METHOD(is_finite)
DEFINE_UNARY_METHOD(is_inf)
DEFINE_UNARY_METHOD(abs)
DEFINE_BINARY_METHOD(min)
DEFINE_BINARY_METHOD(max)
DEFINE_UNARY_METHOD(sign)

#undef DEFINE_UNARY_METHOD
#undef DEFINE_BINARY_METHOD

static PyObject *
function_insn_return(PyJitFunction *self, PyObject *args, PyObject *kwargs)
{
    PyObject *value = NULL;
    jit_value_t jit_value;
    static char *kwlist[] = { "value", NULL };

    if (PyJitFunction_Verify(self) < 0)
        return NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O:Function", kwlist,
                                     &value))
        return NULL;
    if (value == Py_None)
        jit_value = NULL;
    else {
        PyJitValue *pyjit_value = PyJitValue_Cast(value);
        if (!pyjit_value) {
            pyjit_raise_type_error("value", pyjit_value_get_pytype(), value);
            return NULL;
        }
        jit_value = pyjit_value->value;
    }

    return PyBool_FromLong(jit_insn_return(self->function, jit_value));
}

static PyMethodDef function_methods[] = {
    PYJIT_METHOD_NOARGS(function, get_context),
    PYJIT_METHOD_NOARGS(function, get_signature),
    PYJIT_METHOD_KW(function, set_meta),
    PYJIT_METHOD_KW(function, get_meta),
    PYJIT_METHOD_KW(function, free_meta),
    PYJIT_METHOD_EX("next_", function_next, METH_STATIC | METH_KEYWORDS),
    PYJIT_STATIC_METHOD_KW(function, previous),
    /* jit_function_get_entry */
    /* jit_function_get_current */
    PYJIT_METHOD_NOARGS(function, get_nested_parent),
    PYJIT_METHOD_NOARGS(function, is_compiled),
    /* jit_function_set_recompilable */
    /* jit_function_clear_recompilable */
    /* jit_function_is_recompilable */
    PYJIT_METHOD_NOARGS(function, to_closure),
    /* jit_function_from_closure */
    /* jit_function_from_pc */
    /* jit_function_to_vtable_pointer */
    /* jit_function_from_vtable_pointer */
    /* jit_function_set_on_demand_compiler */
    /* jit_function_get_on_demand_compiler */
    PYJIT_METHOD_EX("apply_", function_apply, METH_KEYWORDS),
    /* jit_function_apply_vararg */
    /* jit_function_set_optimization_level */
    /* jit_function_get_optimization_level */
    /* jit_function_get_max_optimization_level */
    /* jit_function_reserve_label */
    /* jit_function_labels_equal */
    /* jit_optimize */
    /* jit_compile */
    /* jit_compile_entry */
    /* jit_function_setup_entry */
    PYJIT_METHOD_EX("compile_", function_compile, METH_NOARGS),
    /* jit_function_compile_entry */

    /* Re-exported methods originally belonging in jit.Value */
    /* jit_value_create */
    /* jit_value_create_nint_constant */
    /* jit_value_create_long_constant */
    /* jit_value_create_float32_constant */
    /* jit_value_create_float64_constant */
    /* jit_value_create_nfloat_constant */
    /* jit_value_create_constant */
    PYJIT_METHOD_KW(function, value_get_param),
    /* jit_value_get_struct_pointer */
    /* jit_value_ref */

    /* Re-exported methods originally belonging in jit.Insn */
    PYJIT_METHOD_KW(function, insn_add),
    PYJIT_METHOD_KW(function, insn_add_ovf),
    PYJIT_METHOD_KW(function, insn_sub),
    PYJIT_METHOD_KW(function, insn_sub_ovf),
    PYJIT_METHOD_KW(function, insn_mul),
    PYJIT_METHOD_KW(function, insn_mul_ovf),
    PYJIT_METHOD_KW(function, insn_div),
    PYJIT_METHOD_KW(function, insn_rem),
    PYJIT_METHOD_KW(function, insn_rem_ieee),
    PYJIT_METHOD_KW(function, insn_neg),
    PYJIT_METHOD_KW(function, insn_and),
    PYJIT_METHOD_KW(function, insn_or),
    PYJIT_METHOD_KW(function, insn_xor),
    PYJIT_METHOD_KW(function, insn_not),
    PYJIT_METHOD_KW(function, insn_shl),
    PYJIT_METHOD_KW(function, insn_shr),
    PYJIT_METHOD_KW(function, insn_ushr),
    PYJIT_METHOD_KW(function, insn_sshr),
    PYJIT_METHOD_KW(function, insn_eq),
    PYJIT_METHOD_KW(function, insn_ne),
    PYJIT_METHOD_KW(function, insn_lt),
    PYJIT_METHOD_KW(function, insn_le),
    PYJIT_METHOD_KW(function, insn_gt),
    PYJIT_METHOD_KW(function, insn_ge),
    PYJIT_METHOD_KW(function, insn_cmpl),
    PYJIT_METHOD_KW(function, insn_cmpg),
    PYJIT_METHOD_KW(function, insn_to_bool),
    PYJIT_METHOD_KW(function, insn_to_not_bool),
    PYJIT_METHOD_KW(function, insn_acos),
    PYJIT_METHOD_KW(function, insn_asin),
    PYJIT_METHOD_KW(function, insn_atan),
    PYJIT_METHOD_KW(function, insn_atan2),
    PYJIT_METHOD_KW(function, insn_cos),
    PYJIT_METHOD_KW(function, insn_cosh),
    PYJIT_METHOD_KW(function, insn_exp),
    PYJIT_METHOD_KW(function, insn_log),
    PYJIT_METHOD_KW(function, insn_log10),
    PYJIT_METHOD_KW(function, insn_pow),
    PYJIT_METHOD_KW(function, insn_sin),
    PYJIT_METHOD_KW(function, insn_sinh),
    PYJIT_METHOD_KW(function, insn_sqrt),
    PYJIT_METHOD_KW(function, insn_tan),
    PYJIT_METHOD_KW(function, insn_tanh),
    PYJIT_METHOD_KW(function, insn_ceil),
    PYJIT_METHOD_KW(function, insn_floor),
    PYJIT_METHOD_KW(function, insn_rint),
    PYJIT_METHOD_KW(function, insn_round),
    PYJIT_METHOD_KW(function, insn_trunc),
    PYJIT_METHOD_KW(function, insn_is_nan),
    PYJIT_METHOD_KW(function, insn_is_finite),
    PYJIT_METHOD_KW(function, insn_is_inf),
    PYJIT_METHOD_KW(function, insn_abs),
    PYJIT_METHOD_KW(function, insn_min),
    PYJIT_METHOD_KW(function, insn_max),
    PYJIT_METHOD_KW(function, insn_sign),
    /* ... */
    PYJIT_METHOD_KW(function, insn_return),
    /* ... */
    { NULL } /* Sentinel */
};

static PyTypeObject PyJitFunction_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                                      /* ob_size */
    "jit.Function",                         /* tp_name */
    sizeof(PyJitFunction),                  /* tp_basicsize */
    0,                                      /* tp_itemsize */
    (destructor)function_dealloc,           /* tp_dealloc */
    0,                                      /* tp_print */
    0,                                      /* tp_getattr */
    0,                                      /* tp_setattr */
    0,                                      /* tp_compare */
    (reprfunc)function_repr,                /* tp_repr */
    0,                                      /* tp_as_number */
    0,                                      /* tp_as_sequence */
    0,                                      /* tp_as_mapping */
    (hashfunc)function_hash,                /* tp_hash */
    (ternaryfunc)function_call,             /* tp_call */
    0,                                      /* tp_str */
    0,                                      /* tp_getattro */
    0,                                      /* tp_setattro */
    0,                                      /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT |
        Py_TPFLAGS_BASETYPE,                /* tp_flags */
    function_doc,                           /* tp_doc */
    0,                                      /* tp_traverse */
    0,                                      /* tp_clear */
    0,                                      /* tp_richcompare */
    offsetof(PyJitFunction, weakreflist),   /* tp_weaklistoffset */
    0,                                      /* tp_iter */
    0,                                      /* tp_iternext */
    function_methods,                       /* tp_methods */
    0,                                      /* tp_members */
    0,                                      /* tp_getset */
    0,                                      /* tp_base */
    0,                                      /* tp_dict */
    0,                                      /* tp_descr_get */
    0,                                      /* tp_descr_set */
    0,                                      /* tp_dictoffset */
    (initproc)function_init,                /* tp_init */
    0,                                      /* tp_alloc */
    PyType_GenericNew                       /* tp_new */
};

int
pyjit_function_init(PyObject *module)
{
    if (PyType_Ready(&PyJitFunction_Type) < 0)
        return -1;

    /* Set up the function cache. */
    function_cache = PyDict_New();
    if (!function_cache)
        return -1;

    Py_INCREF(&PyJitFunction_Type);
    PyModule_AddObject(module, "Function", (PyObject *)&PyJitFunction_Type);

    return 0;
}

const PyTypeObject *
pyjit_function_get_pytype(void)
{
    return &PyJitFunction_Type;
}

PyObject *
PyJitFunction_New(jit_function_t function)
{
    /* This function doesn't really create new function wrappers. Instead, it
     * only returns cached instances which means cache misses indicate bugs.
     */
    long numkey;
    PyObject *object;

    numkey = (long)function;
    object = pyjit_weak_cache_getitem(function_cache, numkey);
    if (!object)
        PyErr_SetString(PyExc_RuntimeError, "function not yet cached");
    return object;
}

int
PyJitFunction_Check(PyObject *o)
{
    return PyObject_IsInstance(o, (PyObject *)&PyJitFunction_Type);
}

PyJitFunction *
PyJitFunction_Cast(PyObject *o)
{
    int r = PyJitFunction_Check(o);
    if (r == 1)
        return (PyJitFunction *)o;
    else if (r < 0 && PyErr_Occurred())
        PyErr_Clear();
    return NULL;
}

int
PyJitFunction_Verify(PyJitFunction *o)
{
    if (!o->context || !o->function) {
        PyErr_SetString(PyExc_ValueError, "function is not initialized");
        return -1;
    }
    return 0;
}

PyJitFunction *
PyJitFunction_CastAndVerify(PyObject *o)
{
    PyJitFunction *function = PyJitFunction_Cast(o);
    if (!function) {
        PyErr_Format(
            PyExc_TypeError, "expected instance of %.100s, not %.100s",
            pyjit_function_get_pytype()->tp_name, Py_TYPE(o)->tp_name);
        return NULL;
    }
    if (PyJitFunction_Verify(function) < 0)
        return NULL;
    return function;
}

