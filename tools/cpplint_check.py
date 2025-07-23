#!/usr/bin/env python3

import subprocess, sys, os
from merge_request import MergeRequest

import cpplint

ignore_lists = ["shaders", "example", "test", "benchmarking", "src/graphic/pathop/clipper2"]


def cpplintCheck(target_sha):
    mr = MergeRequest(target_sha)
    files = mr.GetChangedFiles()

    failed_file = []

    for file_name in files:
        if file_name.endswith('.cc') or file_name.endswith('.hpp'):
            ignored = False
            for prefix in ignore_lists:
                if file_name.startswith(prefix):
                    ignored = True
                    break
            if ignored:
                continue
            cpplint._cpplint_state.ResetErrorCounts()
            cpplint.ProcessFile(file_name, 0)
            if (cpplint._cpplint_state.error_count > 0):
                failed_file.append(file_name)

    if len(failed_file) > 0:
        print("===============cpplint FAILED===============")
        sys.exit(1)
    else:
        print("===============cpplint PASSED===============")
        sys.exit(0)


if __name__ == '__main__':
    cpplintCheck(sys.argv[1])
    pass
