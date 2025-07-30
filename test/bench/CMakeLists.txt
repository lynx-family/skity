
add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/google_benchmark third_party/google_benchmark)

add_executable(skity_benchmarks
    matrix_benchmarks.cc
    sw_benchmarks.cc
    ${CMAKE_SOURCE_DIR}/example/case/basic/example.cc
    ${CMAKE_SOURCE_DIR}/example/case/basic/example.hpp
)

target_include_directories(skity_benchmarks
    PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/example
)

target_compile_options(skity_benchmarks PUBLIC -fno-rtti)
target_compile_options(skity_benchmarks PUBLIC -std=c++17)

target_link_libraries(skity_benchmarks
    PUBLIC
    skity::skity
    benchmark::benchmark
)
if (WIN32)
  target_link_libraries(skity_benchmarks PRIVATE glm::glm-header-only)
else()
  target_link_libraries(skity_benchmarks PRIVATE glm::glm)
endif()
