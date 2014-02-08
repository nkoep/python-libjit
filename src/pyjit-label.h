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

#ifndef ___PYJIT_LABEL_H__
#define ___PYJIT_LABEL_H__

#include "pyjit-common.h"

typedef struct {
    PyObject_HEAD
    jit_label_t label;
    PyObject *weakreflist;
} PyJitLabel;

int pyjit_label_init(PyObject *module);
const PyTypeObject *pyjit_label_get_pytype(void);
PyObject *PyJitLabel_New(jit_label_t label);
PyJitLabel *PyJitLabel_Cast(PyObject *o);

#endif /* ___PYJIT_LABEL_H__ */

