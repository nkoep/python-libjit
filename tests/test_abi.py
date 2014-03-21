import unittest

import jit

class TestAbi(unittest.TestCase):
    def test_constants(self):
        # See LibJIT's jit-type.h for these values.
        self.assertEqual(jit.ABI_CDECL, 0)
        self.assertEqual(jit.ABI_VARARG, 1)
        self.assertEqual(jit.ABI_STDCALL, 2)
        self.assertEqual(jit.ABI_FASTCALL, 3)

