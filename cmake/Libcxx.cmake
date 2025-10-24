# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

#//third_party/libcxx:libcxx
set("target" "third_party__libcxx")
list(APPEND "${target}__cxx_srcs"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/algorithm.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/any.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/bind.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/charconv.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/chrono.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/condition_variable.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/condition_variable_destructor.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/debug.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/exception.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/filesystem/directory_iterator.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/filesystem/int128_builtins.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/filesystem/operations.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/format.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/functional.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/future.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/hash.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/ios.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/ios.instantiations.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/iostream.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/locale.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/memory.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/mutex.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/mutex_destructor.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/new.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/optional.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/random.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/regex.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/ryu/d2fixed.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/ryu/d2s.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/ryu/f2s.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/shared_mutex.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/stdexcept.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/string.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/strstream.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/system_error.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/thread.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/typeinfo.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/utility.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/valarray.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/variant.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/vector.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/verbose_abort.cpp")
# Add Windows-specific source files
if(WIN32)
  list(APPEND "${target}__cxx_srcs"
    "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/support/win32/locale_win32.cpp"
    "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/support/win32/support.cpp"
    "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/support/win32/thread_win32.cpp")
endif()
set("${target}__other_srcs" "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/filesystem/filesystem_common.h")
add_library("${target}" STATIC ${${target}__cxx_srcs} ${${target}__other_srcs})
set_source_files_properties(${${target}__other_srcs} PROPERTIES HEADER_FILE_ONLY "True")
set_property(TARGET "${target}" APPEND PROPERTY INCLUDE_DIRECTORIES
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/include/"
  "${CMAKE_SOURCE_DIR}/third_party/libcxxabi/include/"
  "${CMAKE_SOURCE_DIR}/build/secondary/third_party/libcxx/config/"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/")
target_include_directories(${target} PUBLIC "${CMAKE_SOURCE_DIR}/third_party/libcxx/include/")
target_include_directories(${target} PUBLIC "${CMAKE_SOURCE_DIR}/third_party/libcxxabi/include/")
target_include_directories(${target} PUBLIC "${CMAKE_SOURCE_DIR}/build/secondary/third_party/libcxx/config/")
target_include_directories(${target} PUBLIC "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/")
if(WIN32)
  set_target_properties("${target}" PROPERTIES COMPILE_DEFINITIONS "_LIBCPP_NO_EXCEPTIONS;_LIBCPP_NO_RTTI;_LIBCPP_NO_EXCEPTIONS;_LIBCPP_BUILDING_LIBRARY;_LIBCPP_DISABLE_AVAILABILITY=1;_LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS;")
  target_compile_definitions(${target} PUBLIC _LIBCPP_NO_EXCEPTIONS)
  target_compile_definitions(${target} PUBLIC _LIBCPP_NO_RTTI)
  target_compile_definitions(${target} PUBLIC _LIBCPP_BUILDING_LIBRARY)
  target_compile_definitions(${target} PUBLIC _LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS)
  # clang-cl (MSVC-compatible) flags for Windows
  target_compile_definitions(${target} PUBLIC _LIBCPP_ABI_FORCE_ITANIUM)
  set_target_properties("${target}" PROPERTIES COMPILE_FLAGS "/W3 /std:c++20 /EHsc /permissive- /Zc:__cplusplus /bigobj /wd4996 /D_CRT_SECURE_NO_WARNINGS")
  # MSVC-specific link flags (including clang-cl)
  set_target_properties("${target}" PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE /OPT:REF /OPT:ICF")
else()
  set_target_properties("${target}" PROPERTIES COMPILE_DEFINITIONS "_LIBCPP_NO_EXCEPTIONS;_LIBCPP_NO_RTTI;_LIBCPP_BUILDING_LIBRARY;LIBCXX_BUILDING_LIBCXXABI;USE_OPENSSL=1;_FILE_OFFSET_BITS=64;_LARGEFILE_SOURCE;_LARGEFILE64_SOURCE;__STDC_CONSTANT_MACROS;__STDC_FORMAT_MACROS;_LIBCPP_DISABLE_AVAILABILITY=1;_LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS;_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS;NDEBUG;NVALGRIND;DYNAMIC_ANNOTATIONS_ENABLED=0;")
  target_compile_definitions(${target} PUBLIC _LIBCPP_NO_EXCEPTIONS)
  target_compile_definitions(${target} PUBLIC _LIBCPP_NO_RTTI)
  target_compile_definitions(${target} PUBLIC _LIBCPP_BUILDING_LIBRARY)
  target_compile_definitions(${target} PUBLIC LIBCXX_BUILDING_LIBCXXABI)
  target_compile_definitions(${target} PUBLIC _LARGEFILE_SOURCE)
  target_compile_definitions(${target} PUBLIC _LARGEFILE64_SOURCE)
  target_compile_definitions(${target} PUBLIC __STDC_CONSTANT_MACROS)
  target_compile_definitions(${target} PUBLIC __STDC_FORMAT_MACROS)
  target_compile_definitions(${target} PUBLIC _LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS)
  target_compile_definitions(${target} PUBLIC _LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS)
  target_compile_definitions(${target} PUBLIC NDEBUG)
  target_compile_definitions(${target} PUBLIC NVALGRIND)
  set_target_properties("${target}" PROPERTIES COMPILE_FLAGS "-fno-strict-aliasing -fstack-protector --param=ssp-buffer-size=4 -m64 -march=x86-64 -fPIC -pipe -pthread -fcolor-diagnostics -Wall -Wextra -Wendif-labels -Werror -Wno-missing-field-initializers -Wno-unused-parameter -Wno-vla-extension -Wno-unused-but-set-parameter -Wno-unused-but-set-variable -Wno-implicit-int-float-conversion -Wno-c99-designator -Wno-deprecated-copy -Wno-psabi -Wno-deprecated-non-prototype -Wno-enum-constexpr-conversion -Wno-unqualified-std-cast-call -Wno-non-c-typedef-for-linkage -Wno-range-loop-construct -fvisibility=hidden -Wstring-conversion -Wnewline-eof -O2 -fno-ident -fdata-sections -ffunction-sections -g0 -Wno-thread-safety-analysis -fvisibility-inlines-hidden -nostdinc++ -fno-exceptions -std=c++20 -nostdinc++ -fvisibility=hidden -frtti ")
  set_target_properties("${target}" PROPERTIES LINK_FLAGS "-Wl,--fatal-warnings -m64 -fPIC -Wl,-z,noexecstack -Wl,-z,now -Wl,-z,relro -Wl,-z,defs -pthread -Wl,--undefined-version -L${CMAKE_SOURCE_DIR}/build/linux/debian_sid_amd64-sysroot/usr/local/lib/x86_64-linux-gnu -Wl,-rpath-link=${CMAKE_SOURCE_DIR}/build/linux/debian_sid_amd64-sysroot/usr/local/lib/x86_64-linux-gnu -L${CMAKE_SOURCE_DIR}/build/linux/debian_sid_amd64-sysroot/lib/x86_64-linux-gnu -Wl,-rpath-link=${CMAKE_SOURCE_DIR}/build/linux/debian_sid_amd64-sysroot/lib/x86_64-linux-gnu -L${CMAKE_SOURCE_DIR}/build/linux/debian_sid_amd64-sysroot/usr/lib/x86_64-linux-gnu -Wl,-rpath-link=${CMAKE_SOURCE_DIR}/build/linux/debian_sid_amd64-sysroot/usr/lib/x86_64-linux-gnu -Wl,-O2 -Wl,--gc-sections -Wl,--as-needed ")
endif()

#//third_party/libcxxabi:libcxxabi
# Skip libcxxabi compilation on Windows platform
if(NOT WIN32)
set("target" "third_party__libcxxabi")
list(APPEND "${target}__cxx_srcs"
  "${CMAKE_SOURCE_DIR}/third_party/libcxxabi/src/cxa_exception.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxxabi/src/cxa_personality.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxxabi/src/abort_message.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxxabi/src/cxa_aux_runtime.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxxabi/src/cxa_default_handlers.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxxabi/src/cxa_demangle.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxxabi/src/cxa_exception_storage.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxxabi/src/cxa_handlers.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxxabi/src/cxa_vector.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxxabi/src/cxa_virtual.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxxabi/src/fallback_malloc.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxxabi/src/private_typeinfo.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxxabi/src/stdlib_exception.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxxabi/src/stdlib_stdexcept.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxxabi/src/stdlib_typeinfo.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/libcxxabi/src/cxa_guard.cpp")
add_library("${target}" STATIC ${${target}__cxx_srcs})
set_property(TARGET "${target}" APPEND PROPERTY INCLUDE_DIRECTORIES
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/include/"
  "${CMAKE_SOURCE_DIR}/third_party/libcxxabi/include/"
  "${CMAKE_SOURCE_DIR}/build/secondary/third_party/libcxx/config/"
  "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/")
target_include_directories(${target} PUBLIC "${CMAKE_SOURCE_DIR}/third_party/libcxx/include/")
target_include_directories(${target} PUBLIC "${CMAKE_SOURCE_DIR}/third_party/libcxxabi/include/")
target_include_directories(${target} PUBLIC "${CMAKE_SOURCE_DIR}/build/secondary/third_party/libcxx/config/")
target_include_directories(${target} PUBLIC "${CMAKE_SOURCE_DIR}/third_party/libcxx/src/")
set_target_properties("${target}" PROPERTIES COMPILE_DEFINITIONS "_LIBCPP_BUILDING_LIBRARY;_LIBCXXABI_BUILDING_LIBRARY;LIBCXXABI_SILENT_TERMINATE;_LIBCXXABI_DISABLE_VISIBILITY_ANNOTATIONS;USE_OPENSSL=1;_FILE_OFFSET_BITS=64;_LARGEFILE_SOURCE;_LARGEFILE64_SOURCE;__STDC_CONSTANT_MACROS;__STDC_FORMAT_MACROS;_LIBCPP_DISABLE_AVAILABILITY=1;_LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS;_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS;NDEBUG;NVALGRIND;DYNAMIC_ANNOTATIONS_ENABLED=0;")
target_compile_definitions(${target} PUBLIC _LIBCPP_BUILDING_LIBRARY)
target_compile_definitions(${target} PUBLIC _LIBCXXABI_BUILDING_LIBRARY)
target_compile_definitions(${target} PUBLIC LIBCXXABI_SILENT_TERMINATE)
target_compile_definitions(${target} PUBLIC _LIBCXXABI_DISABLE_VISIBILITY_ANNOTATIONS)
target_compile_definitions(${target} PUBLIC _LARGEFILE_SOURCE)
target_compile_definitions(${target} PUBLIC _LARGEFILE64_SOURCE)
target_compile_definitions(${target} PUBLIC __STDC_CONSTANT_MACROS)
target_compile_definitions(${target} PUBLIC __STDC_FORMAT_MACROS)
target_compile_definitions(${target} PUBLIC _LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS)
target_compile_definitions(${target} PUBLIC _LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS)
target_compile_definitions(${target} PUBLIC NDEBUG)
target_compile_definitions(${target} PUBLIC NVALGRIND)
set_target_properties("${target}" PROPERTIES COMPILE_FLAGS "-fno-strict-aliasing -fstack-protector --param=ssp-buffer-size=4 -m64 -march=x86-64 -fPIC -pipe -pthread -fcolor-diagnostics -Wall -Wextra -Wendif-labels -Werror -Wno-missing-field-initializers -Wno-unused-parameter -Wno-vla-extension -Wno-unused-but-set-parameter -Wno-unused-but-set-variable -Wno-implicit-int-float-conversion -Wno-c99-designator -Wno-deprecated-copy -Wno-psabi -Wno-deprecated-non-prototype -Wno-enum-constexpr-conversion -Wno-unqualified-std-cast-call -Wno-non-c-typedef-for-linkage -Wno-range-loop-construct -fvisibility=hidden -Wstring-conversion -Wnewline-eof -O2 -fno-ident -fdata-sections -ffunction-sections -g0 -fvisibility-inlines-hidden -nostdinc++ -std=c++20 -nostdinc++ -fvisibility=hidden -frtti ")
set_target_properties("${target}" PROPERTIES LINK_FLAGS "-Wl,--fatal-warnings -m64 -fPIC -Wl,-z,noexecstack -Wl,-z,now -Wl,-z,relro -Wl,-z,defs -pthread -Wl,--undefined-version -L${CMAKE_SOURCE_DIR}/build/linux/debian_sid_amd64-sysroot/usr/local/lib/x86_64-linux-gnu -Wl,-rpath-link=${CMAKE_SOURCE_DIR}/build/linux/debian_sid_amd64-sysroot/usr/local/lib/x86_64-linux-gnu -L${CMAKE_SOURCE_DIR}/build/linux/debian_sid_amd64-sysroot/lib/x86_64-linux-gnu -Wl,-rpath-link=${CMAKE_SOURCE_DIR}/build/linux/debian_sid_amd64-sysroot/lib/x86_64-linux-gnu -L${CMAKE_SOURCE_DIR}/build/linux/debian_sid_amd64-sysroot/usr/lib/x86_64-linux-gnu -Wl,-rpath-link=${CMAKE_SOURCE_DIR}/build/linux/debian_sid_amd64-sysroot/usr/lib/x86_64-linux-gnu -Wl,-O2 -Wl,--gc-sections -Wl,--as-needed ")
endif() # Skip libcxxabi compilation on Windows platform