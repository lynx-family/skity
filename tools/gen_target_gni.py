#!/usr/bin/env python3

# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

import argparse
import os
import platform

class TargetGNI:
    target_name: str
    sources: list[str] = []
    include_dirs: list[str] = []
    compile_definitions: list[str] = []
    compile_options: list[str] = []
    link_libs: list[str] = []

    def __init__(self, target_name: str):
        # GN target name can not contain '-'
        target_name = target_name.replace('-', '_')
        self.target_name = target_name

    def set_sources(self, sources: list[str], strip_root: str):
        self.sources = [source.replace(strip_root, '') for source in sources]

    def set_include_dirs(self, include_dirs: list[str], strip_root: str):
        for include_dir in include_dirs:
            if include_dir == strip_root:
                self.include_dirs.append('.')
            elif os.path.isabs(include_dir):
                path = ""
                if os.path.commonpath([include_dir, strip_root]) == strip_root:
                    path = include_dir.replace(strip_root, '')
                else:
                    path = os.path.relpath(include_dir, strip_root)
                self.include_dirs.append(path.replace(strip_root, ''))
            else:
                self.include_dirs.append(include_dir.replace(strip_root, ''))
    
    def set_compile_definitions(self, compile_definitions: list[str]):
        self.compile_definitions = [cd.replace('\"', '\\"') for cd in compile_definitions]

    def set_compile_options(self, compile_options: list[str]):
        self.compile_options = compile_options

    def set_link_libs(self, link_libs: list[str]):
        self.link_libs = link_libs

def is_windows():
    return platform.system().lower() == 'windows'

def gen_target_gni(target_gni: TargetGNI, gn_file: str, gn_rebase_path: str):
    # if gn_file is exist, delete it
    if os.path.exists(gn_file):
        os.remove(gn_file)

    with open(gn_file, 'w') as f:

        f.write("_{}_root = get_path_info(\"{}\", \"abspath\")\n".format(target_gni.target_name, gn_rebase_path))

        f.write("{}_sources = [\n".format(target_gni.target_name))
        for source in target_gni.sources:
            if source.startswith('/'):
                source = source[1:]
            f.write(f'  "$_{target_gni.target_name}_root/{source}",\n')
        f.write("]\n")

        f.write("{}_include_dirs = [\n".format(target_gni.target_name))
        for include_dir in target_gni.include_dirs:
            if include_dir.startswith('/'):
                include_dir = include_dir[1:]
            f.write(f'  "$_{target_gni.target_name}_root/{include_dir}",\n')
        f.write("]\n")

        f.write("{}_compile_definitions = [\n".format(target_gni.target_name))
        for compile_definition in target_gni.compile_definitions:
            if compile_definition == "":
                continue
            f.write(f'  "{compile_definition}",\n')
        f.write("]\n")

        f.write("{}_compile_options = [\n".format(target_gni.target_name))
        for compile_option in target_gni.compile_options:
            f.write(f'  {compile_option},\n')
        f.write("]\n")

        f.write("{}_link_libs = [\n".format(target_gni.target_name))
        for link_lib in target_gni.link_libs:

            if link_lib.find('::@') != -1:
                continue

            if link_lib.startswith('\"'):
                link_lib = link_lib[1:-1]

            link_lib = link_lib.replace('-l', '')

            if is_windows():
                link_lib += ".lib"
            f.write(f'  "{link_lib}",\n')
        f.write("]\n")

    pass

if __name__ == '__main__':
    """
    python3 gen_target_gni.py --target_name <target_name> --gn_file <gn_file> --sources <sources> --include_dirs <include_dirs> --compile_definitions <compile_definitions> --compile_options <compile_options> --strip_root <strip_root>
    """

    parser = argparse.ArgumentParser(
        fromfile_prefix_chars='@',
        description='Generate GN target file from CMake target file',
    )
    parser.add_argument('--target_name', type=str, required=True)
    parser.add_argument('--gn_file', type=str, required=True)
    parser.add_argument('--sources', type=str, nargs='+', required=True)
    parser.add_argument('--include_dirs', type=str, nargs='*', required=False)
    parser.add_argument('--compile_definitions', nargs='*', default=[], required=False)
    parser.add_argument('--compile_options', nargs='*', default=[], required=False)
    parser.add_argument('--link_libs', nargs='*', default=[], required=False)
    parser.add_argument('--strip_root', type=str, required=True)
    parser.add_argument('--gn_rebase_path', type=str, default='.', required=False)

    args = parser.parse_args()

    target_gni = TargetGNI(args.target_name)

    target_gni.set_sources(args.sources, args.strip_root)
    target_gni.set_include_dirs(args.include_dirs, args.strip_root)
    target_gni.set_compile_definitions(args.compile_definitions)
    target_gni.set_compile_options(args.compile_options)
    target_gni.set_link_libs(args.link_libs)

    gen_target_gni(target_gni, args.gn_file, args.gn_rebase_path)

    pass
