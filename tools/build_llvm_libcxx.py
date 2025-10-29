#!/usr/bin/env python3

import os, subprocess
import platform
import sys

def build_libcxx(source_dir: str, build_dir: str, target_dir: str, llvm_cxx_namespace: str):
    config_cmd = [
        'cmake',
        '-S', os.path.join(source_dir, 'runtimes'),
        '-B', build_dir,
        '-DLLVM_ENABLE_RUNTIMES=libcxx',
        '-DLIBCXX_ABI_NAMESPACE={}'.format(llvm_cxx_namespace),
        '-DLIBCXX_ENABLE_RTTI=OFF',
        '-DLIBCXX_ENABLE_EXCEPTIONS=OFF',
        '-DLIBCXX_ENABLE_SHARED=OFF',
        '-DLIBCXX_ENABLE_STATIC=ON',
        '-DCMAKE_BUILD_TYPE=Release',
        '-DCMAKE_INSTALL_PREFIX={}'.format(target_dir),
    ]

    system = platform.system().lower()

    # check if running on windows
    if system == 'windows':
        config_cmd.append('-T')
        config_cmd.append('ClangCL')
    elif system == 'linux':
        config_cmd.append('-DCMAKE_C_COMPILER=clang')
        config_cmd.append('-DCMAKE_CXX_COMPILER=clang++')

    print('Configuring libcxx...')
    print(config_cmd)

    subprocess.run(config_cmd, check=True, text=True)


    build_cmd = [
        'cmake',
        '--build', build_dir,
        '--config', 'Release',
    ]

    subprocess.run(build_cmd, check=True)

    install_cmd = [
        'cmake',
        '--install', build_dir,
        '--config', 'Release',
    ]

    subprocess.run(install_cmd, check=True)

    pass


if __name__ == '__main__':
    """Build libcxx
    usage:
        python3 build_libcxx.py <source_dir> <build_dir> <target_dir> <llvm_cxx_namespace>
    """

    if len(sys.argv) != 5:
        print('Usage: python3 build_libcxx.py <source_dir> <build_dir> <target_dir> <llvm_cxx_namespace>')
        sys.exit(1)

    source_dir = sys.argv[1]
    build_dir = sys.argv[2]
    target_dir = sys.argv[3]
    llvm_cxx_namespace = sys.argv[4]

    build_libcxx(source_dir, build_dir, target_dir, llvm_cxx_namespace)

