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
from _ctypes import CFuncPtr as _CFuncPtr, FUNCFLAG_CDECL as _FUNCFLAG_CDECL

from _jit import *

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

class Closure(_CFuncPtr):
    _flags_ = _FUNCFLAG_CDECL

    _LIBJIT_TO_CTYPES = {
        Type.VOID: None,
        Type.SBYTE: ctypes.c_byte,
        Type.UBYTE: ctypes.c_ubyte,
        Type.USHORT: ctypes.c_ushort,
        Type.SHORT: ctypes.c_short,
        Type.INT: ctypes.c_int,
        Type.UINT: ctypes.c_uint,
        # XXX: These are either int or long, but which is it?
        # Type.NINT: ...,
        # Type.NUINT: ...,
        Type.LONG: ctypes.c_long,
        Type.ULONG: ctypes.c_ulong,
        Type.FLOAT32: ctypes.c_float,
        Type.FLOAT64: ctypes.c_double,
        Type.NFLOAT: ctypes.c_longdouble,
        Type.VOID_PTR: ctypes.c_void_p,

        # TODO: Add system types.
        Type.SYS_BOOL: ctypes.c_bool
    }

    """
    c_bool	_Bool	bool (1)
    c_char	char	1-character string
    c_wchar	wchar_t	1-character unicode string
    c_byte	char	int/long
    c_ubyte	unsigned char	int/long
    c_short	short	int/long
    c_ushort	unsigned short	int/long
    c_int	int	int/long
    c_uint	unsigned int	int/long
    c_long	long	int/long
    c_ulong	unsigned long	int/long
    c_longlong	__int64 or long long	int/long
    c_ulonglong	unsigned __int64 or unsigned long long	int/long
    c_float	float	float
    c_double	double	float
    c_longdouble	long double	float
    c_char_p	char * (NUL terminated)	string or None
    c_wchar_p	wchar_t * (NUL terminated)	unicode or None
    c_void_p	void *	int/long or None
    """

    def __new__(cls, function):
        if not isinstance(function, Function):
            raise TypeError("function must be an instance of jit.Function")
        if function.get_signature().get_abi() != ABI_CDECL:
            raise ValueError("function must use the CDECL calling convention")
        if not function.is_compiled():
            function.compile_()
        addr = function.to_closure()
        if addr is None:
            raise ValueError("failed to obtain closure from function object")
        return super(Closure, cls).__new__(cls, addr)

    def __init__(self, function):
        super(Closure, self).__init__()
        self._function = function
        signature = function.get_signature()
        self.restype = self._convert_libjit_to_ctypes(signature.get_return())
        self.argtypes = [self._convert_libjit_to_ctypes(signature.get_param(i))
                         for i in range(signature.num_params())]

    def _convert_libjit_to_ctypes(self, type_):
        if type_.is_primitive():
            try:
                return self._LIBJIT_TO_CTYPES[type_]
            except KeyError:
                pass
        # TODO: These two need to recurse over their fields.
        elif type_.is_struct():
            raise NotImplementedError
        elif type_.is_union():
            raise NotImplementedError
        raise ValueError(
            "failed to determine ctypes equivalent of jit.Type '%s'" %
            type_.get_name())

