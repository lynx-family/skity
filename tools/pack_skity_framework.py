#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# Copyright 2020 The Lynx Authors. All rights reserved.

# pack skity.xcframework
# this script will build iOS( iphoneos and iphonesimulator ) and macos( x86_64 and arm64 ) frameworks
# and then pack them into xcframework
# Note:
# 1. we build static library since we do not sign this library in our repository
# 2. iphonesimulator arm64 and x86_64 should use lipo to merge into one framework

# cmake -B out/ios_framework_iphonesimulator_x64 -G Xcode -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_INSTALL_PREFIX=out/ios_install_simulator_x64 -DSKITY_CT_FONT=ON -DSKITY_GENERATE_FRAMEWORK=ON -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator

import os
import subprocess
import sys

def cmake_build_and_install(out_dir: str, install_dir: str, target_version: str, arch: str, sys_root: str, version_suffix: str):
    cmake_config_command = [
        'cmake',
        '-B',
        out_dir,
        '-G',
        'Xcode',
        '-DCMAKE_OSX_ARCHITECTURES={}'.format(arch),
        '-DCMAKE_INSTALL_PREFIX={}'.format(install_dir),
        '-DSKITY_CT_FONT=ON',
        '-DSKITY_GENERATE_FRAMEWORK=ON',
        '-DCMAKE_OSX_DEPLOYMENT_TARGET={}'.format(target_version),
        '-DCMAKE_OSX_SYSROOT={}'.format(sys_root),
    ]

    # if version_suffix is not none and not empty
    if version_suffix != '':
        cmake_config_command.append('-DSKITY_BUNDLE_SUFFIX={}'.format(version_suffix))

    print('version_suffix: {}'.format(version_suffix))
    print('-----------------')
    print('cmake_config_command: {}'.format(cmake_config_command))
    print('-----------------')

    # check config result
    subprocess.check_call(cmake_config_command)

    # build framework
    cmake_build_command = [
        'cmake',
        '--build',
        out_dir,
        '--target',
        'skity_framework',
        '--config',
        'Release',
    ]

    # check build result
    subprocess.check_call(cmake_build_command)

    # install framework
    cmake_install_command = [
        'cmake',
        '--install',
        out_dir,
        '--component',
        'Runtime',
        '--config',
        'Release',
    ]

    # check install result
    subprocess.check_call(cmake_install_command)

    return 1

def pack_all_frameworks(version_suffix: str):
    # pack macos framework
    cmake_build_and_install('out/osx_framework', 'out/osx_install', '15.0', 'x86_64;arm64', 'macosx', version_suffix)
    # pack iOS framework
    cmake_build_and_install('out/ios_framework', 'out/ios_install', '11.0', 'arm64', 'iphoneos', version_suffix)
    # pack iOS simulator x86_64 framework
    cmake_build_and_install('out/ios_simulator_framework_x86_64', 'out/ios_simulator_install_x86_64', '11.0', 'x86_64', 'iphonesimulator', version_suffix)
    # pack iOS simulator arm64 framework
    cmake_build_and_install('out/ios_simulator_framework_arm64', 'out/ios_simulator_install_arm64', '11.0', 'arm64', 'iphonesimulator', version_suffix)
    pass

def pack_zip_bundle():
    # delete out/skity.framework.zip if exists
    if os.path.exists('out/skity.framework.zip'):
        subprocess.check_call(['rm', '-rf', 'out/skity.framework.zip'])
    # delete out/framework_pack if exists
    if os.path.exists('out/framework_pack'):
        subprocess.check_call(['rm', '-rf', 'out/framework_pack'])

    # create out/framework_pack
    os.makedirs('out/framework_pack')

    # copy skity.xcframework to out/framework_pack
    subprocess.check_call(['cp', '-R', 'out/skity.xcframework', 'out/framework_pack'])
    # we also copy LICENSE to out/framework_pack
    subprocess.check_call(['cp', 'LICENSE', 'out/framework_pack'])

    # pack out/framework_pack to out/skity.framework.zip
    subprocess.check_call(['zip', '-r', '../skity.framework.zip', 'LICENSE', 'skity.xcframework'], cwd='out/framework_pack')

    # delete out/framework_pack
    subprocess.check_call(['rm', '-rf', 'out/framework_pack'])
    pass


def main(version_suffix: str):
    pack_all_frameworks(version_suffix)

    # before genreate the final xcframework, we needs to merge
    # ios_simulator_framework_x86_64 and ios_simulator_framework_arm64
    # into one framework

    # first we copy ios_simulator_framework_x86_64/skity.framework to ios_simulator_install

    #check if out/ios_simulator_install, if exists, delete it
    if os.path.exists('out/ios_simulator_install'):
        subprocess.check_call(['rm', '-rf', 'out/ios_simulator_install'])
    os.makedirs('out/ios_simulator_install')
    # copy ios_simulator_framework_x86_64/skity.framework to out/ios_simulator_install
    subprocess.check_call(['cp', '-R', 'out/ios_simulator_install_x86_64/skity.framework', 'out/ios_simulator_install'])

    # delete out/ios_framework_install/skity.framework/skity first
    subprocess.check_call(['rm', '-rf', 'out/ios_simulator_install/skity.framework/skity'])

    # merge arm64 and x86_64 into out/ios_framework_install/skity.framework/skity
    subprocess.check_call(['lipo',
                           '-create',
                           'out/ios_simulator_install_x86_64/skity.framework/skity',
                           'out/ios_simulator_install_arm64/skity.framework/skity',
                           '-output',
                           'out/ios_simulator_install/skity.framework/skity',
                        ])

    # if out/skity.xcframework exists, delete it
    if os.path.exists('out/skity.xcframework'):
        subprocess.check_call(['rm', '-rf', 'out/skity.xcframework'])

    # now we can create the final xcframework
    subprocess.check_call(['xcodebuild',
                           '-create-xcframework',
                           '-framework', 'out/ios_install/skity.framework',
                           '-framework', 'out/osx_install/skity.framework',
                           '-framework', 'out/ios_simulator_install/skity.framework',
                           '-output', 'out/skity.xcframework'])

    pack_zip_bundle()


if __name__ == '__main__':
    version_suffix = ''

    if len(sys.argv) > 1:
        version_suffix = sys.argv[1]

    main(version_suffix)
