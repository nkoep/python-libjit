import unittest
import ctypes

import jit

class TestClosure(unittest.TestCase):
    def test_closure(self):
        with jit.Context() as context:
            signature = jit.Type.create_signature(
                jit.ABI_CDECL, jit.Type.INT, (jit.Type.INT,))
            function = jit.Function(context, signature)
            function.insn_return(function.value_get_param(0) * 3)
            closure = jit.Closure(function)
            self.assertEqual(closure(5), 15)

    def test_libjit_to_ctypes_conversion(self):
        convertfunc = jit.Closure._convert_libjit_to_ctypes

        # Test a few basic conversions.
        self.assertIs(convertfunc(jit.Type.VOID), None)
        self.assertIs(convertfunc(jit.Type.INT), ctypes.c_int)
        self.assertIs(convertfunc(jit.Type.VOID_PTR), ctypes.c_void_p)

        # Try converting a signature (which should fail).
        signature = jit.Type.create_signature(jit.ABI_CDECL, None, ())
        with self.assertRaises(ValueError):
            convertfunc(signature)

        # Create a variant data type, i.e. a struct with an integer tag and an
        # anonymous union to represent the actual value of the variant. To test
        # pointer conversions, we also add a `char *name' field.
        union = jit.Type.create_union((jit.Type.FLOAT32, jit.Type.FLOAT64))
        char_pointer = jit.Type.SYS_CHAR.create_pointer()
        struct = jit.Type.create_struct((jit.Type.INT, char_pointer, union))
        names = ("tag", "name", "value")
        struct.set_names(names)

        # Convert to ctypes.
        ctypes_struct = convertfunc(struct)
        # There is no assertIsSubclass, see http://bugs.python.org/issue14819
        self.assertTrue(issubclass(ctypes_struct, ctypes.Structure))
        fields = ctypes_struct._fields_

        # Test the names.
        for idx, name in enumerate(names):
            name = fields[idx][0]
            self.assertEqual(name, names[idx])

        # Check the struct fields.
        self.assertIs(fields[0][1], ctypes.c_int32)
        self.assertIs(fields[1][1]._type_, ctypes.c_char)

        # Check the union fields.
        ctypes_union = fields[2][1]
        # See above.
        self.assertTrue(issubclass(ctypes_union, ctypes.Union))
        fields = ctypes_union._fields_
        # We did not specify names for the union fields so they should have
        # been filled automatically.
        types = (ctypes.c_float, ctypes.c_double)
        for idx in range(len(fields)):
            self.assertEqual(fields[idx][0], str(idx))
            self.assertIs(types[idx], types[idx])

