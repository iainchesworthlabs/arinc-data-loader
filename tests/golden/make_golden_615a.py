#!/usr/bin/env python3
"""An independent writer for the ARINC 615A-3 packets and protocol files, used to make
the golden payloads the C++ tests compare against.

It shares no code with the library. It is written from the tables of section 6.4, from
section 5.3.2.3 (the TFTP packets, WAIT and ABORT) and from Attachment 3 (FIND) of the
standard, with the TFTP layouts from RFCs 1350 and 2347 and the number formats chosen
in docs/arinc615a-codec.md. A mistake in the library has to be matched by the same
mistake here to go unseen. It cannot catch a misreading of the standard that both share;
the places where the standard is unclear are listed in docs/arinc615a-codec.md.

Run it to rewrite tests/golden/golden_615a.hpp:

    python tests/golden/make_golden_615a.py

The scenarios below are mirrored, input for input, by tests/unit/arinc615a_golden_test.cpp.
"""

import pathlib
import struct

VERSION = b"A4"


# ---- Field encoders, one per row of the standard's tables --------------------------------

def u8(v):
    return struct.pack(">B", v)


def u16(v):
    return struct.pack(">H", v)


def zstr(s):
    """TFTP and FIND strings: the characters and a NUL."""
    return s.encode("ascii") + b"\x00"


def ltext(s):
    """Section 6.4's text fields: a length of 8 bits, then the text and a NUL, the length
    counting both. No text at all is a length of 0 and nothing after it."""
    if not s:
        return b"\x00"
    assert len(s) <= 254
    return u8(len(s) + 1) + s.encode("ascii") + b"\x00"


def pct(v):
    """A ratio: three characters, right adjusted, blank filled."""
    assert 0 <= v <= 100
    return ("%3d" % v).encode("ascii")


def protocol_file(*fields):
    """File Length (32 bits, counting itself), Protocol Version, then the fields."""
    body = VERSION + b"".join(fields)
    return struct.pack(">I", 4 + len(body)) + body


# ---- TFTP (RFC 1350, 2347; 615A-3 section 5.3.2) ------------------------------------------

def request(opcode, name, options):
    out = u16(opcode) + zstr(name) + zstr("octet")
    for key, value in options:
        out += zstr(key) + zstr(value)
    return out


RRQ, WRQ, DATA, ACK, ERROR, OACK = 1, 2, 3, 4, 5, 6


def tftp():
    payload = bytes(range(0, 32))
    return {
        "rrq_lui_hex": request(RRQ, "HNPFMS_L.LUI", [("blksize", "1024"), ("tsize", "0"),
                                                      ("port", "1615")]),
        "rrq_checksum_hex": request(RRQ, "APP.LUP", [("checksum_2", "B1C2")]),
        "wrq_lus_hex": request(WRQ, "HNPFMS_L.LUS", [("blksize", "1400"), ("tsize", "87"),
                                                      ("timeout", "2")]),
        "wrq_plain_hex": request(WRQ, "HNPFMS_L.LCS", []),
        "oack_hex": u16(OACK) + zstr("blksize") + zstr("1024") + zstr("checksum_3") +
                    zstr("DEADBEEF"),
        "ack_0_hex": u16(ACK) + u16(0),
        "ack_65535_hex": u16(ACK) + u16(65535),
        "data_1_hex": u16(DATA) + u16(1) + payload,
        "data_65535_hex": u16(DATA) + u16(65535) + payload[:5],
        "data_empty_hex": u16(DATA) + u16(2),
        "error_wait_hex": u16(ERROR) + u16(0) + zstr("WAIT:50"),
        "error_abort_hex": u16(ERROR) + u16(0) + zstr("ABORT:1004"),
        "error_file_not_found_hex": u16(ERROR) + u16(1) + zstr("File not found"),
    }


# ---- FIND (Attachment 3) --------------------------------------------------------------------

def find():
    return {
        "find_request_hex": u16(1) + b"\x00" + b"\x10",
        "find_answer_hex": u16(2) + zstr("HNPFMS") + zstr("FMS") + zstr("L") +
                           zstr("FMS LEFT") + zstr("HNP") + b"\x10",
        "find_answer_empty_hex": u16(2) + b"\x00" * 5 + b"\x10",
    }


# ---- The protocol files of section 6.4 --------------------------------------------------------

def files():
    out = {}

    # 6.4.1 .LCI .LUI .LND .LNO: status code 16, description length 8, description.
    out["initialization_accepted_hex"] = protocol_file(u16(0x0001), ltext(""))
    out["initialization_denied_hex"] = protocol_file(u16(0x1000), ltext("Busy loading"))

    # 6.4.2 .LCL
    def part(pn, amendment, designation):
        return ltext(pn) + ltext(amendment) + ltext(designation)

    def hardware(literal, serial, parts):
        return ltext(literal) + ltext(serial) + u16(len(parts)) + b"".join(parts)

    out["configuration_list_hex"] = protocol_file(
        u16(2),
        hardware("FMS LEFT", "SN0001", [
            part("HNP-HW-0001", "A", "FMGEC 1"),
            part("ACM47-1234-5678", "", "FMS 1 OPS"),
        ]),
        hardware("FMS RIGHT", "SN0002", [part("HNP-HW-0001", "B", "FMGEC 2")]),
    )

    # 6.4.3 .LCS: counter, status, exception timer, estimated time, description.
    out["configuration_status_hex"] = protocol_file(
        u16(7), u16(0x0004), u16(30), u16(120), ltext("Erasing flash"))
    out["configuration_status_idle_hex"] = protocol_file(
        u16(0), u16(0x0001), u16(0), u16(0), ltext(""))

    # 6.4.4 .LUR
    out["upload_request_hex"] = protocol_file(
        u16(2),
        ltext("ACM47.LUH") + ltext("ACM47-1234-5678"),
        ltext("ACM48.LUH") + ltext("ACM4F-5555-6666"),
    )

    # 6.4.5 .LUS: operation status and description, counter, exception timer, estimated
    # time, list ratio, then per load: names, ratio, status, description.
    out["upload_status_hex"] = protocol_file(
        u16(0x0004), ltext("Programming"),
        u16(65535), u16(10), u16(0xFFFF), pct(45),
        u16(2),
        ltext("ACM47.LUH") + ltext("ACM47-1234-5678") + pct(100) + u16(0x0003) + ltext(""),
        ltext("ACM48.LUH") + ltext("ACM4F-5555-6666") + pct(5) + u16(0x0004) + ltext("Erasing"),
    )
    out["upload_status_start_hex"] = protocol_file(
        u16(0x0001), ltext(""), u16(0), u16(0), u16(0), pct(0), u16(0))

    # 6.4.6 .LNR: per file, the name, then the user defined data length and the data.
    out["download_request_hex"] = protocol_file(
        u16(2),
        ltext("LOG1.BIN") + u8(4) + bytes.fromhex("DEADBEEF"),
        ltext("LOG2.BIN") + u8(0),
    )

    # 6.4.7 .LNS
    out["download_status_hex"] = protocol_file(
        u16(0x0002), ltext(""), u16(300), u16(5), u16(60), pct(50),
        u16(2),
        ltext("LOG1.BIN") + u16(0x0003) + ltext(""),
        ltext("LOG2.BIN") + u16(0x1007) + ltext("Disk error"),
    )

    # 6.4.8 .LNL
    out["download_list_hex"] = protocol_file(
        u16(2),
        ltext("LOG1.BIN") + ltext("Maintenance log"),
        ltext("LOG2.BIN") + ltext(""),
    )

    # 6.4.9 .LNA
    out["download_answer_hex"] = protocol_file(
        u16(2), ltext("LOG1.BIN"), ltext("LOG2.BIN"))
    return out


# ---- Output -----------------------------------------------------------------------------------

def hex_literal(name, data):
    """One line if clang-format would keep it on one, else lines of 92 characters."""
    h = data.hex()
    head = "inline constexpr std::string_view %s =" % name
    if len(head) + len(h) + 4 <= 100:
        return '%s "%s";\n' % (head, h)
    lines = [h[i:i + 92] for i in range(0, len(h), 92)]
    body = "\n".join('    "%s"' % line for line in lines)
    return "%s\n%s;\n" % (head, body)


def main():
    out = [
        "#pragma once\n",
        "// Generated by tests/golden/make_golden_615a.py. Do not edit by hand.\n",
        "// Each is the hexadecimal of a packet or file written independently of the library.\n",
        "#include <string_view>\n",
        "namespace adl::test::golden {\n",
    ]
    for group in (tftp(), find(), files()):
        for name, data in group.items():
            out.append(hex_literal(name, data))
    out.append("}  // namespace adl::test::golden\n")
    path = pathlib.Path(__file__).with_name("golden_615a.hpp")
    path.write_text("\n".join(out), encoding="utf-8", newline="\n")
    print("wrote", path)


if __name__ == "__main__":
    main()
