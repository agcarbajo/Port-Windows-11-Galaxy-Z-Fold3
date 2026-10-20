#!/usr/bin/env python3
"""Rebuild the q2q LMH-fixed qcpep8350.sys from the BSP binary.

The Lahaina PEP programs the LMH (limits management) interrupt masks for GSIV 62/63
with values that, on the Galaxy Z Fold3, cause an interrupt storm and a
DPC_WATCHDOG_VIOLATION (0x133). Three ARM64 `mov` immediates are corrected; nothing else
in the image changes. The Authenticode signature and the PE checksum are cleared, so the
output must be signed again (see components/PEP/q2q_pep_lmh, test-signed).

Usage: python make_pep_lmh.py <bsp>/Drivers/SOC/PowerManagement/qcpep8350.sys <output.sys>
"""
import hashlib
import struct
import sys

BSP_SHA256 = 'b5773ed42dcb6010445f92c6b9696eb1b39d617cbb45166e1d6a3a8bbe1a15ff'
PATCHES = ((0x71c24, 0x52b00008, 0x52a80008), (0x71c38, 0x52800028, 0x52b00008), (0x71c40, 0x52800108, 0x52800088))


def rva_offset(raw, rva):
    pe = struct.unpack_from('<I', raw, 0x3c)[0]
    count = struct.unpack_from('<H', raw, pe + 6)[0]
    optional = struct.unpack_from('<H', raw, pe + 20)[0]
    for index in range(count):
        base, size, offset = struct.unpack_from('<III', raw, pe + 24 + optional + index * 40 + 12)
        if base <= rva and rva + 4 <= base + size:
            return offset + rva - base
    raise ValueError('RVA outside file-backed sections')


def unsigned_image(raw):
    pe = struct.unpack_from('<I', raw, 0x3c)[0]
    if raw[:2] != b'MZ' or raw[pe:pe + 4] != b'PE\0\0' or struct.unpack_from('<H', raw, pe + 4)[0] != 0xaa64:
        raise ValueError('Not an ARM64 PE image')
    certificate, size = struct.unpack_from('<II', raw, pe + 24 + 144)
    result = bytearray(raw[:certificate] if size else raw)
    result[pe + 24 + 64:pe + 24 + 68] = bytes(4)    # CheckSum
    result[pe + 24 + 144:pe + 24 + 152] = bytes(8)  # Certificate table directory
    return result


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    raw = open(sys.argv[1], 'rb').read()
    if hashlib.sha256(raw).hexdigest() != BSP_SHA256:
        sys.exit('Unexpected qcpep8350.sys (only the tested BSP 1.0.3528.8900 binary is supported)')
    result = unsigned_image(raw)
    for rva, before, after in PATCHES:
        offset = rva_offset(raw, rva)
        if struct.unpack_from('<I', raw, offset)[0] != before:
            sys.exit('Instruction mismatch at RVA %#x' % rva)
        struct.pack_into('<I', result, offset, after)
    open(sys.argv[2], 'wb').write(result)
    print('Wrote', sys.argv[2], hashlib.sha256(result).hexdigest())


if __name__ == '__main__':
    main()
