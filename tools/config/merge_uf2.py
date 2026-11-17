#!/usr/bin/env python3
"""Merge UF2 files (e.g. firmware + per-unit config) into a single UF2 so
deployment is one drag onto the RPI-RP2 drive.

Usage: python3 merge_uf2.py <out.uf2> <in1.uf2> <in2.uf2> [...]
"""
import struct
import sys

BLOCK = 512


def main():
    if len(sys.argv) < 4:
        print(f"Usage: python3 {sys.argv[0]} <out.uf2> <in1.uf2> <in2.uf2> [...]")
        sys.exit(1)
    out_path, in_paths = sys.argv[1], sys.argv[2:]

    blocks = []
    for p in in_paths:
        data = open(p, "rb").read()
        assert len(data) % BLOCK == 0, f"{p}: not a whole number of UF2 blocks"
        blocks += [bytearray(data[i:i + BLOCK])
                   for i in range(0, len(data), BLOCK)]

    total = len(blocks)
    for i, b in enumerate(blocks):
        # Rewrite blockNo (offset 20) and numBlocks (offset 24).
        struct.pack_into("<I", b, 20, i)
        struct.pack_into("<I", b, 24, total)

    with open(out_path, "wb") as f:
        for b in blocks:
            f.write(b)
    print(f"{out_path}: {total} blocks from {len(in_paths)} inputs")


if __name__ == "__main__":
    main()
