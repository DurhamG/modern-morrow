"""Shared parts of the mock-up generators, such as inventory.py and dialogue.py.

A generator builds MyGUI layout files from real game data and shows them in a running game. The mock-ups are static:
they don't respond to input. The layouts go on the "Mockup" layer, which is sized relative to the screen height, on a
canvas 720 units tall.
"""

import argparse
import json
import sys
from pathlib import Path
from xml.sax.saxutils import quoteattr

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import omw  # noqa: E402

HERE = Path(__file__).resolve().parent
OUT_DIR = omw.AUTOMATION_DIR / "mockups"
PROFILE = "mockup"
CANVAS_HEIGHT = 720

COLOURS = {
    "text": "#caa560",  # Morrowind's normal text colour
    "bright": "#dfc99f",  # Morrowind's header colour
    "dim": "#8c7a55",
    "active": "#ffffff",
    "accent": "#caa560",
    "new": "#7fb3ff",
    "better": "#7fd36b",
    "worse": "#e0675a",
    "stolen": "#e0675a",
    "panel": "#000000",
}

FONT = {"small": 15, "normal": 18, "large": 22, "title": 26}

DEFAULT_START = "Balmora, Caius Cosades' House"


# ---------------------------------------------------------------------------------------------------------------------
# Game data


def read_from_game(lua_file, names, cache, refresh=False):
    """Runs a Lua script in the "mockup" game that prints one JSON object per line, and caches the objects by name.

    The script's "local NAMES = {}" line is filled in with the given names first.
    """
    if cache.is_file() and not refresh:
        return json.loads(cache.read_text(encoding="utf-8"))
    code = (HERE / lua_file).read_text(encoding="utf-8")
    code = code.replace("local NAMES = {}", "local NAMES = {" + ", ".join(json.dumps(n) for n in names) + "}")
    connection = omw.connect(omw.Profile(PROFILE))
    reply = connection.request({"cmd": "lua", "code": code, "context": "player"}, 60)
    if not reply.get("ok"):
        raise SystemExit(f"Reading data from {lua_file} failed: {reply.get('error')}")
    records = {}
    for line in reply["result"]["output"]:
        record = json.loads(line)
        records[record["name"]] = record
    missing = [n for n in names if n not in records]
    if missing:
        print("No records named: " + ", ".join(missing), file=sys.stderr)
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    cache.write_text(json.dumps(records, indent=1), encoding="utf-8")
    return records


# ---------------------------------------------------------------------------------------------------------------------
# Layout building


class Layout:
    """Collects widgets for one MyGUI layout file. Coordinates are canvas units, from the top left."""

    def __init__(self, width):
        self.width = width
        self.widgets = []

    def add(self, widget_type, skin, x, y, w, h, properties):
        props = "".join(f'<Property key="{k}" value={quoteattr(str(v))}/>' for k, v in properties.items())
        self.widgets.append(
            f'<Widget type="{widget_type}" skin="{skin}" position="{int(x)} {int(y)} {int(w)} {int(h)}">'
            f'<Property key="NeedMouse" value="false"/>{props}</Widget>')

    def rect(self, x, y, w, h, colour, alpha=1.0):
        self.add("ImageBox", "ImageBox", x, y, w, h, {"ImageTexture": "white", "Colour": colour, "Alpha": alpha})

    def image(self, x, y, w, h, texture, alpha=1.0, colour=None):
        props = {"ImageTexture": texture, "Alpha": alpha}
        if colour:
            props["Colour"] = colour
        self.add("ImageBox", "ImageBox", x, y, w, h, props)

    def text(self, x, y, w, h, caption, size="normal", colour="text", align="Left VCenter", shadow=True):
        self.add("TextBox", "NormalText", x, y, w, h, {
            "Caption": caption, "FontHeight": FONT.get(size, size), "TextColour": COLOURS.get(colour, colour),
            "TextAlign": align, "TextShadow": "true" if shadow else "false", "TextShadowColour": "0 0 0"})

    def xml(self):
        body = "\n        ".join(self.widgets)
        return ('<?xml version="1.0" encoding="UTF-8"?>\n<MyGUI type="Layout">\n'
                f'    <Widget type="Widget" skin="" layer="Mockup" position="0 0 {self.width} {CANVAS_HEIGHT}">\n'
                f'        {body}\n    </Widget>\n</MyGUI>\n')


def wrap(text, width, size):
    """Splits text into lines that fit the width, guessing at the font's average character width."""
    per_line = max(1, int(width / (FONT.get(size, size) * 0.5)))
    lines, line = [], ""
    for word in text.split():
        if line and len(line) + 1 + len(word) > per_line:
            lines.append(line)
            line = word
        else:
            line = f"{line} {word}".strip()
    lines.append(line)
    return lines


def icon(layout, x, y, size, record, alpha=1.0):
    layout.rect(x, y, size, size, "#1a1610", 0.9)
    layout.image(x + 2, y + 2, size - 4, size - 4, record["icon"], alpha)


def button_glyph(layout, x, y, name, size=22):
    layout.image(x, y, size, size, f"textures/omw_steam_button_{name}.dds", 0.85, "#ccb589")


def chrome(layout, screen, sub_tabs, sub_tab, status):
    """The parts every menu mock-up shares: background, the two rows of tabs and the status line.

    "screen" is the selected top tab, such as "Inventory". "sub_tabs" are the screen's own tabs, and "status" is the
    text at the right end of their row.
    """
    w = layout.width
    layout.rect(0, 0, w, CANVAS_HEIGHT, COLOURS["panel"], 0.97)

    # Top tabs, which switch between the map, inventory, magic and stats screens.
    tabs = ["Map", "Inventory", "Magic", "Stats"]
    tab_w = 120
    x = (w - tab_w * len(tabs)) / 2
    button_glyph(layout, x - 34, 11, "l2")
    for name in tabs:
        selected = name == screen
        layout.text(x, 0, tab_w, 44, name, "large", "active" if selected else "dim", "Center")
        if selected:
            layout.rect(x + 20, 38, tab_w - 40, 2, COLOURS["accent"])
        x += tab_w
    button_glyph(layout, x + 12, 11, "r2")
    layout.rect(0, 44, w, 1, COLOURS["accent"], 0.35)

    # The screen's own tabs.
    x = 32
    button_glyph(layout, x, 59, "l1")
    x += 34
    for name in sub_tabs:
        width = 26 + len(name) * 9
        selected = name == sub_tab
        layout.text(x, 50, width, 40, name, "normal", "bright" if selected else "dim", "Center")
        if selected:
            layout.rect(x + 8, 84, width - 16, 2, COLOURS["accent"])
        x += width + 6
    button_glyph(layout, x + 4, 59, "r1")

    layout.text(w - 32 - 380, 50, 380, 40, status, "small", "dim", "Right VCenter")
    layout.rect(0, 94, w, 1, COLOURS["accent"], 0.2)


def fmt(value):
    if isinstance(value, float):
        return f"{value:.2f}".rstrip("0").rstrip(".")
    return str(value)


def column_headers(layout, x, y, w, headers):
    stats_w = sum(cw for _, cw in headers)
    cx = x + w - stats_w - 8
    for label, cw in headers:
        layout.text(cx, y, cw, 22, label, "small", "dim", "Right VCenter")
        cx += cw


# ---------------------------------------------------------------------------------------------------------------------
# Commands


def canvas_width(connection):
    state = connection.request({"cmd": "state"}, 10)["result"]
    window = state["window"]
    return round(CANVAS_HEIGHT * window["width"] / window["height"])


def show(connection, prefix, name, build):
    width = canvas_width(connection)
    layout = build(width)
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    path = OUT_DIR / f"{prefix}-{name}-{width}.layout"
    path.write_text(layout.xml(), encoding="utf-8")
    reply = connection.request({"cmd": "layout", "path": str(path)}, 10)
    if not reply.get("ok"):
        raise SystemExit(f"Showing {path} failed: {reply.get('error')}")
    return path


def run(description, prefix, mockups, load_data, launch_args=None, prepare=None):
    """The command line of a generator.

    "mockups" maps each mock-up's name to a function that takes the canvas width and the game data, and returns a
    Layout. "load_data" takes a "refresh" flag and returns the game data, read from the game or the cache.

    "launch_args" are the arguments the shots command launches the game with, which default to starting in Caius
    Cosades' house. "prepare", when given, takes a mock-up's name and the connection, and sets up the game behind
    that mock-up, such as by moving the player.
    """
    launch_args = launch_args or ["--start", DEFAULT_START]

    def cmd_show(args):
        data = load_data(args.refresh)
        connection = omw.connect(omw.Profile(args.profile))
        if prepare:
            prepare(args.name, connection)
        path = show(connection, prefix, args.name, lambda width: mockups[args.name](width, data))
        print(f"Showing {path}")

    def cmd_shots(args):
        resolutions = list(omw.RESOLUTIONS) if args.resolution == ["all"] else args.resolution
        for resolution in resolutions:
            omw.main(["launch", "-p", PROFILE, "--resolution", resolution, *launch_args])
            try:
                data = load_data(args.refresh)
                connection = omw.connect(omw.Profile(PROFILE))
                for name, build in mockups.items():
                    if prepare:
                        prepare(name, connection)
                    show(connection, prefix, name, lambda width: build(width, data))
                    out = omw.OUTPUT_DIR / "mockups" / f"{prefix}-{name}-{resolution}.png"
                    out.parent.mkdir(parents=True, exist_ok=True)
                    connection.request({"cmd": "wait", "frames": 3}, 10)
                    connection.request({"cmd": "screenshot", "path": str(out)}, 30)
                    print(f"Saved {out}")
            finally:
                omw.main(["quit", "-p", PROFILE])

    parser = argparse.ArgumentParser(description=description, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    p = sub.add_parser("show", help="show one mock-up in a running game")
    p.add_argument("name", choices=list(mockups))
    p.add_argument("--profile", "-p", default=PROFILE)
    p.add_argument("--refresh", action="store_true", help="read the data from the game again")
    p.set_defaults(func=cmd_show)
    p = sub.add_parser("shots", help="launch the game, capture every mock-up, and quit")
    p.add_argument("--resolution", "-r", nargs="+", default=["deck"], help="deck, tv1080, tv4k, or all")
    p.add_argument("--refresh", action="store_true", help="read the data from the game again")
    p.set_defaults(func=cmd_shots)
    args = parser.parse_args()
    args.func(args)
