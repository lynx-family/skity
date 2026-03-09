#!/usr/bin/env python3
"""
Unified coding style checker for C++/Obj-C++ files.
Works for both GitHub CI and local development.

Features:
- Checks C++/Obj-C++ files using clang-format
- Excludes third_party directories by default
- Supports automatic fixing
- Works with git changed files or merge request files
- Compatible with existing CI workflows
"""

import subprocess
import sys
import os
import argparse
import json
import time
from pathlib import Path

EXIT_SUCCESS = 0
EXIT_CHECK_FAILED = 1
EXIT_INFRA_ERROR = 2
EXIT_USAGE_ERROR = 3

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_CLANG_FORMAT_STYLE_FILE = REPO_ROOT / '.clang-format'


def log_debug(message, verbose=False):
    """Log debug message if verbose mode or CI_DEBUG environment variable is set."""
    if verbose or os.environ.get('CI_DEBUG'):
        print(f"[DEBUG] {message}", file=sys.stderr)

def is_code_file(filepath):
    """Check if file is a C++/Obj-C++ file."""
    code_extensions = {
        '.cpp', '.cc', '.cxx', '.c++', '.h', '.hpp', '.hxx', '.h++',
        '.m', '.mm',  # Objective-C/C++
    }

    path = Path(filepath)
    return path.suffix.lower() in code_extensions


def should_ignore_file(filepath):
    """Check if file should be ignored (e.g., third_party directories)."""
    ignore_patterns = [
        'third_party',
        'third-party', 
        '3rdparty',
        'external',
        'vendor',
        '.git'
    ]

    path_parts = Path(filepath).parts

    for pattern in ignore_patterns:
        if pattern in path_parts:
            return True

    return False


def check_clang_format_available(clang_format_path='clang-format'):
    """Check if clang-format is available."""
    try:
        result = subprocess.run([clang_format_path, '--version'], capture_output=True, text=True, check=True)
        if result.stdout:
            return True
        return False
    except (subprocess.CalledProcessError, FileNotFoundError):
        return False


def build_style_arg(style_file_path):
    """Build clang-format style argument from a style file path."""
    style_path = Path(style_file_path).resolve()
    return f'-style=file:{style_path}'


def check_file_formatting_with_content(filepath, clang_format_path='clang-format',
                                       style_file_path=DEFAULT_CLANG_FORMAT_STYLE_FILE):
    """Check if file follows clang-format style by comparing content."""
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            original_content = f.read()

        if not original_content.strip():
            return True, "File is empty or only whitespace"

        # Use file style resolution based on the target file path so .clang-format
        # is applied consistently, even when content is piped via stdin.
        result = subprocess.run(
            [
                clang_format_path,
                build_style_arg(style_file_path),
                f'-assume-filename={os.path.abspath(filepath)}',
            ],
            input=original_content,
            capture_output=True,
            text=True,
            check=True,
        )

        formatted_content = result.stdout

        if original_content != formatted_content:
            return False, "File needs formatting"
        return True, "File is properly formatted"

    except subprocess.CalledProcessError as e:
        return False, f"Error formatting file: {e}"
    except Exception as e:
        return False, f"Error reading file: {e}"


def fix_file_formatting(filepath, clang_format_path='clang-format',
                        style_file_path=DEFAULT_CLANG_FORMAT_STYLE_FILE):
    """Fix file formatting using clang-format."""
    try:
        if not os.path.exists(filepath):
            return False, f"File does not exist: {filepath}"

        if not os.access(filepath, os.W_OK):
            return False, f"File is not writable: {filepath}"

        subprocess.run(
            [clang_format_path, build_style_arg(style_file_path), '-i', filepath],
            check=True,
            capture_output=True,
            text=True,
        )
        return True, f"Fixed {filepath}"
    except subprocess.CalledProcessError as e:
        return False, f"Error fixing {filepath}: {e}"
    except Exception as e:
        return False, f"Unexpected error fixing {filepath}: {e}"


def filter_code_files(file_list, include_third_party=False):
    """Filter a list of files to only include code files, excluding third_party."""
    filtered_files = []
    
    for filepath in file_list:
        if not filepath:
            continue

        if not is_code_file(filepath):
            continue

        if not include_third_party and should_ignore_file(filepath):
            continue

        if not os.path.exists(filepath):
            if os.environ.get('CI_DEBUG'):
                print(f"Debug: File does not exist: {filepath}", file=sys.stderr)
            continue

        filtered_files.append(filepath)

    return filtered_files


def dedupe_keep_order(items):
    """Dedupe while preserving order."""
    return list(dict.fromkeys(items))


def parse_null_terminated_paths(stdout_bytes):
    """Parse git -z output."""
    if not stdout_bytes:
        return []
    return [
        entry.decode('utf-8', errors='surrogateescape')
        for entry in stdout_bytes.split(b'\0')
        if entry
    ]


def run_git_name_only(args):
    """Run a git command that returns NUL-separated paths."""
    try:
        result = subprocess.run(
            ['git'] + args + ['-z'],
            capture_output=True,
            text=False,
            check=True,
        )
        return parse_null_terminated_paths(result.stdout)
    except subprocess.CalledProcessError as e:
        stderr = e.stderr.decode('utf-8', errors='ignore') if e.stderr else str(e)
        print(f"Error running git {' '.join(args)}: {stderr}", file=sys.stderr)
        return None


def git_ref_exists(ref):
    """Check whether a git ref/commit is available locally."""
    try:
        subprocess.run(
            ['git', 'rev-parse', '--verify', f'{ref}^{{commit}}'],
            capture_output=True,
            text=True,
            check=True,
        )
        return True
    except subprocess.CalledProcessError:
        return False


def run_git_command(args, verbose=False):
    """Run git command and return success flag with stderr message."""
    try:
        result = subprocess.run(
            ['git'] + args,
            capture_output=True,
            text=True,
            check=True,
        )
        log_debug(f"git {' '.join(args)}: {result.stdout.strip()}", verbose)
        return True, ''
    except subprocess.CalledProcessError as e:
        stderr = e.stderr.strip() if e.stderr else str(e)
        log_debug(f"git {' '.join(args)} failed: {stderr}", verbose)
        return False, stderr


def ensure_target_ref_available(target_ref, verbose=False):
    """Ensure target ref exists locally; try fetching in CI if it is missing."""
    if git_ref_exists(target_ref):
        return True, ''

    attempts = []
    base_ref = os.environ.get('GITHUB_BASE_REF', '').strip()

    # Try to fetch the exact ref first (works for branch/tag and some commit SHAs).
    attempts.append(['fetch', '--no-tags', 'origin', target_ref])

    # In GitHub Actions PR workflows, base branch is usually available via env var.
    if base_ref:
        attempts.append(['fetch', '--no-tags', '--depth=200', 'origin', base_ref])
        attempts.append(['fetch', '--no-tags', 'origin', base_ref])

    fetch_errors = []
    for cmd in attempts:
        ok, err = run_git_command(cmd, verbose=verbose)
        if not ok and err:
            fetch_errors.append(f"git {' '.join(cmd)}: {err}")
        if git_ref_exists(target_ref):
            return True, ''

    error_detail = (
        f"Cannot resolve target ref '{target_ref}'. "
        "This is often caused by shallow checkout in CI. "
        "Use a full fetch depth or fetch the base branch/commit before running this script."
    )
    if fetch_errors:
        error_detail += f" Fetch attempts failed: {' | '.join(fetch_errors)}"
    return False, error_detail


def get_changed_files_from_git():
    """Get list of staged changed files from git status."""
    return run_git_name_only(['diff', '--cached', '--name-only', '--diff-filter=ACMRT'])


def get_modified_files_from_git():
    """Get list of modified but unstaged files."""
    return run_git_name_only(['diff', '--name-only', '--diff-filter=ACMRT'])


def get_merge_request_files(target_branch):
    """Get changed files for a merge request against target branch."""
    try:
        target_ok, target_err = ensure_target_ref_available(target_branch)
        if not target_ok:
            print(f"Error getting merge request files: {target_err}", file=sys.stderr)
            return None

        # Get files changed against target branch
        mr_files = run_git_name_only(
            ['diff', f'{target_branch}..HEAD', '--name-only', '--diff-filter=ACMRT']
        )
        if mr_files is None:
            return None

        # Get unstaged files
        unstaged_files = get_modified_files_from_git()
        if unstaged_files is None:
            return None

        # Combine and deduplicate
        all_files = dedupe_keep_order(mr_files + unstaged_files)

        # Convert to absolute paths
        git_root = subprocess.run(
            ['git', 'rev-parse', '--show-toplevel'],
            capture_output=True,
            text=True,
            check=True,
        ).stdout.strip()

        absolute_files = []
        for file in all_files:
            if file and file != '':
                if not os.path.isabs(file):
                    file = os.path.join(git_root, file)
                absolute_files.append(file)

        return absolute_files

    except subprocess.CalledProcessError as e:
        print(f"Error getting merge request files: {e}", file=sys.stderr)
        return None


def maybe_print_json(args, payload):
    if args.json:
        print(json.dumps(payload, ensure_ascii=True, sort_keys=True))


def run_ci_mode(args):
    """Run in CI mode (merge request checking)."""
    started = time.time()
    # Get files for merge request
    files = get_merge_request_files(args.target_branch)
    if files is None:
        payload = {
            'mode': 'ci',
            'status': 'infra_error',
            'message': 'Failed to query changed files from git.',
            'duration_ms': int((time.time() - started) * 1000),
        }
        maybe_print_json(args, payload)
        return EXIT_INFRA_ERROR

    if args.verbose and not args.json:
        print(f"Checking {len(files)} changed files against {args.target_branch}")

    # Filter for code files and exclude third_party
    code_files = filter_code_files(files, include_third_party=False)

    if args.verbose and not args.json:
        print(f"Found {len(code_files)} code files to check")

    failed_files = []
    infra_errors = []
    checks = []

    for filepath in code_files:
        # Check formatting
        is_formatted, message = check_file_formatting_with_content(
            filepath, args.clang_format_path, args.clang_format_style_file
        )
        check_item = {
            'file': filepath,
            'formatted': is_formatted,
            'message': message,
            'fixed': False,
            'fix_error': '',
        }

        if not is_formatted:
            failed_files.append(filepath)
            if args.verbose and not args.json:
                print(f"❌ {filepath}: {message}")
        else:
            if args.verbose and not args.json:
                print(f"✅ {filepath}")

        # Fix formatting if requested
        if args.fix and not is_formatted:
            success, fix_message = fix_file_formatting(
                filepath, args.clang_format_path, args.clang_format_style_file
            )
            check_item['fixed'] = success
            if not success:
                check_item['fix_error'] = fix_message
                infra_errors.append({'file': filepath, 'error': fix_message})
            if args.verbose and not args.json:
                if success:
                    print(f"  Fixed: {fix_message}")
                else:
                    print(f"  Error: {fix_message}")
            if success:
                is_formatted_after_fix, message_after_fix = check_file_formatting_with_content(
                    filepath, args.clang_format_path, args.clang_format_style_file
                )
                check_item['formatted'] = is_formatted_after_fix
                check_item['message'] = message_after_fix
                if is_formatted_after_fix and filepath in failed_files:
                    failed_files.remove(filepath)
                elif not is_formatted_after_fix and filepath not in failed_files:
                    failed_files.append(filepath)

        checks.append(check_item)

    failed_files = sorted(dedupe_keep_order(failed_files))
    duration_ms = int((time.time() - started) * 1000)

    if args.json:
        status = 'ok'
        exit_code = EXIT_SUCCESS
        if infra_errors:
            status = 'infra_error'
            exit_code = EXIT_INFRA_ERROR
        elif failed_files:
            status = 'check_failed'
            exit_code = EXIT_CHECK_FAILED
        maybe_print_json(
            args,
            {
                'mode': 'ci',
                'status': status,
                'target_branch': args.target_branch,
                'checked_file_count': len(code_files),
                'failed_files': failed_files,
                'infra_errors': infra_errors,
                'checks': checks,
                'duration_ms': duration_ms,
            },
        )
        return exit_code

    if infra_errors:
        print("Formatting infra error:")
        for item in infra_errors:
            print(f"{item['file']}: {item['error']}")
        return EXIT_INFRA_ERROR
    if failed_files:
        print("file format check failed")
        print(">>>>>>>>>file format check failed>>>>>>>>>>")
        for file in failed_files:
            print(file)
        print(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>")
        return EXIT_CHECK_FAILED
    else:
        if args.verbose:
            print("All files are properly formatted!")
        return EXIT_SUCCESS


def run_skill_mode(args):
    """Run in skill mode (git status checking)."""
    started = time.time()
    # Check if clang-format is available
    if not check_clang_format_available(args.clang_format_path):
        msg = f"{args.clang_format_path} is not available. Please install clang-format."
        if not args.json:
            print(f"Error: {msg}")
        maybe_print_json(
            args,
            {
                'mode': 'skill',
                'status': 'infra_error',
                'message': msg,
                'duration_ms': int((time.time() - started) * 1000),
            },
        )
        return EXIT_INFRA_ERROR

    # Get files to check
    if args.staged_only:
        files_to_check = get_changed_files_from_git()
    else:
        # Get both staged and unstaged files
        staged_files = get_changed_files_from_git()
        modified_files = get_modified_files_from_git()
        if staged_files is None or modified_files is None:
            files_to_check = None
        else:
            files_to_check = dedupe_keep_order(staged_files + modified_files)

    if files_to_check is None:
        maybe_print_json(
            args,
            {
                'mode': 'skill',
                'status': 'infra_error',
                'message': 'Failed to read changed files from git.',
                'duration_ms': int((time.time() - started) * 1000),
            },
        )
        if not args.json:
            print("Error: Failed to read changed files from git.", file=sys.stderr)
        return EXIT_INFRA_ERROR

    # Filter for code files using shared utilities
    code_files = filter_code_files(files_to_check, include_third_party=args.include_third_party)

    if not code_files:
        if not args.json:
            print("No C++/Obj-C++ files to check.")
        maybe_print_json(
            args,
            {
                'mode': 'skill',
                'status': 'ok',
                'checked_file_count': 0,
                'failed_files': [],
                'infra_errors': [],
                'checks': [],
                'duration_ms': int((time.time() - started) * 1000),
            },
        )
        return EXIT_SUCCESS

    if not args.json:
        print(f"Checking {len(code_files)} file(s)...")

    issues_found = []
    infra_errors = []
    checks = []

    for filepath in code_files:
        if not args.json:
            print(f"Checking {filepath}...", end=' ')

        is_formatted, message = check_file_formatting_with_content(
            filepath, args.clang_format_path, args.clang_format_style_file
        )
        check_item = {
            'file': filepath,
            'formatted': is_formatted,
            'message': message,
            'fixed': False,
            'fix_error': '',
        }

        if is_formatted:
            if not args.json:
                print("✓")
        else:
            if not args.json:
                print("✗")
            issues_found.append((filepath, message))

            if args.fix:
                if not args.json:
                    print(f"  Fixing {filepath}...")
                success, fix_message = fix_file_formatting(
                    filepath, args.clang_format_path, args.clang_format_style_file
                )
                check_item['fixed'] = success
                if success:
                    if not args.json:
                        print(f"  {fix_message}")
                    is_formatted_after_fix, message_after_fix = check_file_formatting_with_content(
                        filepath, args.clang_format_path, args.clang_format_style_file
                    )
                    check_item['formatted'] = is_formatted_after_fix
                    check_item['message'] = message_after_fix
                    if is_formatted_after_fix:
                        issues_found = [item for item in issues_found if item[0] != filepath]
                else:
                    check_item['fix_error'] = fix_message
                    infra_errors.append({'file': filepath, 'error': fix_message})
                    if not args.json:
                        print(f"  Error: {fix_message}")
        checks.append(check_item)

    failed_files = sorted(dedupe_keep_order([filepath for filepath, _ in issues_found]))
    duration_ms = int((time.time() - started) * 1000)

    if args.json:
        status = 'ok'
        exit_code = EXIT_SUCCESS
        if infra_errors:
            status = 'infra_error'
            exit_code = EXIT_INFRA_ERROR
        elif failed_files:
            status = 'check_failed'
            exit_code = EXIT_CHECK_FAILED
        maybe_print_json(
            args,
            {
                'mode': 'skill',
                'status': status,
                'checked_file_count': len(code_files),
                'failed_files': failed_files,
                'infra_errors': infra_errors,
                'checks': checks,
                'duration_ms': duration_ms,
            },
        )
        return exit_code

    if infra_errors:
        print("\nEncountered formatting tool errors:")
        for item in infra_errors:
            print(f"  - {item['file']}: {item['error']}")
        return EXIT_INFRA_ERROR
    if failed_files:
        print(f"\n{len(issues_found)} file(s) need formatting:")
        for filepath, message in issues_found:
            print(f"  - {filepath}: {message}")

        if not args.fix:
            print("\nRun with --fix to automatically format these files.")
            return EXIT_CHECK_FAILED
        return EXIT_CHECK_FAILED
    else:
        print("All files are properly formatted!")

    return EXIT_SUCCESS


def setup_arguments():
    """Setup command line arguments."""
    parser = argparse.ArgumentParser(
        description='Unified coding style checker for C++/Obj-C++ files',
        epilog='Examples:\n'
               '  # CI usage (merge request checking)\n'
               '  %(prog)s main --verbose\n'
               '  %(prog)s main --fix\n\n'
               '  # Skill usage (git status checking)\n'
               '  %(prog)s --staged-only\n'
               '  %(prog)s --fix\n'
               '  %(prog)s --include-third-party\n',
        formatter_class=argparse.RawDescriptionHelpFormatter
    )

    # Mode detection arguments
    parser.add_argument('target_branch', nargs='?', help='Target branch, commit hash, or ref for CI mode (e.g., main, HEAD~1, abc123)')

    # Common arguments
    parser.add_argument('--fix', action='store_true', 
                       help='Automatically fix formatting issues')
    parser.add_argument('--verbose', action='store_true', 
                       help='Show detailed output')
    parser.add_argument('--clang-format-path', default='clang-format',
                       help='Path to clang-format binary (default: clang-format)')
    parser.add_argument(
        '--clang-format-style-file',
        default=str(DEFAULT_CLANG_FORMAT_STYLE_FILE),
        help='Path to .clang-format style file (default: <repo>/.clang-format)',
    )
    parser.add_argument('--json', action='store_true',
                       help='Output machine-readable JSON result')
    
    # Skill-specific arguments
    parser.add_argument('--staged-only', action='store_true', 
                       help='Only check staged files (skill mode)')
    parser.add_argument('--include-third-party', action='store_true',
                       help='Include third_party directories (skill mode)')

    return parser


def main():
    """Main entry point."""
    parser = setup_arguments()
    args = parser.parse_args()
    if args.target_branch == '':
        return EXIT_USAGE_ERROR

    if not os.path.exists(args.clang_format_style_file):
        print(
            f"Error: clang-format style file not found: {args.clang_format_style_file}",
            file=sys.stderr,
        )
        return EXIT_USAGE_ERROR

    # Handle target_branch in both formats: 'main' and 'target_branch=main'
    if args.target_branch:
        # Strip target_branch= prefix if present
        if args.target_branch.startswith('target_branch='):
            args.target_branch = args.target_branch.replace('target_branch=', '', 1)
        # CI mode - target branch provided
        return run_ci_mode(args)
    else:
        # Skill mode - no target branch, use git status
        return run_skill_mode(args)


if __name__ == '__main__':
    sys.exit(main())
