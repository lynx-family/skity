#!/usr/bin/env python3

import subprocess
import sys
import os
from merge_request import MergeRequest


def runCommand(cmd):
    p = subprocess.Popen(
        cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    return p.stdout.readlines()


def checkFormat(path):
    if path.startswith("skity"):
        path = os.path.relpath(path, "skity")

    cmd = '{} {} | diff {} - | wc -l'.format(
        'buildtools/llvm/bin/clang-format', path, path)
    output = runCommand(cmd)
    lines = int(output[0].strip())
    return not lines


if __name__ == '__main__':
    mr = MergeRequest(sys.argv[1])
    files = mr.GetChangedFiles()

    failed_path = []

    for file_name in files:
        if file_name.endswith('.cc') or file_name.endswith('.mm') or file_name.endswith('.hpp') or file_name.endswith('.h'):
            if file_name.count("third_party") > 0:
                continue
            if not checkFormat(file_name):
                failed_path.append(file_name)

    if len(failed_path) > 0:
        print("file format check failed")
        print(">>>>>>>>>file format check failed>>>>>>>>>>")
        for file in failed_path:
            print(file)
        print(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>")
        sys.exit(1)
    else:
        sys.exit(0)
    pass
