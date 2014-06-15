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

def _determine_nint_type(**kwargs):
    if not (len(kwargs) == 1 and "signed" in kwargs):
        raise ValueError("need keyword argument 'signed'")
    signed = kwargs["signed"]
    pointer_size = ctypes.sizeof(ctypes.c_void_p)
    if pointer_size == 4:
        return ctypes.c_int32 if signed else ctypes.c_uint32
    elif pointer_size == 8:
        return ctypes.c_int64 if signed else ctypes.c_uint64
    raise TypeError("unsupported pointer size")

class Closure(_CFuncPtr):
    _flags_ = _FUNCFLAG_CDECL

    _LIBJIT_TO_CTYPES = {
        # Platform-agnostic types
        Type.VOID: None,
        Type.SBYTE: ctypes.c_byte,
        Type.UBYTE: ctypes.c_ubyte,
        Type.USHORT: ctypes.c_ushort,
        Type.SHORT: ctypes.c_int16,
        Type.INT: ctypes.c_int32,
        Type.UINT: ctypes.c_uint32,
        # `nint' represents types with the same width and alignment as pointers
        # on the current platform.
        Type.NINT: _determine_nint_type(signed=True),
        Type.NUINT: _determine_nint_type(signed=False),
        Type.LONG: ctypes.c_long,
        Type.ULONG: ctypes.c_ulong,
        Type.FLOAT32: ctypes.c_float,
        Type.FLOAT64: ctypes.c_double,
        Type.NFLOAT: ctypes.c_longdouble,
        Type.VOID_PTR: ctypes.c_void_p,

        # System types
        Type.SYS_BOOL: ctypes.c_bool,
        Type.SYS_CHAR: ctypes.c_char,
        Type.SYS_SCHAR: ctypes.c_byte,
        Type.SYS_UCHAR: ctypes.c_ubyte,
        Type.SYS_SHORT: ctypes.c_short,
        Type.SYS_USHORT: ctypes.c_ushort,
        Type.SYS_INT: ctypes.c_int,
        Type.SYS_UINT: ctypes.c_uint,
        Type.SYS_LONG: ctypes.c_long,
        Type.SYS_ULONG: ctypes.c_ulong,
        Type.SYS_LONGLONG: ctypes.c_longlong,
        Type.SYS_ULONGLONG: ctypes.c_ulonglong,
        Type.SYS_FLOAT: ctypes.c_float,
        Type.SYS_DOUBLE: ctypes.c_double,
        Type.SYS_LONG_DOUBLE: ctypes.c_longdouble
    }

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

    @staticmethod
    def _convert_libjit_to_ctypes(type_):
        try:
            return Closure._LIBJIT_TO_CTYPES[type_]
        except KeyError:
            pass
        if type_.is_pointer():
            ref_type = type_.get_ref()
            return ctypes.POINTER(Closure._convert_libjit_to_ctypes(ref_type))
        elif type_.is_struct():
            return Closure._create_aggregate_type(ctypes.Structure, type_)
        elif type_.is_union():
            return Closure._create_aggregate_type(ctypes.Union, type_)
        # TODO: Test if the type is tagged with jit.TYPETAG_NAME to obtain a
        #       proper name for the exception.
        raise ValueError(
            "failed to determine ctypes conversion for '%s'" % type_)

    @staticmethod
    def _create_aggregate_type(base_class, type_):
        num_anonymous = 0
        names = set()
        fields = []
        for i in range(type_.num_fields()):
            name = type_.get_name(i)
            # If no name was set for the field, use `num_anonymous' instead. To
            # avoid name collisions, we keep track of already used names.
            if name is None:
                while True:
                    name = str(num_anonymous)
                    num_anonymous += 1
                    if name not in names:
                        break
            elif name in names:
                raise ValueError(
                    "multiple fields with the name '%s' defined" % name)
            names.add(name)

            field = type_.get_field(i)
            fields.append((name, Closure._convert_libjit_to_ctypes(field)))

        type_name = "struct" if base_class is ctypes.Structure else "union"
        return type(type_name, (base_class,), {"_fields_": fields})

