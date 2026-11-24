#!/usr/bin/env python3
"""Writes quest marker review records from review sheets and a file of verdicts.

The quest-review harness command writes a sheet per quest, ending with a review record of what the markers show now.
This script copies that record into ui-rework/quests/reviewed/<quest>.yaml, adding each stage's verdict, and the
reviewer's reason as a comment. Write the sheets after the quest's corrections are in, so the record holds what the
corrected markers show.

A verdicts file has one line per stage: the quest ID, the stage, the verdict (right, corrected or unfixable), and a
short reason in the reviewer's own words, such as:

    A1_1_FindSpymaster 1 corrected Elone only gives directions; Caius is the goal

A quest ID with spaces goes in double quotes.

    python ui-rework/tools/quest_records.py --sheets .automation/quest-review VERDICTS...

It stops without writing anything when a verdict names a stage the sheet doesn't have, or when a sheet's stage has
no verdict.
"""

import argparse
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
RECORDS = REPO_ROOT / "ui-rework" / "quests" / "reviewed"
VERDICTS = ("right", "corrected", "unfixable")


def read_verdicts(paths):
    """Each quest's verdicts, by its ID in lowercase: {stage: (quest ID, verdict, reason)}."""
    verdicts = {}
    problems = []
    for path in paths:
        for number, line in enumerate(Path(path).read_text(encoding="utf-8").splitlines(), start=1):
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            # A quest ID with spaces is in double quotes.
            stripped = line.strip()
            if stripped.startswith('"') and '"' in stripped[1:]:
                end = stripped.index('"', 1)
                parts = [stripped[1:end]] + stripped[end + 1:].split(None, 2)
            else:
                parts = stripped.split(None, 3)
            if len(parts) < 3 or not parts[1].lstrip("-").isdigit() or parts[2] not in VERDICTS:
                problems.append(f"{path}:{number}: not a verdict line: {line!r}")
                continue
            quest, stage, verdict = parts[0], int(parts[1]), parts[2]
            reason = parts[3].strip() if len(parts) > 3 else ""
            verdicts.setdefault(quest.lower(), {})[stage] = (quest, verdict, reason)
    return verdicts, problems


def record_block(sheet):
    """The review record at the bottom of a sheet, as its lines."""
    text = sheet.read_text(encoding="utf-8")
    match = re.search(r"```yaml\n(.*?)```", text[text.rfind("## Review record"):], re.S)
    if not match:
        raise ValueError(f"{sheet} has no review record")
    return match.group(1).splitlines()


def with_verdicts(lines, verdicts, sheet):
    """The record's lines with a verdict after each stage line, and the problems found."""
    result = []
    problems = []
    seen = set()
    for line in lines:
        result.append(line)
        match = re.match(r"  - stage: (-?\d+)$", line)
        if not match:
            continue
        stage = int(match.group(1))
        seen.add(stage)
        if stage not in verdicts:
            problems.append(f"{sheet.name}: stage {stage} has no verdict")
            continue
        _, verdict, reason = verdicts[stage]
        comment = f"  # {reason}" if reason else ""
        result.append(f"    verdict: {verdict}{comment}")
    for stage in sorted(set(verdicts) - seen):
        problems.append(f"{sheet.name}: the verdict for stage {stage} names a stage the record doesn't have")
    return result, problems


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--sheets", required=True, help="the folder the quest-review command wrote the sheets in")
    parser.add_argument("verdicts", nargs="+", help="files of verdicts")
    args = parser.parse_args(argv)

    verdicts, problems = read_verdicts(args.verdicts)
    records = {}
    for key, stages in sorted(verdicts.items()):
        sheet = Path(args.sheets) / f"{key}.md"
        if not sheet.is_file():
            problems.append(f"no sheet for {key} in {args.sheets}")
            continue
        lines, found = with_verdicts(record_block(sheet), stages, sheet)
        problems += found
        quest = next(iter(stages.values()))[0]
        header = [f"# The reviewed quest markers of {quest}.",
                  "# Each stage lists the targets the markers should show, worked out from the content files and",
                  "# questcorrections.yaml, and the reviewer's verdict."]
        records[quest] = "\n".join(header + lines) + "\n"
    if problems:
        print("\n".join(problems), file=sys.stderr)
        return 1
    RECORDS.mkdir(parents=True, exist_ok=True)
    for quest, text in records.items():
        (RECORDS / f"{quest}.yaml").write_text(text, encoding="utf-8", newline="\n")
    print(f"Wrote {len(records)} review record(s) in {RECORDS}.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
