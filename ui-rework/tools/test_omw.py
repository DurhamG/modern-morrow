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

    def test_build_request_keeps_a_touch_path_of_points(self):
        request = omw.build_request(["touch", "action=drag", "path=[[4,400],[160,400]]"])
        self.assertEqual(request["path"], [[4, 400], [160, 400]])

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


class CloudSettingsTest(unittest.TestCase):
    def settings(self, *launch_args):
        with tempfile.TemporaryDirectory() as folder:
            profile = type("FakeProfile", (), {})()
            profile.config_dir = Path(folder) / "config"
            profile.data_dir = Path(folder) / "data"
            profile.config_dir.mkdir()
            args = omw.build_parser().parse_args(["launch", *launch_args])
            omw.write_settings_cfg(profile, args)
            return omw.read_settings_cfg(profile.config_dir / "settings.cfg")

    def test_a_launch_never_reaches_the_real_cloud(self):
        self.assertEqual(self.settings()["Saves"]["cloud address"], omw.NO_CLOUD_ADDRESS)

    def test_cloud_points_at_the_worker_on_this_computer(self):
        self.assertEqual(self.settings("--cloud")["Saves"]["cloud address"], omw.CLOUD_SERVICE_ADDRESS)

    def test_set_comes_last(self):
        sections = self.settings("--cloud", "--set", "Saves.cloud address=http://example.test")
        self.assertEqual(sections["Saves"]["cloud address"], "http://example.test")


class FileLineTest(unittest.TestCase):
    def test_remove_files_deletes_the_matching_files_only(self):
        with tempfile.TemporaryDirectory() as folder:
            data = Path(folder)
            (data / "saves" / "player").mkdir(parents=True)
            (data / "saves" / "player" / "Quicksave.omwsave").write_bytes(b"x")
            (data / "saves" / "player" / "notes.txt").write_bytes(b"x")
            self.assertIsNone(omw.file_line(["remove-files", "saves/*/*.omwsave"], data))
            self.assertFalse((data / "saves" / "player" / "Quicksave.omwsave").exists())
            self.assertTrue((data / "saves" / "player" / "notes.txt").exists())
            self.assertEqual(omw.file_line(["expect-file", "saves/*/*.omwsave"], data),
                             f"no file in {data} matches 'saves/*/*.omwsave'")

    def test_remove_files_with_no_match_is_fine(self):
        with tempfile.TemporaryDirectory() as folder:
            self.assertIsNone(omw.file_line(["remove-files", "saves/*/*.omwsave"], Path(folder)))


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


class CheckTest(unittest.TestCase):
    # A pause menu: a column of buttons, and a row of tabs at the top.
    dump = {"widgets": [
        widget("Window", children=[
            widget("TextBox", text="Save", rect=(100, 100, 80, 20)),
            widget("TextBox", text="Load", rect=(100, 130, 80, 20)),
            widget("TextBox", text="New Game", rect=(100, 160, 80, 20)),
            widget("TextBox", text="Options", rect=(100, 190, 80, 20)),
            widget("TextBox", text="Map", rect=(10, 10, 40, 20)),
            widget("TextBox", text="Inventory", rect=(60, 12, 40, 20)),
        ]),
    ]}

    def test_expect_text_matches_part_of_a_text_ignoring_case(self):
        self.assertEqual(len(omw.widgets_with_text(self.dump, "new game")), 1)
        self.assertEqual(len(omw.widgets_with_text(self.dump, "game")), 1)
        self.assertEqual(omw.widgets_with_text(self.dump, "Exit"), [])

    def test_expect_order_passes_top_to_bottom(self):
        self.assertIsNone(omw.check_order(self.dump, ["Save", "Load", "New Game", "Options"]))

    def test_expect_order_fails_when_swapped(self):
        message = omw.check_order(self.dump, ["Load", "Save"])
        self.assertIn("'Save'", message)
        self.assertIn("not after", message)

    def test_expect_order_fails_when_a_text_is_missing(self):
        self.assertIn("is not on the screen", omw.check_order(self.dump, ["Save", "Exit"]))

    def test_expect_order_allows_the_same_row_left_to_right(self):
        self.assertIsNone(omw.check_order(self.dump, ["Map", "Inventory", "Save"]))
        self.assertIsNotNone(omw.check_order(self.dump, ["Inventory", "Map"]))

    def test_expect_order_takes_the_next_match_after_the_one_before(self):
        dump = {"widgets": [widget("Window", children=[
            widget("TextBox", text="Finished", rect=(0, 10, 50, 20)),
            widget("TextBox", text="In Progress", rect=(0, 40, 50, 20)),
            widget("TextBox", text="Finished", rect=(0, 70, 50, 20)),
        ])]}
        self.assertIsNone(omw.check_order(dump, ["In Progress", "Finished"]))

    def test_lua_output_drops_the_empty_last_line(self):
        self.assertEqual(omw.lua_output({"output": ["42", ""]}), "42")
        self.assertEqual(omw.lua_output({"output": []}), "")


class LogTest(unittest.TestCase):
    lines = [
        "[21:36:05.510 I] Loading cell Balmora",
        "[21:36:05.510 E] Failed to open image: Resource 'textures/x.dds' not found",
        "[21:36:05.600 W] A warning",
        "[21:36:05.700 E] Error in frame: class does not have services",
        "[21:36:05.800 I] Bad LiveCellRef cast to BOOK4 from Static",
    ]

    def test_log_errors_are_found(self):
        errors = omw.log_errors(self.lines, [])
        self.assertEqual(len(errors), 3)

    def test_allowlisted_lines_are_skipped(self):
        allowlist = omw.read_log_allowlist()
        errors = omw.log_errors(self.lines, allowlist)
        self.assertEqual(len(errors), 2)
        self.assertNotIn("does not have services", " ".join(errors))

    def test_expected_errors_are_skipped(self):
        errors = omw.log_errors(self.lines, [], expected=["textures/x.dds", "Bad LiveCellRef"])
        self.assertEqual(errors, ["[21:36:05.700 E] Error in frame: class does not have services"])

    def test_lines_before_the_watch_started_are_skipped(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "openmw.log"
            path.write_text(self.lines[1] + "\n", encoding="utf-8")
            watch = omw.LogWatch(path)
            with open(path, "a", encoding="utf-8") as file:
                file.write(self.lines[3] + "\n")
            self.assertEqual(watch.new_lines(), [self.lines[3]])
            self.assertEqual(omw.LogWatch(path, from_start=True).new_lines(), [self.lines[1], self.lines[3]])


class GoldenTest(unittest.TestCase):
    def dump(self, save_text="Save", rect=(100, 100, 80, 20), extra=()):
        return {"widgets": [widget("Window", name="_Main", children=[
            widget("TextBox", text=save_text, rect=rect, fontHeight=18),
            widget("TextBox", text="Load", rect=(100, 130, 80, 20)),
            *extra,
        ])]}

    def test_golden_ignores_positions_and_sizes(self):
        self.assertEqual(omw.golden_lines(self.dump()), omw.golden_lines(self.dump(rect=(5, 5, 300, 40))))

    def test_golden_notices_changed_text(self):
        self.assertNotEqual(omw.golden_lines(self.dump()), omw.golden_lines(self.dump(save_text="Save game")))

    def test_golden_notices_a_missing_widget(self):
        extra = [widget("TextBox", text="Exit")]
        self.assertNotEqual(omw.golden_lines(self.dump()), omw.golden_lines(self.dump(extra=extra)))

    def test_golden_ignores_focus_and_fading(self):
        focused = self.dump()
        focused["widgets"][0]["children"][0].update(keyFocus=True, alpha=0.5)
        self.assertEqual(omw.golden_lines(self.dump()), omw.golden_lines(focused))

    def test_golden_ignores_the_order_of_siblings(self):
        swapped = self.dump()
        swapped["widgets"][0]["children"].reverse()
        self.assertEqual(omw.golden_lines(self.dump()), omw.golden_lines(swapped))

    def test_golden_keeps_depth(self):
        self.assertEqual(omw.golden_lines(self.dump())[0], "Window '_Main'")
        self.assertIn("  TextBox 'Save'", omw.golden_lines(self.dump()))


if __name__ == "__main__":
    unittest.main()
