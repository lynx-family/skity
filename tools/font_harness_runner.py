# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

"""Launch the native font harness and adapt its reports; never run Skia."""

import os
from pathlib import Path
import platform
import re
import subprocess
import sys
import time

from font_harness_metadata import capture_environment, read_json, write_json


def require(condition, message):
    if not condition:
        raise ValueError(message)


def add_arguments(parser):
    parser.add_argument("--font-action", default="run",
                        choices=["cli-smoke", "case-info", "oracle-check", "probe", "compare", "run"])
    parser.add_argument("--font-case")
    parser.add_argument("--font-manifest")
    parser.add_argument("--font-backend", choices=["freetype", "fontconfig", "coretext", "directwrite"])
    parser.add_argument("--font-oracle-dir")
    parser.add_argument("--font-reference-oracle-dir", help="Check a repeated independent Skia capture")
    parser.add_argument("--font-artifact-root")
    parser.add_argument("--font-profile", default="auto", choices=["auto", "explicit", "controlled", "system"])
    parser.add_argument("--fontconfig-file", help="Explicit Fontconfig configuration for this run")
    parser.add_argument("--font-environment", help="Existing current host snapshot; collected automatically if omitted")


def run_suite(runner, args):
    repo = Path(runner.repo_root).resolve()
    backend = args.font_backend or {"Linux": "freetype", "Windows": "directwrite",
                                    "Darwin": "coretext"}.get(platform.system(), "")
    output = Path(args.font_artifact_root or repo / "local/font-harness/artifacts" /
                  (backend + "-" + time.strftime("%Y%m%d-%H%M%S"))).resolve()
    executable = runner._find_named_test_executable("skity-font")
    if not executable:
        return runner._create_infra_error("missing_executable", "Build skity-font with SKITY_ENABLE_FONT_HARNESS=ON")
    executable = str(Path(executable).resolve())
    env = dict(os.environ, SKITY_FONT_HARNESS_TEST_BINARY=executable)
    if platform.system() == "Linux":
        env.pop("DISPLAY", None)
        env.pop("WAYLAND_DISPLAY", None)
    if getattr(args, "fontconfig_file", None):
        env["FONTCONFIG_FILE"] = str(Path(args.fontconfig_file).resolve())
    records = []

    def invoke(command, report_path=None):
        if report_path:
            Path(report_path).unlink(missing_ok=True)
        result = subprocess.run([str(x) for x in command], cwd=repo, env=env,
                                capture_output=True, text=True, encoding="utf-8", errors="replace")
        write_json(output / "logs" / (str(time.time_ns()) + ".json"),
                   {"command": [str(x) for x in command], "exit_code": result.returncode,
                    "stdout": result.stdout, "stderr": result.stderr})
        data = read_json(report_path) if report_path and Path(report_path).is_file() else {}
        if not report_path:
            data["message"] = result.stdout + result.stderr
        return result.returncode, data

    def record(name, code, data, artifacts, kind="engine_comparison"):
        records.append({"suite": "font-harness", "case_name": name, "status": "FAIL" if code else "PASS",
                        "exit_code": 0 if not code else (1 if code in (1, 7) else 2),
                        "child_exit_code": code, "reason_code": data.get("reason_code", "pass" if not code else "command_failed"),
                        "error": data.get("message", data.get("reason_code", "")) if code else "",
                        "validation_kind": kind, "backend": backend, "stage": args.font_action,
                        "artifacts": artifacts})

    try:
        if args.font_action == "cli-smoke":
            for name, command in (
                ("CLI protocol smoke", [sys.executable, repo / "harness/font/tests/smoke/skity_font_cli_smoke.py", executable, backend]),
                ("Adapter and CLI contract tests", [sys.executable, "-m", "unittest", "discover", "-s", "harness/font/tests/python", "-v"]),
            ):
                code, data = invoke(command)
                if name == "Adapter and CLI contract tests" and not re.search(r"Ran [1-9]\d* tests?", data["message"]):
                    code = code or 6
                record(name, code, data, {}, "harness_selftest")
        else:
            require(bool(args.font_case) != bool(args.font_manifest), "specify exactly one --font-case or --font-manifest")
            manifest = read_json(args.font_manifest) if args.font_manifest else None
            root = (repo / manifest["case_root"]).resolve() if manifest else repo
            paths = [(root / name).resolve() for name in manifest["cases"]] if manifest else [Path(args.font_case).resolve()]
            require(paths and len(set(paths)) == len(paths) and
                    (not manifest or root.is_relative_to(repo) and all(p.is_relative_to(root) for p in paths)),
                    "unsafe or duplicate case paths")
            cases = {path: read_json(path) for path in paths}
            names = [case["id"] for case in cases.values()]
            require(len(set(names)) == len(names) and all(re.fullmatch(r"[A-Za-z0-9_.-]+", n) and n not in (".", "..") for n in names),
                    "unsafe or duplicate artifact names")
            if not args.font_backend:
                backend = (manifest or next(iter(cases.values())))["backend"]
            if args.font_action in ("run", "compare", "oracle-check"):
                require(bool(args.font_oracle_dir), "--font-oracle-dir is required")

            environment = []
            if args.font_action != "case-info":
                snapshot = Path(args.font_environment).resolve() if args.font_environment else output / "environment.json"
                if not args.font_environment:
                    inventory = None
                    if backend == "fontconfig":
                        env_path = output / "env/native.json"
                        _, info = invoke([executable, "env-info", "--backend", backend,
                                          "--repo-root", repo, "--report", env_path], env_path)
                        inventory = info.get("fontconfig_inventory")
                    write_json(snapshot, capture_environment(repo, backend, env, inventory))
                environment = ["--environment", snapshot]

            if manifest and args.font_action == "run" and not args.font_reference_oracle_dir:
                manifest["artifacts"] = {"root": str(output), "report_root": str(output)}
                manifest_path, summary_path = output / "manifest.json", output / "run.json"
                write_json(manifest_path, manifest)
                code, summary = invoke([executable, "run", "--manifest", manifest_path, "--backend", backend,
                                        "--repo-root", repo, "--skia-dir", Path(args.font_oracle_dir).resolve(),
                                        "--report", summary_path, "--profile", args.font_profile, *environment], summary_path)
                items = summary.get("cases", [])
                require(len(items) == len(paths), summary.get("message", "missing or incomplete native run report"))
                for path, item in zip(paths, items):
                    require(item["case_id"] == cases[path]["id"], "native report does not match manifest")
                    artifacts = {key: str(repo / value) for key, value in item["artifacts"].items()}
                    result = 0 if item.get("passed") is True else (item.get("compare_exit_code") or item.get("probe_exit_code") or code or 3)
                    if item.get("passed") is True:
                        require(type(item.get("probe_exit_code")) is int and type(item.get("compare_exit_code")) is int and
                                item["probe_exit_code"] == 0 and item["compare_exit_code"] == 0,
                                "successful native case lacks probe/compare results")
                    record(item["case_id"], result, item, artifacts)
                require(not code or any(r["status"] == "FAIL" for r in records), "native run failed without a case failure")
            else:
                # Diagnostic actions and individual gap cases use the same native commands.
                require(not (manifest and args.font_action == "run"),
                        "use oracle-check for repeat captures, then run the native manifest")
                for path, case in cases.items():
                    name = case["id"]
                    artifacts = {"case": str(path), "actual": str(output / "skity" / (name + "." + backend + ".json"))}
                    kind = {"case-info": "case_schema", "probe": "skity_probe", "oracle-check": "oracle_readiness"}.get(args.font_action, "engine_comparison")
                    if args.font_action in ("case-info", "oracle-check"):
                        report_path = output / args.font_action / (name + ".json")
                        command = [executable, "case-info", "--case", path, "--repo-root", repo, "--report", report_path]
                        flag = "valid"
                        if args.font_action == "oracle-check":
                            expected = Path(args.font_oracle_dir).resolve() / (name + ".skia.json")
                            artifacts["expected"] = str(expected)
                            command = [executable, "artifact-check", "--case", path, "--artifact", expected,
                                       "--repo-root", repo, "--report", report_path, "--profile", args.font_profile, *environment]
                            if args.font_reference_oracle_dir:
                                command += ["--reference", Path(args.font_reference_oracle_dir) / expected.name]
                            flag = "passed"
                        code, data = invoke(command, report_path)
                        code = code or (0 if data.get(flag) is True else 6)
                        artifacts["validation"] = str(report_path)
                        record(name, code, data, artifacts, kind)
                        continue
                    code, data = 0, {}
                    if args.font_action in ("run", "probe"):
                        code, data = invoke([executable, "probe", "--case", path, "--repo-root", repo,
                                             "--backend", backend, "--out", artifacts["actual"], *environment], artifacts["actual"])
                        code = code or (0 if data.get("ok") is True else 7)
                    if not code and args.font_action in ("run", "compare"):
                        artifacts.update(expected=str(Path(args.font_oracle_dir).resolve() / (name + ".skia.json")),
                                         compare=str(output / "compare" / (name + "." + backend + ".compare.json")))
                        code, data = invoke([executable, "compare", "--case", path, "--repo-root", repo, "--backend", backend,
                                             "--expected", artifacts["expected"], "--actual", artifacts["actual"],
                                             "--report", artifacts["compare"], "--profile", args.font_profile, *environment], artifacts["compare"])
                        code = code or (0 if data.get("passed") is True else 6)
                    record(name, code, data, artifacts, kind)
    except (OSError, ValueError, KeyError, TypeError, AttributeError, subprocess.SubprocessError) as error:
        record("Input", 6, {"reason_code": "invalid_input_or_native_report", "message": str(error)}, {})

    failures = [item for item in records if item["status"] == "FAIL"]
    report = {"summary": {"total": len(records), "passed": len(records) - len(failures),
                          "failed": len(failures), "duration_ms": int((time.time() - runner.started) * 1000)},
              "failures": failures, "results": records, "artifact_root": str(output)}
    write_json(output / "agent_test_report.json", report)
    return report
