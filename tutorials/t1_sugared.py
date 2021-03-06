"""Translation of t1.c with syntactic sugar"""

import jit

def run():
    with jit.Context() as context:
        # Build the function signature.
        signature = jit.Type.create_signature(
            jit.ABI_CDECL, jit.Type.INT, [jit.Type.INT] * 3)

        # Create the function object.
        func = jit.Function(context, signature)

        # Construct the function body.
        x = func.value_get_param(0)
        y = func.value_get_param(1)
        z = func.value_get_param(2)

        func.insn_return(x * y + z)

    print "mul_add(3, 5, 2) = %d" % func(3, 5, 2)

