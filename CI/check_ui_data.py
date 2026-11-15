#!/usr/bin/env python3
"""Checks the game's UI data files for mistakes that only show while the game runs.

- Every localisation tag such as #{Interface:Cancel} in the C++ code and the MyGUI files names a key in that
  context's en.yaml. A missing key shows on screen as the raw tag.
- Every tag built from a prefix, such as "#{Interface:InventoryTab" + name + "}", has at least one key with that
  prefix. The rest of such a key can't be found by reading the code.
- Every MyGUI layer that the code or a layout puts widgets on exists in openmw_layers.xml. A missing layer hides the
  widgets without an error.
- Every file under files/data is listed in files/data/CMakeLists.txt. An unlisted file is left out of the build's
  resources folder, and so out of every release.
- Every setting in android/settings.cfg exists in files/settings-default.cfg. The game ignores an unknown setting
  there without saying so.

Run it from anywhere: python3 CI/check_ui_data.py. It prints each problem with its file and line, and exits with 1
when there is any.
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DATA = ROOT / "files" / "data"

# Tags in comments that only show what a tag looks like.
IGNORED_TAGS = {("apps/openmw/mwgui/controllerscreen.hpp", "Context:Key")}

TAG = re.compile(r"#\{(\w+):(\w*)")
# The text after a tag that is only the start of a key, such as "#{Interface:InventoryTab" + name.
PREFIX_END = re.compile(r'" *\+')
YAML_KEY = re.compile(r"^([A-Za-z0-9_]+):")
PANEL_LAYER = re.compile(r'createPanelRoot\("(\w+)"')
LAYOUT_LAYER = re.compile(r'layer="(\w+)"')
DEFINED_LAYER = re.compile(r'<Layer name="(\w+)"')
SECTION = re.compile(r"^\[(.+)\]\s*$")
SETTING = re.compile(r"^([^#=][^=]*?)\s*=")


def relative(path):
    return path.relative_to(ROOT).as_posix()


def source_files():
    for folder in ("apps/openmw", "components"):
        for pattern in ("*.cpp", "*.hpp"):
            yield from sorted((ROOT / folder).rglob(pattern))
    yield from sorted(p for p in (DATA / "mygui").iterdir() if p.is_file())


def lines_of(path):
    with open(path, encoding="utf-8", errors="replace") as file:
        yield from enumerate(file, 1)


def l10n_keys(context):
    path = DATA / "l10n" / context / "en.yaml"
    if not path.is_file():
        return None
    keys = set()
    for _, line in lines_of(path):
        match = YAML_KEY.match(line)
        if match:
            keys.add(match.group(1))
    return keys


def check_l10n(problems):
    keys = {}
    for path in source_files():
        name = relative(path)
        for number, line in lines_of(path):
            for match in TAG.finditer(line):
                context, key = match.group(1), match.group(2)
                if (name, f"{context}:{key}") in IGNORED_TAGS:
                    continue
                if context not in keys:
                    keys[context] = l10n_keys(context)
                known = keys[context]
                where = f"{name}:{number}"
                if known is None:
                    problems.append(f"{where}: #{{{context}:{key}}} names a context with no l10n/{context}/en.yaml")
                elif PREFIX_END.match(line, match.end()):
                    if not any(k.startswith(key) for k in known):
                        problems.append(f"{where}: no key in l10n/{context}/en.yaml starts with {key}")
                elif key not in known:
                    problems.append(f"{where}: #{{{context}:{key}}} is not in l10n/{context}/en.yaml")


def check_layers(problems):
    defined = set()
    for _, line in lines_of(DATA / "mygui" / "openmw_layers.xml"):
        defined.update(DEFINED_LAYER.findall(line))
    for path in sorted((ROOT / "apps/openmw/mwgui").glob("*.cpp")):
        for number, line in lines_of(path):
            for layer in PANEL_LAYER.findall(line):
                if layer not in defined:
                    problems.append(f"{relative(path)}:{number}: layer {layer} is not in openmw_layers.xml")
    for path in sorted((DATA / "mygui").glob("*.layout")):
        for number, line in lines_of(path):
            for layer in LAYOUT_LAYER.findall(line):
                if layer not in defined:
                    problems.append(f"{relative(path)}:{number}: layer {layer} is not in openmw_layers.xml")


def check_data_files_listed(problems):
    listed = set()
    for _, line in lines_of(DATA / "CMakeLists.txt"):
        listed.add(line.strip())
    for path in sorted(DATA.rglob("*")):
        if path.is_file() and path.name != "CMakeLists.txt":
            name = path.relative_to(DATA).as_posix()
            if name not in listed:
                problems.append(f"files/data/{name}: not listed in files/data/CMakeLists.txt")


def settings_of(path):
    settings = set()
    section = None
    for number, line in lines_of(path):
        match = SECTION.match(line)
        if match:
            section = match.group(1)
            continue
        match = SETTING.match(line)
        if match and section is not None:
            settings.add((section, match.group(1).strip(), number))
    return settings


def check_android_settings(problems):
    android = ROOT / "android" / "settings.cfg"
    if not android.is_file():
        return
    defaults = {(section, key) for section, key, _ in settings_of(ROOT / "files" / "settings-default.cfg")}
    for section, key, number in sorted(settings_of(android), key=lambda s: s[2]):
        if (section, key) not in defaults:
            problems.append(f"android/settings.cfg:{number}: [{section}] {key} is not in files/settings-default.cfg")


def main():
    problems = []
    check_l10n(problems)
    check_layers(problems)
    check_data_files_listed(problems)
    check_android_settings(problems)
    for problem in problems:
        print(problem)
    if problems:
        print(f"{len(problems)} problem(s) found.")
        return 1
    print("The UI data files are consistent.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
