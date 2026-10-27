# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

# needs python3 interpreter
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Resolves an ALIAS target name to the target it aliases; a no-op otherwise.
function(_gni_resolve_alias OUT_VAR NAME)
    if (TARGET ${NAME})
        get_target_property(_aliased ${NAME} ALIASED_TARGET)
        if (_aliased)
            set(NAME ${_aliased})
        endif()
    endif()
    set(${OUT_VAR} ${NAME} PARENT_SCOPE)
endfunction()

# Appends <target>'s <property> to the list variable <var> (deduplicated). An
# unset property reads back as <var>-NOTFOUND and must not become an element.
macro(_gni_append_property VAR TGT PROP)
    get_target_property(_extra ${TGT} ${PROP})
    if (_extra)
        if (NOT ${VAR})
            set(${VAR} "")
        endif()
        list(APPEND ${VAR} ${_extra})
        list(REMOVE_DUPLICATES ${VAR})
    endif()
endmacro()

# cmake_to_gni(<target> [MERGE <target>...])
#
# Writes <target>.gni from the target's sources / include dirs / definitions /
# options / link libraries and recurses into the targets it links. MERGE folds
# the listed targets into the same gni (union of all five lists) instead of
# emitting one gni per target: GN consumers build libskity as a single library
# that carries the C ABI, so the standalone skity-capi of the normal CMake
# shape is absorbed into skity.gni. Merged targets are remembered so the link
# recursion never emits them on their own, and alias names are resolved
# everywhere so a target reached as skity::capi lands under its real name.
function(cmake_to_gni TARGET)
    cmake_parse_arguments(GNI "" "" "MERGE" ${ARGN})

    _gni_resolve_alias(TARGET ${TARGET})
    message("cmake_to_gni: ${TARGET}")

    # Sanitize target name for use in custom target names (replace :: with _)
    string(REPLACE "::" "_" SANITIZED_TARGET "${TARGET}")

    get_target_property(SOURCE_LISTS ${TARGET} SOURCES)
    get_target_property(INCLUDE_DIRS ${TARGET} INCLUDE_DIRECTORIES)
    get_target_property(COMPILE_DEFINITIONS ${TARGET} COMPILE_DEFINITIONS)
    get_target_property(COMPILE_OPTIONS ${TARGET} COMPILE_OPTIONS)
    get_target_property(LINK_LIBRARIES ${TARGET} LINK_LIBRARIES)

    foreach(MERGED ${GNI_MERGE})
        _gni_resolve_alias(MERGED ${MERGED})
        message("cmake_to_gni: folding ${MERGED} into ${TARGET}")
        set_property(GLOBAL APPEND PROPERTY SKITY_GNI_MERGED_TARGETS ${MERGED})
        _gni_append_property(SOURCE_LISTS ${MERGED} SOURCES)
        _gni_append_property(INCLUDE_DIRS ${MERGED} INCLUDE_DIRECTORIES)
        _gni_append_property(COMPILE_DEFINITIONS ${MERGED} COMPILE_DEFINITIONS)
        _gni_append_property(COMPILE_OPTIONS ${MERGED} COMPILE_OPTIONS)
        _gni_append_property(LINK_LIBRARIES ${MERGED} LINK_LIBRARIES)
    endforeach()

    # GN rejects a file listed twice in one target (#525): CMake dedupes the
    # SOURCES of repeated target_sources() calls only at build time, so
    # normalise here and flag the CMake side that should be cleaned up
    if (SOURCE_LISTS)
        list(LENGTH SOURCE_LISTS _n_listed)
        list(REMOVE_DUPLICATES SOURCE_LISTS)
        list(LENGTH SOURCE_LISTS _n_unique)
        if (NOT _n_listed EQUAL _n_unique)
            math(EXPR _n_dup "${_n_listed} - ${_n_unique}")
            message(WARNING "cmake_to_gni: ${TARGET} lists ${_n_dup} source(s) more than once; deduplicated for the gni — fix the target_sources() calls")
        endif()
    endif()


    set(COMPILE_DEFINITIONS_LIST "")

    if (COMPILE_DEFINITIONS)
        foreach(def ${COMPILE_DEFINITIONS})
            list(APPEND COMPILE_DEFINITIONS_LIST "${def}")
        endforeach(def ${COMPILE_DEFINITIONS})
    endif()

    set(COMPILE_OPTIONS_LIST "")
    if (COMPILE_OPTIONS)
        foreach(opt ${COMPILE_OPTIONS})
            list(APPEND COMPILE_OPTIONS_LIST "\"${opt}\"")
        endforeach(opt ${COMPILE_OPTIONS})
    endif()

    message("COMPILE_DEFINITIONS: ${COMPILE_DEFINITIONS_LIST}")
    message("COMPILE_OPTIONS: ${COMPILE_OPTIONS_LIST}")

    set(LINK_LIST "")

    if (LINK_LIBRARIES)

        foreach(LIB ${LINK_LIBRARIES})
            if (NOT TARGET ${LIB})
                list(APPEND LINK_LIST "\"${LIB}\"")
            endif()
        endforeach(LIB ${LINK_LIBRARIES})

    endif()

    # write source lists to file
    file(WRITE ${CMAKE_BINARY_DIR}/${SANITIZED_TARGET}.sources "")
    foreach(SRC ${SOURCE_LISTS})
        file(APPEND ${CMAKE_BINARY_DIR}/${SANITIZED_TARGET}.sources "${SRC}\n")
    endforeach()

    add_custom_target(${SANITIZED_TARGET}.gni
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/gen_target_gni.py
            --target_name ${TARGET}
            --gn_file ${CMAKE_SOURCE_DIR}/${SANITIZED_TARGET}.gni
            --sources "@${CMAKE_BINARY_DIR}/${SANITIZED_TARGET}.sources"
            --include_dirs ${INCLUDE_DIRS}
            --compile_definitions ${COMPILE_DEFINITIONS_LIST}
            --compile_options ${COMPILE_OPTIONS_LIST}
            --link_libs ${LINK_LIST}
            --strip_root ${CMAKE_SOURCE_DIR}
            --gn_rebase_path ${SKITY_GN_REBASE_PATH}
        VERBATIM
    )

    # filter link libraries
    foreach(LIB ${LINK_LIBRARIES})
        if(TARGET ${LIB})
            if (${LIB} STREQUAL "pugixml::pugixml")
                set(LIB "pugixml-static")
            endif()
            _gni_resolve_alias(LIB ${LIB})

            # targets folded into another gni never get one of their own
            get_property(_merged GLOBAL PROPERTY SKITY_GNI_MERGED_TARGETS)
            if (LIB IN_LIST _merged)
                message("${LIB} is folded into another gni, skipping")
                continue()
            endif()

            # Sanitize library name for checking .gni target existence
            string(REPLACE "::" "_" SANITIZED_LIB "${LIB}")

            if (TARGET ${SANITIZED_LIB}.gni)
                # skip if library has already been generated
                continue()
            endif()
            message("link library: ${LIB}")
            get_target_property(target_type ${LIB} TYPE)

            if(target_type STREQUAL "INTERFACE_LIBRARY")
                get_target_property(aliased_target ${LIB} ALIASED_TARGET)
                message("${LIB} is an interface library, aliased target: ${aliased_target}")
                if(aliased_target)
                    set(LIB ${aliased_target})
                    # Check if the aliased target is still an INTERFACE_LIBRARY
                    get_target_property(aliased_type ${LIB} TYPE)
                    if(aliased_type STREQUAL "INTERFACE_LIBRARY")
                        message("Skipping INTERFACE_LIBRARY ${LIB} - no sources to process")
                        continue()
                    endif()
                else()
                    get_target_property(NM ${LIB} NAME)
                    message("${LIB} raw name, name: ${NM}")
                    continue()
                endif()
            endif()

            message("${LIB} is a target, type: ${target_type}")

            # Skip INTERFACE_LIBRARY targets as they don't have sources
            if(target_type STREQUAL "INTERFACE_LIBRARY")
                message("Skipping INTERFACE_LIBRARY ${LIB} - no sources to process")
                continue()
            endif()

            # check if library has sources
            get_target_property(TS ${LIB} SOURCES)
            get_target_property(IS ${LIB} INTERFACE_SOURCES)
            if (NOT TS AND NOT IS)
                message("${LIB} has no source files ${TS} ${IS}")
                continue()
            endif()

            cmake_to_gni(${LIB})

            # Use sanitized names for dependencies
            add_dependencies(${SANITIZED_TARGET}.gni ${SANITIZED_LIB}.gni)
        endif()
    endforeach()

endfunction(cmake_to_gni)
