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

#ifndef __PYJIT_MARSHAL_H__
#define __PYJIT_MARSHAL_H__

#include "pyjit-common.h"

int pyjit_marshal_arg_list_from_py(
    PyObject *args, jit_type_t signature, void ***out_args);
void pyjit_marshal_free_arg_list(void **args, unsigned int num_params);
PyObject *pyjit_marshal_arg_to_py(jit_type_t type, void *arg);

#endif /* __PYJIT_MARSHAL_H__ */

