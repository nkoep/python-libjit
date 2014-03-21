import unittest

import jit

class TestValue(unittest.TestCase):
    def setUp(self):
        self.context = jit.Context()
        self.signature = jit.Type.create_signature(
            jit.ABI_CDECL, jit.Type.INT, [jit.Type.INT, jit.Type.INT])
        self.function = jit.Function(self.context, self.signature)

        self.param0 = jit.Value.get_param(self.function, 0)
        self.param1 = jit.Value.get_param(self.function, 1)

    def test_constructors(self):
        value = jit.Value.create(self.function, jit.Type.INT)

    def test_get_param(self):
        jit.Value.get_param(self.function, 0)
        with self.assertRaises(IndexError):
            jit.Value.get_param(self.function, -1)

    def test_comparisons(self):
        self.param0 < self.param1
        self.param0 <= self.param1
        self.param0 == self.param1
        self.param0 != self.param1
        self.param0 >= self.param1
        self.param0 > self.param1

    def test_arithmetic_ops(self):
        self.param0 + self.param1
        self.param0 - self.param1
        self.param0 * self.param1
        self.param0 / self.param1
        self.param0 % self.param1

        -self.param0
        +self.param0
        abs(self.param0)

    def test_logical_ops(self):
        self.param0 << self.param1
        self.param0 >> self.param1
        self.param0 & self.param1
        self.param0 ^ self.param1
        self.param0 | self.param1

    def test_marshal_constants(self):
        with jit.Context() as context:
            signature = jit.Type.create_signature(
                jit.ABI_CDECL, jit.Type.INT, [jit.Type.INT])
            function = jit.Function(context, signature)
            function.insn_return(function.value_get_param(0) * 2)
        self.assertEqual(function(110), 220)

    def test_invalidate_values(self):
        # TODO
        return
        with jit.Context() as context:
            signature = jit.Type.create_signature(
                jit.ABI_CDECL, jit.Type.INT, [jit.Type.INT])
            function = jit.Function(context, signature)
            value = function.value_get_param(0) * 2
            function.insn_return(value)
        # XXX: This should invalidate any jit.Value instances created during
        #      the creation of `function'! jit.Function.__call__ calls
        #      `jit_function_compile' internally if necessary which frees all
        #      resources allocated in the respective jit_function_t, including
        #      any temporary jit_value_t pointers. The only way around this
        #      seems to be to keep track of jit.Value references inside
        #      jit.Function and invalidate them as soon as a function is
        #      compiled.
        self.assertEqual(function(110), 220)

