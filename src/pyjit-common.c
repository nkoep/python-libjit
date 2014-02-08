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

#include "pyjit-common.h"

#include <ctype.h>

char *
pyjit_strtoupper(char *s)
{
    char *p = s;
    while (*p) {
       *p = toupper(*p);
       p++;
    }
    return s;
}

PyObject *
pyjit_repr(PyObject *o, void *ptr, const char *jit_type)
{
    PyObject *repr, *tail;

    repr = PyString_FromFormat(
        "<%.100s object at %p (", Py_TYPE(o)->tp_name, o);
    if (ptr)
        tail = PyString_FromFormat("%s at %p)>", jit_type, ptr);
    else
        tail = PyString_FromString("uninitialized at 0x0)>");
    PyString_Concat(&repr, tail);
    Py_DECREF(tail);
    return repr;
}

PyObject *
pyjit_raise_type_error(const char *arg_name, const PyTypeObject *nominal_type,
                       PyObject *actual_object)
{
    PyErr_Format(PyExc_TypeError,
                 "%s must be of type %.100s, not %.100s", arg_name,
                 nominal_type->tp_name,
                 Py_TYPE(actual_object)->tp_name);
    return NULL;
}

int
pyjit_weak_cache_setitem(PyObject *dict, long numkey, PyObject *o)
{
    PyObject *key, *ref;
    int r;

    key = PyLong_FromLong(numkey);
    ref = PyWeakref_NewRef(o, NULL);
    if (!ref)
        return -1;
    r = PyDict_SetItem(dict, key, ref);
    Py_DECREF(ref);
    Py_DECREF(key);
    return r;
}

PyObject *
pyjit_weak_cache_getitem(PyObject *dict, long numkey)
{
    PyObject *key, *ref, *o;

    key = PyLong_FromLong(numkey);
    ref = PyDict_GetItem(dict, key);
    Py_DECREF(key);
    if (!ref)
        return NULL;
    o = PyWeakref_GetObject(ref);
    /* Drop the cache entry if the referent is no longer live. */
    if (o == Py_None) {
        pyjit_weak_cache_delitem(dict, numkey);
        Py_DECREF(o);
        return NULL;
    }
    Py_INCREF(o);
    return o;
}

int
pyjit_weak_cache_delitem(PyObject *dict, long numkey)
{
    PyObject *key;
    int r;

    key = PyLong_FromLong(numkey);
    r = PyDict_DelItem(dict, key);
    Py_DECREF(key);
    return r;
}

void
pyjit_meta_free_func(void *data)
{
    Py_XDECREF((PyObject *)data);
}

