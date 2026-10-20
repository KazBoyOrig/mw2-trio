"""Generate a non-executed synthetic PE header fixture with no game bytes.

Machine, timestamp and marker match the target; the SHA256 must not match.
This distinguishes exact-hash validation from a weaker build-string check.
"""
from pathlib import Path
import struct
import sys

data = bytearray(0x1200)
data[:2] = b'MZ'
struct.pack_into('<I', data, 0x3c, 0x80)
struct.pack_into('<IHHI', data, 0x80, 0x4550, 0x14c, 1, 0x4b7215e1)
struct.pack_into('<H', data, 0x80 + 20, 0xe0)
opt = 0x80 + 24
struct.pack_into('<H', data, opt, 0x10b)
struct.pack_into('<I', data, opt + 28, 0x400000)
sec = opt + 0xe0
data[sec:sec + 8] = b'.rdata\0\0'
struct.pack_into('<IIII', data, sec + 8, 0x1000, 0x2d2000, 0x1000, 0x200)
marker = 0x200 + (0x6d2784 - 0x400000 - 0x2d2000)
data[marker:marker + 4] = b'184\0'
data[0x1000:0x1024] = b'SYNTHETIC FIXTURE; NO GAME CONTENT.\0\0'
Path(sys.argv[1]).write_bytes(data)
print('Created non-executed synthetic guard fixture')
