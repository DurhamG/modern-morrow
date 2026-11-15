"""Tests of the harness client's helpers that need no game. Run them with:

python -m unittest discover -s ui-rework/tools -p "test_*.py"
"""

import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import omw  # noqa: E402


def widget(type_, text=None, name=None, rect=(0, 0, 10, 10), children=(), **flags):
    node = {"type": type_, "rect": list(rect), "children": list(children)}
    if text is not None:
        node["text"] = text
    if name is not None:
        node["name"] = name
    node.update(flags)
    return node


class ParsingTest(unittest.TestCase):
    def test_parse_value_reads_json_and_falls_back_to_text(self):
        self.assertEqual(omw.parse_value("10"), 10)
        self.assertEqual(omw.parse_value("true"), True)
        self.assertEqual(omw.parse_value("[1, 2]"), [1, 2])
        self.assertEqual(omw.parse_value("Balmora"), "Balmora")
        self.assertEqual(omw.parse_value('"quoted"'), "quoted")

    def test_build_request_splits_key_value_words(self):
        request = omw.build_request(["pad", "button=a", "hold=3"])
        self.assertEqual(request, {"cmd": "pad", "button": "a", "hold": 3})

    def test_build_request_keeps_equals_signs_in_values(self):
        request = omw.build_request(["console", "command=set x to 1 == 1"])
        self.assertEqual(request["command"], "set x to 1 == 1")

    def test_build_request_makes_paths_absolute(self):
        request = omw.build_request(["screenshot", "path=shot.png"])
        self.assertTrue(Path(request["path"]).is_absolute())

    def test_build_request_refuses_words_without_equals(self):
        with self.assertRaises(omw.HarnessError):
            omw.build_request(["wait", "frames"])
        with self.assertRaises(omw.HarnessError):
            omw.build_request([])

    def test_parse_resolution_knows_the_named_screens(self):
        self.assertEqual(omw.parse_resolution("deck"), (1280, 800))
        self.assertEqual(omw.parse_resolution("fold"), (2448, 1848))
        self.assertEqual(omw.parse_resolution("2560x1440"), (2560, 1440))
        with self.assertRaises(omw.HarnessError):
            omw.parse_resolution("huge")


class SettingsTest(unittest.TestCase):
    def test_read_settings_cfg_keeps_sections_and_order(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "settings.cfg"
            path.write_text("# comment\n[Video]\nresolution x = 1280\nresolution y=800\n\n[GUI]\nscaling factor = 2\n",
                            encoding="utf-8")
            sections = omw.read_settings_cfg(path)
        self.assertEqual(list(sections), ["Video", "GUI"])
        self.assertEqual(sections["Video"], {"resolution x": "1280", "resolution y": "800"})
        self.assertEqual(sections["GUI"]["scaling factor"], "2")


class WidgetTest(unittest.TestCase):
    dump = {"widgets": [
        widget("Window", name="_Main", children=[
            widget("TextBox", text="Save", name="SaveButton"),
            widget("TextBox", text="Load", disabled=True),
        ]),
        widget("Window", children=[widget("TextBox", text=" save ")]),
    ]}

    def test_find_widget_by_text_takes_the_last_match_ignoring_case(self):
        found = omw.find_widget_by_text(self.dump, "SAVE")
        self.assertEqual(found["text"], " save ")

    def test_find_widget_by_text_skips_disabled_widgets(self):
        self.assertIsNone(omw.find_widget_by_text(self.dump, "Load"))

    def test_find_widget_by_name(self):
        self.assertEqual(omw.find_widget_by_name(self.dump, "SaveButton")["text"], "Save")
        self.assertIsNone(omw.find_widget_by_name(self.dump, "Missing"))

    def test_format_widget_writes_one_line_per_widget(self):
        lines = []
        omw.format_widget(widget("Window", name="_Main", rect=(1, 2, 3, 4), children=[widget("TextBox", text="a\nb")]),
                          0, lines)
        self.assertEqual(lines[0], "Window '_Main' [1,2 3x4]")
        self.assertTrue(lines[1].startswith("  TextBox [0,0 10x10] text='a\\\\nb'"))


class SteamTest(unittest.TestCase):
    def test_read_binary_vdf_reads_a_shortcut(self):
        data = (b"\x00shortcuts\x00"
                b"\x000\x00"
                b"\x01AppName\x00Modern Morrow\x00"
                b"\x02appid\x00" + (1234).to_bytes(4, "little") +
                b"\x08"
                b"\x08"
                b"\x08")
        parsed = omw.read_binary_vdf(data)
        shortcut = parsed["shortcuts"]["0"]
        self.assertEqual(shortcut["AppName"], "Modern Morrow")
        self.assertEqual(shortcut["appid"], 1234)

    def test_read_binary_vdf_refuses_an_unknown_value_type(self):
        with self.assertRaises(omw.HarnessError):
            omw.read_binary_vdf(b"\x07key\x00")


if __name__ == "__main__":
    unittest.main()
