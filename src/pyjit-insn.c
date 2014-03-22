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

#include "pyjit-insn.h"

#include "pyjit-function.h"
#include "pyjit-label.h"
#include "pyjit-type.h"
#include "pyjit-value.h"

PyDoc_STRVAR(insn_doc, "Wrapper class for jit_insn_t");

static PyObject *insn_cache = NULL;

/* Slot implementations */

void
insn_dealloc(PyJitInsn *self)
{
    if (self->weakreflist)
        PyObject_ClearWeakRefs((PyObject *)self);

    if (self->insn) {
        PYJIT_BEGIN_ALLOW_EXCEPTION
        if (pyjit_weak_cache_delitem(insn_cache, (long)self->insn) < 0) {
            PYJIT_TRACE("this shouldn't have happened");
            abort();
        }
        PYJIT_END_ALLOW_EXCEPTION
    }

    Py_XDECREF(self->function);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

PYJIT_REPR_GENERIC(insn_repr, PyJitInsn, insn)

PYJIT_HASH_GENERIC(insn_hash, PyJitInsn, PyJitInsn_Verify, insn)

/* Regular methods */

static PyObject *
insn_get_opcode(PyJitInsn *self)
{
    return PyInt_FromLong(jit_insn_get_opcode(self->insn));
}

static PyObject *insn_get_function(PyJitInsn *self); /* Forward */

static PyObject *
_value_new(PyJitInsn *self, jit_value_t value)
{
    PyObject *function, *retval;

    function = insn_get_function(self);
    if (!function)
        return NULL;
    retval = PyJitValue_New(value, function);
    Py_DECREF(function);
    return retval;
}

static PyObject *
insn_get_dest(PyJitInsn *self)
{
    jit_value_t dest;

    dest = jit_insn_get_dest(self->insn);
    if (!dest)
        Py_RETURN_NONE;
    return _value_new(self, dest);
}

static PyObject *
insn_get_value1(PyJitInsn *self)
{
    jit_value_t value1;

    value1 = jit_insn_get_value1(self->insn);
    if (!value1)
        Py_RETURN_NONE;
    return _value_new(self, value1);
}

static PyObject *
insn_get_value2(PyJitInsn *self)
{
    jit_value_t value2;

    value2 = jit_insn_get_value1(self->insn);
    if (!value2)
        Py_RETURN_NONE;
    return _value_new(self, value2);
}

static PyObject *
insn_get_label(PyJitInsn *self)
{
    jit_label_t label = jit_insn_get_label(self->insn);
    if (!label)
        Py_RETURN_NONE;
    return PyJitLabel_New(label);
}

static PyObject *
insn_get_function(PyJitInsn *self)
{
    jit_function_t function = jit_insn_get_function(self->insn);
    if (!function)
        Py_RETURN_NONE;
    return PyJitFunction_New(function);
}

static PyObject *
insn_get_name(PyJitInsn *self)
{
    const char *name = jit_insn_get_name(self->insn);
    if (!name)
        Py_RETURN_NONE;
    return PyString_FromString(name);
}

static PyObject *
insn_get_signature(PyJitInsn *self)
{
    jit_type_t signature = jit_insn_get_signature(self->insn);
    if (!signature)
        Py_RETURN_NONE;
    return PyJitType_New(signature);
}

static PyObject *
insn_dest_is_value(PyJitInsn *self)
{
    return PyBool_FromLong(jit_insn_dest_is_value(self->insn));
}

static int
_insn_label_prelude(PyObject *args, PyObject *kwargs,
                    PyJitFunction **jit_function, PyJitLabel **jit_label)
{
    PyObject *func = NULL, *label = NULL;
    static char *kwlist[] = { "func", "label", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OO:Insn", kwlist, &func,
                                     &label))
        return -1;

    *jit_function = PyJitFunction_Cast(func);
    if (!*jit_function) {
        pyjit_raise_type_error("func", pyjit_function_get_pytype(), func);
        return -1;
    }

    *jit_label = PyJitLabel_Cast(label);
    if (!*jit_label) {
        pyjit_raise_type_error("label", pyjit_label_get_pytype(), label);
        return -1;
    }

    return 0;
}

static PyObject *
insn_label(void *null, PyObject *args, PyObject *kwargs)
{
    PyJitFunction *jit_function;
    PyJitLabel *jit_label;

    if (_insn_label_prelude(args, kwargs, &jit_function, &jit_label) < 0)
        return NULL;
    jit_insn_label(jit_function->function, &jit_label->label);
    Py_RETURN_NONE;
}

static PyObject *
insn_new_block(void *null, PyObject *args, PyObject *kwargs)
{
    PyObject *func = NULL;
    PyJitFunction *jit_function;
    static char *kwlist[] = { "func", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O:Insn", kwlist, &func))
        return NULL;

    jit_function = PyJitFunction_Cast(func);
    if (!jit_function) {
        return pyjit_raise_type_error("func", pyjit_function_get_pytype(),
                                      func);
    }
    return PyBool_FromLong(
        jit_insn_new_block(jit_function->function));
}

/* TODO: This can also be reused for jit_insn_import. */
static PyObject *
_insn_load(PyObject *args, PyObject *kwargs,
           jit_value_t (loadfunc)(jit_function_t, jit_value_t))
{
    PyObject *func = NULL, *value = NULL;
    PyJitFunction *jit_function;
    PyJitValue *jit_value;
    static char *kwlist[] = { "func", "value", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OO:Insn", kwlist, &func,
                                     &value))
        return NULL;

    jit_function = PyJitFunction_Cast(func);
    if (!jit_function) {
        return pyjit_raise_type_error("func", pyjit_function_get_pytype(),
                                      func);
    }
    jit_value = PyJitValue_Cast(value);
    if (!jit_value) {
        return pyjit_raise_type_error("value", pyjit_value_get_pytype(),
                                      value);
    }

    return PyJitValue_New(
        loadfunc(jit_function->function, jit_value->value), func);
}

static PyObject *
insn_load_dup(void *null, PyObject *args, PyObject *kwargs)
{
    return _insn_load(args, kwargs, jit_insn_load);
}

static PyObject *
insn_load_small(void *null, PyObject *args, PyObject *kwargs)
{
    return _insn_load(args, kwargs, jit_insn_load_small);
}

static PyObject *
insn_store(void *null, PyObject *args, PyObject *kwargs)
{
    PyObject *func = NULL, *dest = NULL, *value = NULL;
    PyJitFunction *jit_function;
    PyJitValue *jit_value_dest, *jit_value;
    static char *kwlist[] = { "func", "dest", "value", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OO:Insn", kwlist, &func,
                                     &dest, &value))
        return NULL;

    jit_function = PyJitFunction_Cast(func);
    if (!jit_function) {
        return pyjit_raise_type_error("func", pyjit_function_get_pytype(),
                                      func);
    }
    jit_value_dest = PyJitValue_Cast(dest);
    if (!jit_value_dest)
        return pyjit_raise_type_error("dest", pyjit_value_get_pytype(), dest);
    jit_value = PyJitValue_Cast(value);
    if (!jit_value) {
        return pyjit_raise_type_error("value", pyjit_value_get_pytype(),
                                      value);
    }

    jit_insn_store(jit_function->function, jit_value_dest->value,
                   jit_value->value);
    Py_RETURN_NONE;
}

static PyObject *
insn_load_relative(void *null, PyObject *args, PyObject *kwargs)
{
    PyObject *func = NULL, *value = NULL, *type = NULL;
    jit_nint offset;
    PyJitFunction *jit_function;
    PyJitValue *jit_value;
    PyJitType *jit_type;
    static char *kwlist[] = { "func", "value", "offset", "type", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OOlO:Insn", kwlist, &func,
                                     &value, &offset, &type))
        return NULL;

    jit_function = PyJitFunction_Cast(func);
    if (!jit_function) {
        return pyjit_raise_type_error("func", pyjit_function_get_pytype(),
                                      func);
    }
    jit_value = PyJitValue_Cast(value);
    if (!jit_value) {
        return pyjit_raise_type_error("value", pyjit_value_get_pytype(),
                                      value);
    }
    jit_type = PyJitType_Cast(type);
    if (!jit_type) {
        return pyjit_raise_type_error("type", pyjit_type_get_pytype(),
                                      type);
    }

    return PyJitValue_New(
        jit_insn_load_relative(jit_function->function, jit_value->value,
                               offset, jit_type->type), func);
}

static PyObject *
insn_store_relative(void *null, PyObject *args, PyObject *kwargs)
{
    PyObject *func = NULL, *dest = NULL, *value = NULL;
    jit_nint offset;
    PyJitFunction *jit_function;
    PyJitValue *jit_value_dest, *jit_value;
    static char *kwlist[] = { "func", "dest", "offset", "value", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OOlO:Insn", kwlist, &func,
                                     &dest, &offset, &value))
        return NULL;

    jit_function = PyJitFunction_Cast(func);
    if (!jit_function) {
        return pyjit_raise_type_error("func", pyjit_function_get_pytype(),
                                      func);
    }
    jit_value_dest = PyJitValue_Cast(dest);
    if (!jit_value_dest)
        return pyjit_raise_type_error("dest", pyjit_value_get_pytype(), dest);
    jit_value = PyJitValue_Cast(value);
    if (!jit_value) {
        return pyjit_raise_type_error("value", pyjit_value_get_pytype(),
                                      value);
    }

    return PyBool_FromLong(
        jit_insn_store_relative(jit_function->function, jit_value_dest->value,
                                offset, jit_value->value));
}

static PyObject *
insn_add_relative(void *null, PyObject *args, PyObject *kwargs)
{
    PyObject *func = NULL, *value = NULL;
    jit_nint offset;
    jit_value_t jit_value;
    static char *kwlist[] = { "func", "value", "offset", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OOl:Insn", kwlist, &func,
                                     &value, &offset))
        return NULL;

    if (!PyJitFunction_Check(func)) {
        return pyjit_raise_type_error("func", pyjit_function_get_pytype(),
                                      func);
    }
    if (!PyJitValue_Check(value)) {
        return pyjit_raise_type_error("value", pyjit_value_get_pytype(),
                                      value);
    }

    jit_value = jit_insn_add_relative(
        ((PyJitFunction *)func)->function, ((PyJitValue *)value)->value,
        offset);
    return PyJitValue_New(jit_value, func);
}

static int
_insn_load_elem_prelude(PyObject *args, PyObject *kwargs, PyObject **func,
                        PyObject **base_addr, PyObject **index,
                        PyObject **elem_type)
{
    static char *kwlist[] = {
        "func", "base_addr", "index", "elem_type", NULL
    };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OOOO:Insn", kwlist, func,
                                     base_addr, index, elem_type))
        return -1;

    if (!PyJitFunction_Check(*func)) {
        pyjit_raise_type_error("func", pyjit_function_get_pytype(), *func);
        return -1;
    }
    if (!PyJitValue_Check(*base_addr)) {
        pyjit_raise_type_error("base_addr", pyjit_value_get_pytype(),
                               *base_addr);
        return -1;
    }
    if (!PyJitValue_Check(*index)) {
        pyjit_raise_type_error("index", pyjit_value_get_pytype(), *index);
        return -1;
    }
    if (!PyJitType_Check(*elem_type)) {
        pyjit_raise_type_error("elem_type", pyjit_type_get_pytype(),
                               *elem_type);
        return -1;
    }

    return 0;
}

static PyObject *
insn_load_elem(void *null, PyObject *args, PyObject *kwargs)
{
    PyObject *func = NULL, *base_addr = NULL, *index = NULL, *elem_type = NULL;
    jit_value_t value;

    if (_insn_load_elem_prelude(args, kwargs, &func, &base_addr, &index,
                                &elem_type) < 0)
        return NULL;

    value = jit_insn_load_elem(
        ((PyJitFunction *)func)->function, ((PyJitValue *)base_addr)->value,
        ((PyJitValue *)index)->value, ((PyJitType *)elem_type)->type);
    if (!value)
        Py_RETURN_NONE;
    return PyJitValue_New(value, func);
}

static PyObject *
insn_load_elem_address(void *null, PyObject *args, PyObject *kwargs)
{
    PyObject *func = NULL, *base_addr = NULL, *index = NULL, *elem_type = NULL;
    jit_value_t value;

    if (_insn_load_elem_prelude(args, kwargs, &func, &base_addr, &index,
                                &elem_type) < 0)
        return NULL;

    value = jit_insn_load_elem_address(
        ((PyJitFunction *)func)->function, ((PyJitValue *)base_addr)->value,
        ((PyJitValue *)index)->value, ((PyJitType *)elem_type)->type);
    if (!value)
        Py_RETURN_NONE;
    return PyJitValue_New(value, func);
}

static PyObject *
insn_store_elem(void *null, PyObject *args, PyObject *kwargs)
{
    PyObject *func = NULL, *base_addr = NULL, *index = NULL, *value = NULL;
    static char *kwlist[] = { "func", "base_addr", "index", "value", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OOOO:Insn", kwlist, &func,
                                     &base_addr, &index, &value))
        return NULL;

    if (!PyJitFunction_Check(func)) {
        return pyjit_raise_type_error("func", pyjit_function_get_pytype(),
                                      func);
    }
    if (!PyJitValue_Check(base_addr)) {
        return pyjit_raise_type_error("base_addr", pyjit_value_get_pytype(),
                                      base_addr);
    }
    if (!PyJitValue_Check(index)) {
        return pyjit_raise_type_error("index", pyjit_value_get_pytype(),
                                      index);
    }
    if (!PyJitValue_Check(value)) {
        return pyjit_raise_type_error("value", pyjit_value_get_pytype(),
                                      value);
    }

    return PyBool_FromLong(
        jit_insn_store_elem(
            ((PyJitFunction *)func)->function,
            ((PyJitValue *)base_addr)->value, ((PyJitValue *)index)->value,
            ((PyJitValue *)value)->value));
}

/* TODO: jit_insn_flush_struct, jit_insn_push, jit_insn_return, jit_insn_throw,
 *       and jit_insn_return from filter have the same signature as
 *       jit_insn_check_null.
 */
static PyObject *
_insn_func(PyObject *args, PyObject *kwargs,
                int (*booleanfunc)(jit_function_t, jit_value_t))
{
    PyObject *func = NULL, *value = NULL;
    PyJitFunction *jit_function;
    PyJitValue *jit_value;
    static char *kwlist[] = { "func", "value", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OO:Insn", kwlist, &func,
                                     &value))
        return NULL;

    jit_function = PyJitFunction_Cast(func);
    if (!jit_function) {
        return pyjit_raise_type_error("func", pyjit_function_get_pytype(),
                                      func);
    }
    jit_value = PyJitValue_Cast(value);
    if (!jit_value) {
        return pyjit_raise_type_error("value", pyjit_value_get_pytype(),
                                      value);
    }
    return PyBool_FromLong(
        booleanfunc(jit_function->function, jit_value->value));
}

static PyObject *
insn_check_null(void *null, PyObject *args, PyObject *kwargs)
{
    return _insn_func(args, kwargs, jit_insn_check_null);
}

static PyObject *
_insn_unaryfunc(PyObject *args, PyObject *kwargs, pyjit_unaryfunc unaryfunc)
{
    PyObject *func = NULL, *value1 = NULL;
    static char *kwlist[] = { "func", "value1", NULL };
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OO:Insn", kwlist,
                                     &func, &value1))
        return NULL;
    return pyjit_insn_unary_method(func, value1, unaryfunc);
}

#define DEFINE_UNARY_METHOD(name)                           \
static PyObject *                                           \
insn_##name(void *null, PyObject *args, PyObject *kwargs)   \
{                                                           \
    return _insn_unaryfunc(args, kwargs, jit_insn_##name);  \
}

static PyObject *
_insn_binaryfunc(PyObject *args, PyObject *kwargs, pyjit_binaryfunc binaryfunc)
{
    PyObject *func = NULL, *value1 = NULL, *value2 = NULL;
    static char *kwlist[] = { "func", "value1", "value2", NULL };
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OOO:Insn", kwlist,
                                     &func, &value1, &value2))
        return NULL;
    return pyjit_insn_binary_method(func, value1, value2, binaryfunc);
}

#define DEFINE_BINARY_METHOD(name)                          \
static PyObject *                                           \
insn_##name(void *null, PyObject *args, PyObject *kwargs)   \
{                                                           \
    return _insn_binaryfunc(args, kwargs, jit_insn_##name); \
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

static PyObject *
insn_branch(void *null, PyObject *args, PyObject *kwargs)
{
    PyJitFunction *jit_function;
    PyJitLabel *jit_label;

    if (_insn_label_prelude(args, kwargs, &jit_function, &jit_label) < 0)
        return NULL;
    return PyBool_FromLong(
        jit_insn_branch(jit_function->function, &jit_label->label));
}

static PyObject *
_insn_branch_conditionally(
    PyObject *args, PyObject *kwargs,
    int (*branchfunc)(jit_function_t, jit_value_t, jit_label_t *))
{
    PyObject *func = NULL, *value = NULL, *label = NULL;
    PyJitFunction *jit_function;
    PyJitValue *jit_value;
    PyJitLabel *jit_label;
    static char *kwlist[] = { "func", "value", "label", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OOO:Insn", kwlist, &func,
                                     &value, &label))
        return NULL;

    jit_function = PyJitFunction_Cast(func);
    if (!jit_function) {
        return pyjit_raise_type_error("func", pyjit_function_get_pytype(),
                                      func);
    }

    jit_value = PyJitValue_Cast(value);
    if (!jit_value) {
        return pyjit_raise_type_error("value", pyjit_value_get_pytype(),
                                      value);
    }

    jit_label = PyJitLabel_Cast(label);
    if (!jit_label) {
        return pyjit_raise_type_error("label", pyjit_label_get_pytype(),
                                      label);
    }
    return PyBool_FromLong(
        branchfunc(jit_function->function, jit_value->value,
                   &jit_label->label));
}

static PyObject *
insn_branch_if(void *null, PyObject *args, PyObject *kwargs)
{
    return _insn_branch_conditionally(args, kwargs, jit_insn_branch_if);
}

static PyObject *
insn_branch_if_not(void *null, PyObject *args, PyObject *kwargs)
{
    return _insn_branch_conditionally(args, kwargs, jit_insn_branch_if_not);
}

DEFINE_UNARY_METHOD(address_of)

#undef DEFINE_UNARY_METHOD
#undef DEFINE_BINARY_METHOD

static PyObject *
insn_address_of_label(void *null, PyObject *args, PyObject *kwargs)
{
    PyJitFunction *jit_function;
    PyJitLabel *jit_label;

    if (_insn_label_prelude(args, kwargs, &jit_function, &jit_label) < 0)
        return NULL;
    return PyJitValue_New(
        jit_insn_address_of_label(jit_function->function, &jit_label->label),
        (PyObject *)jit_function);
}

static PyObject *
insn_convert(void *null, PyObject *args, PyObject *kwargs)
{
    PyObject *func = NULL, *value = NULL, *type = NULL, *overflow_check = NULL;
    PyJitFunction *jit_function;
    PyJitValue *jit_value;
    PyJitType *jit_type;
    int jit_overflow_check;
    jit_value_t converted_value;
    static char *kwlist[] = {
        "func", "value", "type_", "overflow_check", NULL
    };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OOOO:Insn", kwlist, &func,
                                     &value, &type, &overflow_check))
        return NULL;

    jit_function = PyJitFunction_Cast(func);
    if (!jit_function) {
        return pyjit_raise_type_error("func", pyjit_function_get_pytype(),
                                      func);
    }

    jit_value = PyJitValue_Cast(value);
    if (!jit_value) {
        return pyjit_raise_type_error("value", pyjit_value_get_pytype(),
                                      value);
    }

    jit_type = PyJitType_Cast(type);
    if (!jit_type) {
        return pyjit_raise_type_error("type_", pyjit_type_get_pytype(),
                                      type);
    }
    jit_overflow_check = PyObject_IsTrue(overflow_check);

    converted_value = jit_insn_convert(
        jit_function->function, jit_value->value, jit_type->type,
        jit_overflow_check);
    return PyJitValue_New(converted_value, func);
}

static PyObject *
insn_return(void *null, PyObject *args, PyObject *kwargs)
{
    return _insn_func(args, kwargs, jit_insn_return);
}

static PyMethodDef insn_methods[] = {
    PYJIT_METHOD_NOARGS(insn, get_opcode),
    PYJIT_METHOD_NOARGS(insn, get_dest),
    PYJIT_METHOD_NOARGS(insn, get_value1),
    PYJIT_METHOD_NOARGS(insn, get_value2),
    PYJIT_METHOD_NOARGS(insn, get_label),
    PYJIT_METHOD_NOARGS(insn, get_function),
    /* PYJIT_METHOD_NOARGS(insn, get_native), */
    PYJIT_METHOD_NOARGS(insn, get_name),
    PYJIT_METHOD_NOARGS(insn, get_signature),
    PYJIT_METHOD_NOARGS(insn, dest_is_value),
    PYJIT_STATIC_METHOD_KW(insn, label),
    PYJIT_STATIC_METHOD_KW(insn, new_block),
    PYJIT_METHOD_EX("load", insn_load_dup, METH_STATIC | METH_KEYWORDS),
    PYJIT_METHOD_EX("dup", insn_load_dup, METH_STATIC | METH_KEYWORDS),
    PYJIT_STATIC_METHOD_KW(insn, load_small),
    PYJIT_STATIC_METHOD_KW(insn, store),
    PYJIT_STATIC_METHOD_KW(insn, load_relative),
    PYJIT_STATIC_METHOD_KW(insn, store_relative),
    PYJIT_STATIC_METHOD_KW(insn, add_relative),
    PYJIT_STATIC_METHOD_KW(insn, load_elem),
    PYJIT_STATIC_METHOD_KW(insn, load_elem_address),
    PYJIT_STATIC_METHOD_KW(insn, store_elem),
    PYJIT_STATIC_METHOD_KW(insn, check_null),
    PYJIT_STATIC_METHOD_KW(insn, add),
    PYJIT_STATIC_METHOD_KW(insn, add_ovf),
    PYJIT_STATIC_METHOD_KW(insn, sub),
    PYJIT_STATIC_METHOD_KW(insn, sub_ovf),
    PYJIT_STATIC_METHOD_KW(insn, mul),
    PYJIT_STATIC_METHOD_KW(insn, mul_ovf),
    PYJIT_STATIC_METHOD_KW(insn, div),
    PYJIT_STATIC_METHOD_KW(insn, rem),
    PYJIT_STATIC_METHOD_KW(insn, rem_ieee),
    PYJIT_STATIC_METHOD_KW(insn, neg),
    PYJIT_STATIC_METHOD_KW(insn, and),
    PYJIT_STATIC_METHOD_KW(insn, or),
    PYJIT_STATIC_METHOD_KW(insn, xor),
    PYJIT_STATIC_METHOD_KW(insn, not),
    PYJIT_STATIC_METHOD_KW(insn, shl),
    PYJIT_STATIC_METHOD_KW(insn, shr),
    PYJIT_STATIC_METHOD_KW(insn, ushr),
    PYJIT_STATIC_METHOD_KW(insn, sshr),
    PYJIT_STATIC_METHOD_KW(insn, eq),
    PYJIT_STATIC_METHOD_KW(insn, ne),
    PYJIT_STATIC_METHOD_KW(insn, lt),
    PYJIT_STATIC_METHOD_KW(insn, le),
    PYJIT_STATIC_METHOD_KW(insn, gt),
    PYJIT_STATIC_METHOD_KW(insn, ge),
    PYJIT_STATIC_METHOD_KW(insn, cmpl),
    PYJIT_STATIC_METHOD_KW(insn, cmpg),
    PYJIT_STATIC_METHOD_KW(insn, to_bool),
    PYJIT_STATIC_METHOD_KW(insn, to_not_bool),
    PYJIT_STATIC_METHOD_KW(insn, acos),
    PYJIT_STATIC_METHOD_KW(insn, asin),
    PYJIT_STATIC_METHOD_KW(insn, atan),
    PYJIT_STATIC_METHOD_KW(insn, atan2),
    PYJIT_STATIC_METHOD_KW(insn, cos),
    PYJIT_STATIC_METHOD_KW(insn, cosh),
    PYJIT_STATIC_METHOD_KW(insn, exp),
    PYJIT_STATIC_METHOD_KW(insn, log),
    PYJIT_STATIC_METHOD_KW(insn, log10),
    PYJIT_STATIC_METHOD_KW(insn, pow),
    PYJIT_STATIC_METHOD_KW(insn, sin),
    PYJIT_STATIC_METHOD_KW(insn, sinh),
    PYJIT_STATIC_METHOD_KW(insn, sqrt),
    PYJIT_STATIC_METHOD_KW(insn, tan),
    PYJIT_STATIC_METHOD_KW(insn, tanh),
    PYJIT_STATIC_METHOD_KW(insn, ceil),
    PYJIT_STATIC_METHOD_KW(insn, floor),
    PYJIT_STATIC_METHOD_KW(insn, rint),
    PYJIT_STATIC_METHOD_KW(insn, round),
    PYJIT_STATIC_METHOD_KW(insn, trunc),
    PYJIT_STATIC_METHOD_KW(insn, is_nan),
    PYJIT_STATIC_METHOD_KW(insn, is_finite),
    PYJIT_STATIC_METHOD_KW(insn, is_inf),
    PYJIT_STATIC_METHOD_KW(insn, abs),
    PYJIT_STATIC_METHOD_KW(insn, min),
    PYJIT_STATIC_METHOD_KW(insn, max),
    PYJIT_STATIC_METHOD_KW(insn, sign),
    PYJIT_STATIC_METHOD_KW(insn, branch),
    PYJIT_STATIC_METHOD_KW(insn, branch_if),
    PYJIT_STATIC_METHOD_KW(insn, branch_if_not),
    /* PYJIT_STATIC_METHOD_KW(insn, jump_table), */
    PYJIT_STATIC_METHOD_KW(insn, address_of),
    PYJIT_STATIC_METHOD_KW(insn, address_of_label),
    PYJIT_STATIC_METHOD_KW(insn, convert),
    /* jit_insn_call */
    /* jit_insn_call_indirect */
    /* jit_insn_call_indirect_vtable */
    /* jit_insn_call_native */
    /* jit_insn_call_intrinsic */
    /* jit_insn_incoming_reg */
    /* jit_insn_incoming_frame_posn */
    /* jit_insn_outgoing_reg */
    /* jit_insn_outgoing_frame_posn */
    /* jit_insn_return_reg */
    /* jit_insn_setup_for_nested */
    /* jit_insn_flush_struct */
    /* jit_insn_import */
    /* jit_insn_push */
    /* jit_insn_push_ptr */
    /* jit_insn_set_param */
    /* jit_insn_set_param_ptr */
    /* jit_insn_push_return_area_ptr */
    /* jit_insn_pop_stack */
    /* jit_insn_defer_pop_stack */
    /* jit_insn_flush_defer_pop */
    PYJIT_METHOD_EX("return_", insn_return, METH_STATIC | METH_KEYWORDS),
    /* jit_insn_return_ptr */
    /* jit_insn_default_return */
    /* jit_insn_throw */
    /* jit_insn_get_call_stack */
    /* jit_insn_thrown_exception */
    /* jit_insn_uses_catcher */
    /* jit_insn_start_catcher */
    /* jit_insn_branch_if_pc_not_in_range */
    /* jit_insn_rethrow_unhandled */
    /* jit_insn_start_finally */
    /* jit_insn_return_from_finally */
    /* jit_insn_call_finally */
    /* jit_insn_start_filter */
    /* jit_insn_return_from_filter */
    /* jit_insn_call_filter */
    /* jit_insn_memcpy */
    /* jit_insn_memmove */
    /* jit_insn_memset */
    /* jit_insn_alloca */
    /* jit_insn_move_blocks_to_end */
    /* jit_insn_move_blocks_to_start */
    /* jit_insn_mark_offset */
    /* jit_insn_iter_init */
    /* jit_insn_iter_init_last */
    /* jit_insn_iter_next */
    /* jit_insn_iter_previous */
    { NULL } /* Sentinel */
};

static PyTypeObject PyJitInsn_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                                  /* ob_size */
    "jit.Insn",                         /* tp_name */
    sizeof(PyJitInsn),                  /* tp_basicsize */
    0,                                  /* tp_itemsize */
    (destructor)insn_dealloc,           /* tp_dealloc */
    0,                                  /* tp_print */
    0,                                  /* tp_getattr */
    0,                                  /* tp_setattr */
    0,                                  /* tp_compare */
    (reprfunc)insn_repr,                /* tp_repr */
    0,                                  /* tp_as_number */
    0,                                  /* tp_as_sequence */
    0,                                  /* tp_as_mapping */
    (hashfunc)insn_hash,                /* tp_hash */
    0,                                  /* tp_call */
    0,                                  /* tp_str */
    0,                                  /* tp_getattro */
    0,                                  /* tp_setattro */
    0,                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT |
        Py_TPFLAGS_BASETYPE,            /* tp_flags */
    insn_doc,                           /* tp_doc */
    0,                                  /* tp_traverse */
    0,                                  /* tp_clear */
    0,                                  /* tp_richcompare */
    offsetof(PyJitInsn, weakreflist),   /* tp_weaklistoffset */
    0,                                  /* tp_iter */
    0,                                  /* tp_iternext */
    insn_methods                        /* tp_methods */
};

int
pyjit_insn_init(PyObject *module)
{
    if (PyType_Ready(&PyJitInsn_Type) < 0)
        return -1;

    /* Set up the instruction cache. */
    insn_cache = PyDict_New();
    if (!insn_cache)
        return -1;

    Py_INCREF(&PyJitInsn_Type);
    PyModule_AddObject(module, "Insn", (PyObject *)&PyJitInsn_Type);

    return 0;
}

const PyTypeObject *
pyjit_insn_get_pytype(void)
{
    return &PyJitInsn_Type;
}

PyObject *
pyjit_insn_unary_method(PyObject *func, PyObject *value1,
                        pyjit_unaryfunc unaryfunc)
{
    PyJitFunction *jit_function;
    PyJitValue *jit_value1;
    jit_value_t value;

    jit_function = PyJitFunction_Cast(func);
    if (!jit_function) {
        pyjit_raise_type_error("func", pyjit_function_get_pytype(), func);
        return NULL;
    }
    jit_value1 = PyJitValue_Cast(value1);
    if (!jit_value1) {
        pyjit_raise_type_error("value1", pyjit_value_get_pytype(), value1);
        return NULL;
    }

    value = unaryfunc(jit_function->function, jit_value1->value);
    if (!value)
        Py_RETURN_NONE;
    return PyJitValue_New(value, func);
}

PyObject *
pyjit_insn_binary_method(PyObject *func, PyObject *value1, PyObject *value2,
                         pyjit_binaryfunc binaryfunc)
{
    PyJitFunction *jit_function;
    PyJitValue *jit_value1, *jit_value2;
    jit_value_t value;

    jit_function = PyJitFunction_Cast(func);
    if (!jit_function) {
        pyjit_raise_type_error("func", pyjit_function_get_pytype(), func);
        return NULL;
    }
    jit_value1 = PyJitValue_Cast(value1);
    if (!jit_value1) {
        pyjit_raise_type_error("value1", pyjit_value_get_pytype(), value1);
        return NULL;
    }
    jit_value2 = PyJitValue_Cast(value2);
    if (!jit_value2) {
        pyjit_raise_type_error("value2", pyjit_value_get_pytype(), value2);
        return NULL;
    }

    value = binaryfunc(jit_function->function, jit_value1->value,
                       jit_value2->value);
    if (!value)
        Py_RETURN_NONE;
    return PyJitValue_New(value, func);
}

PyObject *
PyJitInsn_New(jit_insn_t insn, PyObject *function)
{
    long numkey;
    PyObject *object;

    numkey = (long)insn;
    object = pyjit_weak_cache_getitem(insn_cache, numkey);
    if (!object) {
        PyJitInsn *instruction;

        object = PyType_GenericNew(&PyJitInsn_Type, NULL, NULL);
        instruction = (PyJitInsn *)object;
        instruction->insn = insn;
        Py_INCREF(function);
        instruction->function = function;

        if (pyjit_weak_cache_setitem(insn_cache, numkey, object) < 0) {
            Py_DECREF(object);
            return NULL;
        }
    }
    return object;
}

int
PyJitInsn_Check(PyObject *o)
{
    return PyObject_IsInstance(o, (PyObject *)&PyJitInsn_Type);
}

PyJitInsn *
PyJitInsn_Cast(PyObject *o)
{
    int r = PyJitInsn_Check(o);
    if (r == 1)
        return (PyJitInsn *)o;
    else if (r < 0 && PyErr_Occurred())
        PyErr_Clear();
    return NULL;
}

int
PyJitInsn_Verify(PyJitInsn *o)
{
    if (!o->function || !o->insn) {
        PyErr_SetString(PyExc_ValueError, "insn is not initialized");
        return -1;
    }
    return 0;
}

PyJitInsn *
PyJitInsn_CastAndVerify(PyObject *o)
{
    PyJitInsn *insn = PyJitInsn_Cast(o);
    if (!insn) {
        PyErr_Format(
            PyExc_TypeError, "expected instance of %.100s, not %.100s",
            pyjit_insn_get_pytype()->tp_name, Py_TYPE(o)->tp_name);
        return NULL;
    }
    if (PyJitInsn_Verify(insn) < 0)
        return NULL;
    return insn;
}

