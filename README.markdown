# python-libjit
This project provides static Python bindings for
[LibJIT](http://www.gnu.org/software/libjit/), formerly part of the
[DotGNU](http://www.gnu.org/software/dotgnu/) project, to add generic
just-in-time compiler functionality to Python.

## Installation
```bash
python2 setup.py install
```

## Naming Conventions
All C API functions are exposed in the **jit** module. For the most part, the
python-libjit API follows a very straight forward naming convention derived
from the corresponding symbol names of the C API. The basic data types used
throughout LibJIT like `jit_context_t`, `jit_function_t`, etc. are represented
by wrapper classes `jit.Context`, `jit.Function` and so on. Note that certain
types like `jit.Insn` or `jit.Label` are not actually user-instantiable; they
will only be returned as the result of certain method invocations.

A C function such as `jit_type_is_primitive` is exposed in Python as
`jit.Type.is_primitive`. Note that every function supports keyword arguments
with the keyword names corresponding to the argument names as listed in
LibJIT's API
[reference](http://www.gnu.org/software/libjit/doc/libjit_toc.html#SEC_Contents).
An important exception to this rule are argument names
which clash with Python builtin names. In this case, a single underscore is
appended to the original name. For instance, the `type` argument of
`jit_insn_return_ptr` corresponds to the keyword argument `type_` in Python. The
same rules apply to method names, e.g. `jit_insn_return` is available as
`jit.Insn.return_`.

Unfortunately, the naming conventions outlined above lead to a significant amount
of static methods. As an example, the majority of methods of `jit.Insn` are
static methods which expect an instance of `jit.Function` as first argument. For
convenience, such methods are therefore additionally exposed as instance methods on
the respective types. They use the original namespace as method prefix to indicate
the original type they belong to. For instance, `jit.Value.get_param` is also
available as `jit.Function.value_get_param`.

LibJIT exposes a variety of primitive types like `jit_int`, `jit_long`, etc. to
call JIT'ed functions. Right now, Python types such as *None*, *bool*, *int*,
*long*, *float*, and *str* are automatically marshaled to appropriate LibJIT C
types when passed to a function like `jit.Function.apply_`.

## Convenience Routines
### Constructors
Types supplied by python-libjit can either be constructed by calling the counterparts
of routines like `jit_context_create`, `jit_function_create`,
`jit_function_create_nested`, etc. by directly invoking the type wrappers, e.g.
`jit.Context()`. In the case of `jit.Function`, nested functions can
be created by passing the optional `parent` argument to the constructor.

### Context Managers
A LibJIT context should always be locked during the construction of function bodies
inside it. In the C API, this is achieved by calling `jit_context_build_start` and
`jit_context_build_end` before and after the construction of a function, respectively.
Apart from calling their Python equivalents, it is also possible to lock and unlock a
context automatically by using a context manager:
```python
context = jit.Context()
with context:
	...
```

### Temporary Values
Functions in LibJIT are constructed by manipulating temporary values. With
python-libjit, the expressiveness of Python can be used to construct more
readable function bodies. Instead of calling `z = jit.Insn.add(func, x, y)`
where `x` and `y` are instances of `jit.Value`, one may simply write `z = x+y`,
assuming `x` is associated with the `jit.Function` instance `func`.

Another convenient feature is automatic constant coersion. Consider a function
which doubles its unary input argument. One way to construct such a function
body is `func.insn_return(x * jit.Value.create_nint_constant(2))`. In
python-libjit, however, numeric constants in expressions involving `jit.Value`
instances are automatically converted to appropriate `jit.Value` constants such
that the function body can simply be written as `func.insn_return(x*2)`.

### Functions
The intended way to invoke JIT'ed functions in LibJIT is to call a method like
`jit.Function.apply_` which expects a single iterable argument containing the
parameters the wrapped C function is supposed to be called with. In python-libjit,
instances of `jit.Function` behave like regular Python functions, e.g.:
```python
context = jit.Context()
signature = jit.Type.create_signature(
	jit.ABI_CDECL, jit.Type.INT, [jit.Type.INT])
function = jit.Function(context, signature)
function.insn_return(function.value_get_param(0) * 2)
function.compile_()
assert function.apply_([110]) == 220
# Treating jit.Function objects like regular functions also makes sure the wrapped
# jit_function_t is compiled prior to its first invocation. The same does not apply
# to jit.Function.apply_.
assert function(142) == 284
```

## Caveats and Notable Differences to the C API
Apart from the use of Python classes to organize LibJIT's C API into
namespaces, there are a few additional differences between LibJIT's C API and
python-libjit.

Certain functions in the C API expect array types and a corresponding
integer argument to inform the callee about the size of the array. Such
arguments are folded into a single iterable argument in python-libjit. The only
requirement is that the argument implements Python's sequence protocol.

## Project Status
The project is still in early development. At this point, the bindings are
fairly incomplete due to the size of the LibJIT API which exposes almost
1000 functions. Additionally, python-libjit currently only targets Python 2.
Support for py3k will be added eventually once the bindings have matured.

### What's Missing?
* [Handling of basic blocks](http://www.gnu.org/software/libjit/doc/libjit_9.html#Basic-Blocks)
* [Exception handling](http://www.gnu.org/software/libjit/doc/libjit_11.html#Exceptions)
* [Breakpoint debugging](http://www.gnu.org/software/libjit/doc/libjit_12.html#Breakpoint-Debugging)
* [ELF binaries support](http://www.gnu.org/software/libjit/doc/libjit_13.html#ELF-Binaries)
* [Closure support](http://www.gnu.org/software/libjit/doc/libjit_14.html#Function-application-and-closures)

## Omitted Features
* [Intrinsics support](http://www.gnu.org/software/libjit/doc/libjit_10.html#Intrinsics)
* [Miscellaneous utility routines](http://www.gnu.org/software/libjit/doc/libjit_14.html#Utility-Routines):
    - Memory allocation
    - Memory set, copy, compare, etc.
	- String operations
	- Metadata handling
	- Stack walking
	- Dynamic libraries

