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

int
pyjit_abi_init(PyObject *module)
{
    typedef struct {
        const char *name;
        jit_abi_t abi;
    } PyJitAbi;

    PyJitAbi *abi_iter;
    static PyJitAbi abis[] = {
        { "ABI_CDECL", jit_abi_cdecl },
        { "ABI_VARARG", jit_abi_vararg },
        { "ABI_STDCALL", jit_abi_stdcall },
        { "ABI_FASTCALL", jit_abi_fastcall },
        { NULL } /* Sentinel */
    };

    for (abi_iter = abis; abi_iter->name; abi_iter++) {
        if (PyModule_AddIntConstant(module, abi_iter->name, abi_iter->abi) < 0)
            return -1;
    }

    return 0;
}

