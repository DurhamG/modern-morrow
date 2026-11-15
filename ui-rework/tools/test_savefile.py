"""Tests of savefile.py, on a small save built from bytes. Run them with:

python -m unittest discover -s ui-rework/tools -p "test_*.py"
"""

import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import savefile  # noqa: E402


def subrecord(name, data):
    return name.encode() + struct.pack("<I", len(data)) + data


def record(name, *parts):
    data = b"".join(parts)
    return name.encode() + struct.pack("<III", len(data), 0, 0) + data


def save(version=41, extra=()):
    """A save with a header, a SAVE record, a JOUR record and the given extra records."""
    body = [record("SAVE", subrecord("PLNA", b"Lendias\0")), record("JOUR", subrecord("YETA", b"\1\2\3\4"))]
    body += list(extra)
    hedr = struct.pack("<fi", 1.3, 0) + b"\0" * 32 + b"\0" * 256 + struct.pack("<I", len(body))
    header = record("TES3", subrecord("FORM", struct.pack("<I", version)), subrecord("HEDR", hedr))
    return header + b"".join(body)


class SaveFileTest(unittest.TestCase):
    def test_lists_records_in_order(self):
        data = save(extra=[record("UIST", subrecord("QPIN", b"x"))])
        self.assertEqual([name for name, _, _ in savefile.records(data)], ["TES3", "SAVE", "JOUR", "UIST"])

    def test_reads_the_format_version(self):
        self.assertEqual(savefile.format_version(save(43)), 43)

    def test_sets_the_format_version(self):
        changed = savefile.set_format_version(save(41), 44)
        self.assertEqual(savefile.format_version(changed), 44)
        self.assertEqual(len(changed), len(save(41)))

    def test_strip_removes_the_record_and_fixes_the_count(self):
        data = save(extra=[record("UIST", subrecord("QPIN", b"x"))])
        self.assertEqual(savefile.record_count(data), 3)
        stripped = savefile.strip(data, "UIST")
        self.assertEqual([name for name, _, _ in savefile.records(stripped)], ["TES3", "SAVE", "JOUR"])
        self.assertEqual(savefile.record_count(stripped), 2)

    def test_strip_leaves_other_records_byte_for_byte(self):
        plain = save()
        with_uist = save(extra=[record("UIST", subrecord("QPIN", b"x"))])
        stripped = savefile.strip(with_uist, "UIST")
        # Only the record count differs from a save written without the record.
        self.assertEqual(stripped, plain)

    def test_strip_of_a_missing_record_changes_nothing(self):
        self.assertEqual(savefile.strip(save(), "UIST"), save())

    def test_truncated_file_is_an_error(self):
        with self.assertRaises(savefile.SaveFileError):
            savefile.records(save()[:-3])
        with self.assertRaises(savefile.SaveFileError):
            savefile.format_version(b"ABCD")


if __name__ == "__main__":
    unittest.main()
