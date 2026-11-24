"""Tests of quest_records.py, which writes quest marker review records. Run them with:

python -m unittest discover -s ui-rework/tools -p "test_*.py"
"""

import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import quest_records  # noqa: E402

SHEET = """# A1_1_FindSpymaster

## Stages

Text that isn't the record, with a ```yaml block that isn't the record either```.

## Review record

```yaml
quest: A1_1_FindSpymaster
stages:
  - stage: 1
    marked: ["caius cosades"]
    optional: ["cell:balmora, south wall cornerclub"]
  - stage: 10
    marked: ["caius cosades"]
```
"""


class QuestRecordsTest(unittest.TestCase):
    def setUp(self):
        self.folder = Path(tempfile.mkdtemp())
        (self.folder / "a1_1_findspymaster.md").write_text(SHEET, encoding="utf-8")
        self.records = self.folder / "reviewed"
        quest_records.RECORDS = self.records

    def write_verdicts(self, text):
        path = self.folder / "verdicts.txt"
        path.write_text(text, encoding="utf-8")
        return str(path)

    def test_a_record_gets_each_stages_verdict_and_reason(self):
        verdicts = self.write_verdicts(
            "A1_1_FindSpymaster 1 corrected Elone only gives directions\nA1_1_FindSpymaster 10 right\n")
        self.assertEqual(quest_records.main(["--sheets", str(self.folder), verdicts]), 0)
        text = (self.records / "A1_1_FindSpymaster.yaml").read_text(encoding="utf-8")
        self.assertIn("  - stage: 1\n    verdict: corrected  # Elone only gives directions\n    marked:", text)
        self.assertIn("  - stage: 10\n    verdict: right\n    marked:", text)
        self.assertTrue(text.startswith("# The reviewed quest markers of A1_1_FindSpymaster."))

    def test_a_quest_id_with_spaces_is_read_in_quotes(self):
        (self.folder / "11111 test journal.md").write_text(
            SHEET.replace("A1_1_FindSpymaster", "11111 test journal"), encoding="utf-8")
        verdicts = self.write_verdicts('"11111 test journal" 1 right nothing to do\n"11111 test journal" 10 right\n')
        self.assertEqual(quest_records.main(["--sheets", str(self.folder), verdicts]), 0)
        text = (self.records / "11111 test journal.yaml").read_text(encoding="utf-8")
        self.assertIn("    verdict: right  # nothing to do", text)

    def test_a_stage_without_a_verdict_writes_nothing(self):
        verdicts = self.write_verdicts("A1_1_FindSpymaster 1 corrected Elone only gives directions\n")
        self.assertEqual(quest_records.main(["--sheets", str(self.folder), verdicts]), 1)
        self.assertFalse(self.records.exists())

    def test_a_verdict_for_a_stage_the_sheet_doesnt_have_writes_nothing(self):
        verdicts = self.write_verdicts("A1_1_FindSpymaster 1 right\nA1_1_FindSpymaster 10 right\n"
                                       "A1_1_FindSpymaster 12 right\n")
        self.assertEqual(quest_records.main(["--sheets", str(self.folder), verdicts]), 1)
        self.assertFalse(self.records.exists())

    def test_a_line_that_isnt_a_verdict_writes_nothing(self):
        verdicts = self.write_verdicts("A1_1_FindSpymaster 1 fine\nA1_1_FindSpymaster 10 right\n")
        self.assertEqual(quest_records.main(["--sheets", str(self.folder), verdicts]), 1)
        self.assertFalse(self.records.exists())


if __name__ == "__main__":
    unittest.main()
