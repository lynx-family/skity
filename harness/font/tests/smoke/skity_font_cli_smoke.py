# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

import json
import subprocess
import sys
import tempfile
from pathlib import Path


CASE_ID = "font.synthetic.typeface"
FONT_MANAGER_CASE_ID = "font.synthetic.font_manager.default_typeface"
BACKEND = "coretext"
TARGET_PLATFORM = "macos-coretext"


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")


def read_json(path):
    return json.loads(path.read_text())


def make_case():
    return {
        "schema_version": 1,
        "id": CASE_ID,
        "category": "typeface_probe",
        "status": "active",
        "backend": BACKEND,
        "platforms": [TARGET_PLATFORM],
        "font_files": [
            {
                "id": "synthetic",
                "uri": "repo://fonts/synthetic.ttf",
                "collection_index": 0,
            }
        ],
        "typeface_request": {
            "entry": "MakeFromFile",
            "font_file": "synthetic",
        },
        "glyphs": {"chars": ["U+0041"]},
        "compare": {
            "typeface_identity": "normalized_descriptor",
            "font_style": "exact",
        },
    }


def make_manifest(cases=None, manifest_id="font.synthetic.smoke"):
    return {
        "schema_version": 1,
        "id": manifest_id,
        "backend": BACKEND,
        "platforms": [TARGET_PLATFORM],
        "target_platform": TARGET_PLATFORM,
        "case_root": "cases",
        "cases": cases or ["typeface.json"],
        "artifacts": {
            "root": f"artifacts/{TARGET_PLATFORM}",
            "report_root": f"reports/{TARGET_PLATFORM}",
        },
    }


def make_font_manager_case():
    return {
        "schema_version": 1,
        "id": FONT_MANAGER_CASE_ID,
        "category": "font_manager",
        "status": "active",
        "backend": BACKEND,
        "platforms": [TARGET_PLATFORM],
        "font_manager_request": {
            "entry": "GetDefaultTypeface",
            "style": {
                "weight": 400,
                "width": 5,
                "slant": "upright",
            },
            "sample_limit": 1,
        },
        "font_request": {
            "size": 16,
            "scale_x": 1,
            "skew_x": 0,
            "linear_metrics": False,
            "subpixel": True,
            "embolden": False,
            "hinting": "normal",
        },
        "glyphs": {"chars": ["U+0041"]},
        "compare": {
            "typeface_identity": "normalized_descriptor",
            "font_style": "exact",
            "font_metrics": {"mode": "ignore"},
        },
    }


def make_probe_artifact(glyph_id):
    return {
        "schema_version": 1,
        "artifact_type": "font_probe_result",
        "case_id": CASE_ID,
        "backend": BACKEND,
        "ok": True,
        "typeface_result": {
            "typeface": {
                "family_name": "Synthetic",
                "post_script_name": "Synthetic-Regular",
                "collection_index": 0,
                "style": {
                    "weight": 400,
                    "width": 5,
                    "slant": "upright",
                },
            }
        },
        "typeface_probe": {
            "units_per_em": 1000,
            "table_count": 1,
            "glyphs": [
                {
                    "label": "U+0041",
                    "code_point": 65,
                    "glyph_id": glyph_id,
                }
            ],
            "tables": [
                {
                    "tag": "head",
                    "tag_value": 1751474532,
                    "size": 54,
                }
            ],
        },
    }


def make_font_manager_probe_artifact(post_script_name, glyph_id):
    return {
        "schema_version": 1,
        "artifact_type": "font_probe_result",
        "case_id": FONT_MANAGER_CASE_ID,
        "backend": BACKEND,
        "ok": True,
        "font_manager_probe": {
            "category": "font_manager",
            "operation": {"entry": "GetDefaultTypeface"},
            "matched_typefaces": [
                {
                    "available": True,
                    "descriptor": {
                        "family_name": "Synthetic",
                        "post_script_name": post_script_name,
                        "collection_index": 0,
                        "style": {
                            "weight": 400,
                            "width": 5,
                            "slant": "upright",
                        },
                    },
                    "font_style": {
                        "weight": 400,
                        "width": 5,
                        "slant": "upright",
                    },
                    "units_per_em": 1000,
                    "probe_summary": {
                        "glyphs": [
                            {
                                "label": "U+0041",
                                "code_point": 65,
                                "glyph_id": glyph_id,
                            }
                        ],
                        "font_result": {
                            "font_metrics": {
                                "ascent": -10.0,
                                "descent": 3.0,
                                "leading": 1.0,
                            }
                        },
                        "scaler_context_result": {
                            "font_metrics": {
                                "ascent": -10.0,
                                "descent": 3.0,
                                "leading": 1.0,
                            }
                        },
                    },
                }
            ],
        },
    }


def run_command(args):
    return subprocess.run(args, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def fail(label, result):
    print(f"{label} failed", file=sys.stderr)
    print("command:", " ".join(str(arg) for arg in result.args), file=sys.stderr)
    print("exit:", result.returncode, file=sys.stderr)
    print("stdout:", result.stdout, file=sys.stderr)
    print("stderr:", result.stderr, file=sys.stderr)
    return 1


def expect_exit(label, result, expected_exit):
    if result.returncode != expected_exit:
        return fail(label, result)
    return 0


def expect_report_field(path, field, expected):
    report = read_json(path)
    value = report
    for part in field.split("."):
        if isinstance(value, list):
            value = value[int(part)]
        else:
            value = value[part]
    if value != expected:
        print(
            f"{path}: expected {field}={expected!r}, got {value!r}",
            file=sys.stderr,
        )
        return 1
    return 0


def expect_report_has(path, field):
    report = read_json(path)
    value = report
    for part in field.split("."):
        if isinstance(value, list):
            index = int(part)
            if index >= len(value):
                print(f"{path}: missing {field}", file=sys.stderr)
                return 1
            value = value[index]
        elif part in value:
            value = value[part]
        else:
            print(f"{path}: missing {field}", file=sys.stderr)
            return 1
    return 0


def main():
    if len(sys.argv) != 2:
        print("usage: skity_font_cli_smoke.py <skity-font>", file=sys.stderr)
        return 2

    skity_font = Path(sys.argv[1])
    if not skity_font.is_file():
        print(f"skity-font binary does not exist: {skity_font}", file=sys.stderr)
        return 2

    with tempfile.TemporaryDirectory(prefix="skity-font-cli-smoke-") as tmp:
        tmp_dir = Path(tmp)
        repo_root = tmp_dir / "repo"
        case_path = repo_root / "cases/typeface.json"
        font_manager_case_path = repo_root / "cases/font_manager_default.json"
        manifest_path = repo_root / "manifests/smoke.json"
        full_manifest_path = repo_root / "manifests/full.json"
        expected_path = tmp_dir / "expected.skia.json"
        actual_path = tmp_dir / "actual.skity.json"
        mismatch_path = tmp_dir / "actual-mismatch.skity.json"
        font_manager_expected_path = tmp_dir / "font-manager-expected.skia.json"
        font_manager_mismatch_path = tmp_dir / "font-manager-mismatch.skity.json"
        report_dir = tmp_dir / "reports"

        (repo_root / "fonts").mkdir(parents=True)
        (repo_root / "fonts/synthetic.ttf").write_bytes(b"synthetic font bytes")
        write_json(case_path, make_case())
        write_json(font_manager_case_path, make_font_manager_case())
        write_json(manifest_path, make_manifest())
        write_json(
            full_manifest_path,
            make_manifest(
                ["typeface.json", "font_manager_default.json"],
                manifest_id="font.synthetic.full",
            ),
        )
        write_json(expected_path, make_probe_artifact(1))
        write_json(actual_path, make_probe_artifact(1))
        write_json(mismatch_path, make_probe_artifact(2))
        write_json(
            font_manager_expected_path,
            make_font_manager_probe_artifact("Synthetic-Regular", 1),
        )
        write_json(
            font_manager_mismatch_path,
            make_font_manager_probe_artifact("Synthetic-Bold", 2),
        )

        case_info_report = report_dir / "case-info.json"
        result = run_command(
            [
                skity_font,
                "case-info",
                "--case",
                case_path,
                "--repo-root",
                repo_root,
                "--report",
                case_info_report,
            ]
        )
        if expect_exit("case-info", result, 0):
            return 1
        if expect_report_field(case_info_report, "valid", True):
            return 1

        result = run_command([skity_font, "match", "--help"])
        if expect_exit("match help", result, 0):
            return 1

        match_reject_report = report_dir / "match-reject-typeface.json"
        result = run_command(
            [
                skity_font,
                "match",
                "--case",
                case_path,
                "--backend",
                BACKEND,
                "--repo-root",
                repo_root,
                "--out",
                match_reject_report,
            ]
        )
        if expect_exit("match rejects non-font-manager case", result, 3):
            return 1
        if expect_report_field(
            match_reject_report, "reason_code", "schema_validation_failed"
        ):
            return 1
        if expect_report_field(match_reject_report, "ok", False):
            return 1

        match_report = report_dir / "match-font-manager.json"
        result = run_command(
            [
                skity_font,
                "match",
                "--case",
                font_manager_case_path,
                "--backend",
                BACKEND,
                "--repo-root",
                repo_root,
                "--out",
                match_report,
            ]
        )
        if result.returncode == 0:
            if expect_report_field(match_report, "ok", True):
                return 1
            if expect_report_has(match_report, "font_manager_probe.matched_typefaces.0"):
                return 1
            if expect_report_has(
                match_report,
                "font_manager_probe.matched_typefaces.0.probe_summary",
            ):
                return 1
        else:
            if expect_exit("match font_manager", result, 5):
                return 1
            if expect_report_field(match_report, "reason_code", "backend_unavailable"):
                return 1
            if expect_report_field(match_report, "ok", False):
                return 1

        compare_report = report_dir / "compare-pass.json"
        result = run_command(
            [
                skity_font,
                "compare",
                "--case",
                case_path,
                "--expected",
                expected_path,
                "--actual",
                actual_path,
                "--backend",
                BACKEND,
                "--repo-root",
                repo_root,
                "--report",
                compare_report,
            ]
        )
        if expect_exit("compare pass", result, 0):
            return 1
        if expect_report_field(compare_report, "passed", True):
            return 1

        mismatch_report = report_dir / "compare-mismatch.json"
        result = run_command(
            [
                skity_font,
                "compare",
                "--case",
                case_path,
                "--expected",
                expected_path,
                "--actual",
                mismatch_path,
                "--backend",
                BACKEND,
                "--repo-root",
                repo_root,
                "--report",
                mismatch_report,
            ]
        )
        if expect_exit("compare mismatch", result, 1):
            return 1
        if expect_report_field(mismatch_report, "reason_code", "glyph_id_mismatch"):
            return 1

        selection_mismatch_report = report_dir / "compare-selection-mismatch.json"
        result = run_command(
            [
                skity_font,
                "compare",
                "--case",
                font_manager_case_path,
                "--expected",
                font_manager_expected_path,
                "--actual",
                font_manager_mismatch_path,
                "--backend",
                BACKEND,
                "--repo-root",
                repo_root,
                "--report",
                selection_mismatch_report,
            ]
        )
        if expect_exit("compare selection mismatch", result, 1):
            return 1
        if expect_report_field(
            selection_mismatch_report, "reason_code", "selection_mismatch"
        ):
            return 1
        if expect_report_field(selection_mismatch_report, "diff_count", 1):
            return 1
        if expect_report_field(
            selection_mismatch_report,
            "diff_path",
            "font_manager_probe.matched_typefaces[0].identity.post_script_name",
        ):
            return 1

        missing_oracle_report = report_dir / "compare-missing-oracle.json"
        result = run_command(
            [
                skity_font,
                "compare",
                "--case",
                case_path,
                "--expected",
                tmp_dir / "missing.skia.json",
                "--actual",
                actual_path,
                "--backend",
                BACKEND,
                "--repo-root",
                repo_root,
                "--report",
                missing_oracle_report,
            ]
        )
        if expect_exit("compare missing oracle", result, 6):
            return 1
        if expect_report_field(
            missing_oracle_report, "reason_code", "oracle_unavailable"
        ):
            return 1

        run_report = (
            repo_root / f"reports/{TARGET_PLATFORM}/font.synthetic.smoke.latest.json"
        )
        result = run_command(
            [
                skity_font,
                "run",
                "--manifest",
                manifest_path,
                "--backend",
                BACKEND,
                "--repo-root",
                repo_root,
            ]
        )
        if expect_exit("run missing oracle", result, 6):
            return 1
        if expect_report_field(run_report, "target_platform", TARGET_PLATFORM):
            return 1
        if expect_report_field(
            run_report, "artifacts.root", f"artifacts/{TARGET_PLATFORM}"
        ):
            return 1
        if expect_report_field(
            run_report, "artifacts.skia_dir", f"artifacts/{TARGET_PLATFORM}/skia"
        ):
            return 1
        if expect_report_field(run_report, "input_failure_count", 1):
            return 1
        if expect_report_field(run_report, "cases.0.reason_code", "missing_oracle"):
            return 1

        full_run_report = (
            repo_root / f"reports/{TARGET_PLATFORM}/font.synthetic.full.latest.json"
        )
        result = run_command(
            [
                skity_font,
                "run",
                "--manifest",
                full_manifest_path,
                "--backend",
                BACKEND,
                "--repo-root",
                repo_root,
            ]
        )
        if expect_exit("run synthetic full missing oracle", result, 6):
            return 1
        if expect_report_field(full_run_report, "case_count", 2):
            return 1
        if expect_report_field(full_run_report, "input_failure_count", 2):
            return 1
        if expect_report_field(full_run_report, "schema_failure_count", 0):
            return 1
        if expect_report_field(full_run_report, "probe_failure_count", 0):
            return 1
        if expect_report_field(full_run_report, "io_failure_count", 0):
            return 1
        if expect_report_field(full_run_report, "backend_unavailable_count", 0):
            return 1
        if expect_report_field(full_run_report, "cases.0.reason_code", "missing_oracle"):
            return 1
        if expect_report_field(full_run_report, "cases.1.reason_code", "missing_oracle"):
            return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
