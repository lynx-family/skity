# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest import mock


SPEC = importlib.util.spec_from_file_location(
    "skity_test_runner", Path(__file__).resolve().parents[4] / "tools/test-runner.py")
TEST_RUNNER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TEST_RUNNER)


class ExecutableLookupTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.runner = TEST_RUNNER.TestRunner.__new__(TEST_RUNNER.TestRunner)
        self.runner.build_dir = str(self.root)
        self.name = self.runner._candidate_executable_names("skity-font")[0]

    def executable(self, directory, name=None):
        path = self.root / directory / (name or self.name)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("test executable placeholder")
        path.chmod(0o755)
        return str(path)

    def test_current_harness_takes_precedence_over_nested_build(self):
        self.executable("agent_font_link_check/harness/font")
        expected = self.executable("harness/font")
        self.assertEqual(self.runner._find_named_test_executable("skity-font"), expected)

    def test_missing_harness_does_not_fall_back_to_nested_build(self):
        self.executable("agent_font_link_check/harness/font")
        self.assertIsNone(self.runner._find_named_test_executable("skity-font"))

    def test_supports_root_and_configuration_outputs(self):
        for name in ("skity-font", "skity-font.exe"):
            for base in (".", "harness/font"):
                for config in (".", "Debug", "Release", "RelWithDebInfo", "MinSizeRel"):
                    with self.subTest(name=name, base=base, config=config):
                        expected = self.executable(Path(base) / config, name)
                        with mock.patch.object(self.runner, "_candidate_executable_names", return_value=[name]):
                            self.assertEqual(self.runner._find_named_test_executable("skity-font"), expected)
                        Path(expected).unlink()


if __name__ == "__main__":
    unittest.main()
