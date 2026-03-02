---
name: code-style-checker
description: Check and fix coding style for C++/Obj-C++ and CMake files using the unified code_format_check.py script. Use when you need to verify code formatting before git commit or want to automatically format changed files according to project style guidelines.
---

# Code Style Checker

This skill provides direct access to the unified `tools/code_format_check.py` script for checking and fixing coding style in C++/Obj-C++ and CMake files. It uses the project's `.clang-format` configuration and excludes third_party directories by default.

## About the Unified Script

The underlying `tools/code_format_check.py` script serves both GitHub CI and local development needs:
- **Single codebase**: No duplication between CI and local tools
- **Consistent behavior**: Same filtering and formatting logic everywhere
- **Dual mode operation**: Automatically adapts based on usage context

## Usage

`$code-style-checker ...` is a conversation trigger syntax for agents, not a shell command.
For terminal usage, run `python3 tools/code_format_check.py ...`.

### Check staged files only
```bash
python3 tools/code_format_check.py --staged-only
```

### Check all changed files (staged + unstaged)
```bash
python3 tools/code_format_check.py
```

### Check and automatically fix formatting issues
```bash
python3 tools/code_format_check.py --fix
```

### Include third_party directories (not recommended)
```bash
python3 tools/code_format_check.py --include-third-party
```

### Emit machine-readable output for agents
```bash
python3 tools/code_format_check.py --json
```

## Agent Contract

### Exit codes
- `0`: check passed
- `1`: check failed (formatting issues remain)
- `2`: infrastructure/tooling error (for example git/clang-format execution failed)
- `3`: usage error

### JSON output (`--json`)
The script prints one JSON object with these keys:
- `mode`: `"skill"` or `"ci"`
- `status`: `"ok"`, `"check_failed"`, or `"infra_error"`
- `checked_file_count`: integer
- `failed_files`: list of file paths still failing after optional `--fix`
- `infra_errors`: list of `{file, error}` entries when tool execution fails
- `checks`: per-file results with `file`, `formatted`, `message`, `fixed`, `fix_error`
- `duration_ms`: total runtime in milliseconds

## What it checks

The script checks these file types:
- C++ files: `.cpp`, `.cc`, `.cxx`, `.c++`
- C++ headers: `.h`, `.hpp`, `.hxx`, `.h++`
- Objective-C/C++ files: `.m`, `.mm`
- CMake files: `.cmake`, `CMakeLists.txt`

## What it ignores

By default, the script excludes files in these directories:
- `third_party`
- `third-party`
- `3rdparty`
- `external`
- `vendor`
- `.git`

## Requirements

- `clang-format` must be installed and available in PATH, or specify custom path with `--clang-format-path`
- Git repository with `.clang-format` configuration file
- The unified script `tools/code_format_check.py` must exist in your repository

## Integration with CI/CD

The same underlying script is used by your CI/CD workflow:
```bash
tools/code_format_check.py main --verbose
```

This ensures consistent behavior between local development and CI checks.
