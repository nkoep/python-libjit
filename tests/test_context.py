import unittest

import jit

class TestContext(unittest.TestCase):
    def test_constructors(self):
        jit.Context()

    def test_context_locking(self):
        context = jit.Context()
        context.build_start()
        context.build_end()
        with context:
            pass

    def test_meta(self):
        context = jit.Context()
        type_ = 10000
        data = "data"
        context.set_meta(type_, data)
        self.assertEqual(context.get_meta(type_), data)

    def test_meta_numeric(self):
        context = jit.Context()
        context.set_meta_numeric(type_=jit.OPTION_CACHE_LIMIT, data=1)
        self.assertEqual(context.get_meta_numeric(jit.OPTION_CACHE_LIMIT), 1)

    def test_free_meta(self):
        context = jit.Context()
        type_ = 10000
        data = "data"
        context.set_meta(type_, data)
        self.assertEqual(context.get_meta(type_), data)
        context.free_meta(type_)
        self.assertIsNone(context.get_meta(type_))

