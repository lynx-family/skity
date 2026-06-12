# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

set(SKITY_FONT_HARNESS_IOS_PLATFORM OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS" OR CMAKE_OSX_SYSROOT MATCHES "iphone")
  set(SKITY_FONT_HARNESS_IOS_PLATFORM ON)
endif()

if(NOT SKITY_FONT_HARNESS_IOS_PLATFORM)
  return()
endif()

if(NOT CMAKE_GENERATOR MATCHES "Xcode")
  message(WARNING "skity_font_harness_ios_xctest requires the Xcode generator")
  return()
endif()

set(
  SKITY_FONT_HARNESS_IOS_DEVELOPMENT_TEAM
  ""
  CACHE STRING
  "Apple development team id used to sign the iOS font harness runner"
)
set(
  SKITY_FONT_HARNESS_IOS_BUNDLE_ID_PREFIX
  ""
  CACHE STRING
  "Optional bundle id prefix used to sign the iOS font harness runner"
)

set(SKITY_FONT_HARNESS_IOS_CODE_SIGNING_ALLOWED NO)
set(SKITY_FONT_HARNESS_IOS_CODE_SIGNING_REQUIRED NO)
set(SKITY_FONT_HARNESS_IOS_HOST_BUNDLE_ID
    "org.skity.font-harness.ios-host")
set(SKITY_FONT_HARNESS_IOS_TESTS_BUNDLE_ID
    "org.skity.font-harness.ios-tests")
if(CMAKE_OSX_SYSROOT MATCHES "iphoneos" AND
   NOT SKITY_FONT_HARNESS_IOS_DEVELOPMENT_TEAM STREQUAL "")
  set(SKITY_FONT_HARNESS_IOS_CODE_SIGNING_ALLOWED YES)
  set(SKITY_FONT_HARNESS_IOS_CODE_SIGNING_REQUIRED YES)
  if(NOT SKITY_FONT_HARNESS_IOS_BUNDLE_ID_PREFIX STREQUAL "")
    set(SKITY_FONT_HARNESS_IOS_HOST_BUNDLE_ID
        "${SKITY_FONT_HARNESS_IOS_BUNDLE_ID_PREFIX}.skity.host")
    set(SKITY_FONT_HARNESS_IOS_TESTS_BUNDLE_ID
        "${SKITY_FONT_HARNESS_IOS_BUNDLE_ID_PREFIX}.skity.tests")
  endif()
endif()

find_library(SKITY_FONT_HARNESS_UIKIT_FRAMEWORK UIKit REQUIRED)

get_target_property(
  SKITY_FONT_HARNESS_SKITY_LINK_LIBRARIES
  skity
  LINK_LIBRARIES
)
if(NOT "${SKITY_FONT_HARNESS_SKITY_LINK_LIBRARIES}" STREQUAL
   "SKITY_FONT_HARNESS_SKITY_LINK_LIBRARIES-NOTFOUND")
  list(REMOVE_ITEM SKITY_FONT_HARNESS_SKITY_LINK_LIBRARIES "-framework AppKit")
  set_property(
    TARGET skity
    PROPERTY LINK_LIBRARIES ${SKITY_FONT_HARNESS_SKITY_LINK_LIBRARIES}
  )
endif()
target_link_libraries(skity PRIVATE ${SKITY_FONT_HARNESS_UIKIT_FRAMEWORK})
# The XCTest bundle links shared probe code from skity_font_harness and needs
# those Skity C++ symbols exported by the harness build dylib.
target_compile_options(skity PRIVATE -fvisibility=default)

set(SKITY_FONT_HARNESS_XCODE_PLATFORM_SDK iphoneos)
if(CMAKE_OSX_SYSROOT MATCHES "iphonesimulator")
  set(SKITY_FONT_HARNESS_XCODE_PLATFORM_SDK iphonesimulator)
endif()

execute_process(
  COMMAND xcrun --sdk ${SKITY_FONT_HARNESS_XCODE_PLATFORM_SDK}
          --show-sdk-platform-path
  OUTPUT_VARIABLE SKITY_FONT_HARNESS_XCODE_PLATFORM_PATH
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)
find_library(
  SKITY_FONT_HARNESS_XCTEST_FRAMEWORK
  XCTest
  PATHS
    "${SKITY_FONT_HARNESS_XCODE_PLATFORM_PATH}/Developer/Library/Frameworks"
  NO_DEFAULT_PATH
  NO_CMAKE_FIND_ROOT_PATH
)
if(NOT SKITY_FONT_HARNESS_XCTEST_FRAMEWORK)
  find_library(SKITY_FONT_HARNESS_XCTEST_FRAMEWORK XCTest REQUIRED)
endif()

add_executable(skity_font_harness_ios_host_app
  MACOSX_BUNDLE
  ${CMAKE_CURRENT_LIST_DIR}/app/FontHarnessIOSHostApp.mm
)

target_compile_features(skity_font_harness_ios_host_app PRIVATE cxx_std_17)
target_link_libraries(
  skity_font_harness_ios_host_app
  PRIVATE
  skity::skity
  ${SKITY_FONT_HARNESS_UIKIT_FRAMEWORK}
)

set_target_properties(
  skity_font_harness_ios_host_app
  PROPERTIES
  MACOSX_BUNDLE TRUE
  MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_LIST_DIR}/app/Info.plist.in
  XCODE_EXPLICIT_FILE_TYPE "wrapper.application"
  XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED
    "${SKITY_FONT_HARNESS_IOS_CODE_SIGNING_ALLOWED}"
  XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED
    "${SKITY_FONT_HARNESS_IOS_CODE_SIGNING_REQUIRED}"
  XCODE_ATTRIBUTE_DEVELOPMENT_TEAM
    "${SKITY_FONT_HARNESS_IOS_DEVELOPMENT_TEAM}"
  XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
  XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER
    "${SKITY_FONT_HARNESS_IOS_HOST_BUNDLE_ID}"
  XCODE_ATTRIBUTE_SDKROOT "${SKITY_FONT_HARNESS_XCODE_PLATFORM_SDK}"
  XCODE_ATTRIBUTE_SUPPORTED_PLATFORMS "${SKITY_FONT_HARNESS_XCODE_PLATFORM_SDK}"
  XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
  XCODE_GENERATE_SCHEME TRUE
)

add_library(skity_font_harness_ios_xctest
  MODULE
  ${CMAKE_CURRENT_LIST_DIR}/runner/ios_runner.mm
  ${CMAKE_CURRENT_LIST_DIR}/tests/FontHarnessIOSRunnerTests.mm
)

target_compile_features(skity_font_harness_ios_xctest PRIVATE cxx_std_17)
target_include_directories(
  skity_font_harness_ios_xctest
  PRIVATE
  ${CMAKE_SOURCE_DIR}
)
target_link_libraries(
  skity_font_harness_ios_xctest
  PRIVATE
  skity_font_harness
  ${SKITY_FONT_HARNESS_UIKIT_FRAMEWORK}
  ${SKITY_FONT_HARNESS_XCTEST_FRAMEWORK}
)

add_dependencies(
  skity_font_harness_ios_xctest
  skity_font_harness_ios_host_app
)

set_target_properties(
  skity_font_harness_ios_xctest
  PROPERTIES
  BUNDLE TRUE
  BUNDLE_EXTENSION xctest
  MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_LIST_DIR}/tests/Info.plist.in
  XCODE_PRODUCT_TYPE "com.apple.product-type.bundle.unit-test"
  XCODE_ATTRIBUTE_BUNDLE_LOADER "$<TARGET_FILE:skity_font_harness_ios_host_app>"
  XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED
    "${SKITY_FONT_HARNESS_IOS_CODE_SIGNING_ALLOWED}"
  XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED
    "${SKITY_FONT_HARNESS_IOS_CODE_SIGNING_REQUIRED}"
  XCODE_ATTRIBUTE_DEVELOPMENT_TEAM
    "${SKITY_FONT_HARNESS_IOS_DEVELOPMENT_TEAM}"
  XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
  XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER
    "${SKITY_FONT_HARNESS_IOS_TESTS_BUNDLE_ID}"
  XCODE_ATTRIBUTE_SDKROOT "${SKITY_FONT_HARNESS_XCODE_PLATFORM_SDK}"
  XCODE_ATTRIBUTE_SUPPORTED_PLATFORMS "${SKITY_FONT_HARNESS_XCODE_PLATFORM_SDK}"
  XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
  XCODE_ATTRIBUTE_TEST_HOST "$<TARGET_FILE:skity_font_harness_ios_host_app>"
  XCODE_GENERATE_SCHEME TRUE
  XCODE_SCHEME_TEST_CONFIGURATION Debug
)
