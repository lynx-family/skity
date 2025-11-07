

# glfw
set(GLFW_INSTALL OFF CACHE BOOL "disable GLFW_INSTALL")
add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/glfw third_party/glfw)

add_library(skity_example_common STATIC
    ${CMAKE_CURRENT_LIST_DIR}/app.cc
    ${CMAKE_CURRENT_LIST_DIR}/app.hpp
    ${CMAKE_CURRENT_LIST_DIR}/app_utils.cc
    ${CMAKE_CURRENT_LIST_DIR}/app_utils.hpp
    ${CMAKE_CURRENT_LIST_DIR}/window.cc
    ${CMAKE_CURRENT_LIST_DIR}/window.hpp
)

if (${SKITY_SW_RENDERER})
  target_compile_definitions(skity_example_common PUBLIC -DSKITY_EXAMPLE_SW_BACKEND=1)

  target_sources(skity_example_common PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/sw/window_sw.cc
    ${CMAKE_CURRENT_LIST_DIR}/sw/window_sw.hpp
  )

endif()

if (${SKITY_GL_BACKEND})
  target_compile_definitions(skity_example_common PUBLIC -DSKITY_EXAMPLE_GL_BACKEND=1)

  target_sources(skity_example_common PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/gl/window_gl.cc
    ${CMAKE_CURRENT_LIST_DIR}/gl/window_gl.hpp
  )
endif()

if (${SKITY_MTL_BACKEND})
  target_compile_definitions(skity_example_common PUBLIC -DSKITY_EXAMPLE_MTL_BACKEND=1)

  target_sources(skity_example_common PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/mtl/window_mtl.mm
    ${CMAKE_CURRENT_LIST_DIR}/mtl/window_mtl.hpp
  )

  target_link_libraries(skity_example_common PRIVATE "-framework Metal" "-framework QuartzCore")

endif()

add_library(skity::example_common ALIAS skity_example_common)

target_include_directories(skity_example_common PUBLIC ${CMAKE_SOURCE_DIR}/example)
target_include_directories(skity_example_common PUBLIC ${CMAKE_SOURCE_DIR}/third_party/glad/include)

target_link_libraries(skity_example_common PUBLIC glfw skity::skity)
target_link_libraries(skity_example_common PUBLIC skity::codec)

target_compile_definitions(skity_example_common PRIVATE -DEXAMPLE_DEFAULT_FONT="${CMAKE_SOURCE_DIR}/example/images/RobotoMonoNerdFont-Regular.ttf")
