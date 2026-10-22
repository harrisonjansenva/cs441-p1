#!/usr/bin/env python3
"""Generate a large MAXFINDER memory file and print the expected maximum.

usage: gen_big.py <int|fp> <N> <out.mem> [seed]

Layout matches the benchmarks: M[0x10] = output address (0x30), M[0x11] = N,
M[0x12] = address of Array[0]. The array starts at 0x1000 (the benchmarks' 0x20
would overlap the output word for large N). Prints the expected max as 0xXXXXXXXX.
"""
import random
import struct
import sys

kind, n, out = sys.argv[1], int(sys.argv[2]), sys.argv[3]
seed = int(sys.argv[4]) if len(sys.argv) > 4 else 441
BASE = 0x1000
rnd = random.Random(seed)

if kind == "int":
    vals = [rnd.randint(-2**31, 2**31 - 1) for _ in range(n)]
    words = [v & 0xFFFFFFFF for v in vals]
    best = max(vals) & 0xFFFFFFFF
elif kind == "fp":
    words, fl = [], []
    for _ in range(n):
        f = struct.unpack("<f", struct.pack("<f", rnd.uniform(-1e6, 1e6)))[0]
        fl.append(f)
        words.append(struct.unpack("<I", struct.pack("<f", f))[0])
    best = struct.unpack("<I", struct.pack("<f", max(fl)))[0]
else:
    sys.exit(__doc__)

with open(out, "w") as f:
    f.write(f"# big maxfinder-{kind}, N={n}\n26, 32\n")
    f.write(f"0x10, 0x30\n0x11, 0x{n:X}\n0x12, 0x{BASE:X}\n")
    for i, w in enumerate(words):
        f.write(f"0x{BASE + i:X}, 0x{w:08X}\n")
print(f"0x{best:08X}")
