#!/usr/bin/env python3
"""Packs a bank image for the install image's payload (src/lz_unpack.h has the format).

usage: lzpack.py IN.bin OUT.lz
Greedy LZ77 with hash chains; checks its own output with a Python unpacker before writing it.
"""
import struct
import sys
import zlib

MAGIC = 0x5A4C484F
MIN_MATCH = 4
WINDOW = 65535
CHAIN = 256          # candidates tried per position


def ext(n):
    out = bytearray()
    n -= 15
    while n >= 255:
        out.append(255)
        n -= 255
    out.append(n)
    return out


def sequence(lits, off, mlen):
    lit_n = len(lits)
    m = None if off is None else mlen - MIN_MATCH
    token = (min(lit_n, 15) << 4) | (0 if m is None else min(m, 15))
    out = bytearray([token])
    if lit_n >= 15:
        out += ext(lit_n)
    out += lits
    if off is not None:
        out += struct.pack('<H', off)
        if m >= 15:
            out += ext(m)
    return out


def pack(data):
    n = len(data)
    heads = {}
    out = bytearray()
    lit_start = 0
    i = 0

    def insert(p):
        if p + MIN_MATCH <= n:
            heads.setdefault(data[p:p + MIN_MATCH], []).append(p)

    while i < n:
        best_len, best_off = 0, 0
        if i + MIN_MATCH <= n:
            cands = heads.get(data[i:i + MIN_MATCH], ())
            tried = 0
            for p in reversed(cands):
                if i - p > WINDOW:
                    break
                l = MIN_MATCH
                while i + l < n and data[p + l] == data[i + l]:
                    l += 1
                if l > best_len:
                    best_len, best_off = l, i - p
                tried += 1
                if tried >= CHAIN:
                    break
        if best_len >= MIN_MATCH:
            out += sequence(data[lit_start:i], best_off, best_len)
            for p in range(i, i + best_len):
                insert(p)
            i += best_len
            lit_start = i
        else:
            insert(i)
            i += 1
    out += sequence(data[lit_start:n], None, 0)
    return bytes(out)


def unpack(s, raw):
    out = bytearray()
    i = 0

    def rd_ext(v):
        nonlocal i
        if v != 15:
            return v
        while True:
            b = s[i]
            i += 1
            v += b
            if b != 255:
                return v

    while i < len(s):
        t = s[i]
        i += 1
        lit = rd_ext(t >> 4)
        out += s[i:i + lit]
        i += lit
        if i == len(s):
            break
        off = s[i] | s[i + 1] << 8
        i += 2
        mlen = rd_ext(t & 15) + MIN_MATCH
        for _ in range(mlen):
            out.append(out[-off])
    assert len(out) == raw
    return bytes(out)


def main():
    data = open(sys.argv[1], 'rb').read()
    if len(data) % 4:
        sys.exit('lzpack: image size not a multiple of 4')
    stream = pack(data)
    if unpack(stream, len(data)) != data:
        sys.exit('lzpack: round trip failed')
    crc = zlib.crc32(data) & 0xFFFFFFFF
    payload = struct.pack('<IIII', MAGIC, len(data), crc, len(stream)) + stream
    payload += b'\xff' * (-len(payload) % 4)
    open(sys.argv[2], 'wb').write(payload)


if __name__ == '__main__':
    main()
