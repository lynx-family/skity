# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

# needs python3 interpreter
find_package(Python3 REQUIRED COMPONENTS Interpreter)

function(cmake_to_gni TARGET)
    message("cmake_to_gni: ${TARGET}")

    get_target_property(SOURCE_LISTS ${TARGET} SOURCES)
    get_target_property(INCLUDE_DIRS ${TARGET} INCLUDE_DIRECTORIES)
    get_target_property(COMPILE_DEFINITIONS ${TARGET} COMPILE_DEFINITIONS)
    get_target_property(COMPILE_OPTIONS ${TARGET} COMPILE_OPTIONS)
    get_target_property(LINK_LIBRARIES ${TARGET} LINK_LIBRARIES)


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
    file(WRITE ${CMAKE_BINARY_DIR}/${TARGET}.sources "")
    foreach(SRC ${SOURCE_LISTS})
        file(APPEND ${CMAKE_BINARY_DIR}/${TARGET}.sources "${SRC}\n")
    endforeach()

    add_custom_target(${TARGET}.gni
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/gen_target_gni.py
            --target_name ${TARGET}
            --gn_file ${CMAKE_SOURCE_DIR}/${TARGET}.gni
            --sources "@${CMAKE_BINARY_DIR}/${TARGET}.sources"
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

            if (TARGET ${LIB}.gni)
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
                else()
                    get_target_property(NM ${LIB} NAME)
                    message("${LIB} raw name, name: ${NM}")
                    break()
                endif()
            endif()

            message("${LIB} is a target, type: ${target_type}")

            # check if library has sources
            get_target_property(TS ${LIB} SOURCES)
            get_target_property(IS ${LIB} INTERFACE_SOURCES)
            if (NOT TS AND NOT IS)
                message("${LIB} has no source files ${TS} ${IS}")
                continue()
            endif()

            cmake_to_gni(${LIB})

            add_dependencies(${TARGET}.gni ${LIB}.gni)
        endif()
    endforeach()

endfunction(cmake_to_gni)


