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

#include "pyjit-abi.h"
#include "pyjit-common.h"
#include "pyjit-context.h"
#include "pyjit-function.h"
#include "pyjit-insn.h"
#include "pyjit-label.h"
#include "pyjit-type.h"
#include "pyjit-value.h"

#include <jit/jit-dump.h>

PyDoc_STRVAR(pyjit_doc, "Base module for python-libjit");

static PyObject *
pyjit_uses_interpreter(void *null)
{
    return PyBool_FromLong(jit_uses_interpreter());
}

static PyObject *
pyjit_supports_threads(void *null)
{
    return PyBool_FromLong(jit_supports_threads());
}

static PyObject *
pyjit_supports_virtual_memory(void *null)
{
    return PyBool_FromLong(jit_supports_virtual_memory());
}

static PyObject *
pyjit_supports_closures(void *null)
{
    return PyBool_FromLong(jit_supports_closures());
}

static PyFileObject *
_to_file_object(PyObject *o)
{
    if (!PyFile_Check(o)) {
        PyErr_Format(
            PyExc_TypeError,
            "stream expected to be of type 'file', not '%.100s'",
            Py_TYPE(o)->tp_name);
        return NULL;
    }
    return (PyFileObject *)o;
}

static PyObject *
pyjit_dump_type(void *null, PyObject *args, PyObject *kwargs)
{
    PyObject *stream = NULL, *type = NULL;
    PyFileObject *fo;
    FILE *fp;
    PyJitType *jit_type;
    jit_type_t type_;
    static char *kwlist[] = { "stream", "type_", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OO", kwlist, &stream,
                                     &type))
        return NULL;

    fo = _to_file_object(stream);
    if (!fo)
        return NULL;

    jit_type = PyJitType_CastAndVerify(type);
    if (!jit_type)
        return NULL;
    type_ = jit_type->type;

    fp = PyFile_AsFile(stream);
    if (!fp)
        return NULL;
    PyFile_IncUseCount(fo);

    Py_INCREF(type);
    Py_BEGIN_ALLOW_THREADS
    jit_dump_type(fp, type_);
    Py_END_ALLOW_THREADS
    Py_DECREF(type);

    PyFile_DecUseCount(fo);

    Py_RETURN_NONE;
}

static PyObject *
pyjit_dump_value(void *null, PyObject *args, PyObject *kwargs)
{
    PyObject *stream = NULL, *func = NULL, *value = NULL;
    const char *prefix = NULL;
    PyFileObject *fo;
    FILE *fp;
    PyJitFunction *jit_function;
    jit_function_t function;
    PyJitValue *jit_value;
    jit_value_t value_;
    static char *kwlist[] = { "stream", "func", "value", "prefix", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OOOs", kwlist, &stream,
                                     &func, &value, &prefix))
        return NULL;

    fo = _to_file_object(stream);
    if (!fo)
        return NULL;

    jit_function = PyJitFunction_CastAndVerify(func);
    if (!jit_function)
        return NULL;
    function = jit_function->function;

    jit_value = PyJitValue_CastAndVerify(value);
    if (!jit_value)
        return NULL;
    value_ = jit_value->value;

    fp = PyFile_AsFile(stream);
    if (!fp)
        return NULL;
    PyFile_IncUseCount(fo);

    /* If func is dereferenced by another thread during I/O, it may call
     * jit_function_abandon in the process and destroy memory jit_dump_value is
     * still accessing. To remove this possibility, incref func here.
     */
    Py_INCREF(func);
    Py_BEGIN_ALLOW_THREADS
    jit_dump_value(fp, function, value_, prefix);
    Py_END_ALLOW_THREADS
    Py_DECREF(func);

    PyFile_DecUseCount(fo);

    Py_RETURN_NONE;
}

static PyObject *
pyjit_dump_insn(void *null, PyObject *args, PyObject *kwargs)
{
    PyObject *stream = NULL, *func = NULL, *insn = NULL;
    PyFileObject *fo;
    FILE *fp;
    PyJitFunction *jit_function;
    jit_function_t function;
    PyJitInsn *jit_insn;
    jit_insn_t insn_;
    static char *kwlist[] = { "stream", "func", "insn", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OOO", kwlist, &stream,
                                     &func, &insn))
        return NULL;

    fo = _to_file_object(stream);
    if (!fo)
        return NULL;

    jit_function = PyJitFunction_CastAndVerify(func);
    if (!jit_function)
        return NULL;
    function = jit_function->function;

    jit_insn = PyJitInsn_CastAndVerify(insn);
    if (!jit_insn)
        return NULL;
    insn_ = jit_insn->insn;

    fp = PyFile_AsFile(stream);
    if (!fp)
        return NULL;
    PyFile_IncUseCount(fo);

    /* XXX: Do we have to keep a strong reference to insn here? */
    Py_INCREF(func);
    Py_BEGIN_ALLOW_THREADS
    jit_dump_insn(fp, function, insn_);
    Py_END_ALLOW_THREADS
    Py_DECREF(func);

    PyFile_DecUseCount(fo);

    Py_RETURN_NONE;
}

static PyObject *
pyjit_dump_function(void *null, PyObject *args, PyObject *kwargs)
{
    PyObject *stream = NULL, *func = NULL;
    const char *name = NULL;
    PyFileObject *fo;
    FILE *fp;
    PyJitFunction *jit_function;
    jit_function_t function;
    static char *kwlist[] = { "stream", "func", "name", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OOs", kwlist, &stream,
                                     &func, &name))
        return NULL;

    fo = _to_file_object(stream);
    if (!fo)
        return NULL;

    jit_function = PyJitFunction_CastAndVerify(func);
    if (!jit_function)
        return NULL;
    function = jit_function->function;

    fp = PyFile_AsFile(stream);
    if (!fp)
        return NULL;
    PyFile_IncUseCount(fo);

    Py_INCREF(func);
    Py_BEGIN_ALLOW_THREADS
    jit_dump_function(fp, function, name);
    Py_END_ALLOW_THREADS
    Py_DECREF(func);

    PyFile_DecUseCount(fo);

    Py_RETURN_NONE;
}

static PyMethodDef pyjit_methods[] = {
    PYJIT_METHOD_NOARGS(pyjit, uses_interpreter),
    PYJIT_METHOD_NOARGS(pyjit, supports_threads),
    PYJIT_METHOD_NOARGS(pyjit, supports_virtual_memory),
    PYJIT_METHOD_NOARGS(pyjit, supports_closures),

    /* Diagnostic routines */
    PYJIT_METHOD_KW(pyjit, dump_type),
    PYJIT_METHOD_KW(pyjit, dump_value),
    PYJIT_METHOD_KW(pyjit, dump_insn),
    PYJIT_METHOD_KW(pyjit, dump_function),
    { NULL } /*Sentinel */
};

PyMODINIT_FUNC
init_jit(void)
{
    PyObject *module = Py_InitModule3("_jit", pyjit_methods, pyjit_doc);
    if (!module)
        return;

    /* This is also called by jit_context_build_start(), but just to be on the
     * safe side we call it here anyway.
     */
    jit_init();

#define REGISTER_CONSTANT(prefix, name)                 \
if (PyModule_AddIntConstant(module, #prefix"_"#name,    \
                            JIT_##prefix##_##name) < 0) \
    return

    REGISTER_CONSTANT(INVALID, NAME);

    /* Register TYPE_ constants. */
    REGISTER_CONSTANT(TYPE, INVALID);
    REGISTER_CONSTANT(TYPE, VOID);
    REGISTER_CONSTANT(TYPE, SBYTE);
    REGISTER_CONSTANT(TYPE, UBYTE);
    REGISTER_CONSTANT(TYPE, SHORT);
    REGISTER_CONSTANT(TYPE, USHORT);
    REGISTER_CONSTANT(TYPE, INT);
    REGISTER_CONSTANT(TYPE, UINT);
    REGISTER_CONSTANT(TYPE, NINT);
    REGISTER_CONSTANT(TYPE, NUINT);
    REGISTER_CONSTANT(TYPE, LONG);
    REGISTER_CONSTANT(TYPE, ULONG);
    REGISTER_CONSTANT(TYPE, FLOAT32);
    REGISTER_CONSTANT(TYPE, FLOAT64);
    REGISTER_CONSTANT(TYPE, NFLOAT);
    REGISTER_CONSTANT(TYPE, MAX_PRIMITIVE);
    REGISTER_CONSTANT(TYPE, STRUCT);
    REGISTER_CONSTANT(TYPE, UNION);
    REGISTER_CONSTANT(TYPE, SIGNATURE);
    REGISTER_CONSTANT(TYPE, PTR);
    REGISTER_CONSTANT(TYPE, FIRST_TAGGED);

    /* Register TYPETAG_ constants. */
    REGISTER_CONSTANT(TYPETAG, NAME);
    REGISTER_CONSTANT(TYPETAG, STRUCT_NAME);
    REGISTER_CONSTANT(TYPETAG, UNION_NAME);
    REGISTER_CONSTANT(TYPETAG, ENUM_NAME);
    REGISTER_CONSTANT(TYPETAG, CONST);
    REGISTER_CONSTANT(TYPETAG, VOLATILE);
    REGISTER_CONSTANT(TYPETAG, REFERENCE);
    REGISTER_CONSTANT(TYPETAG, OUTPUT);
    REGISTER_CONSTANT(TYPETAG, RESTRICT);
    REGISTER_CONSTANT(TYPETAG, SYS_BOOL);
    REGISTER_CONSTANT(TYPETAG, SYS_CHAR);
    REGISTER_CONSTANT(TYPETAG, SYS_SCHAR);
    REGISTER_CONSTANT(TYPETAG, SYS_UCHAR);
    REGISTER_CONSTANT(TYPETAG, SYS_SHORT);
    REGISTER_CONSTANT(TYPETAG, SYS_USHORT);
    REGISTER_CONSTANT(TYPETAG, SYS_INT);
    REGISTER_CONSTANT(TYPETAG, SYS_UINT);
    REGISTER_CONSTANT(TYPETAG, SYS_LONG);
    REGISTER_CONSTANT(TYPETAG, SYS_ULONG);
    REGISTER_CONSTANT(TYPETAG, SYS_LONGLONG);
    REGISTER_CONSTANT(TYPETAG, SYS_ULONGLONG);
    REGISTER_CONSTANT(TYPETAG, SYS_FLOAT);
    REGISTER_CONSTANT(TYPETAG, SYS_DOUBLE);
    REGISTER_CONSTANT(TYPETAG, SYS_LONGDOUBLE);

    /* Register MEMORY_ constants. */
    REGISTER_CONSTANT(MEMORY, OK);
    REGISTER_CONSTANT(MEMORY, RESTART);
    REGISTER_CONSTANT(MEMORY, TOO_BIG);
    REGISTER_CONSTANT(MEMORY, ERROR);

    /* Register OPTLEVEL_ constants. */
    REGISTER_CONSTANT(OPTLEVEL, NONE);
    REGISTER_CONSTANT(OPTLEVEL, NORMAL);

    /* Register READELF_ constants. */
    REGISTER_CONSTANT(READELF, OK);
    REGISTER_CONSTANT(READELF, CANNOT_OPEN);
    REGISTER_CONSTANT(READELF, NOT_ELF);
    REGISTER_CONSTANT(READELF, WRONG_ARCH);
    REGISTER_CONSTANT(READELF, BAD_FORMAT);
    REGISTER_CONSTANT(READELF, MEMORY);

    /* Register OPTION_ constants. */
    REGISTER_CONSTANT(OPTION, CACHE_LIMIT);
    REGISTER_CONSTANT(OPTION, CACHE_PAGE_SIZE);
    REGISTER_CONSTANT(OPTION, PRE_COMPILE);
    REGISTER_CONSTANT(OPTION, DONT_FOLD);
    REGISTER_CONSTANT(OPTION, POSITION_INDEPENDENT);
    REGISTER_CONSTANT(OPTION, CACHE_MAX_PAGE_FACTOR);

#undef REGISTER_CONSTANT

    /* Initialize and register type wrappers. */
#define INIT_COMPONENT(name)            \
if (pyjit_##name##_init(module) < 0)    \
    return;                             \

    INIT_COMPONENT(abi);
    INIT_COMPONENT(context);
    INIT_COMPONENT(function);
    INIT_COMPONENT(insn);
    INIT_COMPONENT(label);
    INIT_COMPONENT(type);
    INIT_COMPONENT(value);

#undef INIT_COMPONENT
}

