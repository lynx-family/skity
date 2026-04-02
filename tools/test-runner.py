#!/usr/bin/env python3

import subprocess
import sys
import os
import argparse
import multiprocessing
import shutil
import json
import time
import re
from typing import List, Tuple, Optional, Dict, Any

EXIT_SUCCESS = 0
EXIT_TEST_FAILED = 1
EXIT_INFRA_ERROR = 2
EXIT_USAGE_ERROR = 3

class Colors:
    RED = '\033[0;31m'
    GREEN = '\033[0;32m'
    YELLOW = '\033[1;33m'
    BLUE = '\033[0;34m'
    NC = '\033[0m'

class TestRunner:
    def __init__(self, args):
        self.build_dir = args.build_dir
        self.parallel_jobs = args.parallel
        self.test_filter = args.filter
        self.verbose = args.verbose
        self.clean_build = args.clean
        self.no_color = args.no_color or not sys.stdout.isatty()
        self.skip_configure = args.no_configure or args.run_only
        self.skip_build = args.no_build or args.run_only
        self.suite = args.suite
        self.started = time.time()
        
        os.makedirs(self.build_dir, exist_ok=True)
        self.report_file = os.path.join(self.build_dir, "agent_test_report.json")
        self.golden_failures_dir = os.path.join(self.build_dir, "golden_failures")
        os.makedirs(self.golden_failures_dir, exist_ok=True)

    def print_status(self, color: str, message: str):
        if self.no_color:
            print(message)
            return
        print(f"{color}{message}{Colors.NC}")

    def run_command(self, cmd: List[str], cwd: Optional[str] = None) -> Tuple[int, str, str]:
        try:
            if self.verbose:
                self.print_status(Colors.BLUE, f"Running: {' '.join(cmd)}")
            result = subprocess.run(
                cmd, 
                cwd=cwd, 
                capture_output=True, 
                text=True,
                check=False
            )
            return result.returncode, result.stdout, result.stderr
        except Exception as e:
            return 1, "", str(e)

    def clean_build_directory(self):
        if self.clean_build and os.path.exists(self.build_dir):
            self.print_status(Colors.YELLOW, "🧹 Cleaning build directory...")
            shutil.rmtree(self.build_dir)
            # Recreate directories after cleaning
            os.makedirs(self.build_dir, exist_ok=True)
            os.makedirs(self.golden_failures_dir, exist_ok=True)

    def configure_cmake(self) -> Tuple[bool, str]:
        self.print_status(Colors.YELLOW, "🔧 Configuring with CMake...")
        os.makedirs(self.build_dir, exist_ok=True)
        cmd = ["cmake", "-B", self.build_dir, "-DSKITY_TEST=ON", "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"]
        
        if self.suite.startswith("golden"):
            cmd.extend([
                "-DSKITY_MTL_BACKEND=ON",
                "-DSKITY_CODEC_MODULE=ON",
                "-DSKITY_GOLDEN_GUI=OFF"
            ])
            
        exit_code, stdout, stderr = self.run_command(cmd)
        if exit_code != 0:
            self.print_status(Colors.RED, "❌ CMake configuration failed!")
            if self.verbose:
                print(f"STDOUT: {stdout}\nSTDERR: {stderr}")
            return False, stderr or stdout
        return True, ""

    def build_project(self) -> Tuple[bool, str]:
        self.print_status(Colors.YELLOW, f"🔨 Building project with {self.parallel_jobs} parallel jobs...")
        cmd = ["cmake", "--build", self.build_dir, "--parallel", str(self.parallel_jobs)]
        exit_code, stdout, stderr = self.run_command(cmd)
        if exit_code != 0:
            self.print_status(Colors.RED, "❌ Build failed!")
            if self.verbose:
                print(f"STDOUT: {stdout}\nSTDERR: {stderr}")
            return False, stderr or stdout
        return True, ""

    def find_test_executable(self) -> Optional[str]:
        target_exe = "skity_unit_test"
        if self.suite == "golden-shape":
            target_exe = "skity_golden_test_shape"
        elif self.suite == "golden-text":
            target_exe = "skity_golden_test_text"
            
        test_paths = [
            os.path.join(self.build_dir, "test", "ut", target_exe),
            os.path.join(self.build_dir, "test", "golden", target_exe),
            os.path.join(self.build_dir, "test", target_exe),
            os.path.join(self.build_dir, target_exe),
        ]
        for path in test_paths:
            if os.path.exists(path) and os.access(path, os.X_OK):
                return path
                
        for root, dirs, files in os.walk(self.build_dir):
            for file in files:
                if file == target_exe and os.access(os.path.join(root, file), os.X_OK):
                    return os.path.join(root, file)
        return None

    def run_gtest_suite(self) -> Dict[str, Any]:
        test_executable = self.find_test_executable()
        if not test_executable:
            return self._create_infra_error("missing_executable", "Test executable not found!")

        self.print_status(Colors.YELLOW, f"🚀 Running {self.suite} tests...")
        
        gtest_json_path = os.path.join(self.build_dir, "gtest_report.json")
        if os.path.exists(gtest_json_path):
            os.remove(gtest_json_path)

        cmd = [test_executable, f"--gtest_output=json:{gtest_json_path}"]
        env = os.environ.copy()
        env["AGENT_OUT_DIR"] = os.path.abspath(self.golden_failures_dir)
        
        if self.test_filter:
            cmd.append(f"--gtest_filter={self.test_filter}")
            self.print_status(Colors.YELLOW, f"🎯 Filter: {self.test_filter}")

        try:
            result = subprocess.run(cmd, env=env, capture_output=True, text=True, check=False)
            
            # Crash or abnormal exit without JSON generated
            if result.returncode != 0 and not os.path.exists(gtest_json_path):
                return self._create_infra_error(
                    "runtime_crash", 
                    f"Test process crashed with exit code {result.returncode}.\nSTDOUT: {result.stdout}\nSTDERR: {result.stderr}"
                )
            
            # Parse the JSON if it exists
            if os.path.exists(gtest_json_path):
                return self._parse_gtest_json(gtest_json_path, result.returncode, result.stdout)
            else:
                return self._create_infra_error("missing_executable", "Tests ran but gtest_report.json was not generated.")

        except Exception as e:
            return self._create_infra_error("infra_error", f"Failed to execute tests: {e}")

    def _parse_gtest_json(self, json_path: str, exit_code: int, stdout: str) -> Dict[str, Any]:
        try:
            with open(json_path, 'r', encoding='utf-8') as f:
                data = json.load(f)
        except Exception as e:
            return self._create_infra_error("infra_error", f"Failed to parse gtest JSON: {e}")

        # Try to extract golden metrics from stdout
        golden_metrics = {}
        if self.suite.startswith("golden"):
            current_test = ""
            suite_name = ""
            test_name = ""
            lines = stdout.split('\n')
            i = 0
            while i < len(lines):
                line = lines[i].strip()
                if line.startswith("test suite name: "):
                    suite_name = line.replace("test suite name: ", "")
                elif line.startswith("test name: "):
                    test_name = line.replace("test name: ", "")
                    current_test = f"{suite_name}.{test_name}"
                elif line == "[[GOLDEN_TEST_FAILED_METRICS]]":
                    # Parse the JSON block that follows
                    json_str = ""
                    i += 1
                    while i < len(lines) and lines[i].strip() != "}":
                        json_str += lines[i] + "\n"
                        i += 1
                    json_str += "}"
                    try:
                        golden_metrics[current_test] = json.loads(json_str)
                    except:
                        pass
                i += 1

        total = data.get("tests", 0)
        failed_count = data.get("failures", 0)
        passed = total - failed_count

        report = {
            "summary": {
                "total": total,
                "passed": passed,
                "failed": failed_count,
                "duration_ms": int((time.time() - self.started) * 1000)
            },
            "failures": []
        }

        # Parse testsuites and testcases
        for ts in data.get("testsuites", []):
            for tc in ts.get("testsuite", []):
                if tc.get("status") == "RUN" and "failures" in tc:
                    for fail in tc["failures"]:
                        failure_msg = fail.get("failure", "")
                        
                        # Extract file and line using regex: "path/to/file.cc:123\nError message"
                        file_path = ""
                        line_num = 0
                        error_summary = failure_msg
                        
                        match = re.match(r'^(.*?):(\d+)\n(.*)', failure_msg, re.DOTALL)
                        if match:
                            file_path = match.group(1)
                            line_num = int(match.group(2))
                            error_summary = match.group(3).strip()

                        case_name = f"{ts.get('name')}.{tc.get('name')}"
                        
                        stage = "Layer 1: Semantic Validation"
                        reason_code = "assert_failed"
                        oracle = "assertion"
                        backend = "cpu"
                        
                        if self.suite.startswith("golden"):
                            stage = "Layer 3: Visual Regression"
                            reason_code = "pixel_mismatch"
                            oracle = "golden"
                            backend = "metal"

                        failure_record = {
                            "suite": self.suite,
                            "case_name": case_name,
                            "status": "FAIL",
                            "exit_code": exit_code,
                            "stage": stage,
                            "reason_code": reason_code,
                            "backend": backend,
                            "oracle": oracle,
                            "error": error_summary,
                            "artifacts": {}
                        }
                        
                        if self.suite.startswith("golden") and case_name in golden_metrics:
                            gm = golden_metrics[case_name]
                            failure_record["metrics"] = {
                                "diff_percent": gm.get("diff_percent", 0),
                                "max_diff_percent": gm.get("max_diff_percent", 0),
                                "diff_pixel_count": gm.get("diff_pixel_count", 0),
                            }
                            if "diff_bbox" in gm:
                                failure_record["metrics"]["diff_bbox"] = gm["diff_bbox"]
                            if "expected_image" in gm:
                                failure_record["artifacts"]["expected_image"] = gm["expected_image"]
                            if "actual_image" in gm:
                                failure_record["artifacts"]["actual_image"] = gm["actual_image"]
                            if "diff_pixels_file" in gm:
                                failure_record["artifacts"]["diff_pixels_file"] = gm["diff_pixels_file"]
                        
                        if file_path:
                            failure_record["artifacts"]["file"] = file_path
                            failure_record["artifacts"]["line"] = line_num
                            
                        report["failures"].append(failure_record)

        return report

    def _create_infra_error(self, reason_code: str, error_msg: str) -> Dict[str, Any]:
        return {
            "summary": {
                "total": 0,
                "passed": 0,
                "failed": 1,
                "duration_ms": int((time.time() - self.started) * 1000)
            },
            "failures": [
                {
                    "suite": self.suite,
                    "case_name": "Infrastructure",
                    "status": "FAIL",
                    "exit_code": EXIT_INFRA_ERROR,
                    "stage": "Layer 0: Infrastructure",
                    "reason_code": reason_code,
                    "backend": "none",
                    "oracle": "system",
                    "error": error_msg,
                    "artifacts": {}
                }
            ]
        }

    def print_agent_report(self, report: Dict[str, Any]):
        # Write to file
        try:
            with open(self.report_file, 'w', encoding='utf-8') as f:
                json.dump(report, f, indent=2, ensure_ascii=False)
        except Exception as e:
            self.print_status(Colors.RED, f"Failed to write {self.report_file}: {e}")

        # Print wrapped in XML tags
        print("\n<agent-test-report>")
        print(json.dumps(report, indent=2, ensure_ascii=False))
        print("</agent-test-report>\n")
        
        # Human readable summary
        failures = len(report["failures"])
        if failures > 0:
            self.print_status(Colors.RED, f"❌ {failures} failures detected.")
        else:
            self.print_status(Colors.GREEN, f"✅ All tests passed!")

    def run(self) -> int:
        self.print_status(Colors.YELLOW, "🧪 Skity Test Runner (Agent Feedback Loop)")
        self.print_status(Colors.YELLOW, "========================================")

        self.clean_build_directory()

        if not self.skip_configure:
            ok, err = self.configure_cmake()
            if not ok:
                report = self._create_infra_error("configure_failed", err)
                self.print_agent_report(report)
                return EXIT_INFRA_ERROR

        if not self.skip_build:
            ok, err = self.build_project()
            if not ok:
                report = self._create_infra_error("build_failed", err)
                self.print_agent_report(report)
                return EXIT_INFRA_ERROR

        # Dispatch by suite
        if self.suite in ("unit", "golden-shape", "golden-text"):
            report = self.run_gtest_suite()
        else:
            report = self._create_infra_error("usage_error", f"Suite '{self.suite}' is not implemented yet.")
            self.print_agent_report(report)
            return EXIT_USAGE_ERROR

        self.print_agent_report(report)
        
        failures = report.get("failures", [])
        if len(failures) > 0:
            # Check if any failure is an infra error (exit_code != 1)
            # and propagate that specific exit code, else return EXIT_TEST_FAILED.
            for fail in failures:
                if fail.get("exit_code", EXIT_TEST_FAILED) == EXIT_INFRA_ERROR:
                    return EXIT_INFRA_ERROR
            return EXIT_TEST_FAILED
        
        return EXIT_SUCCESS


def main():
    parser = argparse.ArgumentParser(description='Skity Test Runner with AI Agent feedback loop')
    parser.add_argument('--suite', default='unit', choices=['unit', 'golden-shape', 'golden-text'], help='Test suite to run')
    parser.add_argument('--build-dir', default='build', help='Build directory (default: build)')
    parser.add_argument('--filter', default='', help='Run only tests matching pattern (e.g. for gtest)')
    parser.add_argument('--parallel', type=int, default=multiprocessing.cpu_count(), help='Number of parallel build jobs')
    parser.add_argument('--verbose', action='store_true', help='Verbose output')
    parser.add_argument('--clean', action='store_true', help='Clean build before running tests')
    parser.add_argument('--no-color', action='store_true', help='Disable ANSI color output')
    parser.add_argument('--no-configure', action='store_true', help='Skip CMake configure step')
    parser.add_argument('--no-build', action='store_true', help='Skip build step')
    parser.add_argument('--run-only', action='store_true', help='Skip configure and build, run tests only')

    args = parser.parse_args()
    if args.parallel <= 0:
        print("Error: --parallel must be > 0", file=sys.stderr)
        return EXIT_USAGE_ERROR

    runner = TestRunner(args)
    return runner.run()

if __name__ == '__main__':
    sys.exit(main())
