# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

include_guard(GLOBAL)
include(ExternalProject)

# Providers select source layout and version-specific Meson arguments.
# Expat.cmake supplies the expat target and its public header directory.
function(skity_add_fontconfig)
  cmake_parse_arguments(FONTCONFIG "GENERATED_HEADER"
    "SOURCE_DIR;LIBRARY_TARGET" "MESON_OPTIONS" ${ARGN})
  if(FONTCONFIG_UNPARSED_ARGUMENTS OR NOT FONTCONFIG_SOURCE_DIR OR
     NOT FONTCONFIG_LIBRARY_TARGET)
    message(FATAL_ERROR "skity_add_fontconfig requires SOURCE_DIR and LIBRARY_TARGET")
  endif()
  set(fontconfig_source "${FONTCONFIG_SOURCE_DIR}")
  if(NOT EXISTS "${fontconfig_source}/meson.build")
    message(FATAL_ERROR
      "Fontconfig sources are missing at ${fontconfig_source}; sync dependencies or select SKITY_FONT_DEPS_CMAKE_DIR")
  endif()
  set(fontconfig_build "${CMAKE_BINARY_DIR}/third_party/fontconfig")
  set(fontconfig_library_target "${FONTCONFIG_LIBRARY_TARGET}")
  set(fontconfig_byproducts "${fontconfig_build}/${fontconfig_library_target}")
  if(FONTCONFIG_GENERATED_HEADER)
    list(APPEND fontconfig_byproducts "${fontconfig_build}/fontconfig/fontconfig.h")
  endif()

  find_program(SKITY_MESON_EXECUTABLE NAMES meson)
  find_program(SKITY_NINJA_EXECUTABLE NAMES ninja ninja-build)
  find_program(SKITY_GPERF_EXECUTABLE NAMES gperf)
  foreach(tool MESON NINJA GPERF)
    if(NOT SKITY_${tool}_EXECUTABLE)
      message(FATAL_ERROR "Bundled Fontconfig requires ${tool}; see docs/BUILD_AND_RUN.md")
    endif()
  endforeach()
  find_package(PkgConfig REQUIRED)
  find_package(Threads REQUIRED)

  # Configure-time link checks use the same archives as Skity. The private
  # pkg-config directory prevents finding another FreeType or XML parser.
  set(pkgconfig_dir "${fontconfig_build}/deps/$<CONFIG>")
  set(png_archive "${CMAKE_BINARY_DIR}/third_party/libpng/lib/libpng.a")
  set(zlib_dirs "")
  set(zlib_pkgconfig "")
  if(USE_THIRD_PARTY_ZLIB)
    set(zlib_dirs "${CMAKE_BINARY_DIR}/third_party/zlib/lib")
    set(zlib_pkgconfig "-L\"${zlib_dirs}\"")
  endif()
  file(STRINGS "${FREETYPE_DIR}/builds/unix/configure.raw" freetype_version
    REGEX "^version_info='[0-9:]+'.*")
  string(REGEX REPLACE "^version_info='([0-9:]+)'.*" "\\1"
    freetype_version "${freetype_version}")
  string(REPLACE ":" "." freetype_version "${freetype_version}")
  foreach(component MAJOR MINOR MICRO)
    file(STRINGS "${SKITY_EXPAT_INCLUDE_DIR}/expat.h" version_line
      REGEX "^# *define XML_${component}_VERSION +[0-9]+")
    string(REGEX MATCH "[0-9]+$" expat_${component} "${version_line}")
  endforeach()
  file(GENERATE OUTPUT "${pkgconfig_dir}/freetype2.pc" CONTENT
"Name: FreeType
Description: Skity's bundled FreeType
Version: ${freetype_version}
Libs: \"$<TARGET_FILE:freetype2>\" \"${png_archive}\" ${zlib_pkgconfig} -lz -lm
Cflags: -I\"${FREETYPE_DIR}/include\" -DFREETYPE_STATIC_LIB=1
")
  file(GENERATE OUTPUT "${pkgconfig_dir}/expat.pc" CONTENT
"Name: Expat
Description: Skity's bundled Expat
Version: ${expat_MAJOR}.${expat_MINOR}.${expat_MICRO}
Libs: \"$<TARGET_FILE:expat>\" -lm
Cflags: -I\"${SKITY_EXPAT_INCLUDE_DIR}\" -DXML_STATIC
")

  # Use CMake's compiler and flags, including sanitizer flags, for Meson too.
  foreach(kind C EXE_LINKER)
    set(meson_args "")
    separate_arguments(flags NATIVE_COMMAND "${CMAKE_${kind}_FLAGS}")
    foreach(flag IN LISTS flags)
      string(REPLACE "'" "\\'" flag "${flag}")
      string(APPEND meson_args "'${flag}', ")
    endforeach()
    foreach(config Debug Release RelWithDebInfo MinSizeRel)
      string(TOUPPER "${config}" upper_config)
      separate_arguments(flags NATIVE_COMMAND "${CMAKE_${kind}_FLAGS_${upper_config}}")
      foreach(flag IN LISTS flags)
        string(REPLACE "'" "\\'" flag "${flag}")
        string(APPEND meson_args "$<$<CONFIG:${config}>:'${flag}', >")
      endforeach()
    endforeach()
    if(kind STREQUAL "C")
      set(meson_c_args "${meson_args}")
    else()
      set(meson_link_args "${meson_args}")
    endif()
  endforeach()
  if(CMAKE_SYSROOT)
    string(APPEND meson_c_args "'--sysroot=${CMAKE_SYSROOT}', ")
  endif()
  if(CMAKE_C_COMPILER_TARGET)
    string(APPEND meson_c_args "'--target=${CMAKE_C_COMPILER_TARGET}', ")
  endif()
  set(machine_file "${pkgconfig_dir}/machine.ini")
  set(machine_config
"[binaries]
c = '${CMAKE_C_COMPILER}'
ar = '${CMAKE_AR}'
strip = '${CMAKE_STRIP}'
pkg-config = '${PKG_CONFIG_EXECUTABLE}'
gperf = '${SKITY_GPERF_EXECUTABLE}'
[built-in options]
c_args = [${meson_c_args}]
c_link_args = [${meson_c_args}${meson_link_args}]
")
  set(machine_option --native-file)
  if(CMAKE_CROSSCOMPILING)
    set(machine_option --cross-file)
    set(cpu_family "${CMAKE_SYSTEM_PROCESSOR}")
    if(cpu_family MATCHES "^(aarch64|arm64)$")
      set(cpu_family aarch64)
    elseif(cpu_family MATCHES "^arm")
      set(cpu_family arm)
    elseif(cpu_family MATCHES "^i.86$")
      set(cpu_family x86)
    endif()
    include(TestBigEndian)
    test_big_endian(big_endian)
    set(endian little)
    if(big_endian)
      set(endian big)
    endif()
    string(APPEND machine_config
"[host_machine]
system = 'linux'
cpu_family = '${cpu_family}'
cpu = '${CMAKE_SYSTEM_PROCESSOR}'
endian = '${endian}'
[properties]
needs_exe_wrapper = true
")
  endif()
  file(GENERATE OUTPUT "${machine_file}" CONTENT "${machine_config}")

  # Do not install or overwrite system configuration/caches, and do not bake
  # the build directory into FONTCONFIG_PATH.
  ExternalProject_Add(skity_fontconfig_build
    SOURCE_DIR "${fontconfig_source}"
    BINARY_DIR "${fontconfig_build}"
    PREFIX "${CMAKE_BINARY_DIR}/third_party/fontconfig_build"
    DOWNLOAD_COMMAND ""
    UPDATE_COMMAND ""
    CONFIGURE_COMMAND "${CMAKE_COMMAND}" -E env
      "PKG_CONFIG_PATH=" "PKG_CONFIG_LIBDIR=${pkgconfig_dir}"
      "PKG_CONFIG_SYSROOT_DIR=" "NINJA=${SKITY_NINJA_EXECUTABLE}"
      "${SKITY_MESON_EXECUTABLE}" setup --reconfigure --clearcache
      <BINARY_DIR> <SOURCE_DIR> ${machine_option} "${machine_file}"
      --prefix=/usr --sysconfdir=/etc --localstatedir=/var
      --wrap-mode=nodownload --default-library=static
      -Db_staticpic=true -Dbuildtype=plain ${FONTCONFIG_MESON_OPTIONS}
      -Ddoc=disabled -Dtests=disabled -Dtools=disabled -Dcache-build=disabled
      -Dnls=disabled -Diconv=disabled
      -Dbaseconfig-dir=/etc/fonts -Dconfig-dir=/etc/fonts/conf.d
      -Dcache-dir=/var/cache/fontconfig
      -Dtemplate-dir=/usr/share/fontconfig/conf.avail
    BUILD_COMMAND "${SKITY_NINJA_EXECUTABLE}" -C <BINARY_DIR> "${fontconfig_library_target}"
    BUILD_BYPRODUCTS ${fontconfig_byproducts}
    BUILD_ALWAYS TRUE
    INSTALL_COMMAND ""
    DEPENDS freetype2 expat libpng)
  ExternalProject_Add_StepDependencies(skity_fontconfig_build configure
    freetype2 expat libpng)
  # Regenerate Meson's configuration when generated input files change.
  ExternalProject_Add_Step(skity_fontconfig_build configure_inputs
    DEPENDEES patch
    DEPENDERS configure
    DEPENDS "${machine_file}" "${pkgconfig_dir}/freetype2.pc"
      "${pkgconfig_dir}/expat.pc")

  file(MAKE_DIRECTORY "${fontconfig_build}/fontconfig")
  add_library(skity_fontconfig STATIC IMPORTED GLOBAL)
  set_target_properties(skity_fontconfig PROPERTIES
    IMPORTED_LOCATION "${fontconfig_build}/${fontconfig_library_target}"
    INTERFACE_INCLUDE_DIRECTORIES "${fontconfig_build};${fontconfig_source}"
    INTERFACE_LINK_LIBRARIES "freetype2;expat;${SKITY_LIBPNG_NAME};z;Threads::Threads;m"
    INTERFACE_LINK_DIRECTORIES "${CMAKE_BINARY_DIR}/third_party/libpng/lib;${zlib_dirs}")
  add_dependencies(skity_fontconfig skity_fontconfig_build)

endfunction()
