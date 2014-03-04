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
_type_name(int kind)
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
    default:
        break;
    }
    return NULL;

#undef CASE
}

static int
_marshaling_error(int kind)
{
    const char *kind_name = _type_name(kind);
    if (kind_name) {
        PyErr_Format(
            PyExc_TypeError,
            "failed to marshal argument to jit_type_t of kind '%s'",
            kind_name);
    }
    else {
        PyErr_Format(
            PyExc_TypeError,
            "failed to marshal argument to jit_type_t of kind '%d'", kind);
    }
    return -1;
}

static int
_marshal_arg_from_py(PyObject *o, jit_type_t type, void **out_arg)
{
    int kind;
    void *arg;

    *out_arg = NULL;

    kind = jit_type_get_kind(type);
    switch (kind) {
    case JIT_TYPE_INT:
        if (!PyInt_Check(o)) {
            PyErr_Format(
                PyExc_TypeError,
                "argument expected to be of type int, not %.100s",
                Py_TYPE(o)->tp_name);
            return -1;
        }
        arg = PyMem_Malloc(jit_type_get_size(type));
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
        pyjit_marshal_free_arg_list(args_, i);
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

    kind = jit_type_get_kind(type);
    switch (kind) {
    case JIT_TYPE_INT:
        retval = PyInt_FromLong(*(int *)arg);
        break;
    default:
        _marshaling_error(kind);
        break;
    }
    return retval;
}
