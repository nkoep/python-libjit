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

from functools import wraps
import inspect
import ctypes
from _ctypes import CFuncPtr, FUNCFLAG_CDECL

from _jit import *

# XXX: This would be nice to have as a regular instance method on
#      jit.Context. We cannot monkey-patch the type though since this is not
#      allowed for extension types. Implementing a decorator in C is kind of
#      a hassle on the other hand. Maybe we could use a static extension method
#      in C which just stores the function object of `builds_function' in
#      the extension code and then delegate the call to the proper
#      `builds_function' instance method to the one defined here. Another
#      possibility would be to let the jit.Context c'tor accept a single
#      argument if said delegation object hasn't been set yet. Since we set
#      it here, the type will always be initialized properly. The only problem
#      is that users could theoretically import _jit.Context manually.
def builds_function(context, signature):
    num_params = signature.num_params()

    def decorator(f):
        argspec = inspect.getargspec(f)
        num_params_accepted = len(argspec.args)
        if num_params_accepted != num_params:
            numerus = "" if num_params == 1 else "s"
            raise ValueError(
                "decorated function must accept exactly %d argument%s, not %d"
                % (num_params, numerus, num_params_accepted))

        if argspec.varargs is not None:
            raise ValueError(
                "decorated function must not use varargs")

        if argspec.keywords is not None:
            raise ValueError(
                "decorated function must not use keyword arguments")

        # This seems like an arbitrary constraint. However, it'd be too easy
        # for the user to misinterpret default arguments as something that gets
        # passed to the JIT'ed function when it's called when in fact they are
        # only used during construction of the JIT'ed function.
        if argspec.defaults is not None:
            raise ValueError(
                "decorated function must not use default arguments")

        cache = {}
        def get_compiled_function():
            try:
                function = cache["function"]
            except KeyError:
                function = Function(context, signature)
                args = [function.value_get_param(i) for i in range(num_params)]
                function.insn_return(f(*args))
                function.compile_()
                try:
                    # Assigning to `function' here shadows the original
                    # assignment and raises an UnboundLocalError exception.
                    function = Closure(function)
                except ValueError:
                    pass
                cache["function"] = function
            return function

        @wraps(f)
        def wrapper(*args):
            return get_compiled_function()(*args)
        return wrapper
    return decorator

class Closure(CFuncPtr):
    # XXX: This should depend on the calling convention of the wrapped
    #      function. Unfortunately it means we have to create the wrapper class
    #      dynamically since _flags_ has to be set during class creation. We
    #      could just cache it under the appropriate jit.ABI_* key.
    _flags_ = FUNCFLAG_CDECL

    def __new__(cls, function):
        if not isinstance(function, Function):
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
        self.restype = ctypes.c_int
        self.argtypes = [ctypes.c_int]

