#!/usr/bin/env python3
# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.
"""Install, relocate and run a public C++/PNG consumer through test-runner."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-dir", type=Path,
                        default=Path(__file__).resolve().parents[1])
    parser.add_argument("--producer-build-dir", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--consumer-source", type=Path)
    parser.add_argument("--includedir", default="include")
    parser.add_argument("--jobs", type=int, default=2)
    args = parser.parse_args()
    source = args.source_dir.resolve()
    build = args.producer_build_dir.resolve()
    work = args.work_dir.resolve()
    include = Path(args.includedir)
    if include.is_absolute() or ".." in include.parts or not include.parts:
        parser.error("--includedir must be a nonempty relative directory")
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    cache = {}
    for line in (build / "CMakeCache.txt").read_text().splitlines():
        if line and not line.startswith(("#", "//")) and ":" in line and "=" in line:
            key, rest = line.split(":", 1)
            cache[key] = rest.split("=", 1)[1]
    if Path(cache["CMAKE_HOME_DIRECTORY"]).resolve() != source:
        parser.error("Producer cache does not belong to --source-dir")
    if cache.get("SKITY_TEST", "").upper() not in ("OFF", "FALSE", "0"):
        parser.error("Configure the producer with SKITY_TEST=OFF first")
    if cache.get("CMAKE_GENERATOR") != "Ninja":
        parser.error("This fixture requires a Ninja producer build")
    cmake = cache["CMAKE_COMMAND"]
    ninja = cache["CMAKE_MAKE_PROGRAM"]
    consumer = (args.consumer_source or
                source / "test/fixtures/installed_cpp_consumer").resolve()
    work.mkdir(parents=True, exist_ok=False)
    origin = work / "original-prefix"
    relocated = work / "relocated-prefix"
    client_build = work / "consumer-build"
    steps = []
    result = {"status": "FAIL", "steps": steps,
              "scope": "Installed public C++ core/codec consumer; no private IO"}
    env = dict(os.environ, PYTHONDONTWRITEBYTECODE="1")

    def run(name, argv, cwd=source):
        log = work / (name + ".log")
        with log.open("xb") as output:
            process = subprocess.run(argv, cwd=cwd, env=env, stdout=output,
                                     stderr=subprocess.STDOUT, timeout=1800,
                                     check=False)
        steps.append({"name": name, "argv": list(map(str, argv)),
                      "returncode": process.returncode, "log": str(log)})
        if process.returncode:
            raise RuntimeError("{} failed; see {}".format(name, log))

    try:
        run("configure-install", [cmake, "-S", str(source), "-B", str(build),
            "-DSKITY_BUILD_INSTALL=ON", "-DCMAKE_INSTALL_PREFIX=" + str(origin),
            "-DCMAKE_INSTALL_INCLUDEDIR=" + args.includedir])
        run("build-product", [cmake, "--build", str(build),
                              "--parallel", str(args.jobs)])
        run("install", [cmake, "--install", str(build), "--prefix", str(origin)])
        origin.rename(relocated)
        if origin.exists():
            raise RuntimeError("Original install prefix must be absent")
        run("configure-consumer", [cmake, "-S", str(consumer), "-B", str(client_build),
            "-G", "Ninja", "-DCMAKE_MAKE_PROGRAM=" + ninja,
            "-DCMAKE_PREFIX_PATH=" + str(relocated),
            "-DGOOGLETEST_SOURCE_DIR=" + str(source / "third_party/gtest")])
        includes = (client_build / "imported-includes.txt").read_text()
        if str(origin) in includes:
            raise RuntimeError("Imported includes still contain original prefix")
        run("build-consumer", [cmake, "--build", str(client_build),
                               "--parallel", str(args.jobs)])
        env["PATH"] = str(relocated / "bin") + os.pathsep + env.get("PATH", "")
        run("public-consumer", [sys.executable, "-B", str(source / "tools/test-runner.py"),
            "--suite=unit", "--run-only", "--build-dir=" + str(client_build),
            "--filter=InstalledCppProduct.*", "--parallel=1", "--no-color"])
        report = json.loads((client_build / "agent_test_report.json").read_text())
        raw = json.loads((client_build / "gtest_report_0.json").read_text())
        cases = [case for suite in raw["testsuites"] for case in suite["testsuite"]]
        if (len(cases) != 1 or cases[0].get("result") != "COMPLETED" or
                cases[0].get("failures") or report["summary"]["failed"] or
                report["failures"]):
            raise RuntimeError("Expected one completed, passing public consumer case")
        result.update(status="PASS", test_report=report, includedir=args.includedir,
                      original_prefix_absent=not origin.exists())
    except Exception as error:
        result["error"] = str(error)
    finally:
        (work / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))
    return 0 if result["status"] == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
