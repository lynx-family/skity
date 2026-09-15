# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

import copy
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
from types import SimpleNamespace
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "tools"))
from font_harness_metadata import capture_environment, input_fingerprint, sha256, write_json
from font_harness_runner import run_suite


class OracleContractTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.repo = Path(self.temp.name)
        self.binary = os.environ["SKITY_FONT_HARNESS_TEST_BINARY"]
        (self.repo / "font.ttf").write_bytes(b"synthetic contract input")
        self.case = {"schema_version": 1, "id": "font.synthetic", "backend": "freetype",
                     "platforms": ["linux-freetype"], "category": "typeface_probe", "status": "skity_gap",
                     "font_files": [{"uri": "repo://font.ttf", "id": "fixture", "collection_index": 0}],
                     "typeface_request": {"entry": "MakeFromFile", "font_file": "fixture"},
                     "glyphs": {"chars": ["U+0041"]}, "compare": {"typeface_identity": "normalized_descriptor"}}
        self.case_path = self.repo / "case.json"
        write_json(self.case_path, self.case)
        attachment = self.repo / "capture.txt"
        attachment.write_bytes(b"synthetic source attachment")
        self.oracle = {
            "schema_version": 1, "contract_version": 2, "artifact_type": "font_probe_result",
            "runner": "skia", "case_id": self.case["id"], "backend": "freetype", "ok": True,
            "typeface_result": {
                "collection_index": 0, "font_file_id": "fixture", "font_file_uri": "repo://font.ttf",
                "request_entry": "MakeFromFile",
                "identity": {"family_name": "Fixture", "post_script_name": "Fixture",
                             "style": {"weight": 400, "width": 5, "slant": "upright"},
                             "units_per_em": 1000, "glyph_count": 2}},
            "typeface_probe": {
                "table_count": 1, "tables": [{"tag": "head", "size": 1,
                    "full_copied_size": 1, "full_digest": "fnv1a64:fixture"}],
                "glyphs": [{"char": "U+0041", "glyph_id": 1, "contains": True}]},
            "input_fingerprint": input_fingerprint(self.case, self.repo),
            "provenance": {"version": 2, "profile": "explicit", "skia_commit": "a" * 40,
                           "audit": {key: "b" * 64 for key in ("runner_source", "runner_binary", "skia_library", "gn_args")},
                           "attachments": [{"path": "capture.txt", "sha256": sha256(attachment)}]}}
        self.oracle_path = self.repo / "oracle.json"
        self.check()

    def check(self, expected_code=0, artifact=None, repo=None):
        repo = repo or self.repo
        if repo == self.repo:
            write_json(self.oracle_path, artifact if artifact is not None else self.oracle)
        report = repo / "validation.json"
        environment = repo / "environment.json"
        options = ["--environment", str(environment)] if environment.is_file() else []
        result = subprocess.run([self.binary, "artifact-check", "--case", str(repo / "case.json"),
                                 "--artifact", str(repo / "oracle.json"), "--repo-root", str(repo),
                                 "--report", str(report), *options], capture_output=True, text=True)
        data = json.loads(report.read_text())
        self.assertEqual(result.returncode, expected_code, (result.stderr, data))
        self.assertEqual(data["passed"], expected_code == 0, data)
        return data

    def test_gap_oracle_is_ready_without_actual(self):
        self.check()
        self.assertFalse((self.repo / "actual.json").exists())

    def test_json_layout_and_numeric_spelling_do_not_stale_case(self):
        self.case_path.write_text(json.dumps(self.case, ensure_ascii=True) + "\r\n")
        self.check()
        self.case["font_files"][0]["collection_index"] = 0.0
        # JsonCpp accepts integral numeric spellings; the content identity is unchanged.
        write_json(self.case_path, self.case)
        self.check()

    def test_changed_case_and_font_are_rejected(self):
        changed = copy.deepcopy(self.case)
        changed["status"] = "active"
        write_json(self.case_path, changed)
        self.check(6)
        write_json(self.case_path, self.case)
        (self.repo / "font.ttf").write_bytes(b"changed")
        self.check(6)

    def test_bundle_moves_without_generator_binary_or_absolute_paths(self):
        self.check()
        with tempfile.TemporaryDirectory() as target:
            moved = Path(target) / "moved"
            shutil.copytree(self.repo, moved)
            self.check(repo=moved)

    def test_changed_or_escaping_attachment_is_rejected(self):
        (self.repo / "capture.txt").write_bytes(b"changed")
        self.check(6)
        self.oracle["provenance"]["attachments"][0]["path"] = "../capture.txt"
        self.check(6)

    def test_wrong_runner_profile_pin_and_version_are_rejected(self):
        for key, value in (("runner", "skity"), ("contract_version", 1)):
            with self.subTest(key=key):
                a = copy.deepcopy(self.oracle)
                a[key] = value
                self.check(6, a)
        for key, value in (("profile", "system"), ("skia_commit", "z" * 40), ("version", 1)):
            with self.subTest(key=key):
                a = copy.deepcopy(self.oracle)
                a["provenance"][key] = value
                self.check(6, a)

    def test_incomplete_and_malformed_outputs_are_rejected(self):
        for field in ("glyphs", "tables"):
            for value in ([], "bad", None):
                with self.subTest(field=field, value=value):
                    a = copy.deepcopy(self.oracle)
                    a["typeface_probe"][field] = value
                    self.check(6, a)

    def test_malformed_envelopes_and_provenance_return_reports(self):
        for value in ([], "not an object", 42):
            self.check(6, value)
        for key in ("audit", "attachments"):
            a = copy.deepcopy(self.oracle)
            a["provenance"][key] = "bad"
            self.check(6, a)

    def test_collection_count_is_checked_against_input_bytes(self):
        self.case["typeface_request"]["collection_indices"] = "all"
        (self.repo / "font.ttf").write_bytes(b"ttcf\x00\x01\x00\x00\x00\x00\x00\x02")
        write_json(self.case_path, self.case)
        a = copy.deepcopy(self.oracle)
        face, probe = a.pop("typeface_result"), a.pop("typeface_probe")
        probe["collection_index"] = 0
        a.update(typeface_collection={"collection_count": 1, "indices": [0]},
                 typeface_results=[face], typeface_probes=[probe])
        a["input_fingerprint"] = input_fingerprint(self.case, self.repo)
        self.check(6, a)

    def prepare_fontconfig(self, profile="controlled"):
        self.case = {"schema_version": 1, "id": "font.synthetic", "backend": "fontconfig",
                     "platforms": ["linux-fontconfig"], "category": "font_manager", "status": "active",
                     "font_manager_request": {"entry": "MatchFamilyStyleCharacter", "character": "U+10FFFF"},
                     "compare": {"typeface_identity": "normalized_descriptor"}}
        if profile == "system":
            self.case["fontconfig_profile"] = profile
        write_json(self.case_path, self.case)
        write_json(self.repo / "harness/font/platform/linux/fontconfig/fonts.json",
                   {"font_files": ["repo://font.ttf"]})
        (self.repo / "fonts.conf").write_text("<fontconfig><include>rules.conf</include></fontconfig>")
        (self.repo / "rules.conf").write_text("<fontconfig/>")
        self.inventory = {"version": 21701, "files": [str(self.repo / "font.ttf")],
                          "config_files": [str(self.repo / "fonts.conf"), str(self.repo / "rules.conf")]}
        self.refresh_fontconfig_environment()
        self.oracle.pop("typeface_result")
        self.oracle.pop("typeface_probe")
        self.oracle.update(backend="fontconfig",
                           input_fingerprint=input_fingerprint(self.case, self.repo, self.environment),
                           fontconfig_inventory={"files": ["repo://font.ttf"]},
                           font_manager_probe={
                               "request_input": self.case["font_manager_request"],
                               "operation": {"entry": "MatchFamilyStyleCharacter",
                                             "matched_typeface": {"available": False}},
                               "matched_typefaces": [{"available": False}],
                               "font_manager": {"family_count": 1, "family_names": ["Fixture"]}})
        self.oracle["provenance"].update(profile=profile, controlled_fonts=["repo://font.ttf"])
        self.check()

    def refresh_fontconfig_environment(self):
        self.environment = capture_environment(self.repo, "fontconfig", fontconfig_inventory=self.inventory)
        write_json(self.repo / "environment.json", self.environment)

    def test_changed_included_fontconfig_rules_stale_oracle(self):
        self.prepare_fontconfig()
        (self.repo / "rules.conf").write_text("<fontconfig><!--changed--></fontconfig>")
        self.refresh_fontconfig_environment()
        data = self.check(6)
        self.assertIn("stale artifact", json.dumps(data))

    def test_fontconfig_version_and_inventory_changes_stale_oracle(self):
        self.prepare_fontconfig()
        original = copy.deepcopy(self.inventory)
        for field, value in (("version", 21702), ("files", []), ("config_files", [])):
            with self.subTest(field=field):
                self.inventory = dict(original, **{field: value})
                self.refresh_fontconfig_environment()
                self.assertIn("stale artifact", json.dumps(self.check(6)))

    def test_unlisted_controlled_font_is_rejected(self):
        self.prepare_fontconfig()
        for field in (self.oracle["fontconfig_inventory"]["files"],
                      self.oracle["provenance"]["controlled_fonts"]):
            field.append("repo://unlisted.ttf")
        self.assertIn("controlled fixture", json.dumps(self.check(6)))

    def test_fontconfig_case_cannot_be_relabelled_as_system(self):
        self.prepare_fontconfig()
        self.oracle["provenance"]["profile"] = "system"
        self.assertIn("profile does not match", json.dumps(self.check(6)))

    def test_unmatched_system_fonts_require_complete_inventory(self):
        self.prepare_fontconfig("system")
        self.oracle["font_manager_probe"]["font_manager"]["family_names"] = []
        self.assertIn("family inventory", json.dumps(self.check(6)))

    def run_native_summary(self, items, exit_code=0):
        manifest = {"schema_version": 1, "id": "synthetic", "backend": "freetype",
                    "platforms": ["linux-freetype"], "target_platform": "linux-freetype",
                    "case_root": "cases", "cases": ["one.json", "two.json"], "artifacts": {}}
        for index, name in enumerate(manifest["cases"]):
            write_json(self.repo / "cases" / name, dict(self.case, id="font.synthetic." + str(index)))
        manifest_path = self.repo / "manifest.json"
        write_json(manifest_path, manifest)
        args = SimpleNamespace(font_case=None, font_manifest=str(manifest_path), font_backend="freetype",
                               font_action="run", font_profile="auto", font_reference_oracle_dir=None,
                               font_environment=None, font_oracle_dir=str(self.repo),
                               font_artifact_root=str(self.repo / "out"))
        runner = SimpleNamespace(repo_root=str(self.repo), started=time.time(),
                                 _find_named_test_executable=lambda name: sys.executable)

        def native_run(command, **kwargs):
            self.assertEqual(command[1], "run")
            report = Path(command[command.index("--report") + 1])
            if items is not None:
                write_json(report, {"cases": items})
            return SimpleNamespace(returncode=exit_code, stdout="", stderr="")

        with mock.patch("font_harness_runner.subprocess.run", side_effect=native_run):
            return run_suite(runner, args)

    def test_native_missing_or_partial_report_cannot_pass(self):
        for items in (None, [], [{"case_id": "font.synthetic.0"}]):
            with self.subTest(items=items):
                self.assertGreater(self.run_native_summary(items)["summary"]["failed"], 0)

    def test_native_mismatch_and_probe_failure_remain_failures(self):
        items = [
            {"case_id": "font.synthetic.0", "passed": False, "compare_exit_code": 1,
             "reason_code": "path_mismatch", "artifacts": {}},
            {"case_id": "font.synthetic.1", "passed": False, "probe_exit_code": 7,
             "reason_code": "probe_failed", "artifacts": {}}]
        report = self.run_native_summary(items, 7)
        self.assertEqual(report["summary"]["failed"], 2)
        self.assertEqual([x["reason_code"] for x in report["failures"]], ["path_mismatch", "probe_failed"])

    def test_native_success_requires_boolean_pass_and_integer_exit_codes(self):
        valid = [{"case_id": "font.synthetic." + str(i), "passed": True,
                  "probe_exit_code": 0, "compare_exit_code": 0, "artifacts": {}}
                 for i in range(2)]
        self.assertEqual(self.run_native_summary(valid)["summary"]["failed"], 0)
        for field, value in (("passed", "true"), ("passed", 1),
                             ("probe_exit_code", False), ("compare_exit_code", "0")):
            with self.subTest(field=field, value=value):
                items = copy.deepcopy(valid)
                items[0][field] = value
                self.assertGreater(self.run_native_summary(items)["summary"]["failed"], 0)

    def test_direct_compare_and_python_adapter_share_contract(self):
        output = self.repo / "adapter"
        expected = self.repo / (self.case["id"] + ".skia.json")
        actual = output / "skity" / (self.case["id"] + ".freetype.json")
        args = SimpleNamespace(font_case=str(self.case_path), font_manifest=None,
                               font_backend="freetype", font_action="compare",
                               font_profile="auto", font_reference_oracle_dir=None,
                               font_environment=None, font_oracle_dir=str(self.repo),
                               font_artifact_root=str(output))
        runner = SimpleNamespace(repo_root=str(self.repo), started=time.time(),
                                 _find_named_test_executable=lambda name: self.binary)
        for missing, expected_code in ((False, 0), (True, 6)):
            with self.subTest(missing=missing):
                oracle = copy.deepcopy(self.oracle)
                if missing:
                    oracle["typeface_probe"]["glyphs"] = []
                write_json(expected, oracle)
                oracle["producer"] = "skity"
                write_json(actual, oracle)
                report_path = self.repo / "direct.json"
                direct = subprocess.run(
                    [self.binary, "compare", "--case", str(self.case_path),
                     "--expected", str(expected), "--actual", str(actual),
                     "--repo-root", str(self.repo), "--report", str(report_path)],
                    capture_output=True, text=True)
                data = json.loads(report_path.read_text())
                self.assertEqual(direct.returncode, expected_code, data)
                adapted = run_suite(runner, args)
                self.assertEqual(adapted["results"][0]["child_exit_code"], direct.returncode)
                self.assertEqual(adapted["results"][0]["reason_code"], data["reason_code"])

    def test_native_success_without_probe_results_cannot_pass(self):
        items = [{"case_id": "font.synthetic." + str(i), "passed": True, "artifacts": {}} for i in range(2)]
        self.assertGreater(self.run_native_summary(items)["summary"]["failed"], 0)


if __name__ == "__main__":
    unittest.main()
