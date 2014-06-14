"""One-to-one translation of t1.c"""

import jit

def run():
    # Create a context to hold the JIT's primary state.
    context = jit.Context()

    # Lock the context while we build and compile the function.
    jit.Context.build_start(context)

    # Build the function signature.
    params = [jit.Type.INT, jit.Type.INT, jit.Type.INT]
    signature = jit.Type.create_signature(
        jit.ABI_CDECL, jit.Type.INT, params)

    # Create the function object.
    func = jit.Function(context, signature)

    # Construct the function body.
    x = jit.Value.get_param(func, 0)
    y = jit.Value.get_param(func, 1)
    z = jit.Value.get_param(func, 2)

    temp1 = jit.Insn.mul(func, x, y)
    temp2 = jit.Insn.add(func, temp1, z)
    jit.Insn.return_(func, temp2)

    # Compile the function.
    jit.Function.compile_(func)

    jit.Context.build_end(context)

    # Execute the function and print the result.
    args = [3, 5, 2]
    result = jit.Function.apply_(func, args)
    print "mul_add(3, 5, 2) = %d" % result

