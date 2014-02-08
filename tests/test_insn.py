import unittest

class TestInsn(unittest.TestCase):
    def setUp(self):
        context = jit.Context()
        signature = jit.Type.create_signature(
            jit.ABI_CDECL, jit.Type.INT, params=[jit.Type.INT, jit.Type.INT])

        self.function = jit.Function(context, signature)
        self.value0 = jit.Value.get_param(self.function, 0)
        self.value1 = jit.Value.get_param(self.function, 1)

    def test_arithmetic_methods(self):
        self.function.insn_add(self.value0, self.value1)

    def test_logical_methods(self):
        # Unary methods
        methods = ("neg not").split()
        for name in methods:
            meth = getattr(jit.Insn, name)
            value = meth(self.function, self.value0)

        # Binary methods
        methods = ("and or xor eq ne lt le gt ge cmpl cmpg").split()
        for name in methods:
            meth = getattr(jit.Insn, name)
            value = meth(self.function, self.value0, self.value1)

    def test_trigonometric_methods(self):
        methods = ("acos asin atan atan cos cosh exp log log10 sin sinh sqrt "
                   "tan tanh").split()
        for name in methods:
            meth = getattr(jit.Insn, name)
            value = meth(self.function, self.value0)
            self.assertNotEqual(value, self.value0)
            self.assertNotEqual(value, self.value1)

