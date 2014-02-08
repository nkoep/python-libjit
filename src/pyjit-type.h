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

#ifndef ___PYJIT_TYPE_H__
#define ___PYJIT_TYPE_H__

#include "pyjit-common.h"

typedef struct {
    PyObject_HEAD
    jit_type_t type;
    PyObject *weakreflist;
} PyJitType;

int pyjit_type_init(PyObject *module);
const PyTypeObject *pyjit_type_get_pytype(void);
PyObject *PyJitType_New(jit_type_t type);
int PyJitType_Check(PyObject *o);
PyJitType *PyJitType_Cast(PyObject *o);
int PyJitType_Verify(PyJitType *o);
PyJitType *PyJitType_CastAndVerify(PyObject *o);

#endif /* ___PYJIT_TYPE_H__ */

