#!/usr/bin/env python3
"""Reads and edits OpenMW save files, for tests that need a save as another OpenMW version writes it.

A save is a list of records. Each record has a 4-character name, the size of its data, an unused word and flags,
and then its data, which is a list of subrecords: a 4-character name, a size, and the data. The first record, TES3,
is the header. Its FORM subrecord holds the save's format version, and its HEDR subrecord ends with the number of
records after the header, in its last four bytes. The author and description before it are fixed-size strings in old
formats and sized strings in newer ones. See components/esm3/esmwriter.cpp and loadtes3.cpp.

    savefile.py records FILE            prints each record's name and size
    savefile.py format FILE [VERSION]   prints the format version, or sets it
    savefile.py strip FILE NAME -o OUT  writes a copy without the records of that name, such as UIST, which is what
                                        an OpenMW version without the controller menus writes
"""

import argparse
import struct
import sys
from pathlib import Path

RECORD_HEADER = struct.Struct("<4sIII")
SUBRECORD_HEADER = struct.Struct("<4sI")


class SaveFileError(Exception):
    pass


def records(data):
    """Each record as (name, start, end), where start is its header and end is just after its data."""
    result = []
    pos = 0
    while pos < len(data):
        if pos + RECORD_HEADER.size > len(data):
            raise SaveFileError(f"The file ends inside a record's header, at byte {pos}.")
        name, size, _, _ = RECORD_HEADER.unpack_from(data, pos)
        end = pos + RECORD_HEADER.size + size
        if end > len(data):
            raise SaveFileError(f"The record {name.decode('latin-1')} at byte {pos} runs past the end of the file.")
        result.append((name.decode("latin-1"), pos, end))
        pos = end
    return result


def subrecords(data, record_start, record_end):
    """Each subrecord of a record as (name, data start, data end)."""
    result = []
    pos = record_start + RECORD_HEADER.size
    while pos < record_end:
        name, size = SUBRECORD_HEADER.unpack_from(data, pos)
        start = pos + SUBRECORD_HEADER.size
        result.append((name.decode("latin-1"), start, start + size))
        pos = start + size
    return result


def header_subrecord(data, wanted):
    found = records(data)
    if not found or found[0][0] != "TES3":
        raise SaveFileError("The file doesn't start with a TES3 header.")
    for name, start, end in subrecords(data, found[0][1], found[0][2]):
        if name == wanted:
            return start, end
    raise SaveFileError(f"The header has no {wanted} subrecord.")


def format_version(data):
    start, _ = header_subrecord(data, "FORM")
    return struct.unpack_from("<I", data, start)[0]


def set_format_version(data, version):
    start, _ = header_subrecord(data, "FORM")
    result = bytearray(data)
    struct.pack_into("<I", result, start, version)
    return bytes(result)


def record_count(data):
    _, end = header_subrecord(data, "HEDR")
    return struct.unpack_from("<I", data, end - 4)[0]


def strip(data, name):
    """A copy of the save without the records of that name, with the header's record count lowered to match."""
    kept = bytearray()
    removed = 0
    for record_name, start, end in records(data):
        if record_name == name:
            removed += 1
        else:
            kept += data[start:end]
    if removed:
        _, end = header_subrecord(bytes(kept), "HEDR")
        count = struct.unpack_from("<I", kept, end - 4)[0]
        struct.pack_into("<I", kept, end - 4, count - removed)
    return bytes(kept)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("records").add_argument("file", type=Path)
    format_parser = commands.add_parser("format")
    format_parser.add_argument("file", type=Path)
    format_parser.add_argument("version", type=int, nargs="?")
    strip_parser = commands.add_parser("strip")
    strip_parser.add_argument("file", type=Path)
    strip_parser.add_argument("name")
    strip_parser.add_argument("-o", "--output", type=Path, required=True)
    args = parser.parse_args(argv)

    try:
        data = args.file.read_bytes()
        if args.command == "records":
            for name, start, end in records(data):
                print(f"{name} {end - start - RECORD_HEADER.size}")
        elif args.command == "format":
            if args.version is None:
                print(format_version(data))
            else:
                args.file.write_bytes(set_format_version(data, args.version))
        else:
            args.output.write_bytes(strip(data, args.name))
    except SaveFileError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
