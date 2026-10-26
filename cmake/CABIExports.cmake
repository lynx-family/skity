# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

# Export policy for the SKITY_UNIFIED_LIB shape: a shared library built with
# this helper exports exactly its skity_* C ABI and nothing else.
#
# Two layers are needed. -fvisibility=hidden (plus SKITY_HIDE_CXX_API
# neutralizing SKITY_API) lowers the library's OWN C++ definitions to local —
# measured on the host unified build, that layer alone already leaves zero
# skity:: symbols exported; SKITY_C_API carries an explicit
# visibility("default") so the C entry points survive it. What that layer
# cannot reach is everything linked in from OTHER targets with default
# visibility, and that is what the linker allow-list removes: for libskity
# the wgsl-cross objects (its own -fvisibility=hidden applies to Android
# release builds only — ~1.7k wgx:: symbols leak on desktop without this) and
# the statically linked freetype + libpng that freetype pulls in (~700
# symbols); for libskity-codec the wuffs, libjpeg-turbo and libwebp archives
# (~850). On MSVC there is no export-table filtering to do: withdrawing
# __declspec(dllexport) through SKITY_HIDE_CXX_API already keeps the C++ API
# out of the DLL, and static archives never export on their own.
set(SKITY_CABI_ONLY_MAP "${CMAKE_CURRENT_LIST_DIR}/../module/capi/cabi_only.map")

function(skity_export_c_abi_only target)
  if (MSVC)
    return()
  endif()

  target_compile_options(${target} PRIVATE
    -fvisibility=hidden
    -fvisibility-inlines-hidden
  )

  if (APPLE)
    target_link_options(${target} PRIVATE "-Wl,-exported_symbol,_skity_*")
  else()
    target_link_options(${target} PRIVATE
      "-Wl,--exclude-libs,ALL"
      "-Wl,--version-script=${SKITY_CABI_ONLY_MAP}"
    )
    set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "${SKITY_CABI_ONLY_MAP}")
  endif()
endfunction()
