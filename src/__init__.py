# python-libjit, Copyright 2014 Niklas Koep
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from _ctypes import CFuncPtr, FUNCFLAG_CDECL

from _jit import *

class Closure(CFuncPtr):
    # XXX: This should depend on the calling convention of the wrapped
    #      function. Unfortunately it means we have to create the wrapper class
    #      dynamically since _flags_ has to be set during class creation. We
    #      could just cache it under the appropriate jit.ABI_* key.
    _flags_ = FUNCFLAG_CDECL

    def __new__(cls, function):
        if not isinstance(function, jit.Function):
            raise TypeError("function must be an instance of jit.Function")
        if not function.is_compiled():
            function.compile_()
        addr = function.to_closure()
        if addr is None:
            raise ValueError("failed to obtain closure from function object")
        return super(Closure, cls).__new__(cls, addr)

    def __init__(self, function):
        super(Closure, self).__init__()
        self._function = function
        # TODO: Use the function signature to annotate the wrapped FuncPtr.
        import ctypes
        self.restype = ctypes.c_int
        self.argtypes = [ctypes.c_int]

del CFuncPtr, FUNCFLAG_CDECL

