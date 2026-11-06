"""Run the real browser sorter and file-type predicates without PS2 hardware."""
import ctypes as C
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


def function(path, signature):
    source = path.read_text()
    start = source.index(signature)
    # These small predicates contain no nested blocks.
    return source[start:source.index("\n}", start) + 2]


class Stats(C.Structure):
    _fields_ = [("attributes", C.c_uint16), ("modified", C.c_uint64)]


class File(C.Structure):
    _fields_ = [("name", C.c_char * 256), ("title", C.c_char * 256), ("stats", Stats)]


def entry(name, directory=False, title="", modified=0):
    file = File()
    file.name = name.encode()
    file.title = title.encode()
    file.stats.attributes = 0x20 if directory else 0
    file.stats.modified = modified
    return file


class SortTests(unittest.TestCase):
    def sorted(self, files, mode=4, descending=0, titles=0, valid_time=1, first=0):
        for name, value in (("file_sort", mode), ("file_sort_descending", descending),
                            ("show_titles", titles), ("time_valid", valid_time)):
            C.c_int.in_dll(library, name).value = value
        data = (File * len(files))(*files)
        library.sort(data, first, len(data) - 1)
        return [file.name.decode() for file in data]

    def mixed_files(self):
        return [entry("z.txt", title="A"), entry("game.BIN"), entry("Z_boot.ELF"),
                entry("Apps", directory=True, title="Z"), entry("save.PSU"),
                entry("second.CFG"), entry("alpha.KELF"), entry("README"),
                entry("photo.JPG"), entry("lang.cnf"), entry("launch.xlf"),
                entry("zFolder.TXT", directory=True, title="A"), entry("notes.TXT"),
                entry("first.cfg"), entry("other.bin"), entry("args.ARG"),
                entry("save.PsV"), entry("bare.")]

    def test_type_groups_and_case_insensitive_order(self):
        expected = ["Apps", "zFolder.TXT", "alpha.KELF", "launch.xlf", "Z_boot.ELF",
                    "args.ARG", "first.cfg", "second.CFG", "lang.cnf", "notes.TXT", "z.txt",
                    "bare.", "README", "game.BIN", "other.bin", "photo.JPG", "save.PSU", "save.PsV"]
        for descending in (0, 1):
            for titles in (0, 1):
                with self.subTest(descending=descending, titles=titles):
                    self.assertEqual(self.sorted(self.mixed_files(), descending=descending, titles=titles), expected)

    def test_every_auto_open_text_type_precedes_other_extensions(self):
        extensions = ["ARG", "CFG", "CHT", "CNF", "INI", "JS", "LUA", "PBT",
                      "TOML", "TXT", "XML", "YAML", "YML"]
        files = [entry("other.A")] + [entry("a." + ext) for ext in reversed(extensions)]
        self.assertEqual(self.sorted(files), ["a." + ext for ext in extensions] + ["other.A"])

    def test_executable_extensions_form_one_name_sorted_group(self):
        files = [entry("z.ELF"), entry("0.txt"), entry("m.XLF"), entry("a.KELF")]
        self.assertEqual(self.sorted(files), ["a.KELF", "m.XLF", "z.ELF", "0.txt"])

    def test_last_extension_missing_extension_and_folder_attributes(self):
        files = [entry("README"), entry("archive.psv.max"), entry("c.PsV"),
                 entry(".hidden"), entry("A.MAX"), entry("empty"), entry("b.")]
        self.assertEqual(self.sorted(files),
                         ["b.", "empty", "README", ".hidden", "A.MAX", "archive.psv.max", "c.PsV"])
        files = [entry("folder.txt", directory=True), entry("app.elf"),
                 entry("archive.kelf", directory=True)]
        self.assertEqual(self.sorted(files), ["archive.kelf", "folder.txt", "app.elf"])

    def test_existing_filename_sort_in_both_directions(self):
        files = [entry("zeta.bin"), entry("DirZ", directory=True), entry("y.elf"),
                 entry("alpha.txt"), entry("A.KELF"), entry("B.ELF"), entry("DirA", directory=True)]
        self.assertEqual(self.sorted(files, mode=1),
                         ["DirA", "DirZ", "B.ELF", "y.elf", "A.KELF", "alpha.txt", "zeta.bin"])
        self.assertEqual(self.sorted(files, mode=1, descending=1),
                         ["DirZ", "DirA", "y.elf", "B.ELF", "zeta.bin", "alpha.txt", "A.KELF"])

    def test_existing_title_sort_keeps_titled_entries_first(self):
        files = [entry("dirA", True, "B"), entry("no.bin"), entry("z.ELF"),
                 entry("a.txt", title="Z"), entry("dirNo", True), entry("a.elf"),
                 entry("dirZ", True, "A"), entry("z.cfg", title="A")]
        self.assertEqual(self.sorted(files, mode=2),
                         ["dirZ", "dirA", "dirNo", "a.elf", "z.ELF", "z.cfg", "a.txt", "no.bin"])
        self.assertEqual(self.sorted(files, mode=2, descending=1),
                         ["dirA", "dirZ", "dirNo", "z.ELF", "a.elf", "a.txt", "z.cfg", "no.bin"])

    def test_existing_timestamp_sort_and_fallback(self):
        files = [entry("dirA", True, "Z", 2), entry("z.ELF", modified=1),
                 entry("a.txt", title="Z", modified=2), entry("dirZ", True, "A", 1),
                 entry("a.ELF", modified=2), entry("z.bin", title="A", modified=1)]
        self.assertEqual(self.sorted(files, mode=3),
                         ["dirZ", "dirA", "z.ELF", "a.ELF", "z.bin", "a.txt"])
        self.assertEqual(self.sorted(files, mode=3, descending=1),
                         ["dirA", "dirZ", "a.ELF", "z.ELF", "a.txt", "z.bin"])
        self.assertEqual(self.sorted(files, mode=3, valid_time=0),
                         ["dirA", "dirZ", "a.ELF", "z.ELF", "a.txt", "z.bin"])
        self.assertEqual(self.sorted(files, mode=3, valid_time=0, titles=1),
                         ["dirZ", "dirA", "a.ELF", "z.ELF", "z.bin", "a.txt"])

    def test_no_sort_parent_entry_and_small_lists(self):
        files = self.mixed_files()
        self.assertEqual(self.sorted(files, mode=0), [file.name.decode() for file in files])
        self.assertEqual(self.sorted([], mode=4), [])
        self.assertEqual(self.sorted([entry("one")]), ["one"])
        files.insert(0, entry("..", directory=True))
        self.assertEqual(self.sorted(files, first=1)[0], "..")

    def test_type_comparator_is_consistent(self):
        self.sorted([], mode=4)
        files = self.mixed_files()
        for a in files:
            self.assertEqual(library.cmpFile(C.byref(a), C.byref(a)), 0)
            for b in files:
                ab = library.cmpFile(C.byref(a), C.byref(b))
                ba = library.cmpFile(C.byref(b), C.byref(a))
                self.assertEqual((ab > 0) - (ab < 0), (ba < 0) - (ba > 0))


PLATFORM = r'''
#include <stdint.h>
#include <string.h>
#include <strings.h>
#define GUI_SORT_H
#define FILER_SHARED_H
#define TRUE 1
#define FALSE 0
#define sceMcFileAttrSubdir 0x20
#define stricmp strcasecmp
typedef uint64_t u64;
typedef struct { uint16_t AttrFile; uint64_t _Modify; } FileStats;
typedef struct { char name[256]; unsigned char title[256]; FileStats stats; } FILEINFO;
int file_sort, file_sort_descending, time_valid, show_titles;
int fileShowTitles(void) { return show_titles; }
'''

with tempfile.TemporaryDirectory(prefix="wle-sort-tests-") as directory:
    path = Path(directory)
    source = (PLATFORM + function(ROOT / "src/filer_fileops.c", "int genCmpFileExt(") + "\n" +
              function(ROOT / "src/main_fileops.c", "int IsTextEditorFileType(") +
              '\n#include "src/gui_sort.c"\n')
    (path / "sort.c").write_text(source)
    compiler = shlex.split(os.environ.get("CC", "cc"))
    flags = shlex.split(os.environ.get("CFLAGS", ""))
    subprocess.run(compiler + flags + ["-std=c99", "-Wall", "-Wextra", "-Werror", "-shared", "-fPIC",
                   "-I" + str(ROOT), "-I" + str(ROOT / "include"), str(path / "sort.c"),
                   "-o", str(path / "sort.so")], check=True)
    library = C.CDLL(str(path / "sort.so"))
    library.sort.argtypes = [C.POINTER(File), C.c_int, C.c_int]
    library.sort.restype = None
    library.cmpFile.argtypes = [C.POINTER(File), C.POINTER(File)]
    library.cmpFile.restype = C.c_int
    result = unittest.TextTestRunner().run(unittest.defaultTestLoader.loadTestsFromTestCase(SortTests))
    if not result.wasSuccessful():
        raise SystemExit(1)
