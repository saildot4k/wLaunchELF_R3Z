#!/usr/bin/env python3
"""Verify the fixed SMAP and its peer ABI in unpacked LaunchELF executables."""
import argparse
import hashlib
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]


def table(data, magic):
    header = struct.pack("<III8s", magic, 0, 0x100, b"smap")
    offset = data.find(header)
    if offset < 0 or data.find(header, offset + 1) >= 0:
        raise ValueError("Expected one smap version 1.0 table")
    return offset + len(header)


def verify(module_path, executables):
    module = module_path.read_bytes()
    start = table(module, 0x41C00000)
    exports = struct.unpack_from("<9I", module, start)
    if len(set(exports[:4])) != 1 or not all(exports[:8]) or exports[8] != 0:
        raise ValueError("SMAP must retain four reserved and four function exports")
    peers = []
    for name in ("udpfs_ministack.irx", "udpfs_ioman.irx"):
        path = module_path.parent / name
        if not path.is_file():
            path = ROOT / "iop/__precompiled" / name
        peer = path.read_bytes()
        pos = table(peer, 0x41E00000)
        while struct.unpack_from("<I", peer, pos)[0] == 0x03E00008:
            ordinal = struct.unpack_from("<I", peer, pos + 4)[0] & 0xFFFF
            if ordinal >= 8:
                raise ValueError(f"{name} imports absent SMAP ordinal {ordinal}")
            pos += 8
        peers.append((name, peer))
    old = (ROOT / "iop/__precompiled/udpfs_smap.irx").read_bytes()
    if module == old:
        raise ValueError("SMAP is still the defective historical module")
    for executable in executables:
        elf = executable.read_bytes()
        if not elf.startswith(b"\x7fELF"):
            raise ValueError(f"{executable}: expected an unpacked ELF")
        if module not in elf or old in elf:
            raise ValueError(f"{executable}: fixed SMAP missing or old SMAP still embedded")
        for name, peer in peers:
            if peer not in elf:
                raise ValueError(f"{executable}: checked peer {name} is not embedded")
        print(f"PASS {executable.name}: fixed SMAP ({len(module)} bytes), peer ABI preserved, "
              f"SHA-256 {hashlib.sha256(module).hexdigest()}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("module", type=Path)
    parser.add_argument("executables", nargs="+", type=Path)
    args = parser.parse_args()
    verify(args.module, args.executables)
