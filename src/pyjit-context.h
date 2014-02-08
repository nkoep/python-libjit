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

#ifndef ___PYJIT_CONTEXT_H__
#define ___PYJIT_CONTEXT_H__

#include "pyjit-common.h"

typedef struct {
    PyObject_HEAD
    jit_context_t context;
    PyObject *weakreflist;
} PyJitContext;

int pyjit_context_init(PyObject *module);
const PyTypeObject *pyjit_context_get_pytype(void);
PyObject *PyJitContext_New(jit_context_t context);
int PyJitContext_Check(PyObject *o);
PyJitContext *PyJitContext_Cast(PyObject *o);
int PyJitContext_Verify(PyJitContext *o);
PyJitContext *PyJitContext_CastAndVerify(PyObject *o);

#endif /* ___PYJIT_CONTEXT_H__ */

