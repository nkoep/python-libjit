import unittest

class TestJitFunctions(unittest.TestCase):
    def test_jit_functions(self):
        self.assertIsInstance(jit.uses_interpreter(), bool)
        self.assertIsInstance(jit.supports_threads(), bool)
        self.assertIsInstance(jit.supports_virtual_memory(), bool)
        self.assertIsInstance(jit.supports_closures(), bool)

    def test_diagnostic_routines(self):
        import os

        context = jit.Context()
        signature = jit.Type.create_signature(
            jit.ABI_CDECL, jit.Type.VOID, [jit.Type.INT])
        function = jit.Function(context, signature)
        value = function.value_get_param(0)
        jit.Insn.return_(function, value)

        with open(os.devnull) as f:
            jit.dump_type(f, signature)
            jit.dump_value(f, function, value, "prefix")
            # TODO: Requires jit.Block and the ability to iterate over it.
            # jit.dump_insn(f, function, insn)
            jit.dump_function(f, function, "name")

