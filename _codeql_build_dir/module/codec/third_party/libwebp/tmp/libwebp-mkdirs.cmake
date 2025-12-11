# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/runner/work/skity/skity/third_party/libwebp")
  file(MAKE_DIRECTORY "/home/runner/work/skity/skity/third_party/libwebp")
endif()
file(MAKE_DIRECTORY
  "/home/runner/work/skity/skity/_codeql_build_dir/module/codec/third_party/libwebp/src/libwebp-build"
  "/home/runner/work/skity/skity/_codeql_build_dir/module/codec/third_party/libwebp"
  "/home/runner/work/skity/skity/_codeql_build_dir/module/codec/third_party/libwebp/tmp"
  "/home/runner/work/skity/skity/_codeql_build_dir/module/codec/third_party/libwebp/src/libwebp-stamp"
  "/home/runner/work/skity/skity/_codeql_build_dir/module/codec/third_party/libwebp/src"
  "/home/runner/work/skity/skity/_codeql_build_dir/module/codec/third_party/libwebp/src/libwebp-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/runner/work/skity/skity/_codeql_build_dir/module/codec/third_party/libwebp/src/libwebp-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/runner/work/skity/skity/_codeql_build_dir/module/codec/third_party/libwebp/src/libwebp-stamp${cfgdir}") # cfgdir has leading slash
endif()
