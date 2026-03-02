#!/usr/bin/env python3

import subprocess
import sys
import os
import re
import argparse
import multiprocessing
import shutil
import json
import time
from typing import List, Tuple, Optional

EXIT_SUCCESS = 0
EXIT_TEST_FAILED = 1
EXIT_INFRA_ERROR = 2
EXIT_USAGE_ERROR = 3

class Colors:
    RED = '\033[0;31m'
    GREEN = '\033[0;32m'
    YELLOW = '\033[1;33m'
    BLUE = '\033[0;34m'
    NC = '\033[0m'  # No Color

class TestRunner:
    def __init__(self):
        self.build_dir = "build"
        self.parallel_jobs = multiprocessing.cpu_count()
        self.test_filter = ""
        self.verbose = False
        self.clean_build = False
        self.json_output = False
        self.no_color = False
        self.skip_configure = False
        self.skip_build = False
        self.started = time.time()

    def print_status(self, color: str, message: str):
        """Print colored status message"""
        if self.json_output:
            return
        if self.no_color:
            clean = re.sub(r'[\U00010000-\U0010ffff]', '', message).strip()
            print(clean)
            return
        print(f"{color}{message}{Colors.NC}")

    def get_cpu_count(self) -> int:
        """Get number of CPU cores"""
        try:
            return multiprocessing.cpu_count()
        except:
            return 4

    def run_command(self, cmd: List[str], cwd: Optional[str] = None) -> Tuple[int, str, str]:
        """Run a command and return exit code, stdout, stderr"""
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
        """Clean build directory if requested"""
        if self.clean_build and os.path.exists(self.build_dir):
            self.print_status(Colors.YELLOW, "🧹 Cleaning build directory...")
            shutil.rmtree(self.build_dir)

    def configure_cmake(self) -> bool:
        """Configure project with CMake"""
        self.print_status(Colors.YELLOW, "🔧 Configuring with CMake...")

        # Create build directory if it doesn't exist
        os.makedirs(self.build_dir, exist_ok=True)

        cmd = ["cmake", "-B", self.build_dir, "-DSKITY_TEST=ON"]
        exit_code, stdout, stderr = self.run_command(cmd)

        if exit_code != 0:
            self.print_status(Colors.RED, f"❌ CMake configuration failed!")
            if self.verbose:
                print(f"STDOUT: {stdout}")
                print(f"STDERR: {stderr}")
            return False, {'step': 'configure', 'error': stderr or stdout}

        return True, {}

    def build_project(self) -> bool:
        """Build the project"""
        self.print_status(Colors.YELLOW, f"🔨 Building project with {self.parallel_jobs} parallel jobs...")

        cmd = ["cmake", "--build", self.build_dir, "--parallel", str(self.parallel_jobs)]
        exit_code, stdout, stderr = self.run_command(cmd)

        if exit_code != 0:
            self.print_status(Colors.RED, "❌ Build failed!")
            if self.verbose:
                print(f"STDOUT: {stdout}")
                print(f"STDERR: {stderr}")
            return False, {'step': 'build', 'error': stderr or stdout}

        return True, {}

    def find_test_executable(self) -> Optional[str]:
        """Find the test executable"""
        test_paths = [
            os.path.join(self.build_dir, "test", "skity_unit_test"),
            os.path.join(self.build_dir, "skity_unit_test"),
        ]

        for path in test_paths:
            if os.path.exists(path) and os.access(path, os.X_OK):
                return path

        # Search recursively if not found
        for root, dirs, files in os.walk(self.build_dir):
            for file in files:
                if file == "skity_unit_test" and os.access(os.path.join(root, file), os.X_OK):
                    return os.path.join(root, file)

        return None

    def parse_failed_tests(self, output: str) -> List[Tuple[str, str]]:
        """Parse failed tests from gtest output"""
        failed_tests = []

        # Pattern for failed tests
        failed_pattern = r'^\[\s+FAILED\s+\]\s+(.+)$'
        detail_pattern = r'(Expected|Actual|Error|Failure|vs)'

        lines = output.split('\n')
        current_test = None

        for line in lines:
            # Check for failed test
            failed_match = re.match(failed_pattern, line.strip())
            if failed_match:
                current_test = failed_match.group(1)
                failed_tests.append((current_test, ""))
                continue

            # Check for failure details
            if current_test and re.search(detail_pattern, line, re.IGNORECASE):
                # Update the last failed test with details
                if failed_tests:
                    test_name, details = failed_tests[-1]
                    if details:
                        details += f" | {line.strip()}"
                    else:
                        details = line.strip()
                    failed_tests[-1] = (test_name, details)

        return failed_tests

    def run_tests(self) -> Tuple[int, str]:
        """Run tests and return exit code and output"""
        test_executable = self.find_test_executable()
        if not test_executable:
            self.print_status(Colors.RED, "❌ Test executable not found!")
            return EXIT_INFRA_ERROR, ""

        self.print_status(Colors.YELLOW, "🚀 Running tests...")

        cmd = [test_executable]

        # Set environment variables
        env = os.environ.copy()
        if self.test_filter:
            env['GTEST_FILTER'] = self.test_filter
            self.print_status(Colors.YELLOW, f"🎯 Running tests matching: {self.test_filter}")

        try:
            result = subprocess.run(
                cmd, 
                env=env,
                capture_output=True, 
                text=True,
                check=False
            )

            return result.returncode, result.stdout + result.stderr

        except Exception as e:
            self.print_status(Colors.RED, f"❌ Failed to run tests: {e}")
            return EXIT_INFRA_ERROR, ""

    def report_test_results(self, exit_code: int, output: str):
        """Report test results including failures"""
        if not self.json_output:
            print(output)

        # Parse failed tests
        failed_tests = self.parse_failed_tests(output)

        if failed_tests:
            self.print_status(Colors.RED, "❌ Failed test cases detected:")
            for test_name, details in failed_tests:
                self.print_status(Colors.RED, f"  - {test_name}")
                if details:
                    self.print_status(Colors.RED, f"    Details: {details}")

            # Count total tests
            total_tests = len(re.findall(r'^\[.*\]', output, re.MULTILINE))
            passed_tests = len(re.findall(r'^\[\s+OK\s+\]', output, re.MULTILINE))

            self.print_status(Colors.RED, f"❌ Test Results: {len(failed_tests)} failed out of {total_tests} total tests")
            return False, {
                'status': 'test_failed',
                'exit_code': EXIT_TEST_FAILED,
                'failed_tests': [{'name': n, 'details': d} for n, d in failed_tests],
                'passed_test_markers': passed_tests,
                'total_test_markers': total_tests,
            }

        elif exit_code != 0:
            self.print_status(Colors.RED, f"❌ Test execution failed with exit code: {exit_code}")
            return False, {
                'status': 'infra_error',
                'exit_code': EXIT_INFRA_ERROR,
                'failed_tests': [],
                'test_process_exit_code': exit_code,
            }

        else:
            # Count passed tests
            passed_tests = len(re.findall(r'^\[\s+OK\s+\]', output, re.MULTILINE))
            self.print_status(Colors.GREEN, f"✅ All tests passed! ({passed_tests} tests)")
            return True, {
                'status': 'ok',
                'exit_code': EXIT_SUCCESS,
                'failed_tests': [],
                'passed_test_markers': passed_tests,
            }

    def print_json_result(self, payload):
        payload['duration_ms'] = int((time.time() - self.started) * 1000)
        print(json.dumps(payload, ensure_ascii=True, sort_keys=True))

    def run(self, args):
        """Main execution"""
        # Parse arguments
        self.build_dir = args.build_dir
        self.parallel_jobs = args.parallel
        self.test_filter = args.filter
        self.verbose = args.verbose
        self.clean_build = args.clean
        self.json_output = args.json
        self.no_color = args.no_color or not sys.stdout.isatty()
        self.skip_configure = args.no_configure or args.run_only
        self.skip_build = args.no_build or args.run_only

        self.print_status(Colors.YELLOW, "🧪 Skity Test Runner")
        self.print_status(Colors.YELLOW, "===================")

        # Clean build directory if requested
        self.clean_build_directory()

        # Configure with CMake
        if not self.skip_configure:
            configured, configure_error = self.configure_cmake()
            if not configured:
                if self.json_output:
                    self.print_json_result(
                        {
                            'mode': 'test_runner',
                            'status': 'infra_error',
                            'exit_code': EXIT_INFRA_ERROR,
                            'stage': 'configure',
                            'error': configure_error.get('error', ''),
                        }
                    )
                return EXIT_INFRA_ERROR

        # Build project
        if not self.skip_build:
            built, build_error = self.build_project()
            if not built:
                if self.json_output:
                    self.print_json_result(
                        {
                            'mode': 'test_runner',
                            'status': 'infra_error',
                            'exit_code': EXIT_INFRA_ERROR,
                            'stage': 'build',
                            'error': build_error.get('error', ''),
                        }
                    )
                return EXIT_INFRA_ERROR

        # Run tests
        exit_code, output = self.run_tests()

        # Report results
        success, result_payload = self.report_test_results(exit_code, output)
        if self.json_output:
            self.print_json_result(
                {
                    'mode': 'test_runner',
                    'status': result_payload['status'],
                    'exit_code': result_payload['exit_code'],
                    'build_dir': self.build_dir,
                    'filter': self.test_filter,
                    'skip_configure': self.skip_configure,
                    'skip_build': self.skip_build,
                    'failed_tests': result_payload.get('failed_tests', []),
                    'passed_test_markers': result_payload.get('passed_test_markers', 0),
                    'total_test_markers': result_payload.get('total_test_markers', 0),
                    'test_process_exit_code': result_payload.get('test_process_exit_code', 0),
                }
            )

        return EXIT_SUCCESS if success else result_payload['exit_code']

def main():
    parser = argparse.ArgumentParser(description='Skity Test Runner with failure reporting')
    parser.add_argument('--build-dir', default='build', help='Build directory (default: build)')
    parser.add_argument('--filter', default='', help='Run only tests matching pattern')
    parser.add_argument('--parallel', type=int, default=multiprocessing.cpu_count(), 
                       help='Number of parallel build jobs')
    parser.add_argument('--verbose', action='store_true', help='Verbose output')
    parser.add_argument('--clean', action='store_true', help='Clean build before running tests')
    parser.add_argument('--json', action='store_true', help='Output machine-readable JSON result')
    parser.add_argument('--no-color', action='store_true', help='Disable ANSI color output')
    parser.add_argument('--no-configure', action='store_true', help='Skip CMake configure step')
    parser.add_argument('--no-build', action='store_true', help='Skip build step')
    parser.add_argument('--run-only', action='store_true', help='Skip configure and build, run tests only')

    args = parser.parse_args()
    if args.parallel <= 0:
        print("Error: --parallel must be > 0", file=sys.stderr)
        return EXIT_USAGE_ERROR

    runner = TestRunner()
    return runner.run(args)

if __name__ == '__main__':
    sys.exit(main())
