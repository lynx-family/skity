---
name: test-runner
description: Build and run Skity unit tests with CMake and gtest filtering, including automatic failed-test extraction and machine-readable JSON output. Use when validating specific test suites or running minimal relevant regression tests after code changes.
---

# Test Runner Skill

A skill for building and running unit tests for the Skity project. Supports CMake configuration, building tests, and running specific test cases with gtest filtering. **Now includes automatic failure reporting!**

## Quick Commands

```bash
# Run all unit tests with automatic failure reporting
python3 tools/test-runner.py

# Run specific test cases with failure reporting
python3 tools/test-runner.py --filter "GeometryTest.*"

# Clean build and run tests with failure reporting
python3 tools/test-runner.py --clean

# Verbose output with failure details
python3 tools/test-runner.py --verbose

# Machine-readable output for agents
python3 tools/test-runner.py --json
```

## Script Features

- **🚨 Automatic failure detection**: Reports failed test cases with detailed information
- **📊 Detailed failure reports**: Shows failed test names, error details, and context
- **🎨 Colored output**: Clear visual feedback with red highlighting for failures
- **🔧 Flexible filtering**: Run specific test suites or individual tests
- **🔄 Cross-platform**: Works on Windows, macOS, and Linux
- **⚡ Parallel building**: Optimized build performance
- **🧹 Clean build option**: Force rebuild from scratch
- **🔍 Verbose mode**: Detailed test output for debugging

## Usage Examples

`$test-runner ...` is a conversation trigger syntax for agents, not a shell command.
For terminal usage, run `python3 tools/test-runner.py ...`.

### Basic Usage
```bash
# Run all tests with failure reporting
python3 tools/test-runner.py

# Run only geometry tests
python3 tools/test-runner.py --filter "GeometryTest.*"

# Run tests matching a pattern
python3 tools/test-runner.py --filter "*vector*"
```

### Advanced Usage
```bash
# Clean build and run tests
python3 tools/test-runner.py --clean

# Custom build directory
python3 tools/test-runner.py --build-dir my-build

# Verbose output for debugging
python3 tools/test-runner.py --verbose

# Custom parallel jobs
python3 tools/test-runner.py --parallel 8

# Reuse existing build artifacts (skip configure + build)
python3 tools/test-runner.py --run-only

# Skip only one phase
python3 tools/test-runner.py --no-configure
python3 tools/test-runner.py --no-build
```

## Agent Contract

### Exit codes
- `0`: tests passed
- `1`: test assertion failures
- `2`: infrastructure/tooling error (configure/build/runtime failure)
- `3`: usage error (for example invalid arguments)

### JSON output (`--json`)
The script prints one JSON object with these keys:
- `mode`: `"test_runner"`
- `status`: `"ok"`, `"test_failed"`, or `"infra_error"`
- `exit_code`: normalized script exit code
- `build_dir`, `filter`, `skip_configure`, `skip_build`
- `failed_tests`: list of `{name, details}`
- `passed_test_markers`, `total_test_markers`, `test_process_exit_code`
- `duration_ms`

## Failure Reporting

When tests fail, the script will:

1. **List all failed test cases** with clear red highlighting
2. **Show failure details** including expected vs actual values
3. **Provide error context** from the test output
4. **Return non-zero exit code** for CI/CD integration

Example failure output:
```
❌ Failed test cases detected:
  - GeometryTest.VectorAddition
    Details: Expected: 42, Actual: 24
  - MathTest.MatrixMultiplication
    Details: Matrix dimensions don't match
```

## Test Categories

Based on the test structure, you can filter by:

- **Geometry tests**: `GeometryTest.*`
- **Math tests**: `MathTest.*`
- **Graphics tests**: `GraphicTest.*` or specific like `PathTest.*`
- **Color tests**: `ColorTest.*`
- **Canvas tests**: `CanvasStateTest.*`
- **Text tests**: `TextTest.*`, `TextRunTest.*`
- **Memory tests**: `ArenaAllocatorTest.*`, `ArrayListTest.*`
- **Hardware tests**: `HWBufferLayoutTest.*`, `HWPipelineKeyTest.*`

## Script Location

- **Main script**: `tools/test-runner.py`
- **Language**: Python 3.6+
- **Platform**: Windows, macOS, Linux

## Manual Commands (Legacy)

If you prefer manual control, you can still use the original commands:

```bash
# Configure with CMake
cmake -B build -DSKITY_TEST=ON

# Build the project
cmake --build build --parallel $(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Run all tests
./build/test/skity_unit_test

# Run specific tests
GTEST_FILTER="GeometryTest.*" ./build/test/skity_unit_test
```

## Requirements

- Python 3.6+
- CMake 3.16+
- C++ compiler with C++17 support
- Google Test (automatically handled by project dependencies)

## Tips

- Use `$(nproc)` on Linux or `sysctl -n hw.ncpu` on macOS for optimal parallel building
- The test executable is always located at `{build-dir}/test/skity_unit_test`
- Use `GTEST_FILTER` environment variable to run specific test cases
- Add `--gtest_list_tests` to see all available tests: `./build/test/skity_unit_test --gtest_list_tests`
- The script provides better failure reporting than raw gtest output
