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

#ifndef __PYJIT_INSN_H__
#define __PYJIT_INSN_H__

#include "pyjit-common.h"

typedef jit_value_t (*pyjit_unaryfunc)(jit_function_t, jit_value_t);
typedef jit_value_t (*pyjit_binaryfunc)(
    jit_function_t, jit_value_t, jit_value_t);

typedef struct {
    PyObject_HEAD
    /* XXX: An insn is owned by a jit_block_t which in turn is owned by a
     *      jit_function_t. The function field should thus be changed to a
     *      block field once the jit.Block type is available.
     */
    PyObject *function;
    jit_insn_t insn;
    PyObject *weakreflist;
} PyJitInsn;

int pyjit_insn_init(PyObject *module);
const PyTypeObject *pyjit_insn_get_pytype(void);
PyObject *pyjit_insn_unary_method(
    PyObject *func, PyObject *value1, pyjit_unaryfunc unaryfunc);
PyObject *pyjit_insn_binary_method(
    PyObject *func, PyObject *value1, PyObject *value2,
    pyjit_binaryfunc binaryfunc);

PyObject *PyJitInsn_New(jit_insn_t insn, PyObject *function);
int PyJitInsn_Check(PyObject *o);
PyJitInsn *PyJitInsn_Cast(PyObject *o);
int PyJitInsn_Verify(PyJitInsn *o);
PyJitInsn *PyJitInsn_CastAndVerify(PyObject *o);

#endif /* __PYJIT_INSN_H__ */

