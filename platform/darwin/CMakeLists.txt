
# We use static library to prevent code sign at build time
add_library(skity_framework STATIC)

set(FM_VERSION_STRING "${CMAKE_PROJECT_VERSION}${SKITY_BUNDLE_SUFFIX}")

configure_file(
  ${CMAKE_CURRENT_LIST_DIR}/Info.plist.in
  ${CMAKE_CURRENT_BINARY_DIR}/Info.plist
  @ONLY
)

# set all source file from skity to skity_framework
get_target_property(skity_lib_sources skity SOURCES)

if (skity_lib_sources STREQUAL "skity_lib_sources-NOTFOUND")
  message(FATAL_ERROR "skity_lib_sources is not found")
endif()

target_sources(skity_framework PRIVATE ${skity_lib_sources})

# set all include directory from skity to skity_framework
get_target_property(skity_include_dirs skity INCLUDE_DIRECTORIES)

if (skity_include_dirs STREQUAL "skity_include_dirs-NOTFOUND")
  message(FATAL_ERROR "skity_include_dirs is not found")
endif()

message("skity_include_dirs: ${skity_include_dirs}")

target_include_directories(skity_framework PRIVATE ${skity_include_dirs})

# set all compile option from skity to skity_framework
get_target_property(skity_compile_options skity COMPILE_OPTIONS)

if (skity_compile_options STREQUAL "skity_compile_options-NOTFOUND")
  message(FATAL_ERROR "skity_compile_options is not found")
endif()

message("skity_compile_options: ${skity_compile_options}")

target_compile_options(skity_framework PRIVATE ${skity_compile_options})


# set all compile definition from skity to skity_framework
get_target_property(skity_compile_definitions skity COMPILE_DEFINITIONS)

if (skity_compile_definitions STREQUAL "skity_compile_definitions-NOTFOUND")
  message(FATAL_ERROR "skity_compile_definitions is not found")
endif()

message("skity_compile_definitions: ${skity_compile_definitions}")

target_compile_definitions(skity_framework PRIVATE ${skity_compile_definitions})

# set all link library from skity to skity_framework
get_target_property(skity_link_libraries skity LINK_LIBRARIES)

if (skity_link_libraries STREQUAL "skity_link_libraries-NOTFOUND")
  message(FATAL_ERROR "skity_link_libraries is not found")
endif()

message("skity_link_libraries: ${skity_link_libraries}")

target_link_libraries(skity_framework PRIVATE ${skity_link_libraries})

# set all link directory from skity to skity_framework
get_target_property(skity_link_directories skity LINK_DIRECTORIES)

if (NOT skity_link_directories STREQUAL "skity_link_directories-NOTFOUND")
  message("skity_link_directories: ${skity_link_directories}")
  target_link_directories(skity_framework PRIVATE ${skity_link_directories})
endif()

# compile wgx into skity_framework so we not needs to use libtool

get_target_property(wgx_sources wgsl-cross SOURCES)
if (wgx_sources STREQUAL "wgx_sources-NOTFOUND")
  message(FATAL_ERROR "wgx_sources is not found")
endif()

target_sources(skity_framework PRIVATE ${wgx_sources})

# set wgx include directory
get_target_property(wgx_include_dirs wgsl-cross INCLUDE_DIRECTORIES)
if (wgx_include_dirs STREQUAL "wgx_include_dirs-NOTFOUND")
  message(FATAL_ERROR "wgx_include_dirs is not found")
endif()

target_include_directories(skity_framework PRIVATE ${wgx_include_dirs})

# set wgx compile definition
get_target_property(wgx_compile_definitions wgsl-cross COMPILE_DEFINITIONS)
if (wgx_compile_definitions STREQUAL "wgx_compile_definitions-NOTFOUND")
  message(FATAL_ERROR "wgx_compile_definitions is not found")
endif()

target_compile_definitions(skity_framework PRIVATE ${wgx_compile_definitions})


# set cxx standard
set_target_properties(skity_framework PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
)


# set Framework property
set_target_properties(skity_framework PROPERTIES
    FRAMEWORK TRUE
    PUBLIC_HEADER "public/skity.h"
    MACOSX_FRAMEWORK_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/Info.plist"
    OUTPUT_NAME "skity"
    XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "" # disable code sign since we only publish static library
)

# install framework
install(
    TARGETS skity_framework
    FRAMEWORK
    DESTINATION "${CMAKE_INSTALL_PREFIX}"
    COMPONENT Runtime
)

# install public header
install(
    FILES "public/skity.h"
    DESTINATION "${CMAKE_INSTALL_PREFIX}/skity.framework/Headers"
    COMPONENT Runtime
)

# install c++ header manually
install(
    FILES "${CMAKE_SOURCE_DIR}/include/skity/macros.hpp" "${CMAKE_SOURCE_DIR}/include/skity/skity.hpp"
    DESTINATION "${CMAKE_INSTALL_PREFIX}/skity.framework/Headers"
    COMPONENT Runtime
)

install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/include/skity/effect"
    DESTINATION "${CMAKE_INSTALL_PREFIX}/skity.framework/Headers"
    COMPONENT Runtime
)

install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/include/skity/geometry"
    DESTINATION "${CMAKE_INSTALL_PREFIX}/skity.framework/Headers"
    COMPONENT Runtime
)

install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/include/skity/gpu"
    DESTINATION "${CMAKE_INSTALL_PREFIX}/skity.framework/Headers"
    COMPONENT Runtime
)

install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/include/skity/graphic"
    DESTINATION "${CMAKE_INSTALL_PREFIX}/skity.framework/Headers"
    COMPONENT Runtime
)

install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/include/skity/io"
    DESTINATION "${CMAKE_INSTALL_PREFIX}/skity.framework/Headers"
    COMPONENT Runtime
)

install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/include/skity/recorder"
    DESTINATION "${CMAKE_INSTALL_PREFIX}/skity.framework/Headers"
    COMPONENT Runtime
)

install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/include/skity/render"
    DESTINATION "${CMAKE_INSTALL_PREFIX}/skity.framework/Headers"
    COMPONENT Runtime
)

install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/include/skity/text"
    DESTINATION "${CMAKE_INSTALL_PREFIX}/skity.framework/Headers"
    COMPONENT Runtime
)

install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/include/skity/utils"
    DESTINATION "${CMAKE_INSTALL_PREFIX}/skity.framework/Headers"
    COMPONENT Runtime
)
