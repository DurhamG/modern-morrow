#!/usr/bin/env python3
"""Runs the checks to make before a release, in order, and stops at the first that fails.

1. The UI data files' check, CI/check_ui_data.py.
2. The harness client's own tests.
3. components-tests and openmw-tests from the build folder.
4. Upstream's Lua tests that need Morrowind's data, scripts/data/morrowind_tests, against the Morrowind folder named
   in the user's openmw.cfg.
5. Every harness scenario, with its checks.

The first three also run in CI. The last two need Morrowind's data, so they only run here.

    python ui-rework/tools/prerelease.py [--build-dir DIR] [--resolution deck]
"""

import argparse
import re
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import omw  # noqa: E402

REPO_ROOT = omw.REPO_ROOT


def morrowind_folder(user_cfg):
    """The Morrowind folder whose Data Files the user's openmw.cfg names."""
    for line in Path(user_cfg).read_text(encoding="utf-8-sig").splitlines():
        match = re.match(r'\s*data\s*=\s*"?(.*?)"?\s*$', line)
        if match and Path(match.group(1)).name.lower() == "data files":
            folder = Path(match.group(1)).parent
            if (folder / "Morrowind.ini").is_file():
                return folder
    raise omw.HarnessError(f"{user_cfg} names no Data Files folder with a Morrowind.ini beside it.")


def lua_tests_config():
    """A folder with a settings.cfg for upstream's Lua tests. Without it, the game minimizes when its window loses
    focus, as when the person running the checks works in another window, and a minimized game stops running the
    tests."""
    folder = omw.AUTOMATION_DIR / "morrowind_tests_config"
    folder.mkdir(parents=True, exist_ok=True)
    (folder / "settings.cfg").write_text("[Video]\nminimize on focus loss = false\n", encoding="utf-8")
    return folder


def program(build_dir, name):
    path = Path(build_dir) / (name + (".exe" if sys.platform == "win32" else ""))
    if not path.is_file():
        raise omw.HarnessError(f"{path} doesn't exist. Build it first.")
    return str(path)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build-dir", default=None, help="the build's binaries (default as omw.py finds them)")
    parser.add_argument("--user-cfg", default=None, help="the openmw.cfg that names Morrowind's Data Files")
    parser.add_argument("--resolution", "-r", nargs="+", default=["deck"], help="resolutions for the scenarios")
    args = parser.parse_args(argv)

    build_dir = Path(args.build_dir) if args.build_dir else omw.default_build_dir()
    python = sys.executable
    try:
        folder = morrowind_folder(args.user_cfg or omw.default_user_cfg())
        steps = [
            ("UI data files", [python, str(REPO_ROOT / "CI" / "check_ui_data.py")]),
            ("harness client tests",
             [python, "-m", "unittest", "discover", "-s", str(REPO_ROOT / "ui-rework" / "tools"), "-p", "test_*.py"]),
            ("components-tests", [program(build_dir, "components-tests")]),
            ("openmw-tests", [program(build_dir, "openmw-tests")]),
            ("Morrowind Lua tests",
             [python, str(REPO_ROOT / "scripts" / "integration_tests.py"), "--omw", program(build_dir, "openmw"),
              "--workdir", str(omw.AUTOMATION_DIR / "morrowind_tests"), "--config", str(lua_tests_config()),
              "--data", str(folder / "Data Files"),
              "--ini", str(folder / "Morrowind.ini"), str(REPO_ROOT / "scripts" / "data" / "morrowind_tests")]),
            ("scenarios", [python, str(Path(__file__).resolve().parent / "omw.py"), "scenario", "all", "-r",
                           *args.resolution]),
        ]
    except omw.HarnessError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    for name, command in steps:
        start = time.monotonic()
        result = subprocess.run(command, cwd=REPO_ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                                errors="replace")
        took = time.monotonic() - start
        if result.returncode != 0:
            lines = result.stdout.splitlines()
            # The lines that name what failed can be anywhere in a long output, such as the scenarios'.
            failures = [line for line in lines[:-30] if "FAILED" in line]
            print(f"FAILED {name} after {took:.0f} s. Its failures and the end of its output:")
            print("\n".join(failures + lines[-30:]))
            return 1
        print(f"ok {name} ({took:.0f} s)")
    print("All checks before a release passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
