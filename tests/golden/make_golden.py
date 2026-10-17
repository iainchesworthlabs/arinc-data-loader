#!/usr/bin/env python3
"""An independent writer for ARINC 665-3 files, used to make the golden payloads
the C++ tests compare against.

It shares no code with the library. It is written from the tables in section 2
and 3 and from Appendix C of the standard, with the CRC from the sample program
in Appendix K and the digests from Python's hashlib, so a mistake in the library
has to be matched by the same mistake here to go unseen. It cannot catch a
misreading of the standard that both share; the places where the standard is
unclear are listed in docs/arinc665-coverage.md.

Run it to rewrite tests/golden/golden_payloads.hpp:

    python tests/golden/make_golden.py

The scenario below is mirrored, input for input, by tests/unit/golden_test.cpp.
"""

import hashlib
import pathlib
import struct


# ---- Appendix K: table-driven, most significant bit first, not reflected ------

def crc(data, width, poly, init, xor_out):
    top = 1 << (width - 1)
    mask = (1 << width) - 1
    table = []
    for i in range(256):
        reg = i << (width - 8)
        for _ in range(8):
            reg = ((reg << 1) ^ poly) & mask if reg & top else (reg << 1) & mask
        table.append(reg)
    value = init
    for byte in data:
        value = ((value << 8) & mask) ^ table[((value >> (width - 8)) ^ byte) & 0xFF]
    return value ^ xor_out


def crc8(data):
    return crc(data, 8, 0x01, 0x00, 0x00)


def crc16(data):
    return crc(data, 16, 0x1021, 0xFFFF, 0x0000)


def crc32(data):
    return crc(data, 32, 0x04C11DB7, 0xFFFFFFFF, 0xFFFFFFFF)


# ---- Section 5: check values ---------------------------------------------------

CRC8, CRC16, CRC32, MD5, SHA1 = 1, 2, 3, 4, 5


def check_value_bytes(kind, data):
    if kind == CRC8:
        return struct.pack(">H", crc8(data))  # preceded by a zero byte (section 5.1)
    if kind == CRC16:
        return struct.pack(">H", crc16(data))
    if kind == CRC32:
        return struct.pack(">I", crc32(data))
    if kind == MD5:
        return hashlib.md5(data).digest()
    if kind == SHA1:
        return hashlib.sha1(data).digest()
    raise ValueError(kind)


def cv_block(kind, value):
    """Check value length, type and value; one zero word if there is none."""
    if kind is None:
        return struct.pack(">H", 0)
    return struct.pack(">HH", 4 + len(value), kind) + value


# ---- Fields --------------------------------------------------------------------

def u16(v):
    return struct.pack(">H", v)


def u32(v):
    return struct.pack(">I", v)


def u64(v):
    return struct.pack(">Q", v)


def text(s):
    """A length in characters, then the characters, padded with a NUL to a word."""
    raw = s.encode("ascii")
    return u16(len(raw)) + raw + (b"\x00" if len(raw) % 2 else b"")


def words(b):
    assert len(b) % 2 == 0
    return len(b) // 2


def chain(entries):
    """Entries that each begin with a relative pointer to the next: the size of
    the entry in words, counting the pointer; zero in the last."""
    out = b""
    for i, body in enumerate(entries):
        pointer = 0 if i == len(entries) - 1 else words(body) + 1
        out += u16(pointer) + body
    return out


def check_characters(mmm, ssss_ssss):
    x = 0
    for c in mmm + ssss_ssss.replace("-", ""):
        x ^= ord(c)
    return "%02X" % x


def part_number(mmm, ssss_ssss):
    return mmm + check_characters(mmm, ssss_ssss) + "-" + ssss_ssss


# ---- The scenario ----------------------------------------------------------------

LOAD_PN = part_number("ACM", "1234-5678")  # ACM47-1234-5678, the standard's Appendix E example
DATA1_PN = part_number("ACM", "0001-0001")
DATA2_PN = part_number("ACM", "0002-0002")
BATCH_PN = part_number("ACM", "0100-0200")
MEDIA_SET_PN = "ACM-SET-0001"

DATA1 = bytes(range(16))
DATA2 = b"HELLO"  # five bytes: three words, the last one half full
CONFIG = b"ABCD"
USER_DATA = bytes([0xDE, 0xAD, 0xBE, 0xEF])
NON_LOAD = b"XYZ"

HEADER_NAME = "ACM47.LUH"


def header_file(load_crc_value=0, check_value=b"", with_final=True):
    """Table 2.2.3-1 / Figure C-1."""
    # (name, pn, content, check type) in the order they are listed.
    data = [("APP.LUP", DATA1_PN, DATA1, CRC32), ("DATA.LUP", DATA2_PN, DATA2, None)]
    support = [("CONFIG.LDR", "", CONFIG, MD5)]

    load_pn = text(LOAD_PN)
    load_type = text("OPS") + u16(0x0001)
    target_ids = u16(2) + text("ACME") + text("0A2B")
    positions = u16(1) + text("ACME") + u16(2) + text("L") + text("R")

    data_entries = []
    for name, pn, content, kind in data:
        cv = cv_block(kind, check_value_bytes(kind, content) if kind else b"")
        data_entries.append(
            text(name) + text(pn) + u32((len(content) + 1) // 2) + u16(crc16(content))
            + u64(len(content)) + cv)
    data_files = u16(len(data)) + chain(data_entries)

    support_entries = []
    for name, pn, content, kind in support:
        cv = cv_block(kind, check_value_bytes(kind, content) if kind else b"")
        support_entries.append(
            text(name) + text(pn) + u32(len(content)) + u16(crc16(content)) + cv)
    support_files = u16(len(support)) + chain(support_entries)

    # Pointers are absolute word offsets; the fixed part is 20 words.
    at = 20
    p_load_pn = at
    at += words(load_pn)
    p_load_type = at
    at += words(load_type)
    p_target = at
    at += words(target_ids)
    p_positions = at
    at += words(positions)
    p_data = at
    at += words(data_files)
    p_support = at
    at += words(support_files)
    p_user = at
    at += words(USER_DATA)
    p_check = at

    check = cv_block(CRC16, check_value) if with_final else cv_block(CRC16, b"\x00\x00")
    body = (load_pn + load_type + target_ids + positions + data_files + support_files
            + USER_DATA + check)
    total = 20 + words(body) + 3  # the Header File CRC and the 32-bit Load CRC
    fixed = (u32(total) + u16(0x8004) + u16(0x0000) + u32(p_load_pn) + u32(p_target)
             + u32(p_data) + u32(p_support) + u32(p_user) + u32(p_load_type)
             + u32(p_positions) + u32(p_check))
    assert words(fixed) == 20
    without_crcs = fixed + body
    return without_crcs, load_crc_value


def finish_header(without_crcs, load_crc_value):
    return without_crcs + u16(crc16(without_crcs)) + u32(load_crc_value)


def build_header():
    without_crcs, _ = header_file(0, b"\x00\x00", with_final=False)
    # The Load Check Value (16-bit CRC) covers the header up to the check value
    # fields, then the data files, then the support files.
    p_check = struct.unpack(">I", without_crcs[36:40])[0]
    covered = without_crcs[: p_check * 2] + DATA1 + DATA2 + CONFIG
    check_value = struct.pack(">H", crc16(covered))
    without_crcs, _ = header_file(0, check_value)
    # The Load CRC covers the header but for its own last two words, then the files.
    with_header_crc = without_crcs + u16(crc16(without_crcs))
    load_crc_value = crc32(with_header_crc + DATA1 + DATA2 + CONFIG)
    return finish_header(without_crcs, load_crc_value)


def batch_file():
    """Table 2.3.1-1."""
    pn = text(BATCH_PN)
    comment = text("Test batch")
    blocks = [
        text("ACME-L") + u16(1) + text(HEADER_NAME) + text(LOAD_PN),
        text("0A2B-1") + u16(2) + text(HEADER_NAME) + text(LOAD_PN)
        + text("ACM48.LUH") + text(part_number("ACM", "5555-6666")),
    ]
    targets = u16(len(blocks)) + chain(blocks)
    p_pn = 8
    p_targets = p_pn + words(pn) + words(comment)
    total = 8 + words(pn) + words(comment) + words(targets) + 1
    body = (u32(total) + u16(0x9004) + u16(0) + u32(p_pn) + u32(p_targets) + pn + comment
            + targets)
    return body + u16(crc16(body))


def finish_list(body):
    return body + u16(crc16(body))


def loads_list(member):
    """Table 3.2.3.1 / Figure C-4."""
    media = text(MEDIA_SET_PN) + bytes([member, 2])
    entry = (text(LOAD_PN) + text(HEADER_NAME) + u16(1) + u16(2) + text("ACME") + text("0A2B"))
    loads = u16(1) + chain([entry])
    p_media = 10
    p_loads = p_media + words(media)
    total = 10 + words(media) + words(loads) + 1
    return finish_list(u32(total) + u16(0xA004) + u16(0) + u32(p_media) + u32(p_loads)
                       + u32(0) + media + loads)


def batches_list(member):
    """Table 3.2.3.3-1."""
    media = text(MEDIA_SET_PN) + bytes([member, 2])
    entry = text(BATCH_PN) + text("ACM47.LUB") + u16(2)
    batches = u16(1) + chain([entry])
    p_media = 10
    p_batches = p_media + words(media)
    total = 10 + words(media) + words(batches) + 1
    return finish_list(u32(total) + u16(0xA004) + u16(0) + u32(p_media) + u32(p_batches)
                       + u32(0) + media + batches)


def files_list(member, listing, check_kind):
    """Table 3.2.3.2 / Figure C-5 (with the File Name field the table has)."""
    entries = []
    for name, path, on_member, content in listing:
        value = check_value_bytes(CRC32, content)
        entries.append(text(name) + text(path) + u16(on_member) + u16(crc16(content))
                       + cv_block(CRC32, value))
    media = text(MEDIA_SET_PN) + bytes([member, 2])
    files = u16(len(entries)) + chain(entries)
    p_media = 12
    p_files = p_media + words(media)
    p_check = p_files + words(files)

    def assemble(value):
        check = cv_block(check_kind, value)
        total = p_check + words(check) + 1
        return (u32(total) + u16(0xA004) + u16(0) + u32(p_media) + u32(p_files) + u32(0)
                + u32(p_check) + media + files + check)

    placeholder = assemble(b"\x00" * len(check_value_bytes(check_kind, b"")))
    value = check_value_bytes(check_kind, placeholder[: p_check * 2])
    return finish_list(assemble(value))


def media_set():
    header = build_header()
    batch = batch_file()
    root = "\\ACM4712345678\\"
    loads = [loads_list(1), loads_list(2)]
    batches = [batches_list(1), batches_list(2)]

    listing = []
    for m in (1, 2):
        listing.append(("LOADS.LUM", "\\", m, loads[m - 1]))
    for m in (1, 2):
        listing.append(("BATCHES.LUM", "\\", m, batches[m - 1]))
    listing += [
        (HEADER_NAME, root, 1, header),
        ("APP.LUP", root, 1, DATA1),
        ("DATA.LUP", root + "disk001\\", 2, DATA2),
        ("CONFIG.LDR", root, 1, CONFIG),
        ("ACM47.LUB", "\\", 2, batch),
        ("NON_LOAD.CRC", "\\", 1, NON_LOAD),
    ]
    files = [files_list(1, listing, SHA1), files_list(2, listing, SHA1)]
    return {"loads": loads, "batches": batches, "files": files, "batch": batch, "header": header}


def hex_literal(name, data):
    h = data.hex()
    lines = [h[i:i + 96] for i in range(0, len(h), 96)]
    body = "\n".join('    "%s"' % line for line in lines)
    return "inline constexpr std::string_view %s =\n%s;\n" % (name, body)


def main():
    m = media_set()
    out = [
        "#pragma once\n",
        "// Generated by tests/golden/make_golden.py. Do not edit by hand.\n",
        "// Each is the hexadecimal of a file written independently of the library.\n",
        "#include <string_view>\n",
        "namespace adl::test::golden {\n",
        hex_literal("header_file_hex", m["header"]),
        hex_literal("batch_file_hex", m["batch"]),
        hex_literal("loads_list_member_1_hex", m["loads"][0]),
        hex_literal("loads_list_member_2_hex", m["loads"][1]),
        hex_literal("batches_list_member_1_hex", m["batches"][0]),
        hex_literal("batches_list_member_2_hex", m["batches"][1]),
        hex_literal("files_list_member_1_hex", m["files"][0]),
        hex_literal("files_list_member_2_hex", m["files"][1]),
        "}  // namespace adl::test::golden\n",
    ]
    path = pathlib.Path(__file__).with_name("golden_payloads.hpp")
    path.write_text("\n".join(out), encoding="utf-8", newline="\n")
    print("wrote", path)
    print("load PN", LOAD_PN, "data PNs", DATA1_PN, DATA2_PN, "batch PN", BATCH_PN)


if __name__ == "__main__":
    main()
