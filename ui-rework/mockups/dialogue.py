#!/usr/bin/env python3
"""Builds mock-ups of the reworked dialogue window as MyGUI layout files, and shows them in a running game.

The conversation panel sits along the bottom of the screen, over the game, with the NPC in view above it. The
mock-ups use real lines from Arrille, the trader in Seyda Neen, and Fargoth, whose "ring" topic asks the player to
choose a reply. mockup.py has the parts all the mock-up generators share.

    python ui-rework/mockups/dialogue.py show topics         # show one mock-up in the running "mockup" profile
    python ui-rework/mockups/dialogue.py shots -r deck tv4k  # launch, capture every mock-up, quit

See ui-rework/design/dialogue.md for the design.
"""

import json

from mockup import COLOURS, HERE, Layout, button_glyph, run, wrap

# The panel's place on the canvas, which is 720 units tall.
PANEL_TOP = 392
PANEL_BOTTOM = 704
MARGIN = 24
TEXT_SIZE = 20
LINE_HEIGHT = 26
ROW_HEIGHT = 28

PANEL_COLOUR = "#0d0b08"

ARRILLE_GREETING = ("Welcome to Arrille's Tradehouse. I barter for goods or coin. I also have a small selection of "
                    "spells for sale. No credit. You want a [little advice]? That's free. That's ALL that's free. "
                    "Would you like to hear about our most [popular potions]? Our most [popular scrolls]?")

ARRILLE_ADVICE = ("If you want to live to a ripe old age, buy a weapon and as much armor as you can wear and still run "
                  "from trouble. Buy a spell or two. And [practice your skills]. Don't practice on citizens. We call "
                  "that foul murder, and we don't like it. But you can kill smugglers and bandits and other outlaws "
                  "all you like.")

FARGOTH_RING = ("I swear one of the Guards has it. I had it last week before their weekly \"Let's shake down Fargoth\" "
                "ritual. An engraved healing ring, family heirloom of mine. You haven't seen it have you?")


def coloured_lines(text, width):
    """Wraps text with [keywords] in brackets, and colours the keywords blue, as the game shows topics in answers."""
    lines = wrap(text.replace("[", "").replace("]", ""), width, TEXT_SIZE)
    keywords = []
    rest = text
    while "[" in rest:
        start = rest.index("[")
        end = rest.index("]", start)
        keywords.append(rest[start + 1:end])
        rest = rest[end + 1:]
    result = []
    for line in lines:
        for keyword in keywords:
            # A keyword can be split across two lines, in which case each part is coloured.
            for part in (keyword, *keyword.split(" ")):
                if part in line and f"{COLOURS['new']}{part}" not in line:
                    line = line.replace(part, f"{COLOURS['new']}{part}{COLOURS['text']}", 1)
                    break
        result.append(line)
    return result


def panel(layout):
    """The opaque panel along the bottom of the screen, with a thin gold line along its top."""
    w = layout.width
    layout.rect(MARGIN, PANEL_TOP, w - 2 * MARGIN, PANEL_BOTTOM - PANEL_TOP, PANEL_COLOUR)
    layout.rect(MARGIN, PANEL_TOP, w - 2 * MARGIN, 1, COLOURS["accent"], 0.6)


def columns(layout):
    """The x position and width of the conversation on the left, and of the options on the right."""
    w = layout.width
    split = int(w * 0.62)
    return (MARGIN + 24, split - MARGIN - 48), (split, w - MARGIN - 24 - split)


def conversation(layout, name, disposition, topic, text, earlier):
    """The NPC's name and disposition, then the latest answer under its topic."""
    (x, w), (ox, _) = columns(layout)
    y = PANEL_TOP + 14
    layout.text(x, y, w, 34, name, "title", "bright")
    layout.text(x + 12 + len(name) * 14, y + 4, 200, 30, f"Disposition  {disposition}", "small", "dim")
    # A short bar under the disposition, as the game shows it.
    bar_x = x + 12 + len(name) * 14 + 110
    layout.rect(bar_x, y + 16, 80, 6, "#000000", 0.8)
    layout.rect(bar_x, y + 16, int(80 * disposition / 100), 6, "#3a5aa8")
    if earlier:
        # Earlier answers are above, reached with the right stick.
        button_glyph(layout, x + w - 112, y + 6, "rstick", 20)
        layout.text(x + w - 86, y + 4, 86, 26, "Earlier", "small", "dim")
    y += 46
    layout.rect(x, y - 6, w, 1, COLOURS["accent"], 0.2)
    if topic:
        layout.text(x, y, w, 28, topic, TEXT_SIZE, "bright")
        y += 30
    for line in coloured_lines(text, w):
        if y + LINE_HEIGHT > PANEL_BOTTOM - 48:
            break
        layout.text(x, y, w, LINE_HEIGHT, line, TEXT_SIZE, "text")
        y += LINE_HEIGHT
    # A faint line between the conversation and the options.
    layout.rect(ox - 24, PANEL_TOP + 16, 1, PANEL_BOTTOM - PANEL_TOP - 32, COLOURS["accent"], 0.15)


def option_tabs(layout, selected):
    _, (x, w) = columns(layout)
    y = PANEL_TOP + 16
    button_glyph(layout, x, y + 5, "l1")
    tx = x + 32
    for name in ("Topics", "Services"):
        tab_w = 24 + len(name) * 10
        layout.text(tx, y, tab_w, 32, name, "normal", "bright" if name == selected else "dim", "Center")
        if name == selected:
            layout.rect(tx + 8, y + 30, tab_w - 16, 2, COLOURS["accent"])
        tx += tab_w + 4
    button_glyph(layout, tx + 2, y + 5, "r1")
    return y + 44


def option_list(layout, y, rows, focus):
    """Rows of options. Each row is a label, or a (label, colour) pair, or a group header starting with "#"."""
    _, (x, w) = columns(layout)
    index = 0
    for row in rows:
        if y + ROW_HEIGHT > PANEL_BOTTOM - 12:
            break
        if isinstance(row, str) and row.startswith("#"):
            layout.text(x, y + 2, w, 22, row[1:], "small", "dim")
            layout.rect(x, y + 23, w, 1, COLOURS["accent"], 0.2)
            y += 28
            continue
        label, colour = (row, "bright") if isinstance(row, str) else row
        if index == focus:
            layout.rect(x - 8, y, w + 8, ROW_HEIGHT - 2, COLOURS["accent"], 0.16)
            layout.rect(x - 8, y, 3, ROW_HEIGHT - 2, COLOURS["accent"])
            colour = "active"
        layout.text(x + 4, y, w - 8, ROW_HEIGHT - 2, label, "normal", colour)
        y += ROW_HEIGHT
        index += 1
    return y


def hints(layout, buttons):
    """Button hints along the bottom of the conversation, so the options can use the panel's full height."""
    (x, _), _ = columns(layout)
    y = PANEL_BOTTOM - 38
    for glyph, label in buttons:
        button_glyph(layout, x, y + 1, glyph)
        width = len(label) * 10 + 8
        layout.text(x + 30, y - 2, width, 28, label, "normal", "bright")
        x += 30 + width + 22


def mock_topics(width, _data):
    layout = Layout(width)
    panel(layout)
    conversation(layout, "Arrille", 40, "little advice", ARRILLE_ADVICE, earlier=True)
    y = option_tabs(layout, "Topics")
    option_list(layout, y, [
        "#New",
        ("practice your skills", "new"),
        ("popular potions", "new"),
        ("popular scrolls", "new"),
        "Background",
        "latest rumors",
        "#Asked",
        ("little advice", "dim"),
    ], focus=0)
    hints(layout, [("a", "Ask"), ("y", "Next section"), ("b", "Leave")])
    return layout


def mock_services(width, _data):
    layout = Layout(width)
    panel(layout)
    conversation(layout, "Arrille", 40, None, ARRILLE_GREETING, earlier=False)
    y = option_tabs(layout, "Services")
    _, (x, w) = columns(layout)
    for i, (name, detail) in enumerate([("Persuasion", "Admire, intimidate, taunt or bribe"),
                                        ("Barter", "Buy and sell goods"),
                                        ("Spells", "8 spells for sale")]):
        if i == 1:
            layout.rect(x - 8, y, w + 8, 48, COLOURS["accent"], 0.16)
            layout.rect(x - 8, y, 3, 48, COLOURS["accent"])
        layout.text(x + 4, y + 2, w - 8, 26, name, "normal", "active" if i == 1 else "bright")
        layout.text(x + 4, y + 24, w - 8, 22, detail, "small", "dim")
        y += 52
    hints(layout, [("a", "Open"), ("b", "Leave")])
    return layout


def mock_choice(width, _data):
    layout = Layout(width)
    panel(layout)
    conversation(layout, "Fargoth", 40, "ring", FARGOTH_RING, earlier=True)
    (_, _), (x, w) = columns(layout)
    y = PANEL_TOP + 20
    layout.text(x, y, w, 28, "Choose a reply", "normal", "bright")
    layout.rect(x, y + 32, w, 1, COLOURS["accent"], 0.2)
    y += 44
    for i, choice in enumerate(["Uhhh. No, sorry. No ring.", "Yes, I found it! Here take it."]):
        lines = wrap(choice, w - 16, "normal")
        h = 10 + 24 * len(lines)
        if i == 1:
            layout.rect(x - 8, y, w + 8, h, COLOURS["accent"], 0.16)
            layout.rect(x - 8, y, 3, h, COLOURS["accent"])
        for n, line in enumerate(lines):
            layout.text(x + 4, y + 5 + 24 * n, w - 8, 24, line, "normal", "active" if i == 1 else "bright")
        y += h + 6
    hints(layout, [("a", "Reply")])
    return layout


MOCKUPS = {"topics": mock_topics, "services": mock_services, "choice": mock_choice}

# The NPC each mock-up shows, and how far from them to stand. Fargoth is short, so standing further back keeps his
# face above the panel.
SPEAKERS = {"topics": "arrille", "services": "arrille", "choice": "fargoth"}
DISTANCES = {"arrille": 160, "fargoth": 300}
FARGOTH_SPOT = "player->position -10213 -71483 153 0"
ARRILLE_SPOT = "coc \"Seyda Neen, Arrille's Tradehouse\""

_current_speaker = []


def prepare(name, connection):
    """Moves the player in front of the mock-up's NPC, and hides the HUD and crosshair."""
    speaker = SPEAKERS[name]
    if _current_speaker != [speaker]:
        spot = FARGOTH_SPOT if speaker == "fargoth" else ARRILLE_SPOT
        connection.request({"cmd": "console", "command": spot}, 30)
        connection.request({"cmd": "wait", "seconds": 3}, 30)
        # Arriving in Seyda Neen shows a tutorial message, which Return closes.
        connection.request({"cmd": "key", "key": "Return"}, 30)
        connection.request({"cmd": "wait", "frames": 10}, 30)
        code = (HERE / "facenpc.lua").read_text(encoding="utf-8")
        code = code.replace("local NPC_ID = ''", f"local NPC_ID = {json.dumps(speaker)}")
        code = code.replace("local DISTANCE = 160", f"local DISTANCE = {DISTANCES[speaker]}")
        reply = connection.request({"cmd": "lua", "code": code, "context": "global"}, 30)
        if not reply.get("ok") or reply["result"]["output"] != [f"facing {speaker}"]:
            raise SystemExit(f"Moving to {speaker} failed: {reply.get('error') or reply['result']['output']}")
        connection.request({"cmd": "wait", "frames": 30}, 30)
        _current_speaker[:] = [speaker]
    connection.request({"cmd": "lua", "context": "player",
                        "code": "require('openmw.interfaces').UI.setHudVisibility(false)"}, 30)


if __name__ == "__main__":
    run(__doc__, "dialogue", MOCKUPS, lambda refresh: {}, ["--start", "Seyda Neen, Arrille's Tradehouse"], prepare)
