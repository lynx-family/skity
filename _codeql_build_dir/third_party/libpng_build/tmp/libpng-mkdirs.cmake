# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/runner/work/skity/skity/third_party/libpng")
  file(MAKE_DIRECTORY "/home/runner/work/skity/skity/third_party/libpng")
endif()
file(MAKE_DIRECTORY
  "/home/runner/work/skity/skity/_codeql_build_dir/third_party/libpng_build/src/libpng-build"
  "/home/runner/work/skity/skity/_codeql_build_dir/third_party/libpng_build"
  "/home/runner/work/skity/skity/_codeql_build_dir/third_party/libpng_build/tmp"
  "/home/runner/work/skity/skity/_codeql_build_dir/third_party/libpng_build/src/libpng-stamp"
  "/home/runner/work/skity/skity/_codeql_build_dir/third_party/libpng_build/src"
  "/home/runner/work/skity/skity/_codeql_build_dir/third_party/libpng_build/src/libpng-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/runner/work/skity/skity/_codeql_build_dir/third_party/libpng_build/src/libpng-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/runner/work/skity/skity/_codeql_build_dir/third_party/libpng_build/src/libpng-stamp${cfgdir}") # cfgdir has leading slash
endif()
