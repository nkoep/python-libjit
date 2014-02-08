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

#ifndef __PYJIT_FUNCTION_H__
#define __PYJIT_FUNCTION_H__

#include "pyjit-common.h"

typedef struct {
    PyObject_HEAD
    PyObject *context;
    PyObject *signature;
    jit_function_t function;
    PyObject *weakreflist;
} PyJitFunction;

int pyjit_function_init(PyObject *module);
const PyTypeObject *pyjit_function_get_pytype(void);
PyObject *PyJitFunction_New(jit_function_t function);
int PyJitFunction_Check(PyObject *o);
PyJitFunction *PyJitFunction_Cast(PyObject *o);
int PyJitFunction_Verify(PyJitFunction *o);
PyJitFunction *PyJitFunction_CastAndVerify(PyObject *o);

#endif /* __PYJIT_FUNCTION_H__ */

