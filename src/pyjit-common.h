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

#ifndef __PYJIT_COMMON_H__
#define __PYJIT_COMMON_H__

#include <Python.h>
#include <jit/jit.h>

#include <stddef.h>

#if PY_MAJOR_VERSION >= 3
#define PYJIT_IS_PY3K
#endif

#define PYJIT_BEGIN_ALLOW_EXCEPTION {                           \
    PyObject *_err_type, *_err_value, *_err_traceback;          \
    int _have_error = PyErr_Occurred() ? 1 : 0;                 \
    if (_have_error)                                            \
        PyErr_Fetch(&_err_type, &_err_value, &_err_traceback);  \

#define PYJIT_END_ALLOW_EXCEPTION                               \
    if (_have_error)                                            \
        PyErr_Restore(_err_type, _err_value, _err_traceback);   \
    }

#define PYJIT_METHOD_EX(name, symbol, flags) \
{ name, (PyCFunction)symbol, flags, NULL }

#define PYJIT_METHOD_NOARGS(prefix, name) \
PYJIT_METHOD_EX(#name, prefix##_##name, METH_NOARGS)

#define PYJIT_METHOD_KW(prefix, name) \
PYJIT_METHOD_EX(#name, prefix##_##name, METH_KEYWORDS)

#define PYJIT_STATIC_METHOD_NOARGS(prefix, name) \
PYJIT_METHOD_EX(#name, prefix##_##name, METH_STATIC | METH_NOARGS)

#define PYJIT_STATIC_METHOD_KW(prefix, name) \
PYJIT_METHOD_EX(#name, prefix##_##name, METH_STATIC | METH_KEYWORDS)

#define PYJIT_UNUSED(arg) \
(void)arg

#ifdef PYJIT_DEBUG
#define PYJIT_TRACE(...)                                                \
do {                                                                    \
    fprintf(stderr, "TRACE:%s:%d:%s: ", basename(__FILE__), __LINE__,   \
            __FUNCTION__);                                              \
    fprintf(stderr, __VA_ARGS__);                                       \
    fprintf(stderr, "\n");                                              \
} while (0)
#else
#define PYJIT_TRACE(...)
#endif

#define PYJIT_REPR_GENERIC(name, type, field)                           \
static PyObject *                                                       \
name(type *self)                                                        \
{                                                                       \
    return pyjit_repr((PyObject *)self, self->field, "jit_"#field"_t"); \
}

#define PYJIT_HASH_GENERIC(name, type, verifyfunc, field)   \
static long                                                 \
name(type *self)                                            \
{                                                           \
    if (verifyfunc(self) < 0)                               \
        return -1;                                          \
    return (long)self->field;                               \
}

char *pyjit_strtoupper(char *s);
PyObject *pyjit_repr(PyObject *o, void *ptr, const char *jit_type);
PyObject *pyjit_raise_type_error(
    const char *arg_name, const PyTypeObject *nominal_type,
    PyObject *actual_object);
int pyjit_weak_cache_setitem(PyObject *dict, long numkey, PyObject *o);
PyObject *pyjit_weak_cache_getitem(PyObject *dict, long numkey);
int pyjit_weak_cache_delitem(PyObject *dict, long numkey);
void pyjit_meta_free_func(void *data);

#endif /* __PYJIT_COMMON_H__ */

