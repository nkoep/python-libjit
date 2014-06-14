# python-libjit
This project provides static Python bindings for
[LibJIT](http://www.gnu.org/software/libjit/), formerly part of the
[DotGNU](http://www.gnu.org/software/dotgnu/) project, to add generic
just-in-time compiler functionality to Python.

## Project Status
This is an experimental project which currently only targets Python 2; support
for py3k may be added eventually. The documentation below may be incomplete or
inaccurate in parts; certain parts may outline planned rather than actual
behavior. Type marshaling in particular is currently in a sad state. The
bindings are still fairly incomplete due to the size of the LibJIT API which
exposes close to a thousand functions. Furthermore, the provided API may be
changed considerably where needed; features may be added or removed without
concern for API stability. Also note that the code is developed against the
master branch of [LibJIT's git
repository](http://git.savannah.gnu.org/cgit/libjit.git), not a specific
version. Use at your own risk!

## Installation
To install, do the usual
```text
# python2 setup.py install --prefix=/usr
```
dance. To run the test suite or the ported LibJIT tutorials, issue
```text
$ python2 setup.py test
```
or
```text
$ python2 setup.py tutorials
```
respectively.

## Naming Conventions
All C API functions are exposed in the `jit` module. For the most part, the
python-libjit API follows a very straight forward naming convention as derived
from the corresponding symbol names of the C API. The basic data types used
throughout LibJIT like `jit_context_t`, `jit_function_t`, etc. are represented
by wrapper classes `jit.Context`, `jit.Function` and so on. Note that certain
types like `jit.Insn` or `jit.Label` are not actually user-instantiable; they
only appear as return values of other methods.

A C function such as `jit_type_is_primitive` is exposed in Python as
`jit.Type.is_primitive`. Note that every function supports keyword arguments
with the keyword names corresponding to the argument names as listed in
LibJIT's API
[reference](http://www.gnu.org/software/libjit/doc/libjit_toc.html#SEC_Contents).
An important exception to this rule are argument names
which clash with Python builtin names. In this case, a single underscore is
appended to the original name as per PEP 8. For instance, the `type` argument
of `jit_insn_return_ptr` can be provided using the keyword argument `type_`.
The same rules apply to method names, e.g. `jit_insn_return` is exposed as
`jit.Insn.return_`.

Unfortunately, the naming convention outlined above leads to a significant
number of static methods. For instance, the majority of methods of `jit.Insn`
are static methods which expect an instance of `jit.Function` as first
argument.  For convenience, such methods are therefore additionally exposed as
instance methods of more appropriate types. They use the original namespace as
method prefix to indicate the original type they belong to, e.g. the instance
method `jit.Function.value_get_param` provides the same functionality as the
static method `jit.Value.get_param`.

### Constants
In contrast to regular LibJIT functions which are organized into distinct
classes, preprocessor constants are exposed in the `jit` module itself with
their common `JIT` prefix stripped off, e.g. `JIT_TYPE_INT` corresponds to
`jit.TYPE_INT`.

### Primitive Types
When building a function, it is necessary to specify the function's signature
in terms of `jit.Type` instances. Apart from user-defined types such as
structs, LibJIT comes with a set of predefined primitive types. In the C API,
those are global instances of type `jit_type_t` like `jit_type_int`,
`jit_type_uint`, etc. Since they are different from preprocessor constants,
they are provided as class attributes of `jit.Type` itself, e.g. `jit_type_int`
is provided as `jit.Type.INT`, `jit_type_uint` as `jit.Type.UINT` and so on.

## Argument Marshaling
LibJIT provides a variety of primitive C data types like `jit_int`, `jit_long`,
etc. to call JIT'ed functions. Note that these are proper types in the C API,
not meta types like `jit_type_int` which are used to describe function
signatures and aggregate data types like structs and unions. Right now, Python
types such as *NoneType*, *bool*, *int*, *long*, *float*, and *str* are
automatically marshaled to appropriate LibJIT C types when passed to a method
like `jit.Function.apply_`. Types like `jit.Int`, `jit.Long`, etc. may be
provided at some point if necessary. The same holds for aggregate types which
could be expressed similarly to the way *ctypes* handles [structures and
unions](https://docs.python.org/2/library/ctypes.html#structures-and-unions).

## Leveraging Python Features
### Constructors
Types supplied by python-libjit can either be constructed by calling the
counterparts of routines like `jit_context_create`, `jit_function_create`,
`jit_function_create_nested`, etc. or by directly calling the type wrappers,
e.g. `jit.Context()`. In the case of `jit.Function`, nested functions can be
created by passing the optional `parent` argument to the constructor.

### Context Managers
A LibJIT context should always be locked during the construction of function
bodies inside it to avoid modification by other threads. In the C API, this is
achieved by calling `jit_context_build_start` and `jit_context_build_end`
before and after the construction of a function, respectively. Apart from
calling their direct Python counterparts, it is also possible to lock and
unlock a context automatically by using a (Python) context manager:
```python
with jit.Context() as context:
	pass
```

### Temporary Values
Function bodies in LibJIT are built by manipulating temporary values in
three-address form. In Python, the expressiveness of the language can be
leveraged to construct more readable function bodies. Instead of calling
`z = jit.Insn.add(func, x, y)` where `x` and `y` are instances of `jit.Value`,
one may simply write `z = x+y`, assuming `x` is associated with the
`jit.Function` instance `func`. This also implies that it is possible to write
more complicated statements such as `w=x*y+z` without explicitly creating
another temporary value `v=x*y` first, and then calculate `w=v+z`.

Another convenient feature is automatic coercion of constants. Consider a unary
function which doubles its input argument. One way to construct such a function
body is
```python
y = x * jit.Value.create_nint_constant(2)
func.insn_return(y)
```
However, numeric constants in expressions involving `jit.Value` are
automatically converted to appropriate `jit.Value` constants such that the
function body can be simplified to `func.insn_return(x*2)`.

### Calling JIT'ed Functions
One way to invoke JIT'ed functions in LibJIT is to call a method like
`jit_function_apply` with it. The Python equivalent `jit.Function.apply_`
expects a single iterable argument containing the parameters which the wrapped
C function is supposed to be called with. For convenience, it is also possible
to treat instances of `jit.Function` like regular Python callables, e.g.:
```python
context = jit.Context()
signature = jit.Type.create_signature(
	jit.ABI_CDECL, jit.Type.INT, [jit.Type.INT])
function = jit.Function(context, signature)
function.insn_return(function.value_get_param(0) * 2)
function.compile_()
assert function.apply_([110]) == 220
# Treating jit.Function objects like regular functions also makes sure the
# wrapped jit_function_t is compiled prior to its first invocation. The same
# does not apply to jit.Function.apply_.
assert function(142) == 284
```

### Closures
While general function application is facilitated through routines such as
`jit_function_apply` and the like, LibJIT also supports calling functions via
closures. Note that in this context, a closure does not refer to a function
closing over free variables; in LibJIT it merely represents a pointer to a
block of memory containing the JIT'ed function body (in addition to some
metadata). Calling such a closure therefore requires the user to cast the
return value of `jit_function_to_closure` to an appropriate function pointer.
To support fast function calls, python-libjit therefore supplies an auxiliary
`jit.Closure` class which wraps the raw function pointer via *ctypes*.

## Caveats and Notable Differences to the C API
Apart from the use of Python classes to organize LibJIT's API into appropriate
namespaces, there are a few additional differences between the C and Python
APIs. For instance, certain functions in the C API expect array types and
corresponding integer arguments to inform the callee about the size of the
array. Such arguments are folded into a single iterable argument in Python. The
only requirement is that the argument implements the [sequence
protocol](https://docs.python.org/2/extending/newtypes.html#abstract-protocol-support).
Furthermore, when constructing function signatures (see
`jit.Type.create_signature`) `None` is a valid return type which gets
automatically converted to `jit.Type.VOID`.

### What's Missing?
* [Handling of basic blocks](http://www.gnu.org/software/libjit/doc/libjit_9.html#Basic-Blocks)
* [Exception handling](http://www.gnu.org/software/libjit/doc/libjit_11.html#Exceptions)
* [Breakpoint debugging](http://www.gnu.org/software/libjit/doc/libjit_12.html#Breakpoint-Debugging)
* [ELF binaries support](http://www.gnu.org/software/libjit/doc/libjit_13.html#ELF-Binaries)

## Omitted Features
* [Intrinsics support](http://www.gnu.org/software/libjit/doc/libjit_10.html#Intrinsics)
* [Miscellaneous utility routines](http://www.gnu.org/software/libjit/doc/libjit_14.html#Utility-Routines):
    - Memory allocation
    - Memory set, copy, compare, etc.
	- String operations
	- Metadata handling
	- Stack walking
	- Dynamic libraries

