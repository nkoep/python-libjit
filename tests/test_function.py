import unittest

import jit

class TestFunction(unittest.TestCase):
    def setUp(self):
        context = jit.Context()
        signature = jit.Type.create_signature(
            jit.ABI_CDECL, jit.Type.INT, params=[jit.Type.INT])
        self.function = jit.Function(context, signature)
        self.value = jit.Value.get_param(self.function, 0)
        self.function2 = jit.Function(context, signature)

    def test_functions_are_identical(self):
        self.assertEqual(self.function, self.value.get_function())

    def test_trigonometric_methods(self):
        self.function.insn_acos(self.value)

    def test_function_next(self):
        context = self.function.get_context()
        self.assertEqual(jit.Function.next_(context), self.function)
        self.assertEqual(jit.Function.next_(context, self.function),
                         self.function2)
        with self.assertRaises(TypeError):
            jit.Function.next_(0)
        with self.assertRaises(TypeError):
            jit.Function.next_(context, 0)

    def test_function_previous(self):
        context = self.function.get_context()
        self.assertEqual(jit.Function.previous(context), self.function2)
        self.assertEqual(
            jit.Function.previous(context, self.function2), self.function)
        with self.assertRaises(TypeError):
            jit.Function.previous(0)
        with self.assertRaises(TypeError):
            jit.Function.previous(context, 0)

    def test_function_nested_parent(self):
        context = self.function.get_context()
        signature = self.function.get_signature()
        nested_function = jit.Function(
            context, signature, parent=self.function)
        self.assertEqual(nested_function.get_nested_parent(), self.function)

    def test_signature_with_void_argument(self):
        with jit.Context() as context:
            signature = jit.Type.create_signature(
                jit.ABI_CDECL, jit.Type.INT, [jit.Type.VOID, jit.Type.INT])
            function = jit.Function(context, signature)
            function.insn_return(function.value_get_param(1))
            arg = 220
        self.assertEqual(function(None, arg), arg)

    def test_decorator(self):
        context = jit.Context()
        signature = jit.Type.create_signature(
            jit.ABI_CDECL, jit.Type.INT, [jit.Type.INT])

        # Varargs are forbidden.
        with self.assertRaises(ValueError):
            @jit.builds_function(context, signature)
            def func(x, *args):
                pass

        # Keyword arguments are forbidden.
        with self.assertRaises(ValueError):
            @jit.builds_function(context, signature)
            def func(x, **kwargs):
                pass

        # Default arguments are forbidden.
        with self.assertRaises(ValueError):
            @jit.builds_function(context, signature)
            def func(x=None):
                pass

        # Error out when the function expects another number of arguments than
        # the jit.Type function signature says. Note that jit.Type.VOID is a
        # valid argument in LibJIT.
        with self.assertRaises(ValueError):
            @jit.builds_function(context, signature)
            def func():
                pass

        @jit.builds_function(context, signature)
        def func(x):
            for i in range(5):
                x *= 2
            return x
        self.assertEqual(func(1), 32)

