#!/usr/bin/env python3
"""Run native save tests with a C compiler, Python 3 and zlib development files."""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parent.parent
compiler = shlex.split(os.environ.get("CC", "cc"))
flags = shlex.split(os.environ.get("CFLAGS", ""))
libs = shlex.split(os.environ.get("SAVE_TEST_LIBS", "-lz"))
sources = ["src/save_formats.c", "src/save_crypto.c", "src/save_lzari.c"]

with tempfile.TemporaryDirectory(prefix="wle-save-tests-") as directory:
    library = str(Path(directory) / "formats.so")
    manager = str(Path(directory) / "manager")
    subprocess.run(compiler + flags + ["-Wall", "-Wextra", "-shared", "-fPIC", "-Iinclude"] +
                   sources + libs + ["-o", library], cwd=root, check=True)
    subprocess.run([sys.executable, "tests/test_save_formats.py", library], cwd=root, check=True)
    subprocess.run(compiler + flags + ["-Wall", "-Wextra", "-Iinclude", "-include", "tests/save_test_platform.h",
                   "tests/test_save_manager.c"] + sources + ["src/save_manager.c", "src/save_vmc.c"] +
                   libs + ["-o", manager], cwd=root, check=True)
    subprocess.run([manager], cwd=root, check=True)
