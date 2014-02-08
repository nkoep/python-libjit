import unittest

class TestType(unittest.TestCase):
    def test_constructors(self):
        with self.assertRaises(TypeError):
            jit.Type()

    def test_create_struct(self):
        struct = jit.Type.create_struct([jit.Type.INT, jit.Type.INT])
        self.assertTrue(struct.is_struct())

    def test_create_union(self):
        union = jit.Type.create_union((jit.Type.INT, jit.Type.UINT))
        self.assertTrue(union.is_union())

    def test_create_signature(self):
        signature = jit.Type.create_signature(
            jit.ABI_FASTCALL, jit.Type.INT, (jit.Type.INT,))
        self.assertIsInstance(signature, jit.Type)
        self.assertTrue(signature.is_signature)

    def test_create_pointer(self):
        pointer = jit.Type.INT.create_pointer()
        self.assertIsInstance(pointer, jit.Type)
        self.assertEqual(pointer.get_ref(), jit.Type.INT)
        self.assertIsNone(jit.Type.INT.get_ref())

    def test_create_tagged(self):
        tagged_type = jit.Type.INT.create_tagged(jit.TYPETAG_NAME, data="data")
        self.assertIsInstance(tagged_type, jit.Type)
        self.assertEqual(tagged_type.get_tagged_type(), jit.Type.INT)

    def test_set_names(self):
        struct = jit.Type.create_struct((jit.Type.INT,))
        name = "some_int"
        self.assertTrue(struct.set_names([name]))
        self.assertEqual(struct.get_name(0), name)
        self.assertIsNone(struct.get_name(1))
        # Names must be strings.
        with self.assertRaises(TypeError):
            jit.Type.VOID.set_names((1,))

    def test_set_size_and_alignment(self):
        fields = (jit.Type.INT, jit.Type.VOID_PTR)
        struct = jit.Type.create_struct(fields)
        size = alignment = -1
        struct.set_size_and_alignment(size, alignment)
        # FIXME: For some reason, get_size() returns the optimal size here
        #        whereas get_alignment() just returns -1.
        self.assertIsInstance(struct.get_size(), long)
        self.assertEqual(struct.get_alignment(), alignment)

    def test_set_offset(self):
        struct = jit.Type.create_struct([jit.Type.INT, jit.Type.VOID])
        offset = 7
        struct.set_offset(1, offset)
        self.assertEqual(struct.get_offset(1), offset)

    def test_get_kind(self):
        self.assertEqual(jit.Type.INT.get_kind(), jit.TYPE_INT)

    def test_num_fields(self):
        fields = (jit.Type.INT, jit.Type.VOID)
        struct = jit.Type.create_struct(fields)
        num_fields = struct.num_fields()
        self.assertIsInstance(num_fields, long)
        self.assertEqual(num_fields, len(fields))

    def test_get_field(self):
        field = jit.Type.VOID
        union = jit.Type.create_union([field])
        self.assertEqual(union.get_field(0), field)

    def test_find_name(self):
        self.assertEqual(jit.Type.INT.find_name("name"), jit.INVALID_NAME)
        fields = (jit.Type.INT, jit.Type.VOID)
        struct = jit.Type.create_struct(fields)
        names = ["field1", "field2"]
        struct.set_names(names)
        self.assertEqual(struct.find_name("field2"), names.index("field2"))

    def test_num_params(self):
        signature = jit.Type.create_signature(
            jit.ABI_VARARG, jit.Type.VOID, (jit.Type.VOID,))
        self.assertEqual(signature.num_params(), 1)

    def test_get_return(self):
        signature = jit.Type.create_signature(
            jit.ABI_VARARG, jit.Type.VOID, (jit.Type.VOID,))
        self.assertEqual(signature.get_return(), jit.Type.VOID)
        # Primitive type is not a signature type.
        self.assertIsNone(jit.Type.INT.get_return())

    def test_get_param(self):
        signature = jit.Type.create_signature(
            jit.ABI_CDECL, jit.Type.INT, (jit.Type.INT,))
        param = signature.get_param(0)

        self.assertIsInstance(param, jit.Type)
        self.assertEqual(param.get_kind(), jit.TYPE_INT)
        self.assertIsNone(signature.get_param(2)) # Out-of-range
        self.assertIsNone(jit.Type.INT.get_param(-1)) # Not a signature

    def test_get_abi(self):
        signature = jit.Type.create_signature(
            jit.ABI_VARARG, jit.Type.INT, (jit.Type.INT,))
        self.assertEqual(signature.get_abi(), jit.ABI_VARARG)
        # Non-signature types always return jit.ABI_CDECL.
        self.assertEqual(jit.Type.INT.get_abi(), jit.ABI_CDECL)

    def test_tagged_type(self):
        untagged_type = jit.Type.INT
        # Untagged types should return None.
        self.assertIsNone(untagged_type.get_tagged_type())
        self.assertEqual(untagged_type.get_tagged_kind(), -1)

        kind = jit.TYPETAG_CONST
        type_ = jit.Type.INT.create_tagged(kind)
        underlying = jit.Type.VOID
        type_.set_tagged_type(underlying=underlying)
        self.assertEqual(type_.get_tagged_type(), underlying)
        self.assertEqual(type_.get_tagged_kind(), kind)

    def test_set_get_tagged_data(self):
        tagged_type = jit.Type.INT.create_tagged(jit.TYPETAG_NAME, data=None)
        data = "data"
        tagged_type.set_tagged_data(data=data)
        self.assertEqual(tagged_type.get_tagged_data(), data)

    def test_is_primitive(self):
        self.assertTrue(jit.Type.VOID.is_primitive())

    def test_best_alignment(self):
        self.assertIsInstance(jit.Type.best_alignment(), long)

    def test_normalize(self):
        type_ = jit.Type.VOID
        type_normalized = type_.normalize()
        self.assertEqual(type_, type_normalized)
        self.assertTrue(type_ is type_normalized)

    def test_remove_tags(self):
        tagged_type = jit.Type.INT.create_tagged(jit.TYPETAG_NAME, data="data")
        collapsed_type = tagged_type.remove_tags()
        self.assertNotEqual(tagged_type, collapsed_type)
        self.assertEqual(collapsed_type, jit.Type.INT)

    def test_promote_int(self):
        types = [(jit.Type.SBYTE, jit.Type.INT),
                 (jit.Type.USHORT, jit.Type.UINT),
                 (jit.Type.VOID_PTR, jit.Type.VOID_PTR)]
        for type_, type_int_promoted in types:
            self.assertEqual(type_.promote_int(), type_int_promoted)

    def test_return_via_pointer(self):
        self.assertIsInstance(jit.Type.INT.return_via_pointer(), bool)

    def test_has_tag(self):
        self.assertIsInstance(jit.Type.INT.has_tag(jit.TYPETAG_CONST), bool)
        tagged_type = jit.Type.INT.create_tagged(jit.TYPETAG_NAME, data="data")
        self.assertTrue(tagged_type.has_tag(jit.TYPETAG_NAME))

