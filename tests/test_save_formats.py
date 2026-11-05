#!/usr/bin/env python3
"""Native codec tests; no PS2 hardware or thirdparty checkout required.

Build: cc -shared -fPIC -Iinclude src/save_{formats,crypto,lzari}.c -lz -o /tmp/wle-formats.so
Run:   python3 tests/test_save_formats.py /tmp/wle-formats.so
"""
import ctypes as C
import base64
import hashlib
import hmac
import random
import struct
import sys
import unittest
import zlib

LIBRARY = sys.argv.pop(1) if len(sys.argv) > 1 else "/tmp/wle-formats.so"
lib = C.CDLL(LIBRARY)
libc = C.CDLL(None)
libc.free.argtypes = [C.c_void_p]


class Entry(C.Structure):
    _fields_ = [("name", C.c_char * 33), ("size", C.c_uint32), ("mode", C.c_uint32),
                ("created", C.c_ubyte * 8), ("modified", C.c_ubyte * 8), ("data", C.c_void_p)]


class Archive(C.Structure):
    _fields_ = [("system", C.c_int), ("count", C.c_int), ("root", Entry),
                ("files", C.POINTER(Entry)), ("storage", C.c_void_p)]


lib.saveDecode.argtypes = [C.c_void_p, C.c_size_t, C.c_char_p, C.POINTER(Archive)]
lib.saveEncode.argtypes = [C.POINTER(Archive), C.c_int, C.POINTER(C.c_void_p), C.POINTER(C.c_size_t)]
lib.saveArchiveFree.argtypes = [C.POINTER(Archive)]
lib.ps1CardFormat.argtypes = [C.c_void_p]
lib.ps1CardValidate.argtypes = [C.c_void_p]
lib.ps1CardPut.argtypes = [C.c_void_p, C.POINTER(Archive)]
lib.ps1CardExtract.argtypes = [C.c_void_p, C.c_int, C.POINTER(Archive)]
lib.ps1CardFind.argtypes = [C.c_void_p, C.c_char_p]
lib.ps1CardDelete.argtypes = [C.c_void_p, C.c_char_p]
lib.ps1CardRename.argtypes = [C.c_void_p, C.c_char_p, C.c_char_p]
lib.ps1CardFreeBlocks.argtypes = [C.c_void_p]
lib.ps1CardEncode.argtypes = [C.c_void_p, C.c_int, C.c_void_p, C.c_size_t,
                             C.POINTER(C.c_void_p), C.POINTER(C.c_size_t)]
lib.ps1CardDecode.argtypes = [C.c_void_p, C.c_size_t, C.c_void_p, C.POINTER(C.c_int)]
lib.saveSign.argtypes = [C.c_void_p, C.c_size_t, C.c_int]
lib.saveSha256.argtypes = [C.c_void_p, C.c_size_t, C.c_void_p]
lib.saveMcxCrypt.argtypes = [C.c_void_p, C.c_size_t, C.c_int]
lib.lzari.argtypes = [C.c_void_p, C.c_int, C.c_void_p, C.c_int]


def archive(system=1, name=b"BASLUS-99999TEST", payloads=None):
    if payloads is None:
        payloads = [(name, b"SC\x11\x02" + bytes(range(256)) * 63 + bytes(252))]
    a = Archive(system=system, count=len(payloads))
    a.root.name = name
    a.root.mode = 0x8427 if system == 2 else 0
    entries = (Entry * len(payloads))()
    buffers = []
    for i, (filename, data) in enumerate(payloads):
        b = C.create_string_buffer(data)
        buffers.append(b)
        entries[i].name, entries[i].size = filename, len(data)
        entries[i].mode = 0x8497 if system == 2 else 0
        entries[i].data = C.cast(b, C.c_void_p)
    a.files = entries
    a._keep = (entries, buffers)
    return a


def encode(a, format):
    p, n = C.c_void_p(), C.c_size_t()
    result = lib.saveEncode(C.byref(a), format, C.byref(p), C.byref(n))
    if result < 0:
        raise ValueError("encode failed")
    data = C.string_at(p, n.value)
    libc.free(p)
    return data


def decode(data, filename=b"BASLUS-99999TEST"):
    a = Archive()
    result = lib.saveDecode(data, len(data), filename, C.byref(a))
    try:
        if result < 0:
            return None
        return (a.system, a.root.name, [(a.files[i].name, C.string_at(a.files[i].data, a.files[i].size))
                                       for i in range(a.count)])
    finally:
        lib.saveArchiveFree(C.byref(a))


def w32(data, off, value):
    struct.pack_into("<I", data, off, value)


class SaveFormats(unittest.TestCase):
    def setUp(self):
        self.a = archive()
        self.payload = C.string_at(self.a.files[0].data, self.a.files[0].size)
        self.raw = C.create_string_buffer(131072)
        lib.ps1CardFormat(self.raw)

    def test_ps1_single_save_formats(self):
        self.assertEqual(len(self.payload), 16384)
        for format, header in [(1, 132), (2, 128), (3, 54), (4, 0)]:
            with self.subTest(format=format):
                data = encode(self.a, format)
                self.assertEqual(len(data), len(self.payload) + header)
                self.assertEqual(data[header:], self.payload)
                self.assertEqual(decode(data), (1, self.a.root.name, [(self.a.root.name, self.payload)]))
                self.assertIsNone(decode(data[:-1]))
        mcs = encode(self.a, 2)
        self.assertEqual(mcs[0], 0x51)
        self.assertEqual(mcs[8:10], b"\xff\xff")
        self.assertEqual(__import__("functools").reduce(int.__xor__, mcs[:128]), 0)

    def test_ps2_psv_offsets_and_metadata(self):
        icon = bytearray(964)
        icon[:4] = b"PS2D"
        for off in (0x104, 0x144, 0x184):
            icon[off:off+9] = b"view.icn\0"
        a = archive(2, payloads=[(b"icon.sys", bytes(icon)), (b"view.icn", b"icon-payload"), (b"DATA", b"game")])
        data = encode(a, 1)
        self.assertEqual(struct.unpack_from("<I", data, 100)[0], 3)
        self.assertEqual(struct.unpack_from("<I", data, 120)[0], 5)
        self.assertEqual(struct.unpack_from("<I", data, 68)[0], 160+3*60)
        for off in (76, 84, 92):
            self.assertEqual(struct.unpack_from("<II", data, off), (160+3*60+964, 12))
        self.assertEqual(decode(data)[2], [(b"icon.sys", bytes(icon)), (b"view.icn", b"icon-payload"), (b"DATA", b"game")])
        for off in (100, 120, 160+56, 160+16):
            bad = bytearray(data)
            w32(bad, off, 0xffffffff)
            self.assertIsNone(decode(bytes(bad)), off)

    def test_archive_names_and_duplicate_entries(self):
        for name in (b"../escape", b"..", b"bad/path", b"mc0:name", b"bad\\path", b"\x01control"):
            bad = bytearray(encode(self.a, 1))
            bad[100:120] = name.ljust(20, b"\0")
            self.assertIsNone(decode(bytes(bad)), name)
        a = archive(2, payloads=[(b"DATA", b"x"), (b"DATA", b"y")])
        with self.assertRaises(ValueError):
            encode(a, 1)
        a = archive(2, payloads=[(b"DATA", b"x")])
        a.root.mode = 0x8497
        with self.assertRaises(ValueError):
            encode(a, 1)
        a.root.mode, a.files[0].mode = 0x8427, 0x8000
        with self.assertRaises(ValueError):
            encode(a, 1)

    def test_ps1_cards_all_containers(self):
        self.assertEqual(lib.ps1CardPut(self.raw, C.byref(self.a)), 0)
        for format, length in [(5, 131072), (6, 131072), (7, 0x20f40), (8, 0x20040), (9, 0x20080), (10, 0x200a0)]:
            with self.subTest(format=format):
                p, n = C.c_void_p(), C.c_size_t()
                self.assertEqual(lib.ps1CardEncode(self.raw, format, None, 0, C.byref(p), C.byref(n)), 0)
                data = C.string_at(p, n.value)
                libc.free(p)
                self.assertEqual(n.value, length)
                out, actual = C.create_string_buffer(131072), C.c_int()
                self.assertEqual(lib.ps1CardDecode(data, len(data), out, C.byref(actual)), 0)
                self.assertEqual(out.raw, self.raw.raw)
                self.assertLess(lib.ps1CardDecode(data[:-1], len(data)-1, out, C.byref(actual)), 0)

    def test_card_space_rename_delete_and_chains(self):
        self.assertEqual(lib.ps1CardFreeBlocks(self.raw), 15)
        self.assertEqual(lib.ps1CardPut(self.raw, C.byref(self.a)), 0)
        before = self.raw.raw
        self.assertLess(lib.ps1CardPut(self.raw, C.byref(self.a)), 0)
        self.assertEqual(before, self.raw.raw)
        self.assertEqual(lib.ps1CardFreeBlocks(self.raw), 13)
        self.assertEqual(lib.ps1CardRename(self.raw, self.a.root.name, b"BASLUS-99999NEW"), 0)
        self.assertEqual(lib.ps1CardFind(self.raw, b"BASLUS-99999NEW"), 0)
        self.assertEqual(lib.ps1CardDelete(self.raw, b"BASLUS-99999NEW"), 0)
        self.assertEqual(lib.ps1CardFreeBlocks(self.raw), 15)
        huge = archive(name=b"FULL", payloads=[(b"FULL", b"SC"+bytes(8192*15-2))])
        self.assertEqual(lib.ps1CardPut(self.raw, C.byref(huge)), 0)
        before = self.raw.raw
        self.assertLess(lib.ps1CardPut(self.raw, C.byref(self.a)), 0)
        self.assertEqual(before, self.raw.raw)
        self.assertEqual(lib.ps1CardValidate(self.raw), 0)
        # Cycle a continuation back to the first block; reject without a loop.
        self.raw[256+8] = b"\x00"
        self.raw[256+9] = b"\x00"
        self.assertLess(lib.ps1CardValidate(self.raw), 0)

    def test_fragmented_multiblock_save(self):
        one = archive(name=b"ONE", payloads=[(b"ONE", b"SC"+bytes(8190))])
        self.assertEqual(lib.ps1CardPut(self.raw, C.byref(one)), 0)
        one.root.name, one.files[0].name = b"TWO", b"TWO"
        self.assertEqual(lib.ps1CardPut(self.raw, C.byref(one)), 0)
        self.assertEqual(lib.ps1CardDelete(self.raw, b"ONE"), 0)
        self.assertEqual(lib.ps1CardPut(self.raw, C.byref(self.a)), 0)
        out = Archive()
        self.assertEqual(lib.ps1CardExtract(self.raw, 0, C.byref(out)), 0)
        try:
            self.assertEqual(C.string_at(out.files[0].data, out.files[0].size), self.payload)
        finally:
            lib.saveArchiveFree(C.byref(out))

    def test_xps_variable_headers(self):
        def entry(name, size, mode):
            e = bytearray(250)
            struct.pack_into("<H", e, 0, 250)
            e[2:2+len(name)] = name
            w32(e, 66, size)
            w32(e, 78, ((mode & 255) << 8) | (mode >> 8))
            return e
        data = (b"\r\0\0\0SharkPortSave\0\0\0\0" + struct.pack("<I", 500) + b"a"*500 +
                struct.pack("<I", 0) + bytes(8) + entry(b"BASLUS-99999TEST", 3, 0x8427) +
                entry(b"DATA", 5, 0x8497) + b"hello")
        self.assertEqual(decode(data)[2], [(b"DATA", b"hello")])
        self.assertIsNone(decode(data[:-1]))

    def test_max_lzari_multiple_calls(self):
        payload = struct.pack("<I", 5) + b"DATA".ljust(32, b"\0") + b"hello" + bytes(15)
        for _ in range(2):
            compressed = C.create_string_buffer(4096)
            n = lib.lzari(payload, len(payload), compressed, len(compressed))
            self.assertGreater(n, 4)
            data = (b"Ps2PowerSave" + bytes(4) + b"BASLUS-99999TEST".ljust(32, b"\0") +
                    b"icon.sys".ljust(32, b"\0") + struct.pack("<II", n, 1) + compressed.raw[:n])
            self.assertEqual(decode(data)[2], [(b"DATA", b"hello")])
            bad = bytearray(data)
            w32(bad, 88, 0xffffffff)
            self.assertIsNone(decode(bytes(bad)))

    def test_cbs_encrypted_fixture(self):
        header = bytearray(296)
        header[:4] = b"CFU\0"
        w32(header, 8, 296)
        w32(header, 12, 69)
        # Incorrect compressedSize is common; the bounded payload length wins.
        w32(header, 16, 0xffffffff)
        header[20:36] = b"BASLUS-99999TEST"
        w32(header, 72, 0x8427)
        encrypted = base64.b64decode("t9p7ARv0AZD7pDhdGilVz2sBa2NmMazBCxrPewk=")
        self.assertEqual(decode(bytes(header)+encrypted)[2], [(b"DATA", b"hello")])
        w32(header, 12, 0xffffffff)
        self.assertIsNone(decode(bytes(header)+encrypted))

    def test_small_and_corrupted_inputs(self):
        data = encode(self.a, 1)
        for n in range(134):
            self.assertIsNone(decode(data[:n]))
        rnd = random.Random(51)
        for i in range(3000):
            data = rnd.randbytes(rnd.randrange(2, 512))
            decode(data)

    def test_sha256_independent_vectors(self):
        for n in (0, 1, 55, 56, 63, 64, 65, 130, 131200):
            data = bytes(i % 256 for i in range(n))
            out = C.create_string_buffer(32)
            lib.saveSha256(data, len(data), out)
            self.assertEqual(out.raw, hashlib.sha256(data).digest())

    def test_psv_signing_against_openssl(self):
        try:
            crypto = C.CDLL("libcrypto.so.3")
        except OSError:
            self.skipTest("OpenSSL needed for independent AES reference")
        # AES_KEY is 60 uint32_t words plus an int round count.
        crypto.AES_set_encrypt_key.argtypes = [C.c_void_p, C.c_int, C.c_void_p]
        crypto.AES_set_decrypt_key.argtypes = [C.c_void_p, C.c_int, C.c_void_p]
        crypto.AES_encrypt.argtypes = crypto.AES_decrypt.argtypes = [C.c_void_p]*3

        def aes(block, key, encrypt=False):
            ctx, out = C.create_string_buffer(244), C.create_string_buffer(16)
            setup = crypto.AES_set_encrypt_key if encrypt else crypto.AES_set_decrypt_key
            setup(key, 128, ctx)
            (crypto.AES_encrypt if encrypt else crypto.AES_decrypt)(block, out, ctx)
            return out.raw

        iv = bytes.fromhex("b30ffeedb7dc5eb7133da60d1b6b2cdc")
        for system in (1, 2):
            a = self.a if system == 1 else archive(2, payloads=[(b"DATA", b"hello")])
            data = bytearray(encode(a, 1))
            data[28:48] = bytes(20)
            seed = bytes(data[8:28])
            if system == 1:
                key = bytes.fromhex("ab5abc9fc1f49de6a051dbaefa518859")
                salt = bytes(x^y for x, y in zip(aes(seed[:16], key), iv))
                salt += bytes(x^y for x, y in zip(aes(seed[:16], key, True)[:4], seed[16:]))
            else:
                key = bytes.fromhex("ea02ceef5bb4d2998f611910d77f51c6")
                salt = bytes(x^y for x, y in zip(aes(seed[:16], key), iv))
                salt += bytes(x^y for x, y in zip(aes(seed[16:]+bytes(12), key)[:4], seed[:4]))
            expected = hmac.new(salt, bytes(data), hashlib.sha1).digest()
            self.assertEqual(encode(a, 1)[28:48], expected)


if __name__ == "__main__":
    unittest.main()
