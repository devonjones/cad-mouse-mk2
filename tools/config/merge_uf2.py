#!/usr/bin/env python3
"""Merge UF2 files (e.g. firmware + per-unit config) into a single UF2 so
deployment is one drag onto the RPI-RP2 drive.

The RP2040 bootrom decides whether to erase a 4 KB sector from the block
*number*, not its address: blocks block_no // 16 share one "erased" flag, and
the sector erased is that of the first block in the group to arrive. A
naively concatenated file puts the config's blocks in the firmware's last
group, so the config sector is never erased and the new block is ANDed onto
the old one (corrupt CRC). So the output is laid out one 16-block group per
4 KB sector: blocks sorted by address, each sector padded to 16 blocks with
0xFF pages for that same sector, which programming leaves unchanged.

Usage: python3 merge_uf2.py <out.uf2> <in1.uf2> <in2.uf2> [...]
"""
import struct
import sys

BLOCK = 512
PAGE = 256
SECTOR = 4096
PAGES_PER_SECTOR = SECTOR // PAGE
UF2_MAGIC0 = 0x0A324655
UF2_MAGIC1 = 0x9E5D5157
UF2_MAGIC_END = 0x0AB16F30


def sector_aligned(blocks):
    by_sector = {}
    for b in blocks:
        (addr,) = struct.unpack_from("<I", b, 12)
        by_sector.setdefault(addr // SECTOR, {})[addr] = b
    out = []
    template = blocks[0]
    for sector in sorted(by_sector):
        pages = by_sector[sector]
        base = sector * SECTOR
        for i in range(PAGES_PER_SECTOR):
            addr = base + i * PAGE
            if addr in pages:
                out.append(pages[addr])
            else:
                filler = bytearray(template)
                struct.pack_into("<I", filler, 12, addr)
                filler[32:32 + PAGE] = b"\xff" * PAGE
                out.append(filler)
    return out


def main():
    if len(sys.argv) < 4:
        print(f"Usage: python3 {sys.argv[0]} <out.uf2> <in1.uf2> <in2.uf2> [...]")
        sys.exit(1)
    out_path, in_paths = sys.argv[1], sys.argv[2:]

    blocks = []
    targets = {}
    for p in in_paths:
        with open(p, "rb") as f:
            data = f.read()
        if not data or len(data) % BLOCK:
            sys.exit(f"{p}: not a whole number of UF2 blocks")
        for i in range(0, len(data), BLOCK):
            b = bytearray(data[i:i + BLOCK])
            m0, m1 = struct.unpack_from("<II", b, 0)
            (m_end,) = struct.unpack_from("<I", b, BLOCK - 4)
            if (m0, m1, m_end) != (UF2_MAGIC0, UF2_MAGIC1, UF2_MAGIC_END):
                sys.exit(f"{p}: block {i // BLOCK} has bad UF2 magic")
            addr, size = struct.unpack_from("<II", b, 12)
            if size != PAGE or addr % PAGE:
                sys.exit(f"{p}: block {i // BLOCK} at 0x{addr:08X} has a "
                         f"{size}-byte payload; expected 256-byte, "
                         f"page-aligned payloads")
            # Overlapping writes would make the result depend on block
            # order; refuse rather than guess which input wins.
            if addr in targets:
                sys.exit(f"{p}: address 0x{addr:08X} also written by "
                         f"{targets[addr]}")
            targets[addr] = p
            blocks.append(b)

    blocks = sector_aligned(blocks)
    total = len(blocks)
    for i, b in enumerate(blocks):
        struct.pack_into("<I", b, 20, i)
        struct.pack_into("<I", b, 24, total)

    with open(out_path, "wb") as f:
        for b in blocks:
            f.write(b)
    print(f"{out_path}: {total} blocks from {len(in_paths)} inputs")


if __name__ == "__main__":
    main()
