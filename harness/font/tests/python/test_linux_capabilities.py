# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

import copy
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


@unittest.skipUnless(sys.platform == "linux", "Linux capability tests")
class LinuxCapabilitiesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.repo = Path(__file__).resolve().parents[4]
        cls.binary = os.environ["SKITY_FONT_HARNESS_TEST_BINARY"]
        cls.base = json.loads((cls.repo / "harness/font/cases/typeface_probe/roboto_regular_file_freetype.json").read_text())

    def run_case(self, case, action="probe", backend="freetype"):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "case.json"
            output = Path(directory) / "report.json"
            path.write_text(json.dumps(case))
            command = [self.binary, action, "--case", str(path), "--repo-root", str(self.repo), "--report", str(output)]
            if action != "case-info":
                command += ["--backend", backend]
            result = subprocess.run(command, cwd=self.repo, capture_output=True, text=True)
            data = json.loads(output.read_text()) if output.exists() else {}
            return result.returncode, data

    def test_file_probe_is_available_without_display(self):
        self.assertNotIn("DISPLAY", os.environ)
        self.assertNotIn("WAYLAND_DISPLAY", os.environ)
        code, data = self.run_case(self.base)
        self.assertEqual(code, 0, data)
        self.assertTrue(data["ok"])

    def test_other_freetype_platform_is_not_enabled(self):
        case = copy.deepcopy(self.base)
        case["platforms"] = ["android-freetype"]
        code, data = self.run_case(case)
        self.assertEqual(code, 5, data)
        self.assertEqual(data["reason_code"], "backend_unavailable")

    def test_fontconfig_is_unavailable(self):
        case = {"schema_version": 1, "id": "font.synthetic.system", "category": "font_manager",
                "status": "skity_gap", "backend": "fontconfig", "platforms": ["linux-fontconfig"],
                "font_manager_request": {"entry": "GetDefaultTypeface"}}
        code, data = self.run_case(case, backend="fontconfig")
        self.assertEqual(code, 5, data)
        self.assertEqual(data["reason_code"], "backend_unavailable")

    def test_manager_source_cannot_bypass_explicit_gate(self):
        case = copy.deepcopy(self.base)
        case["category"] = "glyph_path"
        case.pop("typeface_request")
        case["font_manager_request"] = {"entry": "MatchFamilyStyle", "family_name": None}
        case["font_request"] = {"size": 64}
        code, data = self.run_case(case)
        self.assertEqual(code, 5, data)
        self.assertEqual(data["reason_code"], "backend_unavailable")

    def test_invalid_collection_index_is_probe_failure(self):
        case = copy.deepcopy(self.base)
        case["font_files"][0]["collection_index"] = 9999
        code, data = self.run_case(case)
        self.assertEqual(code, 7, data)
        self.assertFalse(data["ok"])

    def test_missing_file_is_schema_failure(self):
        case = copy.deepcopy(self.base)
        case["font_files"][0]["uri"] = "repo://test/fonts/does-not-exist.ttf"
        code, data = self.run_case(case)
        self.assertEqual(code, 3, data)

    def test_bad_font_bytes_fail_without_crashing(self):
        local = self.repo / "local/font-harness"
        local.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=local) as directory:
            font = Path(directory) / "bad.ttf"
            font.write_bytes(b"not a font")
            case = copy.deepcopy(self.base)
            case["font_files"][0]["uri"] = "repo://" + font.relative_to(self.repo).as_posix()
            code, data = self.run_case(case)
            self.assertEqual(code, 7, data)
            self.assertFalse(data["ok"])

    def test_invalid_variation_axis_is_schema_failure(self):
        case = copy.deepcopy(self.base)
        case["typeface_request"].update(entry="MakeVariation", source_entry="MakeFromFile",
                                         variation_position=[{"axis": "bad", "value": 700}])
        code, data = self.run_case(case)
        self.assertEqual(code, 3, data)
        self.assertFalse(data["ok"])

    def test_surrogate_is_rejected_by_case_info(self):
        case = copy.deepcopy(self.base)
        case["glyphs"] = {"chars": ["U+D800"]}
        code, data = self.run_case(case, action="case-info")
        self.assertEqual(code, 3, data)
        self.assertFalse(data["valid"])


if __name__ == "__main__":
    unittest.main()