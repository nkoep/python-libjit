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

#include "pyjit-label.h"

PyDoc_STRVAR(label_doc, "Wrapper class for jit_label_t");

static PyObject *label_cache = NULL;

/* Slot implementations */

static void
label_dealloc(PyJitLabel *self)
{
    if (self->weakreflist)
        PyObject_ClearWeakRefs((PyObject *)self);

    if (self->label) {
        PYJIT_BEGIN_ALLOW_EXCEPTION
        if (pyjit_weak_cache_delitem(label_cache, (long)self->label) < 0) {
            PYJIT_TRACE("this shouldn't have happened");
            abort();
        }
        PYJIT_END_ALLOW_EXCEPTION
    }

    Py_TYPE(self)->tp_free((PyObject *)self);
}

static int
_label_verify(PyJitLabel *self)
{
    return 0;
}

PYJIT_HASH_GENERIC(label_hash, PyJitLabel, _label_verify, label)

static int
label_init(PyJitLabel *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = { NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, ":Label", kwlist))
        return -1;

    self->label = jit_label_undefined;

    /* Cache the label. */
    return pyjit_weak_cache_setitem(
        label_cache, (long)self->label, (PyObject *)self);
}

static PyTypeObject PyJitLabel_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                                  /* ob_size */
    "jit.Label",                        /* tp_name */
    sizeof(PyJitLabel),                 /* tp_basicsize */
    0,                                  /* tp_itemsize */
    (destructor)label_dealloc,          /* tp_dealloc */
    0,                                  /* tp_print */
    0,                                  /* tp_getattr */
    0,                                  /* tp_setattr */
    0,                                  /* tp_compare */
    0,                                  /* tp_repr */
    0,                                  /* tp_as_number */
    0,                                  /* tp_as_sequence */
    0,                                  /* tp_as_mapping */
    (hashfunc)label_hash,               /* tp_hash */
    0,                                  /* tp_call */
    0,                                  /* tp_str */
    0,                                  /* tp_getattro */
    0,                                  /* tp_setattro */
    0,                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT |
        Py_TPFLAGS_BASETYPE,            /* tp_flags */
    label_doc,                          /* tp_doc */
    0,                                  /* tp_traverse */
    0,                                  /* tp_clear */
    0,                                  /* tp_richcompare */
    offsetof(PyJitLabel, weakreflist),  /* tp_weaklistoffset */
    0,                                  /* tp_iter */
    0,                                  /* tp_iternext */
    0,                                  /* tp_methods */
    0,                                  /* tp_members */
    0,                                  /* tp_getset */
    0,                                  /* tp_base */
    0,                                  /* tp_dict */
    0,                                  /* tp_descr_get */
    0,                                  /* tp_descr_set */
    0,                                  /* tp_dictoffset */
    (initproc)label_init,               /* tp_init */
    0,                                  /* tp_alloc */
    PyType_GenericNew                   /* tp_new */
};

int
pyjit_label_init(PyObject *module)
{
    if (PyType_Ready(&PyJitLabel_Type) < 0)
        return -1;

    /* Set up the label cache. */
    label_cache = PyDict_New();
    if (!label_cache)
        return -1;

    Py_INCREF(&PyJitLabel_Type);
    PyModule_AddObject(module, "Label", (PyObject *)&PyJitLabel_Type);

    return 0;
}

const PyTypeObject *
pyjit_label_get_pytype(void)
{
    return &PyJitLabel_Type;
}

PyObject *
PyJitLabel_New(jit_label_t label)
{
    long numkey;
    PyObject *object;

    numkey = (long)label;
    object = pyjit_weak_cache_getitem(label_cache, numkey);
    if (!object) {
       PyJitLabel *jit_label = PyObject_New(PyJitLabel, &PyJitLabel_Type);
       jit_label->label = label;
       object = (PyObject *)jit_label;
       if (pyjit_weak_cache_setitem(label_cache, numkey, object) < 0) {
           Py_DECREF(object);
           return NULL;
       }
    }
    return object;
}

PyJitLabel *
PyJitLabel_Cast(PyObject *o)
{
    int r = PyObject_IsInstance(o, (PyObject *)&PyJitLabel_Type);
    if (r == 1)
        return (PyJitLabel *)o;
    else if (r < 0 && PyErr_Occurred())
        PyErr_Clear();
    return NULL;
}

