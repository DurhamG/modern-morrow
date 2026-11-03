#!/usr/bin/env python3
"""Builds mock-ups of the reworked inventory screen as MyGUI layout files, and shows them in a running game.

The mock-ups are static: they don't respond to input. They use real item names, icons and stats, which this script
reads from the game the first time it runs and caches in .automation/mockups/items.json. The layouts go on the
"Mockup" layer, which is sized relative to the screen height, on a canvas 720 units tall.

    python ui-rework/mockups/inventory.py show equipment     # show one mock-up in the running "mockup" profile
    python ui-rework/mockups/inventory.py shots -r deck tv4k # launch, capture every mock-up, quit

See ui-rework/design/inventory.md for the design.
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
ITEM_CACHE = OUT_DIR / "items.json"
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

WEAPON_TYPES = ["Short Blade", "Long Blade", "Long Blade, two-handed", "Blunt", "Blunt, two-handed",
                "Blunt, two-handed", "Spear", "Axe", "Axe, two-handed", "Bow", "Crossbow", "Thrown", "Arrow", "Bolt"]

# The armour type's base weight, from the fHelmWeight and similar game settings. Armour up to 60% of it is light,
# and up to 90% is medium.
ARMOR_BASE_WEIGHT = [5, 30, 10, 10, 15, 20, 5, 5, 15, 5, 5]

# The mock character's inventory. "slot" marks equipped items.
INVENTORY = [
    {"name": "Netch Leather Helm", "slot": "Helmet"},
    {"name": "Netch Leather Left Pauldron", "slot": "Left pauldron"},
    {"name": "Netch Leather Right Pauldron", "slot": "Right pauldron"},
    {"name": "Netch Leather Cuirass", "slot": "Cuirass"},
    {"name": "Common Shirt", "slot": "Shirt"},
    {"name": "Netch Leather Left Gauntlet", "slot": "Left gauntlet"},
    {"name": "Netch Leather Right Gauntlet", "slot": "Right gauntlet"},
    {"name": "Netch Leather Greaves", "slot": "Greaves"},
    {"name": "Common Pants", "slot": "Pants"},
    {"name": "Netch Leather Boots", "slot": "Boots"},
    {"name": "Iron Longsword", "slot": "Weapon"},
    {"name": "Netch Leather Shield", "slot": "Off hand"},
    {"name": "Iron Arrow", "slot": "Ammunition", "count": 24},
    {"name": "Common Amulet", "slot": "Amulet"},
    {"name": "Expensive Ring", "slot": "Left ring"},
    {"name": "Common Belt", "slot": "Belt"},
    {"name": "Bonemold Cuirass", "new": True},
    {"name": "Chitin Cuirass"},
    {"name": "Boiled Netch Leather Cuirass"},
    {"name": "Expensive Shirt", "new": True},
    {"name": "Common Robe"},
    {"name": "Expensive Robe"},
    {"name": "Bonemold Boots", "new": True},
    {"name": "Chitin Helm"},
    {"name": "Steel Longsword", "new": True},
    {"name": "Silver Longsword"},
    {"name": "Huntsman Longsword"},
    {"name": "Stalhrim Longsword of Flame", "new": True, "stolen": True},
    {"name": "Steel Dagger"},
    {"name": "Chitin War Axe"},
    {"name": "Iron Saber"},
    {"name": "Steel Katana"},
    {"name": "Long Bow"},
    {"name": "Chitin Short Bow"},
    {"name": "Torch"},
]

AREAS = [
    ("Head", ["Helmet"]),
    ("Shoulders", ["Left pauldron", "Right pauldron"]),
    ("Chest", ["Cuirass", "Shirt", "Robe"]),
    ("Arms", ["Left gauntlet", "Right gauntlet"]),
    ("Legs", ["Greaves", "Pants", "Skirt"]),
    ("Feet", ["Boots"]),
    ("Weapon", ["Weapon"]),
    ("Off hand", ["Off hand"]),
    ("Ammunition", ["Ammunition"]),
    ("Amulet", ["Amulet"]),
    ("Rings", ["Left ring", "Right ring"]),
    ("Belt", ["Belt"]),
]


# ---------------------------------------------------------------------------------------------------------------------
# Item data


def load_items(refresh=False):
    if ITEM_CACHE.is_file() and not refresh:
        return json.loads(ITEM_CACHE.read_text(encoding="utf-8"))
    names = sorted({entry["name"] for entry in INVENTORY} | {"Bonemold Cuirass", "Steel Longsword"})
    code = (HERE / "itemdata.lua").read_text(encoding="utf-8")
    code = code.replace("local NAMES = {}", "local NAMES = {" + ", ".join(json.dumps(n) for n in names) + "}")
    connection = omw.connect(omw.Profile(PROFILE))
    reply = connection.request({"cmd": "lua", "code": code, "context": "player"}, 60)
    if not reply.get("ok"):
        raise SystemExit(f"Reading item data failed: {reply.get('error')}")
    items = {}
    for line in reply["result"]["output"]:
        record = json.loads(line)
        items[record["name"]] = record
    missing = [n for n in names if n not in items]
    if missing:
        print("No records named: " + ", ".join(missing), file=sys.stderr)
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    ITEM_CACHE.write_text(json.dumps(items, indent=1), encoding="utf-8")
    return items


def inventory(items):
    """The mock inventory, with each entry's record attached. Entries without a record are left out."""
    result = []
    for entry in INVENTORY:
        if entry["name"] in items:
            result.append({**entry, "record": items[entry["name"]]})
    return result


def armor_class(record):
    base = ARMOR_BASE_WEIGHT[record["type"]]
    if record["weight"] <= base * 0.6:
        return "Light"
    if record["weight"] <= base * 0.9:
        return "Medium"
    return "Heavy"


def best_attack(record):
    attacks = [(record[f"{a}MinDamage"], record[f"{a}MaxDamage"]) for a in ("chop", "slash", "thrust")]
    return max(attacks, key=lambda pair: pair[1])


def slot_of(record):
    """Which equipment slot an item fits, for listing it in an area."""
    kind, t = record["kind"], record.get("type")
    if kind == "armor":
        return ["Helmet", "Cuirass", "Left pauldron", "Right pauldron", "Greaves", "Boots", "Left gauntlet",
                "Right gauntlet", "Off hand", "Left gauntlet", "Right gauntlet"][t]
    if kind == "clothing":
        return ["Pants", "Boots", "Shirt", "Belt", "Robe", "Right gauntlet", "Left gauntlet", "Skirt", "Ring",
                "Amulet"][t]
    if kind == "weapon":
        return "Ammunition" if t in (12, 13) else "Weapon"
    if kind == "light":
        return "Off hand"
    return None


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
            "Caption": caption, "FontHeight": FONT[size], "TextColour": COLOURS.get(colour, colour),
            "TextAlign": align, "TextShadow": "true" if shadow else "false", "TextShadowColour": "0 0 0"})

    def xml(self):
        body = "\n        ".join(self.widgets)
        return ('<?xml version="1.0" encoding="UTF-8"?>\n<MyGUI type="Layout">\n'
                f'    <Widget type="Widget" skin="" layer="Mockup" position="0 0 {self.width} {CANVAS_HEIGHT}">\n'
                f'        {body}\n    </Widget>\n</MyGUI>\n')


def wrap(text, width, size):
    """Splits text into lines that fit the width, guessing at the font's average character width."""
    per_line = max(1, int(width / (FONT[size] * 0.5)))
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


def chrome(layout, sub_tab):
    """The parts every inventory mock-up shares: background, the two rows of tabs and the status line."""
    w = layout.width
    layout.rect(0, 0, w, CANVAS_HEIGHT, COLOURS["panel"], 0.97)

    # Top tabs, which switch between the map, inventory, magic and stats screens.
    tabs = ["Map", "Inventory", "Magic", "Stats"]
    tab_w = 120
    x = (w - tab_w * len(tabs)) / 2
    button_glyph(layout, x - 34, 11, "l2")
    for name in tabs:
        selected = name == "Inventory"
        layout.text(x, 0, tab_w, 44, name, "large", "active" if selected else "dim", "Center")
        if selected:
            layout.rect(x + 20, 38, tab_w - 40, 2, COLOURS["accent"])
        x += tab_w
    button_glyph(layout, x + 12, 11, "r2")
    layout.rect(0, 44, w, 1, COLOURS["accent"], 0.35)

    # Inventory tabs.
    subs = ["Recent", "Equipment", "Consumables", "Notes", "Tools", "Misc"]
    x = 32
    button_glyph(layout, x, 59, "l1")
    x += 34
    for name in subs:
        width = 26 + len(name) * 9
        selected = name == sub_tab
        layout.text(x, 50, width, 40, name, "normal", "bright" if selected else "dim", "Center")
        if selected:
            layout.rect(x + 8, 84, width - 16, 2, COLOURS["accent"])
        x += width + 6
    button_glyph(layout, x + 4, 59, "r1")

    # Status line: armour rating, encumbrance and gold.
    layout.text(w - 32 - 380, 50, 380, 40, "Armor  21      Weight  96 / 250      Gold  1,250", "small", "dim",
                "Right VCenter")
    layout.rect(0, 94, w, 1, COLOURS["accent"], 0.2)


def details_panel(layout, x, y, w, h, title_entry, rows, compare_entry=None, action=None):
    """The panel that shows the highlighted item in full, beside the equipped item it would replace.

    "rows" is a list of (label, value, compare value). Values are numbers, damage ranges such as "2-14", or other
    strings. With a compare_entry, this item's value is green where it's better than the equipped one, and red where
    it's worse. Ranges compare by their maximum.
    """
    layout.rect(x, y, w, h, "#15120c", 0.9)
    layout.rect(x, y, w, 1, COLOURS["accent"], 0.3)
    record = title_entry["record"]
    icon(layout, x + 16, y + 16, 64, record)
    layout.text(x + 92, y + 14, w - 108, 34, record["name"], "large", "bright")
    layout.text(x + 92, y + 46, w - 108, 26, describe(record), "small", "dim")
    tags = []
    if title_entry.get("new"):
        tags.append(("New", COLOURS["new"]))
    if title_entry.get("stolen"):
        tags.append(("Stolen", COLOURS["stolen"]))
    tx = x + 92
    for label, colour in tags:
        layout.text(tx, y + 70, 70, 20, label, "small", colour)
        tx += 60

    ry = y + 104
    if compare_entry:
        layout.text(x + 16, ry, w - 32, 24, "", "small", "dim")
        layout.text(x + w - 16 - 150, ry, 72, 24, "This", "small", "dim", "Right VCenter")
        layout.text(x + w - 16 - 72, ry, 72, 24, "Equipped", "small", "dim", "Right VCenter")
        ry += 26
    for label, value, other in rows:
        layout.text(x + 16, ry, w - 32, 28, label, "normal", "text")
        value_x = x + w - 16 - (150 if compare_entry else 72)
        colour = "bright"
        mine, theirs = comparable(value), comparable(other)
        if compare_entry and mine is not None and theirs is not None and mine != theirs:
            higher_is_better = label not in ("Weight",)
            colour = "better" if (mine > theirs) == higher_is_better else "worse"
        layout.text(value_x, ry, 72, 28, fmt(value), "normal", colour, "Right VCenter")
        if compare_entry:
            layout.text(x + w - 16 - 72, ry, 72, 28, fmt(other), "normal", "dim", "Right VCenter")
        ry += 30
    effects = record.get("effects") or []
    if effects:
        ry += 8
        layout.text(x + 16, ry, w - 32, 24, "Enchantment", "small", "dim")
        ry += 24
        for effect in effects:
            layout.text(x + 16, ry, w - 32, 26, effect, "normal", COLOURS["new"])
            ry += 26
    if compare_entry:
        ry += 12
        layout.text(x + 16, ry, w - 32, 24, "Replaces " + compare_entry["record"]["name"], "small", "dim")
    if action:
        button_glyph(layout, x + 16, y + h - 40, "a")
        layout.text(x + 44, y + h - 44, w - 60, 30, action, "normal", "bright")


def comparable(value):
    """A number to compare a stat by, or None. A damage range such as "2-14" compares by its maximum."""
    if isinstance(value, (int, float)):
        return value
    if isinstance(value, str) and "-" in value:
        try:
            return float(value.split("-")[1])
        except ValueError:
            return None
    return None


def describe(record):
    kind = record["kind"]
    if kind == "weapon":
        return WEAPON_TYPES[record["type"]]
    if kind == "armor":
        return f"{armor_class(record)} armor"
    if kind == "clothing":
        return "Clothing"
    return kind.capitalize()


def fmt(value):
    if isinstance(value, float):
        return f"{value:.2f}".rstrip("0").rstrip(".")
    return str(value)


def list_row(layout, x, y, w, entry, columns, focused=False, row_h=40):
    """One row of an item list: icon, name with markers, then stat columns from the right edge."""
    record = entry["record"]
    if focused:
        layout.rect(x, y, w, row_h, COLOURS["accent"], 0.16)
        layout.rect(x, y, 3, row_h, COLOURS["accent"])
    icon(layout, x + 8, y + 4, row_h - 8, record)
    name = record["name"]
    if entry.get("count", 1) > 1:
        name += f" ({entry['count']})"
    stats_w = sum(cw for _, cw in columns)
    name_w = w - (row_h + 12) - stats_w - 8
    colour = "active" if focused else ("bright" if entry.get("slot") else "text")
    layout.text(x + row_h + 12, y, name_w, row_h, name, "normal", colour)
    markers = []
    if entry.get("slot"):
        markers.append(("Equipped", "dim"))
    if entry.get("new"):
        markers.append(("New", COLOURS["new"]))
    if entry.get("stolen"):
        markers.append(("Stolen", COLOURS["stolen"]))
    mx = x + row_h + 12
    if markers:
        # Markers sit under the name, so a long name and its markers never collide.
        layout.widgets.pop()
        layout.text(x + row_h + 12, y + 1, name_w, 24, name, "normal", colour)
        for label, mcolour in markers:
            layout.text(mx, y + 22, 90, 16, label, "small", mcolour)
            mx += len(label) * 8 + 14
    cx = x + w - stats_w - 8
    for value, cw in columns:
        layout.text(cx, y, cw, row_h, fmt(value), "normal", "text", "Right VCenter")
        cx += cw


def column_headers(layout, x, y, w, headers):
    stats_w = sum(cw for _, cw in headers)
    cx = x + w - stats_w - 8
    for label, cw in headers:
        layout.text(cx, y, cw, 22, label, "small", "dim", "Right VCenter")
        cx += cw


# ---------------------------------------------------------------------------------------------------------------------
# The mock-ups


def equipped_in(entries, slot):
    for entry in entries:
        if entry.get("slot") == slot:
            return entry
    return None


def area_summary(record):
    kind = record["kind"]
    if kind == "armor":
        return f"{armor_class(record)} armor, Armor {record['baseArmor']}"
    if kind == "weapon" and record["type"] not in (12, 13):
        low, high = best_attack(record)
        return f"{WEAPON_TYPES[record['type']]}, Damage {low}-{high}"
    return describe(record)


def mock_equipment(width, items):
    """The equipment overview, with the Chest area highlighted."""
    entries = inventory(items)
    layout = Layout(width)
    chrome(layout, "Equipment")

    panel_w = max(320, int(width * 0.3))
    cols_w = width - 64 - panel_w - 24
    col_w = (cols_w - 16) // 2
    top = 110
    card_h = 92
    for index, (area, slots) in enumerate(AREAS):
        column, row = divmod(index, 6)
        x = 32 + column * (col_w + 16)
        y = top + row * (card_h + 6)
        focused = area == "Chest"
        layout.rect(x, y, col_w, card_h, "#15120c", 0.9)
        if focused:
            layout.rect(x, y, col_w, card_h, COLOURS["accent"], 0.16)
            layout.rect(x, y, 3, card_h, COLOURS["accent"])
        worn = [equipped_in(entries, s) for s in slots]
        worn = [e for e in worn if e]
        has_new = any(e.get("new") and slot_of(e["record"]) in slots for e in entries if not e.get("slot"))
        layout.text(x + 84, y + 6, col_w - 100, 22, area.upper(), "small", "dim")
        if has_new:
            layout.text(x + col_w - 70, y + 6, 58, 22, "New", "small", COLOURS["new"], "Right VCenter")
        if not worn:
            layout.rect(x + 12, y + 14, 62, 62, "#1a1610", 0.9)
            layout.text(x + 84, y + 30, col_w - 100, 30, "Nothing equipped", "normal", "dim")
            continue
        top_entry = worn[0]
        icon(layout, x + 12, y + 14, 62, top_entry["record"])
        name = top_entry["record"]["name"]
        if top_entry.get("count", 1) > 1:
            name += f" ({top_entry['count']})"
        layout.text(x + 84, y + 28, col_w - 96, 28, name, "normal", "active" if focused else "bright")
        others = [e["record"]["name"] for e in worn[1:]]
        second = ("+ " + ", ".join(others)) if others else area_summary(top_entry["record"])
        layout.text(x + 84, y + 56, col_w - 96, 24, second, "small", "dim")

    # Details for the highlighted area: every layer it holds.
    px = width - 32 - panel_w
    layout.rect(px, top, panel_w, CANVAS_HEIGHT - top - 24, "#15120c", 0.9)
    layout.text(px + 16, top + 10, panel_w - 32, 32, "Chest", "title", "bright")
    y = top + 52
    for slot in ["Cuirass", "Shirt", "Robe"]:
        entry = equipped_in(entries, slot)
        layout.text(px + 16, y, panel_w - 32, 22, slot.upper(), "small", "dim")
        y += 24
        if entry:
            icon(layout, px + 16, y, 40, entry["record"])
            layout.text(px + 66, y, panel_w - 82, 22, entry["record"]["name"], "normal", "bright")
            layout.text(px + 66, y + 20, panel_w - 82, 22, area_summary(entry["record"]), "small", "dim")
        else:
            layout.text(px + 16, y, panel_w - 32, 40, "Nothing equipped", "normal", "dim")
        y += 54
    layout.text(px + 16, y + 6, panel_w - 32, 24, "New: Bonemold Cuirass, Expensive Shirt", "small", COLOURS["new"])
    button_glyph(layout, px + 16, CANVAS_HEIGHT - 24 - 44, "a")
    layout.text(px + 44, CANVAS_HEIGHT - 24 - 48, panel_w - 60, 30, "Change chest gear", "normal", "bright")
    return layout


def mock_area(width, items, area, slots, focus_name, headers, columns_for, detail_rows):
    """An equipment area: its slots on the left, the items that fit in the middle, details on the right."""
    entries = inventory(items)
    layout = Layout(width)
    chrome(layout, "Equipment")

    top = 110
    bottom = CANVAS_HEIGHT - 24
    slots_w = max(200, int(width * 0.18))
    panel_w = max(300, int(width * 0.27))
    list_x = 32 + slots_w + 16
    list_w = width - 32 - panel_w - 16 - list_x

    # Slots.
    layout.rect(32, top, slots_w, bottom - top, "#15120c", 0.9)
    layout.text(48, top + 10, slots_w - 32, 32, area, "title", "bright")
    y = top + 52
    for slot in slots:
        entry = equipped_in(entries, slot)
        layout.text(48, y, slots_w - 32, 22, slot.upper(), "small", "dim")
        y += 24
        if entry:
            icon(layout, 48, y, 40, entry["record"])
            lines = wrap(entry["record"]["name"], slots_w - 64, "small")[:2]
            line_h = 40 / len(lines)
            for i, line in enumerate(lines):
                layout.text(96, y + i * line_h, slots_w - 64, line_h, line, "small", "bright")
        else:
            layout.rect(48, y, 40, 40, "#1a1610", 0.9)
            layout.text(96, y, slots_w - 64, 40, "Empty", "small", "dim")
        y += 54
    layout.text(48, bottom - 34, slots_w - 32, 24, "Pick a slot to filter", "small", "dim")

    # Items that fit the area, grouped by slot.
    layout.rect(list_x, top, list_w, bottom - top, "#15120c", 0.9)
    column_headers(layout, list_x, top + 6, list_w, headers)
    layout.text(list_x + 16, top + 6, 200, 22, "Sorted by value", "small", "dim")
    y = top + 32
    focus_entry = None
    for slot in slots:
        group = [e for e in entries if slot_of(e["record"]) == slot]
        if not group:
            continue
        if len(slots) > 1:
            layout.text(list_x + 12, y, list_w - 24, 24, slot, "small", "bright")
            layout.rect(list_x + 12, y + 23, list_w - 24, 1, COLOURS["accent"], 0.25)
            y += 28
        group.sort(key=lambda e: -e["record"]["value"])
        for entry in group:
            if y + 40 > bottom:
                break
            focused = entry["record"]["name"] == focus_name
            if focused:
                focus_entry = entry
            list_row(layout, list_x + 4, y, list_w - 8, entry, list(zip(columns_for(entry["record"]),
                                                                           [cw for _, cw in headers])), focused)
            y += 42
        y += 6

    # Details of the highlighted item, beside the equipped one.
    px = width - 32 - panel_w
    equipped = equipped_in(entries, slot_of(focus_entry["record"])) if focus_entry else None
    details_panel(layout, px, top, panel_w, bottom - top, focus_entry,
                  detail_rows(focus_entry["record"], equipped["record"] if equipped else None),
                  equipped, "Equip")
    return layout


def mock_chest(width, items):
    def columns(record):
        if record["kind"] == "armor":
            return [record["baseArmor"], record["weight"], record["value"]]
        return ["-", record["weight"], record["value"]]

    def rows(record, other):
        def get(r, key, default="-"):
            return r.get(key, default) if r else "-"
        return [("Armor rating", get(record, "baseArmor"), get(other, "baseArmor")),
                ("Condition", get(record, "health"), get(other, "health")),
                ("Weight", record["weight"], other["weight"] if other else "-"),
                ("Value", record["value"], other["value"] if other else "-")]

    return mock_area(width, items, "Chest", ["Cuirass", "Shirt", "Robe"], "Bonemold Cuirass",
                     [("Armor", 64), ("Weight", 64), ("Value", 64)], columns, rows)


def mock_weapon(width, items):
    def columns(record):
        low, high = best_attack(record)
        return [f"{low}-{high}", record["speed"], record["weight"], record["value"]]

    def rows(record, other):
        result = []
        for attack in ("chop", "slash", "thrust"):
            mine = f"{record[attack + 'MinDamage']}-{record[attack + 'MaxDamage']}"
            theirs = f"{other[attack + 'MinDamage']}-{other[attack + 'MaxDamage']}" if other else "-"
            result.append((attack.capitalize(), mine, theirs))
        result += [("Speed", record["speed"], other["speed"] if other else "-"),
                   ("Reach", record["reach"], other["reach"] if other else "-"),
                   ("Condition", record["health"], other["health"] if other else "-"),
                   ("Weight", record["weight"], other["weight"] if other else "-"),
                   ("Value", record["value"], other["value"] if other else "-")]
        return result

    return mock_area(width, items, "Weapon", ["Weapon"], "Steel Longsword",
                     [("Damage", 70), ("Speed", 58), ("Weight", 62), ("Value", 62)], columns, rows)


MOCKUPS = {"equipment": mock_equipment, "chest": mock_chest, "weapon": mock_weapon}


# ---------------------------------------------------------------------------------------------------------------------
# Commands


def canvas_width(connection):
    state = connection.request({"cmd": "state"}, 10)["result"]
    window = state["window"]
    return round(CANVAS_HEIGHT * window["width"] / window["height"])


def show(connection, name, items):
    width = canvas_width(connection)
    layout = MOCKUPS[name](width, items)
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    path = OUT_DIR / f"inventory-{name}-{width}.layout"
    path.write_text(layout.xml(), encoding="utf-8")
    reply = connection.request({"cmd": "layout", "path": str(path)}, 10)
    if not reply.get("ok"):
        raise SystemExit(f"Showing {path} failed: {reply.get('error')}")
    return path


def cmd_show(args):
    items = load_items(args.refresh)
    connection = omw.connect(omw.Profile(args.profile))
    path = show(connection, args.name, items)
    print(f"Showing {path}")


def cmd_shots(args):
    resolutions = list(omw.RESOLUTIONS) if args.resolution == ["all"] else args.resolution
    for resolution in resolutions:
        omw.main(["launch", "-p", PROFILE, "--resolution", resolution, "--start", "Balmora, Caius Cosades' House"])
        try:
            items = load_items(args.refresh)
            connection = omw.connect(omw.Profile(PROFILE))
            for name in MOCKUPS:
                show(connection, name, items)
                out = omw.OUTPUT_DIR / "mockups" / f"inventory-{name}-{resolution}.png"
                out.parent.mkdir(parents=True, exist_ok=True)
                connection.request({"cmd": "wait", "frames": 3}, 10)
                connection.request({"cmd": "screenshot", "path": str(out)}, 30)
                print(f"Saved {out}")
        finally:
            omw.main(["quit", "-p", PROFILE])


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    p = sub.add_parser("show", help="show one mock-up in a running game")
    p.add_argument("name", choices=list(MOCKUPS))
    p.add_argument("--profile", "-p", default=PROFILE)
    p.add_argument("--refresh", action="store_true", help="read the item data from the game again")
    p.set_defaults(func=cmd_show)
    p = sub.add_parser("shots", help="launch the game, capture every mock-up, and quit")
    p.add_argument("--resolution", "-r", nargs="+", default=["deck"], help="deck, tv1080, tv4k, or all")
    p.add_argument("--refresh", action="store_true", help="read the item data from the game again")
    p.set_defaults(func=cmd_shots)
    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
