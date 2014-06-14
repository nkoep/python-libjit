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

#include "pyjit-marshal.h"

const char *
_kind_name(int kind)
{
#define CASE(name)              \
case JIT_TYPE_##name:           \
    return "JIT_TYPE_"#name;    \

    switch (kind) {
    CASE(INVALID)
    CASE(VOID)
    CASE(SBYTE)
    CASE(UBYTE)
    CASE(SHORT)
    CASE(USHORT)
    CASE(INT)
    CASE(UINT)
    CASE(NINT)
    CASE(NUINT)
    CASE(LONG)
    CASE(ULONG)
    CASE(FLOAT32)
    CASE(FLOAT64)
    CASE(NFLOAT)
    CASE(STRUCT)
    CASE(UNION)
    CASE(SIGNATURE)
    CASE(PTR)
    CASE(FIRST_TAGGED)
    default:
        break;
    }
    return NULL;

#undef CASE
}

static int
_marshaling_error(int kind)
{
    const char *kind_name = _kind_name(kind);
    static const char *header = "failed to marshal argument to jit_type_t of "
                                "kind";

    if (kind_name)
        PyErr_Format(PyExc_TypeError, "%s '%s'", header, kind_name);
    else
        PyErr_Format(PyExc_TypeError, "%s '%d'", header, kind);
    return -1;
}

static int
_marshaling_type_error(const char *type, PyObject *o)
{
    PyErr_Format(PyExc_TypeError,
                 "argument expected to be of type %s, not %.100s", type,
                 Py_TYPE(o)->tp_name);
    return -1;
}

static int
_marshal_arg_from_py(PyObject *o, jit_type_t type, void **out_arg)
{
    unsigned long size;
    void *arg;
    int kind;

    /* This will return 0 for jit_type_void which is fine for `PyMem_Free' as
     * the function performs NULL checks.
     */
    size = jit_type_get_size(type);
    arg = PyMem_Malloc(size);
    kind = jit_type_get_kind(type);
    switch (kind) {
    case JIT_TYPE_VOID:
        break;

    case JIT_TYPE_SBYTE:
    case JIT_TYPE_UBYTE:

    /* TODO: Perform proper range checks. */
    case JIT_TYPE_INT:
        if (!PyInt_Check(o))
            return _marshaling_type_error(PyInt_Type.tp_name, o);
        *(int *)arg = PyLong_AsLong(o);
        break;

    default:
        return _marshaling_error(kind);
    }

    *out_arg = arg;
    return 0;
}

int
pyjit_marshal_arg_list_from_py(
    PyObject *args, jit_type_t signature, void ***out_args)
{
    unsigned int i, num_params;
    void **args_;

    *out_args = NULL;

    num_params = jit_type_num_params(signature);
    args_ = PyMem_New(void *, num_params);
    memset(args, 0, num_params);

    for (i = 0; i < num_params; i++) {
        int r;
        PyObject *item;
        jit_type_t item_type;

        item = PySequence_ITEM(args, i);
        if (!item)
            break;
        item_type = jit_type_get_param(signature, i);
        r = _marshal_arg_from_py(item, item_type, &args_[i]);
        Py_DECREF(item);
        if (r < 0)
            break;
    }

    if (i != num_params) {
        /* Free the arguments allocated so far. */
        pyjit_marshal_free_arg_list(args_, num_params);
        return -1;
    }

    *out_args = args_;
    return 0;
}

void
pyjit_marshal_free_arg_list(void **args, unsigned int num_params)
{
    unsigned int i;

    for (i = 0; i < num_params; i++)
        PyMem_Free(args[i]);
    PyMem_Free(args);
}

PyObject *
pyjit_marshal_arg_to_py(jit_type_t type, void *arg)
{
    PyObject *retval = NULL;
    int kind;

    /* XXX: This probably needs some compile-time tests to marshal all types
     *      properly.
     */
    kind = jit_type_get_kind(type);
    switch (kind) {
    case JIT_TYPE_VOID:
        retval = Py_None;
        Py_INCREF(retval);
        break;

    case JIT_TYPE_SBYTE:
    case JIT_TYPE_SHORT:
    case JIT_TYPE_INT:
        retval = PyInt_FromLong(*(int *)arg);
        break;

    case JIT_TYPE_UBYTE:
    case JIT_TYPE_USHORT:
    case JIT_TYPE_UINT:
        retval = PyInt_FromSize_t(*(unsigned int *)arg);
        break;

    case JIT_TYPE_NINT:
    case JIT_TYPE_LONG:
        retval = PyLong_FromLong(*(long *)arg);
        break;

    case JIT_TYPE_NUINT:
    case JIT_TYPE_ULONG:
        retval = PyLong_FromUnsignedLong(*(unsigned long *)arg);
        break;

    case JIT_TYPE_FLOAT32:
    case JIT_TYPE_FLOAT64:
        retval = PyFloat_FromDouble(*(double *)arg);
        break;

    default:
        _marshaling_error(kind);
        break;
    }
    return retval;
}

